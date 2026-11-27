// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <optional>
#include <string>

namespace shell {

class Intents;

/*
	A friend's world to go to once the menu is up: asked for from inside a world
	(the page sets it aside, then leaves, so the world is saved first), or by the
	launcher starting the game (--join). The menu's page takes it (join.pending)
	and joins the usual way (social.join), with its own joining screen.
	Main thread only, as both verbs are.
*/
class Joins
{
public:
	struct Friend {
		std::string id;   // their ArcLabs account id
		std::string name; // their handle, for the joining screen
	};
	struct Opts {
		Intents &intents;            // the session's: answered in the menu and in a world alike
		std::optional<Friend> first; // from the command line (--join id:handle)
	};
	explicit Joins(Opts opts);

	// "<id>:<handle>", as --join gives it; throws std::invalid_argument for anything else.
	static Friend parse(const std::string &arg);

private:
	std::optional<Friend> m_pending;
};

} // namespace shell
