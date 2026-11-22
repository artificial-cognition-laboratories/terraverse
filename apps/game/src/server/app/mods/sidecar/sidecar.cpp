#include "src/server/app/mods/sidecar/sidecar.h"

#include "src/shared/platform/system/log.h"
#include "src/server/app/mods/sidecar/msgpack.h"
#include "src/server/app/mods/sidecar/ops.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/util/text/string.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <cstdio>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <spawn.h>
#include "src/shared/platform/network/socket.h"
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace sidecar {

namespace {

using Clock = std::chrono::steady_clock;

// How long mods may take. Over the first, the server says so; over the last, the
// sidecar is taken to have hung, and is killed and restarted.
constexpr int LOAD_MS = 60'000;
constexpr int WINDOW_WARN_MS = 50;
constexpr int WINDOW_HUNG_MS = 10'000;
// Deaths within a minute of game time before mods are left off
constexpr size_t DEATHS_TO_GIVE_UP = 5;
// How long a sidecar told to stop has before it's killed
constexpr int STOP_MS = 500;

struct Frame {
	Op op;
	u32 id;
	mp::Value payload;
};

// The sidecar is gone: it exited, crashed, hung, or broke the protocol.
struct Dead : Error {
	using Error::Error;
};

std::string frameBytes(Op op, u32 id, const std::function<void(mp::Writer &)> &payload)
{
	mp::Writer w;
	w.array(3);
	w.integer((s64)op);
	w.integer(id);
	payload(w);
	const std::string &body = w.bytes();
	std::string frame(4, '\0');
	const u32 n = (u32)body.size();
	for (int i = 0; i < 4; i++)
		frame[i] = (char)((n >> (i * 8)) & 0xff);
	return frame + body;
}

#ifdef _WIN32

// `terra runtime <folder> --pipe <name>`, joined by a named pipe only it may open. A
// Windows process gets no fd 3, so the engine names the pipe and the sidecar opens it.
// The engine's end is overlapped, so a read can wait with a deadline.
class Child {
public:
	Child(const std::string &terra, const std::string &folder)
	{
		try {
			spawn(terra, folder);
		} catch (...) {
			release();
			throw;
		}
	}

	~Child() { release(); }

	Child(const Child &) = delete;
	Child &operator=(const Child &) = delete;

private:
	// What the constructor does: anything it made before throwing, release() undoes.
	void spawn(const std::string &terra, const std::string &folder)
	{
		static std::atomic<u32> made{ 0 };
		const std::string name = "\\\\.\\pipe\\terraverse-sidecar-" + std::to_string(GetCurrentProcessId()) +
				"-" + std::to_string(made++) + "-" + std::to_string(GetTickCount64());
		// only one instance, and the first: nobody else can be serving this name. Remote
		// clients are refused; a local one that isn't our child is refused on connect.
		m_pipe = CreateNamedPipeW(utf8_to_wide(name).c_str(),
				PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
				PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
				1, 64 * 1024, 64 * 1024, 0, nullptr);
		if (m_pipe == INVALID_HANDLE_VALUE)
			throw Error("can't make a pipe for it: " + porting::ConvertError(GetLastError()));
		for (HANDLE *e : { &m_connect.hEvent, &m_read.hEvent, &m_write.hEvent }) {
			*e = CreateEventW(nullptr, TRUE, FALSE, nullptr);
			if (!*e)
				throw Error("CreateEvent: " + porting::ConvertError(GetLastError()));
		}

		// listen before it starts, so its open finds us
		if (ConnectNamedPipe(m_pipe, &m_connect)) {
			m_connected = true;
		} else {
			const DWORD e = GetLastError();
			if (e == ERROR_PIPE_CONNECTED)
				m_connected = true;
			else if (e == ERROR_IO_PENDING)
				m_connecting = true;
			else
				throw Error("ConnectNamedPipe: " + porting::ConvertError(e));
		}

		std::string cmdline = porting::QuoteArgv(terra) + " runtime " + porting::QuoteArgv(folder) + " --pipe " + porting::QuoteArgv(name);
		std::wstring wcmd = utf8_to_wide(cmdline);

		// what it prints goes where ours does: a crash's last words. Only those handles
		// are inherited, never whatever else the engine has open.
		STARTUPINFOEXW si{};
		si.StartupInfo.cb = sizeof si;
		std::vector<HANDLE> inherit;
		auto inheritable = [&](DWORD which) -> HANDLE {
			HANDLE h = GetStdHandle(which);
			if (!h || h == INVALID_HANDLE_VALUE)
				return nullptr;
			HANDLE dup = nullptr;
			if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &dup, 0, TRUE, DUPLICATE_SAME_ACCESS))
				return nullptr;
			inherit.push_back(dup);
			return dup;
		};
		si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
		si.StartupInfo.hStdInput = nullptr;
		si.StartupInfo.hStdOutput = inheritable(STD_OUTPUT_HANDLE);
		si.StartupInfo.hStdError = inheritable(STD_ERROR_HANDLE);

		// the list and our copies of the handles, gone however this ends
		std::vector<char> attrs;
		LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
		struct Done {
			std::vector<HANDLE> &inherit;
			LPPROC_THREAD_ATTRIBUTE_LIST &list;
			~Done()
			{
				if (list)
					DeleteProcThreadAttributeList(list);
				for (HANDLE h : inherit)
					CloseHandle(h);
			}
		} done{ inherit, list };
		if (!inherit.empty()) {
			SIZE_T size = 0;
			InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
			attrs.resize(size);
			if (!InitializeProcThreadAttributeList(reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data()), 1, 0, &size))
				throw Error("can't hand it our output: " + porting::ConvertError(GetLastError()));
			list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
			if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
						inherit.data(), inherit.size() * sizeof(HANDLE), nullptr, nullptr))
				throw Error("can't hand it our output: " + porting::ConvertError(GetLastError()));
			si.lpAttributeList = list;
		}

		PROCESS_INFORMATION pi{};
		// no window of its own: the engine may be a windowed program with no console
		const BOOL ok = CreateProcessW(utf8_to_wide(terra).c_str(), wcmd.data(), nullptr, nullptr,
				inherit.empty() ? FALSE : TRUE, CREATE_NO_WINDOW | (list ? EXTENDED_STARTUPINFO_PRESENT : 0),
				nullptr, nullptr, &si.StartupInfo, &pi);
		const DWORD spawn_error = GetLastError();
		if (!ok)
			throw Error("can't start " + terra + ": " + porting::ConvertError(spawn_error));
		CloseHandle(pi.hThread);
		m_process = pi.hProcess;
		m_pid = pi.dwProcessId;
	}

