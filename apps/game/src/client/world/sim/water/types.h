// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/matter/fill/fill.h"
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

// A river's rise at node p, sixteenths, blended between its map blocks (fill::surface::riseAt)
inline float riseAt(const Rivers &rivers, v3s16 p)
{
	return fill::surface::riseAt([&](v3s16 block) -> std::optional<float> {
		auto r = rivers.find(block);
		return r == rivers.end() ? std::nullopt : std::optional<float>(r->second.rise);
	}, p);
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
