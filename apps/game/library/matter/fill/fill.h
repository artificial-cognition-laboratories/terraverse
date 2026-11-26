// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include "src/shared/base/irrlichttypes.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

namespace shape { class Shape; }

/*
	Fill: liquid in a node's open space (sprint 81). A node holds one
	material, and a sculpted one only some of its sixteenths; a fill is
	beside it, so water lies in a pit cut in the sand and the sand stays
	sand. It's an amount, not sixteenths: how full the node's open space is,
	what liquid it mostly is, and its colour (the mixture's). The world's
	matter runs it; the client draws it (client/liquids).
*/
namespace fill {

constexpr u8 FULL = 255;

// A full block of water the world's network keeps (sprint 115: a river's or
// a lake's, as generated), in its param2: held at its level, as the sea is,
// not an amount free to run. A river is a staircase of blocks; run as a pool
// its upper steps drained into the lower and over their banks. Water placed
// or run by the world's matter has none.
constexpr u8 HELD = 0x80;
// ... and held water a dammed lake laid over the land (sprint 115, phase 5,
// with HELD): its ground is under it, and it goes when the lake drains
constexpr u8 LAKE = 0x40;
// ... and in its low bits, the sixteenths of its top node under the surface
// (sprint 122: a river slopes down its run to a sixteenth): 1..15; 0, full
constexpr u8 PART = 0x0F;
inline int partOf(u8 param2)
{
	const int p = param2 & PART;
	return p ? p : 16;
}
inline u8 withPart(u8 param2, int part)
{
	return u8((param2 & ~PART) | (part >= 16 ? 0 : part & PART));
}

// A river's surface (sprint 122, R2): one function, read by the server's matter, the
// client's water, its flat drawing and swimming alike, so none of them disagree
namespace surface {
// Held water's top node's surface over the node's floor, sixteenths: its part, risen
// or fallen by its river's rise, kept within the node (past its top is a flood, and
// under its floor low water through the whole channel: neither yet, sprint 122 R6)
inline int of(u8 param2, float rise)
{
	return std::clamp(partOf(param2) + int(std::lround(rise)), 0, 16);
}
// A river's rise at node p, sixteenths: its map blocks' rises blended between
// their middles, so neighbours rise alike and a river doesn't step at a block's
// edge. `riseOf(block)`: a block's rise, or nothing (no river of its own; a river
// crossing a block's floor or roof is the block under's or over's). None: 0.
template <typename RiseOf>
float riseAt(const RiseOf &riseOf, v3s16 p)
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
		for (s16 dy : { 0, -1, 1 }) {
			const std::optional<float> rise = riseOf(v3s16(s16(bx + dx), s16(by + dy), s16(bz + dz)));
			if (!rise)
				continue;
			sum += w * *rise;
			weight += w;
			break;
		}
	}
	return weight > 1e-6f ? sum / weight : 0;
}
} // namespace surface

// Liquids by name, numbered as this process first meets them (a map
// block's fills are saved by name, so the numbers never leave it).
u16 liquid(const std::string &name);
const std::string &name(u16 liquid);

struct Fill
{
	u16 liquid = 0;
	u8 amount = 0;          // of the node's open space, /FULL
	u8 r = 0, g = 0, b = 0; // what it looks like
	bool operator==(const Fill &o) const
	{
		return liquid == o.liquid && amount == o.amount && r == o.r && g == o.g && b == o.b;
	}
};

// A node's open space, from the bottom up: how many of its sixteenths
// are empty in each layer (all 256 in a node with no shape).
struct Room
{
	u16 layers[16];
	u32 total() const;
	static Room of(const shape::Shape *s);
	// Where a liquid's surface stands, in node heights (0..1), holding
	// `share` of its open space, filling the empty sixteenths from the bottom.
	float surface(float share) const;
};

class Fills
{
public:
	const Fill *get(v3s16 rel) const;
	// Keeps `f` for the node at rel; an empty one (amount 0) drops it.
	void set(v3s16 rel, const Fill &f);
	void erase(v3s16 rel) { m_fills.erase(key(rel)); }
	bool empty() const { return m_fills.empty(); }

	template <class F>
	void each(const F &f) const
	{
		for (const auto &[k, x] : m_fills)
			f(unkey(k), x);
	}

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

private:
	static u16 key(v3s16 rel);
	static v3s16 unkey(u16 k);

	std::unordered_map<u16, Fill> m_fills;
};

} // namespace fill
