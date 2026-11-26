// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/server/world/run/machines/piece/machine.h"
#include <optional>
#include <string>
#include <vector>

class IGameDef;

namespace workshop {

/*
	The starter parts (sprint 76): ready to carry onto a chassis, made of
	the game's own shapes and materials as anyone's would be (Trailmakers'
	and Scrap Mechanic's suspension blocks, here built). A wheel; a corner
	(a steel hub on a spring, a rubber wheel on it) that steers, is driven,
	both or neither. Every player's library starts with them; theirs of the
	same name is theirs. Made from the game's materials:steel and
	materials:rubber: without them there are none.
*/
struct Starter
{
	const char *name;
	// none: a wheel alone; else a corner, its wheel turning so
	std::optional<piece::Joint> corner;
};
extern const Starter STARTERS[5];

// Throws std::out_of_range on a name that isn't one, and std::runtime_error
// when the game hasn't the materials.
piece::Machine starter(IGameDef *gamedef, const std::string &name);
std::vector<std::string> starterNames();

} // namespace workshop
