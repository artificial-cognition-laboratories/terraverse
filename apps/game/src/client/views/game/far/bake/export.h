// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

struct GameParams;

namespace far {

/*
	`luanti --export-palette <file> --world <dir> --gameid <game>`: the
	game's baked palette (far/baked.h), from its own content and textures.
	The world's mods are loaded as a server would (no thread, no players),
	its images given to the client's texture source as a client is given
	them, on the NULL video driver (no window), and the palette made exactly
	as the far view makes it in game. The world must be a terra one. Throws
	on any failure; writes nothing then.
*/
void exportPalette(const GameParams &params, const std::string &path);

} // namespace far
