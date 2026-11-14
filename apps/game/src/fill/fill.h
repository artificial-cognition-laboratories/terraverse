// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <iostream>
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