public:
	void send(const std::string &bytes)
	{
		if (!m_connected)
			throw Dead("it never opened its pipe");
		size_t sent = 0;
		while (sent < bytes.size()) {
			DWORD n = 0;
			ResetEvent(m_write.hEvent);
			if (!WriteFile(m_pipe, bytes.data() + sent, (DWORD)(bytes.size() - sent), nullptr, &m_write) &&
					GetLastError() != ERROR_IO_PENDING)
				throw Dead("it stopped listening (" + porting::ConvertError(GetLastError()) + ")");
			if (!GetOverlappedResult(m_pipe, &m_write, &n, TRUE))
				throw Dead("it stopped listening (" + porting::ConvertError(GetLastError()) + ")");
			sent += n;
		}
	}

	// Up to `cap` bytes, waiting at most `ms`: 0 if none came. Throws Dead once it's gone.
	size_t receive(char *buf, size_t cap, int ms)
	{
		const auto until = Clock::now() + std::chrono::milliseconds(ms);
		if (!m_connected) {
			// until it opens the pipe, the pipe can't say it died: watch the process too
			HANDLE both[2] = { m_connect.hEvent, m_process };
			const DWORD r = WaitForMultipleObjects(2, both, FALSE, (DWORD)ms);
			if (r == WAIT_TIMEOUT)
				return 0;
			if (r == WAIT_OBJECT_0 + 1)
				throw Dead(exitReason());
			if (r != WAIT_OBJECT_0)
				throw Dead("waiting for it to connect: " + porting::ConvertError(GetLastError()));
			DWORD unused = 0;
			m_connecting = false;
			if (!GetOverlappedResult(m_pipe, &m_connect, &unused, FALSE))
				throw Dead("it couldn't connect: " + porting::ConvertError(GetLastError()));
			// the pipe's name is guessable: make sure it's ours that opened it
			ULONG client = 0;
			if (!GetNamedPipeClientProcessId(m_pipe, &client) || client != m_pid)
				throw Dead("something other than it opened its pipe");
			m_connected = true;
			ms = (int)std::max<s64>(0, std::chrono::duration_cast<std::chrono::milliseconds>(until - Clock::now()).count());
		}
		// a read outlives a wait that times out, so its buffer is ours, not the caller's
		if (!m_reading) {
			ResetEvent(m_read.hEvent);
			if (!ReadFile(m_pipe, m_buf, sizeof m_buf, nullptr, &m_read)) {
				const DWORD e = GetLastError();
				if (e == ERROR_BROKEN_PIPE)
					throw Dead(exitReason());
				if (e != ERROR_IO_PENDING)
					throw Dead("reading from it: " + porting::ConvertError(e));
			}
			m_reading = true;
		}
		const DWORD w = WaitForSingleObject(m_read.hEvent, (DWORD)ms);
		if (w == WAIT_TIMEOUT)
			return 0;
		DWORD n = 0;
		m_reading = false;
		if (!GetOverlappedResult(m_pipe, &m_read, &n, FALSE)) {
			const DWORD e = GetLastError();
			if (e == ERROR_BROKEN_PIPE)
				throw Dead(exitReason());
			throw Dead("reading from it: " + porting::ConvertError(e));
		}
		n = std::min<DWORD>(n, (DWORD)cap);
		memcpy(buf, m_buf, n);
		return n;
	}

	// Whether it has exited, waiting up to `ms` for it to.
	bool exited(int ms)
	{
		return !m_process || WaitForSingleObject(m_process, (DWORD)ms) == WAIT_OBJECT_0;
	}

