// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include "src/shared/base/irrlichttypes.h"
#include <array>
#include <cmath>
#include <unordered_map>

// What the water layer is told each frame, from the server and the scene
namespace liquids {

// Liquid poured, from a spout to where it lands (TOCLIENT_STREAM), in nodes
struct Stream
{
	v3f from, to;
	u8 r = 0, g = 0, b = 0;
	u16 cups = 0; // each a sixteenth-cube drop
};

// A map block's river as the weather fills it (TOCLIENT_RIVERS, sprint 115):
// its held water's surface over or under the level generated, and how fast it runs
struct River
{
	s16 rise = 0;      // sixteenths
	float speed = 0;   // nodes/s
	std::array<float, 2> way{}; // the way it runs (x, z), a unit vector; zero: a lake, or not known
};
using Rivers = std::unordered_map<v3s16, River>;

// A river's rise at node p, sixteenths: its map blocks' rises blended between
// their middles, so a river risen more in one block than the next slopes
// between them, not steps at the block's edge (blocks read different cells'
// flows). A block with no river of its own isn't counted; none: 0.
inline float riseAt(const Rivers &rivers, v3s16 p)
{
	constexpr int B = 16; // MAP_BLOCKSIZE (mapblock.h's, not included here)
	const float fx = (p.X - (B - 1) * 0.5f) / B, fz = (p.Z - (B - 1) * 0.5f) / B;
	const int bx = int(std::floor(fx)), bz = int(std::floor(fz));
	const float tx = fx - bx, tz = fz - bz;
	const s16 by = s16(std::floor(p.Y / float(B)));
	float sum = 0, weight = 0;
	for (int k = 0; k < 4; k++) {
		const int dx = k & 1, dz = k >> 1;
		const float w = (dx ? tx : 1 - tx) * (dz ? tz : 1 - tz);
		// (a river crossing a block's floor or roof: the one above or below)
		for (s16 dy : { 0, -1, 1 }) {
			auto r = rivers.find(v3s16(s16(bx + dx), s16(by + dy), s16(bz + dz)));
			if (r == rivers.end())
				continue;
			sum += w * r->second.rise;
			weight += w;
			break;
		}
	}
	return weight > 1e-6f ? sum / weight : 0;
}

// Something that pushes water aside: an object, the player; in nodes, its
// feet's middle and how far it reaches
struct Mover
{
	u32 id;
	v3f feet;
	float radius, height;
};

} // namespace liquids
