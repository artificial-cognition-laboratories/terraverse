// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/world.h"
#include <json/json.h>

class IGameDef;

namespace workshop {

class Materials;

/*
	The wiki's machines, baked when the palette is (bun palette): built from the
	starter parts by the game's Machine and compiled by the game's compile, with
	no world in the wiki to build them in. Each is what physics sees, plus what
	each of its bodies looks like.
*/
Json::Value bakeShowcase(IGameDef *gamedef, const Materials &materials);
physics::Assembly assemblyOf(const Json::Value &machine); // throws BaseException on what isn't one

} // namespace workshop
