// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/terrain.h"
#include "src/shared/core/irrlichttypes_bloated.h"
#include <SColor.h>
#include <array>
#include <unordered_map>
#include <vector>

class ITextureSource;
class NodeDefManager;

namespace far {

// A block's faces, in tile order: up, down, +X, −X, +Z, −Z.
enum Face { UP, DOWN, EAST, WEST, NORTH, SOUTH };

/*
	What each block looks like from afar: the average of each face's texture
	(what the GPU's smallest mipmap shows), tinted as the near mesher tints it
	(by the node's colour: its palette by param2, or one colour). Plain data,
	read-only once made, so the far view's workers share it.

	Made two ways, the same values: live from a game's node definitions and
	textures (in game, and by the exporter), or read back from the game's
	baked palette (far/baked.h: the menu, and anything without a server).
*/
class Palette
{
public:
	struct Look
	{
		std::array<video::SColorf, 6> face; // 0..1 sRGB: the texture's average (with its tile colour)
		std::array<bool, 6> tinted;         // the face takes the node's colour
		std::vector<video::SColorf> tints;  // the node's colour: one, or 256 by param2 (a palette)
		video::SColorf over_black;          // the top texture over black: its colour times its opacity
		float clarity;                      // how much shows through it: 1 − its opacity
	};

	// From a game's content (client: the textures are the texture source's).
	// Throws for a block with a face that has no texture, or a palette that
	// won't load.
	static Palette live(const NodeDefManager *ndef, ITextureSource *tsrc, const std::vector<content_t> &blocks);

	void add(content_t id, Look look);
	// The block's look; throws if it has none.
	const Look &look(content_t id) const;
	const std::unordered_map<content_t, Look> &looks() const { return m_looks; }

	// A face's colour, tinted for its param2.
	video::SColorf face(MapNode n, Face f) const;
	// A liquid's top seen over `floor` lit to `lit` (0..1): its texture laid
	// over what's below it, pixel by pixel, as the near world blends it.
	video::SColorf through(MapNode liquid, video::SColorf floor, float lit) const;

private:
	std::unordered_map<content_t, Look> m_looks;
};

// Every block a planet's content names: what it can put in view, and the
// ground its species grow on.
std::vector<content_t> blocksIn(const Content &content);

} // namespace far
