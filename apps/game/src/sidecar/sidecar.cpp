#include "sidecar.h"

#include "log.h"
#include "msgpack.h"
#include "ops.h"
#include "porting.h"

#include <chrono>

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
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

} // namespace

#ifdef _WIN32

struct Sidecar::Process {};

Sidecar::Sidecar(std::string terra, std::string folder, Host host) :
	m_terra(std::move(terra)), m_folder(std::move(folder)), m_host(std::move(host))
{
	throw Error("TypeScript mods aren't wired on Windows yet (they need named pipes)");
}
Sidecar::~Sidecar() = default;
void Sidecar::joined(const std::string &) {}
void Sidecar::left(const std::string &) {}
void Sidecar::step(float, double) {}

#else

struct Sidecar::Process {
	pid_t pid = -1;
	int fd = -1;
	std::string in;  // bytes read, not yet a whole frame

	~Process() { kill(); }

	void kill()
	{
		if (fd >= 0) {
			::close(fd);
			fd = -1;
		}
		if (pid > 0) {
			::kill(pid, SIGKILL);
			::waitpid(pid, nullptr, 0);
			pid = -1;
		}
	}

	void send(const std::string &bytes)
	{
		size_t sent = 0;
		while (sent < bytes.size()) {
			ssize_t n = ::send(fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
			if (n < 0 && errno == EINTR)
				continue;
			if (n <= 0)
				throw Dead(std::string("it stopped listening (") + std::strerror(errno) + ")");
			sent += n;
		}
	}

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
			pollfd p{ fd, POLLIN, 0 };
			int r = ::poll(&p, 1, (int)left);
			if (r < 0 && errno == EINTR)
				continue;
			if (r < 0)
				throw Dead(std::string("poll: ") + std::strerror(errno));
			if (r == 0)
				continue;
			char buf[64 * 1024];
			ssize_t n = ::read(fd, buf, sizeof buf);
			if (n < 0 && errno == EINTR)
				continue;
			if (n <= 0)
				throw Dead(exitReason());
			in.append(buf, n);
		}
	}

	// Why it's gone, once its end of the socket closed.
	std::string exitReason()
	{
		int status = 0;
		// it closed its socket: give it a moment to finish exiting
		for (int i = 0; i < 100 && pid > 0; i++) {
			pid_t r = ::waitpid(pid, &status, WNOHANG);
			if (r == pid) {
				pid = -1;
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
	} catch (const Dead &) {
		// already gone
	}
	for (int i = 0; i < 50 && m_process->pid > 0; i++) {
		if (::waitpid(m_process->pid, nullptr, WNOHANG) == m_process->pid)
			m_process->pid = -1;
		else
			usleep(10'000);
	}
	m_process.reset();
}

// When .terra/manifest.json was last written: prepare writes it last, so a change means
// the mods were prepared again and are whole on disk.
std::string Sidecar::manifestTime() const
{
	struct stat st;
	if (::stat((m_folder + "/.terra/manifest.json").c_str(), &st) != 0)
		return "";
	return std::to_string(st.st_mtim.tv_sec) + "." + std::to_string(st.st_mtim.tv_nsec);
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
	auto process = std::make_unique<Process>();
	int sv[2];
	if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0)
		throw Error(std::string("socketpair: ") + std::strerror(errno));
	process->fd = sv[0];
	// The sidecar's end becomes its fd 3. dup2 onto itself would keep close-on-exec.
	int child = sv[1];
	if (child == 3) {
		child = ::fcntl(3, F_DUPFD_CLOEXEC, 4);
		::close(3);
	}
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, child, 3);
	std::string runtime = "runtime";
	std::vector<char *> argv{ m_terra.data(), runtime.data(), m_folder.data(), nullptr };
	int err = ::posix_spawn(&process->pid, m_terra.c_str(), &actions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	::close(child);
	if (err != 0) {
		process->pid = -1;
		throw Error("can't start " + m_terra + ": " + std::strerror(err));
	}

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
			const mp::Value &p = f.payload;
			if (p.at("protocol").asInt() != PROTOCOL)
				throw Dead("it speaks protocol " + std::to_string(p.at("protocol").asInt()) +
						", this engine " + std::to_string(PROTOCOL) + ": the engine and terra are from different builds");
			for (const mp::Value &b : p.at("blocks").asArray()) {
				Content::Block block;
				block.name = b.at("name").asString();
				block.description = b.at("description").asString();
				const auto &tiles = b.at("tiles").asArray();
				if (tiles.size() != 6)
					throw Dead(block.name + ": a block has six faces");
				for (size_t i = 0; i < 6; i++)
					block.tiles[i] = tiles[i].asString();
				block.matter = b.at("matter").asString();
				block.share = b.at("share").asNumber();
				content.blocks.push_back(std::move(block));
			}
			for (const mp::Value &m : p.at("media").asArray())
				content.media.push_back({ m.at("name").asString(), m.at("path").asString() });
		} catch (const mp::Error &e) {
			throw Dead(std::string("its content isn't what the engine reads: ") + e.what());
		}

		// what a restarted sidecar must match: its declarations, in order
		std::string fingerprint;
		for (const auto &b : content.blocks) {
			fingerprint += b.name + '\n' + b.matter + '\n' + std::to_string(b.share) + '\n';
			for (const auto &t : b.tiles)
				fingerprint += t + '\n';
		}
		for (const auto &m : content.media)
			fingerprint += m.name + '\n' + m.path + '\n';

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
			w.array(2);
			w.integer(e.kind);
			w.string(e.player);
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

#endif

} // namespace sidecar