private:
	HANDLE m_pipe = INVALID_HANDLE_VALUE;
	HANDLE m_process = nullptr;
	DWORD m_pid = 0;
	OVERLAPPED m_connect{}, m_read{}, m_write{};
	bool m_connected = false;
	bool m_connecting = false;  // a connect is pending on m_connect
	bool m_reading = false;     // a read is pending on m_read, into m_buf
	char m_buf[64 * 1024];

	// Kills it if it's still running, and closes everything.
	void release()
	{
		if (m_pipe != INVALID_HANDLE_VALUE) {
			// nothing pending may still write into m_buf, or signal events about to close.
			// Writes never are: send waits for each.
			CancelIoEx(m_pipe, nullptr);
			DWORD unused = 0;
			if (m_connecting)
				GetOverlappedResult(m_pipe, &m_connect, &unused, TRUE);
			if (m_reading)
				GetOverlappedResult(m_pipe, &m_read, &unused, TRUE);
			m_connecting = m_reading = false;
			CloseHandle(m_pipe);
			m_pipe = INVALID_HANDLE_VALUE;
		}
		if (m_process) {
			if (WaitForSingleObject(m_process, 0) != WAIT_OBJECT_0) {
				TerminateProcess(m_process, 1);
				WaitForSingleObject(m_process, INFINITE);
			}
			CloseHandle(m_process);
			m_process = nullptr;
		}
		for (HANDLE *e : { &m_connect.hEvent, &m_read.hEvent, &m_write.hEvent })
			if (*e) {
				CloseHandle(*e);
				*e = nullptr;
			}
	}

	// Why it's gone, once its end of the pipe closed.
	std::string exitReason()
	{
		// it closed its pipe: give it a moment to finish exiting
		if (WaitForSingleObject(m_process, 1000) != WAIT_OBJECT_0)
			return "it closed its pipe";
		DWORD code = 0;
		if (!GetExitCodeProcess(m_process, &code))
			return "it exited";
		// an NTSTATUS (0xC0000005, an access violation) reads better in hex
		if (code >= 0xC0000000u) {
			char hex[16];
			snprintf(hex, sizeof hex, "0x%08lX", (unsigned long)code);
			return std::string("it crashed with ") + hex;
		}
		return "it exited with status " + std::to_string(code);
	}
};

