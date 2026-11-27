// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/system/spawn.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <crt_externs.h>
#define SPAWN_ENVIRON (*_NSGetEnviron())
#else
extern char **environ;
#define SPAWN_ENVIRON environ
#endif
#endif

namespace spawn {

#ifdef _WIN32
namespace {
	// One argument as CreateProcess's command line parses it back (CommandLineToArgvW's rules)
	std::string quoted(const std::string &arg);
}

bool detached(const std::vector<std::string> &argv)
{
	if (argv.empty())
		return false;
	std::string line;
	for (const std::string &arg : argv)
		line += (line.empty() ? "" : " ") + quoted(arg);
	STARTUPINFOA startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	if (!CreateProcessA(argv[0].c_str(), line.data(), nullptr, nullptr, FALSE,
			DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &startup, &process))
		return false;
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

namespace {

std::string quoted(const std::string &arg)
{
	if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos)
		return arg;
	std::string out = "\"";
	size_t slashes = 0;
	for (char c : arg) {
		if (c == '\\') {
			slashes++;
			continue;
		}
		out.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
		slashes = 0;
		out += c;
	}
	out.append(slashes * 2, '\\');
	return out + "\"";
}

} // namespace
#else
bool detached(const std::vector<std::string> &argv)
{
	if (argv.empty())
		return false;
	std::vector<char *> args;
	for (const std::string &arg : argv)
		args.push_back(const_cast<char *>(arg.c_str())); // (posix_spawn's signature; it doesn't write them)
	args.push_back(nullptr);
	pid_t pid;
	// its own process group, so a Ctrl-C meant for this one doesn't end it
	posix_spawnattr_t attr;
	posix_spawnattr_init(&attr);
	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&attr, 0);
	const int failed = posix_spawn(&pid, argv[0].c_str(), nullptr, &attr, args.data(), SPAWN_ENVIRON);
	posix_spawnattr_destroy(&attr);
	return failed == 0;
}
#endif

} // namespace spawn
