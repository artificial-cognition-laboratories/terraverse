// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/palette.h"
#include "far/terrain.h"
#include <functional>
#include <json/json.h>
#include <string>

namespace far {

/*
	A game's baked palette (games/terrafirma/palette.json): everything the
	generator places, by role, the tree species, and how each block looks
	from afar, by block name. Exported from the game's own content and
	textures (`luanti --export-palette`), so a planet drawn with no server
	(the menu, the create screen, the Planetarium before a world is joined)
	is the game's planet, block for block.

	Read back, its blocks get ids of its own (by their order in the file;
	air and ignore are the engine's): consistent across content and
	palette, never a server's.
*/
struct Baked
{
	static constexpr int VERSION = 3; // 2: no ice block (the polar wall went, sprint 86); 3: soil kinds (sprint 111)

	std::vector<std::string> names; // by id; "" for ids no block has
	Content content;
	Palette palette;

	// `name` gives a block's name for its id (the server's node definitions).
	static Json::Value write(const Content &content, const Palette &palette,
			const std::function<std::string(content_t)> &name);
	// Throws BaseException for another version, a missing or malformed
	// field, or a block with no look.
	static Baked read(const Json::Value &json);
	// The file a game ships (`<share>/games/<game>/palette.json`), read.
	// Throws if it isn't there or won't read.
	static Baked load(const std::string &game);
};

} // namespace far