#else

// `terra runtime <folder>`, with its end of a socket pair as fd 3.
class Child {
public:
	Child(const std::string &terra, const std::string &folder)
	{
		int sv[2];
#ifdef SOCK_CLOEXEC
		if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0)
			throw Error(std::string("socketpair: ") + std::strerror(errno));
#else
		// macOS has no SOCK_CLOEXEC: set it just after (a process another thread started in
		// between could inherit the pair; the engine starts none but this one).
		if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
			throw Error(std::string("socketpair: ") + std::strerror(errno));
		::fcntl(sv[0], F_SETFD, FD_CLOEXEC);
		::fcntl(sv[1], F_SETFD, FD_CLOEXEC);
#endif
		m_fd = sv[0];
		// The sidecar's end becomes its fd 3. dup2 onto itself would keep close-on-exec.
		int child = sv[1];
		if (child == 3) {
			child = ::fcntl(3, F_DUPFD_CLOEXEC, 4);
			::close(3);
		}
		posix_spawn_file_actions_t actions;
		posix_spawn_file_actions_init(&actions);
		posix_spawn_file_actions_adddup2(&actions, child, 3);
		std::string program = terra, runtime = "runtime", where = folder;
		std::vector<char *> argv{ program.data(), runtime.data(), where.data(), nullptr };
		int err = ::posix_spawn(&m_pid, terra.c_str(), &actions, nullptr, argv.data(), environ);
		posix_spawn_file_actions_destroy(&actions);
		::close(child);
		if (err != 0) {
			m_pid = -1;
			::close(m_fd);
			m_fd = -1;
			throw Error("can't start " + terra + ": " + std::strerror(err));
		}
	}

	~Child() { kill(); }

	Child(const Child &) = delete;
	Child &operator=(const Child &) = delete;

	void send(const std::string &bytes)
	{
		size_t sent = 0;
		while (sent < bytes.size()) {
			ssize_t n = ::send(m_fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
			if (n < 0 && errno == EINTR)
				continue;
			if (n <= 0)
				throw Dead(std::string("it stopped listening (") + std::strerror(errno) + ")");
			sent += n;
		}
	}

	// Up to `cap` bytes, waiting at most `ms`: 0 if none came. Throws Dead once it's gone.
	size_t receive(char *buf, size_t cap, int ms)
	{
		pollfd p{ m_fd, POLLIN, 0 };
		int r = ::poll(&p, 1, ms);
		if (r < 0 && errno == EINTR)
			return 0;
		if (r < 0)
			throw Dead(std::string("poll: ") + std::strerror(errno));
		if (r == 0)
			return 0;
		ssize_t n = ::read(m_fd, buf, cap);
		if (n < 0 && errno == EINTR)
			return 0;
		if (n <= 0)
			throw Dead(exitReason());
		return (size_t)n;
	}

	// Whether it has exited, waiting up to `ms` for it to.
	bool exited(int ms)
	{
		for (int waited = 0; m_pid > 0; waited += 10) {
			if (::waitpid(m_pid, nullptr, WNOHANG) == m_pid) {
				m_pid = -1;
				break;
			}
			if (waited >= ms)
				return false;
			usleep(10'000);
		}
		return true;
	}

private:
	pid_t m_pid = -1;
	int m_fd = -1;

	void kill()
	{
		if (m_fd >= 0) {
			::close(m_fd);
			m_fd = -1;
		}
		if (m_pid > 0) {
			::kill(m_pid, SIGKILL);
			::waitpid(m_pid, nullptr, 0);
			m_pid = -1;
		}
	}

	// Why it's gone, once its end of the socket closed.
	std::string exitReason()
	{
		int status = 0;
		// it closed its socket: give it a moment to finish exiting
		for (int i = 0; i < 100 && m_pid > 0; i++) {
			pid_t r = ::waitpid(m_pid, &status, WNOHANG);
			if (r == m_pid) {
				m_pid = -1;
				if (WIFEXITED(status))
					return "it exited with status " + std::to_string(WEXITSTATUS(status));
				if (WIFSIGNALED(status))
					return std::string("it was killed by ") + strsignal(WTERMSIG(status));
				return "it exited";
			}
			usleep(10'000);
		}
		return "it closed its socket";
	}
};

#endif

} // namespace

// The running sidecar: its process, and the frames it sends.
struct Sidecar::Process {
	Child child;
	std::string in;  // bytes read, not yet a whole frame

	Process(const std::string &terra, const std::string &folder) : child(terra, folder) {}

	void send(const std::string &bytes) { child.send(bytes); }

	// The next frame, waiting until `deadline`. Throws Dead if it doesn't come.
	Frame read(Clock::time_point deadline, const char *waiting_for)
	{
		for (;;) {
			if (in.size() >= 4) {
				const u32 n = (u8)in[0] | (u8)in[1] << 8 | (u8)in[2] << 16 | (u32)(u8)in[3] << 24;
				if (in.size() >= 4 + (size_t)n) {
					mp::Value v;
					try {
						v = mp::decode((const u8 *)in.data() + 4, n);
					} catch (const mp::Error &e) {
						throw Dead(std::string("it sent a frame that isn't MessagePack: ") + e.what());
					}
					in.erase(0, 4 + (size_t)n);
					const auto &parts = v.kind() == mp::Value::Kind::Array ? v.asArray() : std::vector<mp::Value>{};
					if (parts.size() != 3 || parts[0].kind() != mp::Value::Kind::Int || parts[1].kind() != mp::Value::Kind::Int)
						throw Dead("it sent a frame that isn't [op, id, payload]");
					return Frame{ (Op)parts[0].asInt(), (u32)parts[1].asInt(), parts[2] };
				}
			}
			const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
			if (left <= 0)
				throw Dead(std::string("it hung, waiting for ") + waiting_for);
			char buf[64 * 1024];
			in.append(buf, child.receive(buf, sizeof buf, (int)left));
		}
	}
};

Sidecar::Sidecar(std::string terra, std::string folder, Host host) :
	m_terra(std::move(terra)), m_folder(std::move(folder)), m_host(std::move(host))
{
	m_manifest_time = manifestTime();
	try {
		start();
	} catch (const Dead &e) {
		throw Error("the mods couldn't load: " + std::string(e.what()) + ". What they said is above.");
	}
}

Sidecar::~Sidecar()
{
	stop();
}

// Ask it to stop, give it a moment, then let Process kill it.
void Sidecar::stop()
{
	if (!m_process)
		return;
	try {
		m_process->send(frameBytes(Op::Stop, 0, [](mp::Writer &w) { w.nil(); }));
		m_process->child.exited(STOP_MS);
	} catch (const Dead &) {
		// already gone
	}
	m_process.reset();
}

// When .terra/manifest.json was last written: prepare writes it last, so a change means
// the mods were prepared again and are whole on disk.
std::string Sidecar::manifestTime() const
{
	const std::string path = m_folder + "/.terra/manifest.json";
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExW(utf8_to_wide(path).c_str(), GetFileExInfoStandard, &data))
		return "";
	const FILETIME &t = data.ftLastWriteTime;
	return std::to_string(((u64)t.dwHighDateTime << 32) | t.dwLowDateTime);
#else
	struct stat st;
	if (::stat(path.c_str(), &st) != 0)
		return "";
#ifdef __APPLE__
	const timespec &mtime = st.st_mtimespec; // Darwin's name for st_mtim
#else
	const timespec &mtime = st.st_mtim;
#endif
	return std::to_string(mtime.tv_sec) + "." + std::to_string(mtime.tv_nsec);
#endif
}

