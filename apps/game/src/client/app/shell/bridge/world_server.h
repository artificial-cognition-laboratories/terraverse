// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>
#include <vector>

namespace shell {

// A world's own server folder (<world>/server): the mods it runs, pinned in its
// package.json and bun.lock, prepared into .terra/ for the sidecar. A world made
// before worlds had one runs the game's (Server: the share's server folder) until
// its mods are first changed, which gives it its own.
class WorldServer
{
public:
	struct Opts {
		std::string game; // the game's own server folder, prepared: the official mods
	};
	explicit WorldServer(Opts o);

	// A new world's server: the game's official mods at the versions this build ships,
	// plus `mods` (@scope/name[@version]) from the registry, installed and prepared by
	// terra. Throws Refused with terra's reason; the folder is left as far as it got.
	void make(const std::string &world, const std::vector<std::string> &mods) const;

	// What `world` runs, as the page's WorldMods (bridge/types.ts): each mod, its pinned
	// version, whether it's the game's, and the blocks it has put in this world.
	Json::Value mods(const std::string &world) const;
	// Added, or moved to the version named (@scope/name@1.2.0). Throws Refused.
	Json::Value add(const std::string &world, const std::vector<std::string> &mods) const;
	// Taken out, unless it has put blocks in this world. Throws Refused.
	Json::Value remove(const std::string &world, const std::string &name) const;
	// How many mods it runs beyond the game's: what the Play list shows.
	int added(const std::string &world) const;

	// The server folder `world` runs: its own, or "" for the game's.
	static std::string folder(const std::string &world);

private:
	void terra(const std::string &dir, const std::vector<std::string> &args, const std::string &doing) const;
	std::string m_game;
};

} // namespace shell
