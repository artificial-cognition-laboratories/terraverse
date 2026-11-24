// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/home/browser.h"

#include <cerrno>
#include <cstdio>
#include <filesystem>
#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif

namespace home {

namespace {
bool inUse(const std::string &dir);
} // namespace

std::string freeBrowserCache(const std::string &base)
{
	if (!inUse(base))
		return base;
	for (int n = 2;; n++) {
		const std::string dir = base + "-" + std::to_string(n);
		if (!inUse(dir))
			return dir;
	}
}

namespace {

bool inUse(const std::string &dir)
{
	namespace fs = std::filesystem;
#ifdef _WIN32
	// Chromium holds `lockfile` open while it runs: a file in use can't be removed
	const fs::path lock = fs::path(dir) / "lockfile";
	if (!fs::exists(lock))
		return false;
	return std::remove(lock.string().c_str()) != 0;
#else
	// SingletonLock is a link to "<host>-<pid>": in use while that process lives on this machine
	std::error_code ec;
	const fs::path lock = fs::path(dir) / "SingletonLock";
	if (!fs::is_symlink(fs::symlink_status(lock, ec)))
		return false;
	const std::string target = fs::read_symlink(lock, ec).string();
	const size_t dash = target.rfind('-');
	if (ec || dash == std::string::npos)
		return false;
	char host[256] = {};
	gethostname(host, sizeof(host) - 1);
	if (target.substr(0, dash) != host)
		return false; // another machine's (a shared home): not ours to judge, and Chromium re-locks it
	const long pid = std::strtol(target.c_str() + dash + 1, nullptr, 10);
	return pid > 0 && (kill((pid_t)pid, 0) == 0 || errno == EPERM);
#endif
}

} // namespace

} // namespace home