// Prepared again: a new sidecar on the new build. Same content, and the universe never
// notices; different content, and it has to restart (needsRestart).
void Sidecar::reload()
{
	actionstream << "mods: prepared again, reloading" << std::endl;
	stop();
	// a reload is a fresh chance: mods left off after crashing get one
	m_off = false;
	m_deaths.clear();
	try {
		start();
		actionstream << (m_content_changed ? "mods: reloaded, with new content" : "mods: reloaded") << std::endl;
	} catch (const Error &e) {
		errorstream << "mods: can't reload: " << e.what() << ". Mods are off until they're prepared again." << std::endl;
		m_process.reset();
		m_off = true;
	}
}

void Sidecar::start()
{
	auto process = std::make_unique<Process>(m_terra, m_folder);

	// Logs while the mods load, then their content.
	const auto deadline = Clock::now() + std::chrono::milliseconds(LOAD_MS);
	for (;;) {
		Frame f = process->read(deadline, "its mods to load");
		if (f.op == Op::Log) {
			handleLog(f.payload);
			continue;
		}
		if (f.op != Op::Content)
			throw Dead("it sent op " + std::to_string((int)f.op) + " before its content");

		Content content;
		try {
			content = sidecar::content::read(f.payload);
		} catch (const Error &e) {
			throw Dead(e.what());
		}
		const std::string fingerprint = sidecar::content::fingerprint(content);

		if (!m_has_content) {
			m_content = std::move(content);
			m_content_bytes = fingerprint;
			m_has_content = true;
		} else if (fingerprint != m_content_bytes) {
			warningstream << "mods: they declare different content than the universe started with: it needs a restart" << std::endl;
			m_content_changed = true;
		}
		break;
	}
	m_process = std::move(process);
}

