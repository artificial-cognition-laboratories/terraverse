// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

/*
	One engine to a world (sprint 126): a server holds its world's `world.lock` while it runs, so
	the same world can't be open in the game and served for Axon at once, two engines writing one
	map. The OS lets go of it if the engine dies, so a crash never leaves a world locked.
*/
class WorldLock
{
public:
	struct Opts
	{
		std::string world; // the world's folder
	};
	// Holding it, or a ServerError saying it's open elsewhere.
	explicit WorldLock(Opts o);
	~WorldLock();
	WorldLock(const WorldLock &) = delete;
	WorldLock &operator=(const WorldLock &) = delete;

private:
#ifdef _WIN32
	void *m_handle;
#else
	int m_fd;
#endif
};
