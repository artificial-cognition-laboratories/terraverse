// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "content/subgames.h"
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>

namespace terragen { struct WorldCode; }

namespace shell {

// A world code from the page; a bad one is refused with a message for the player.
terragen::WorldCode readWorldCode(const std::string &code);

// The saved worlds of one game: list, create, delete. Worlds of other games
// in the same user directory are not ours and are never shown or touched.
class Worlds
{
public:
	explicit Worlds(const std::string &gameid) : m_gameid(gameid) {}

	std::vector<WorldSpec> list() const;
	// `code` is the world code (TF1.…): terragen builds the land from it.
	// `settings` go into the world's world.mt (see validateSetting in
	// worlds.cpp). Throws Refused.
	WorldSpec create(const std::string &name, const std::string &code,
			const Json::Value &settings) const;
	void remove(const std::string &path) const;      // throws Refused
	void pin(const std::string &path, bool pinned) const;
	void rename(const std::string &path, const std::string &name) const; // Refused for no name
	// A Lab scenario's own world (a scenario from Lab::find), built fresh at
	// <user>/lab on every call: its map generator (`world.mapgen`, default
	// singlenode) and `tf_lab = <id>`, which is what tf_lab builds. Not in
	// list().
	WorldSpec lab(const Json::Value &scenario) const;
	WorldSpec find(const std::string &path) const;   // throws Refused
	// Before playing a world: its creative_mode / enable_damage (world.mt)
	// become the engine's, as Luanti's own menu does when you pick a world.
	void select(const WorldSpec &world) const;
	// When a world was last opened (seconds since the epoch: select()), if
	// ever. Throws for a value that isn't one.
	std::optional<int64_t> lastOpened(const WorldSpec &world) const;
	// The world code of the world opened last, if any was (the menu shows
	// its system), of those this build can still make (an older build's code
	// is passed over). Lab worlds aren't listed, so never this.
	std::optional<std::string> lastVisited() const;

private:
	std::string m_gameid;
};

} // namespace shell
