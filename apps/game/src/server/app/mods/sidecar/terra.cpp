#include "terra.h"
#include "sidecar.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include "src/shared/util/text/string.h"
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

namespace sidecar {

namespace {
std::string outputFile();
std::string slurp(const std::string &path);
}

Terra Terra::locate()
{
	if (const char *given = std::getenv("TERRA_CLI"); given && *given) {
		if (!fs::PathExists(given))
			throw Error(std::string("TERRA_CLI names ") + given + ", and there's nothing there");
		return Terra(given);
	}
	char exe[1024];
	if (!porting::getCurrentExecPath(exe, sizeof exe))
		throw Error("can't tell where this engine is, to find terra beside it");
#ifdef _WIN32
	std::string path = fs::RemoveLastPathComponent(exe) + DIR_DELIM + "terra.exe";
#else
	std::string path = fs::RemoveLastPathComponent(exe) + DIR_DELIM + "terra";
#endif
	if (!fs::PathExists(path))
		throw Error("terra should be beside the engine, and there's none at " + path +
				" (build it: cd apps/cli && bun run build)");
	return Terra(path);
}

#ifdef _WIN32

Terra::Ran Terra::run(const std::vector<std::string> &args, const std::string &cwd) const
{
	const std::string out = outputFile();
	SECURITY_ATTRIBUTES inherit{ sizeof inherit, nullptr, TRUE };
	HANDLE file = CreateFileW(utf8_to_wide(out).c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit,
			CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		throw Error("can't make a file for terra's output: " + porting::ConvertError(GetLastError()));

	std::string cmdline = porting::QuoteArgv(m_path);
	for (const std::string &a : args)
		cmdline += " " + porting::QuoteArgv(a);
	std::wstring wcmd = utf8_to_wide(cmdline);
	STARTUPINFOW si{};
	si.cb = sizeof si;
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = file;
	si.hStdError = file;
	PROCESS_INFORMATION pi{};
	const BOOL ok = CreateProcessW(utf8_to_wide(m_path).c_str(), wcmd.data(), nullptr, nullptr, TRUE,
			CREATE_NO_WINDOW, nullptr, utf8_to_wide(cwd).c_str(), &si, &pi);
	const DWORD spawn_error = GetLastError();
	CloseHandle(file);
	if (!ok) {
		fs::DeleteSingleFileOrEmptyDirectory(out);
		throw Error("can't start " + m_path + ": " + porting::ConvertError(spawn_error));
	}
	CloseHandle(pi.hThread);
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD code = 0;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hProcess);
	Ran ran{ static_cast<int>(code), slurp(out) };
	fs::DeleteSingleFileOrEmptyDirectory(out);
	return ran;
}

#else

Terra::Ran Terra::run(const std::vector<std::string> &args, const std::string &cwd) const
{
	const std::string out = outputFile();
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
	posix_spawn_file_actions_addopen(&actions, 1, out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	posix_spawn_file_actions_adddup2(&actions, 1, 2);
	posix_spawn_file_actions_addchdir_np(&actions, cwd.c_str());

	std::vector<std::string> owned{ m_path };
	owned.insert(owned.end(), args.begin(), args.end());
	std::vector<char *> argv;
	for (std::string &a : owned)
		argv.push_back(a.data());
	argv.push_back(nullptr);

	pid_t pid = 0;
	const int err = ::posix_spawn(&pid, m_path.c_str(), &actions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	if (err != 0) {
		fs::DeleteSingleFileOrEmptyDirectory(out);
		throw Error("can't start " + m_path + ": " + std::strerror(err));
	}
	int status = 0;
	while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
	Ran ran{ WIFEXITED(status) ? WEXITSTATUS(status) : -1, slurp(out) };
	fs::DeleteSingleFileOrEmptyDirectory(out);
	return ran;
}

#endif

namespace {

// one per run, so two at once (two windows, or a test beside the game) never share
std::string outputFile()
{
	static std::atomic<unsigned> made{ 0 };
	return (std::filesystem::temp_directory_path() /
			("terra-run-" + std::to_string(porting::getTimeMs()) + "-" + std::to_string(made++) + ".log")).string();
}

std::string slurp(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

} // namespace

} // namespace sidecar
