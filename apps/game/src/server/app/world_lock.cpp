// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/app/world_lock.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#ifdef _WIN32
	#include <windows.h>
#else
	#include <fcntl.h>
	#include <sys/file.h>
	#include <unistd.h>
#endif

namespace {

std::string pathOf(const std::string &world) { return world + DIR_DELIM "world.lock"; }

std::string openElsewhere(const std::string &world)
{
	return "the world at " + world + " is open in another game or server: one engine to a world";
}

} // namespace

#ifdef _WIN32

WorldLock::WorldLock(Opts o)
{
	fs::CreateAllDirs(o.world);
	// no sharing: a second opener is refused while this handle lives
	m_handle = CreateFileA(pathOf(o.world).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (m_handle == INVALID_HANDLE_VALUE)
		throw ServerError(openElsewhere(o.world));
}

WorldLock::~WorldLock()
{
	CloseHandle(m_handle);
}

#else

WorldLock::WorldLock(Opts o) :
	m_fd(-1)
{
	fs::CreateAllDirs(o.world);
	m_fd = ::open(pathOf(o.world).c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
	if (m_fd < 0)
		throw ServerError("can't open " + pathOf(o.world) + " to hold the world");
	if (::flock(m_fd, LOCK_EX | LOCK_NB) != 0) {
		::close(m_fd);
		throw ServerError(openElsewhere(o.world));
	}
}

WorldLock::~WorldLock()
{
	::close(m_fd); // and the lock with it
}

#endif