void Sidecar::restart(const std::string &why)
{
	errorstream << "mods: the sidecar stopped: " << why << std::endl;
	m_process.reset();
	m_deaths.push_back(m_now);
	while (!m_deaths.empty() && m_deaths.front() < m_now - 60)
		m_deaths.erase(m_deaths.begin());
	if (m_deaths.size() >= DEATHS_TO_GIVE_UP) {
		errorstream << "mods: the sidecar stopped " << m_deaths.size()
				<< " times in a minute. Mods are off until the server restarts." << std::endl;
		m_off = true;
		return;
	}
	try {
		start();
		actionstream << "mods: restarted" << std::endl;
	} catch (const Error &e) {
		errorstream << "mods: can't restart: " << e.what() << ". Mods are off until the server restarts." << std::endl;
		m_process.reset();
		m_off = true;
	}
}

void Sidecar::joined(const std::string &player)
{
	m_events.push_back({ (u8)Event::Join, player });
}

void Sidecar::left(const std::string &player)
{
	m_events.push_back({ (u8)Event::Leave, player });
}

bool Sidecar::command(const std::string &player, const std::string &name, const std::string &text)
{
	if (m_off || !m_process)
		return false;
	m_events.push_back({ (u8)Event::Command, player, name, text });
	return true;
}

void Sidecar::step(float dtime, double now)
{
	m_now = now;
	if (Clock::now() >= m_next_look) {
		m_next_look = Clock::now() + std::chrono::seconds(1);
		if (const std::string t = manifestTime(); !t.empty() && t != m_manifest_time) {
			m_manifest_time = t;
			reload();
		}
	}
	if (m_off || !m_process) {
		m_events.clear();
		return;
	}
	try {
		window(dtime, now);
	} catch (const Dead &e) {
		// the events it was handed are lost with it: what happened, happened
		restart(e.what());
	}
}

void Sidecar::window(float dtime, double now)
{
	const auto started = Clock::now();
	std::vector<Happened> events;
	events.swap(m_events);
	m_process->send(frameBytes(Op::Step, 0, [&](mp::Writer &w) {
		w.map(3);
		w.string("now");
		w.number(now);
		w.string("dt");
		w.number(dtime);
		w.string("events");
		w.array((u32)events.size());
		for (const Happened &e : events) {
			const bool command = e.kind == (u8)Event::Command;
			w.array(command ? 4 : 2);
			w.integer(e.kind);
			w.string(e.player);
			if (command) {
				w.string(e.command);
				w.string(e.text);
			}
		}
	}));

	const auto deadline = started + std::chrono::milliseconds(WINDOW_HUNG_MS);
	for (;;) {
		Frame f = m_process->read(deadline, "a window to close");
		if (f.op == Op::Idle)
			break;
		if (f.op == Op::Log) {
			handleLog(f.payload);
			continue;
		}
		if (f.op == Op::Call) {
			handleCall(f.id, f.payload);
			continue;
		}
		throw Dead("it sent op " + std::to_string((int)f.op) + " inside a window");
	}

	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
	if (ms > WINDOW_WARN_MS)
		warningstream << "mods: took " << ms << " ms in one step" << std::endl;
}

// [level, mod, text]: a mod's line, into the server's log under its name.
void Sidecar::handleLog(const mp::Value &message)
{
	try {
		const auto &parts = message.asArray();
		if (parts.size() != 3)
			throw mp::Error("a log line is [level, mod, text]");
		const std::string &mod = parts[1].asString();
		const std::string line = "mods: " + (mod.empty() ? std::string() : "[" + mod + "] ") + parts[2].asString();
		switch ((Level)parts[0].asInt()) {
		case Level::Error: errorstream << line << std::endl; break;
		case Level::Warn: warningstream << line << std::endl; break;
		default: actionstream << line << std::endl; break;
		}
	} catch (const mp::Error &e) {
		throw Dead(std::string("it sent a log line the engine can't read: ") + e.what());
	}
}

// [call, ...args], answered when id isn't 0.
void Sidecar::handleCall(u32 id, const mp::Value &message)
{
	auto reply = [&](bool ok, const std::function<void(mp::Writer &)> &value) {
		if (id == 0) {
			if (!ok)
				errorstream << "mods: a call failed with nobody to tell" << std::endl;
			return;
		}
		m_process->send(frameBytes(Op::Reply, id, [&](mp::Writer &w) {
			w.array(2);
			w.boolean(ok);
			value(w);
		}));
	};

	try {
		const auto &args = message.asArray();
		if (args.empty())
			throw mp::Error("a call names what it calls");
		const s64 call = args[0].asInt();
		auto arg = [&](size_t i) -> const mp::Value & {
			if (i + 1 >= args.size())
				throw mp::Error("call " + std::to_string(call) + " needs argument " + std::to_string(i));
			return args[i + 1];
		};
		switch ((Call)call) {
		case Call::PlayerTell:
			m_host.tell(arg(0).asString(), arg(1).asString());
			reply(true, [](mp::Writer &w) { w.nil(); });
			return;
		case Call::GameAnnounce:
			m_host.announce(arg(0).asString());
			reply(true, [](mp::Writer &w) { w.nil(); });
			return;
		case Call::CommandsSet: {
			std::vector<Host::Command> commands;
			for (const mp::Value &c : arg(0).asArray()) {
				const auto &parts = c.asArray();
				if (parts.size() != 3)
					throw mp::Error("a command is [name, description, params]");
				commands.push_back({ parts[0].asString(), parts[1].asString(), parts[2].asString() });
			}
			m_host.commands(commands);
			reply(true, [](mp::Writer &w) { w.nil(); });
			return;
		}
		case Call::PlayerCan: {
			const bool may = m_host.can(arg(0).asString(), arg(1).asString());
			reply(true, [&](mp::Writer &w) { w.boolean(may); });
			return;
		}
		case Call::PlayerKick:
			m_host.kick(arg(0).asString(), arg(1).asString());
			reply(true, [](mp::Writer &w) { w.nil(); });
			return;
		case Call::PlayersList: {
			const std::vector<std::string> names = m_host.players();
			reply(true, [&](mp::Writer &w) {
				w.array((u32)names.size());
				for (const std::string &n : names)
					w.string(n);
			});
			return;
		}
		}
		throw mp::Error("no call " + std::to_string(call));
	} catch (const mp::Error &e) {
		// the protocol is generated for both sides: a malformed call is a bug in terra
		throw Dead(std::string("it made a call the engine can't read: ") + e.what());
	}
}

} // namespace sidecar
