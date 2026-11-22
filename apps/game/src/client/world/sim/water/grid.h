// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/parts.h"
#include "src/shared/base/irr_v3d.h"
#include "src/shared/base/irrlichttypes.h"
#include <algorithm>
#include <array>
#include <optional>
#include <vector>

/*
	The columns of water round the camera (sprint 82): a sixteenth wide each,
	in tiles of 16×16, each column with the ground under it, its depth and its
	flows. The state the water layer's leaves share; it only knows its own
	shape, and how it follows the camera.
*/
namespace liquids {

// A tile: 16×16 of the grid's columns
struct Tile
{
	bool known = false;   // looked at once: its water's been set from the server's
	bool scanned = false; // its ground and water looked at since they last changed
	bool stale = false;   // its ground changed while it was dry: looked at when water comes
	bool awake = false;
	bool drawn = false;   // its texels are as its water is
	bool sea = false;     // some of it is the sea's: held at its level, not to an amount
	float quiet = 0;      // s still
	float target = 0;     // nodes³ of water over its ground, as the server has it (not the sea's)
	float above = 0;      // ... of it over the sea's level (beside the sea, only that's held: what's under is the sea's)
	float credit = 0;     // nodes³ landed as drops the server hasn't sent back yet
	float volume = 0;     // nodes³ it holds now
	int pool = 0;         // the water it's joined to, held to the server as one
	bool coloured = false;
	u8 r = 0, g = 0, b = 0;
	bool wet = false;     // any top shown
	bool level = false;   // every top shown, at one height: drawn as one quad where it can be
	float mean = 0;       // its tops' mean, sixteenths over the grid's base
	float shade = 1;      // its light
	float low = 0, high = 0; // its lowest side's foot and highest top, sixteenths over the base (to cull it)
	bool relevel = false; // its columns' held levels to be found again from its nodes' and its neighbours'
};

class Grid
{
public:
	struct Opts
	{
		int res;    // columns a node a side
		int nodes;  // nodes a side
		int follow; // nodes the camera goes from its middle before it follows
		int scans;  // tiles looked at a frame
	};
	explicit Grid(Opts o);

	const int res, nodes, follow, scans;
	const int cols;  // columns a side
	const int tiles; // tiles a side

	bool placed = false;
	v3s16 origin; // its corner node (x, z) and the height it reaches round (y)
	std::vector<s32> floor, shown, sea; // per column, world sixteenths: ground, top drawn, the held level (NONE: none)
	std::vector<s32> want;              // per column, world sixteenths: where the server's water stands (NONE: none)
	std::vector<float> depth;           // per column, node heights
	std::vector<std::array<float, 4>> out; // per column: flow out of each side (+x −x +z −z), node²/s
	// per column: the river's current (x, z), nodes/s: held water above the
	// sea runs toward its nearest step down (sprint 115), carrying what's on it
	std::vector<std::array<float, 2>> current;
	std::vector<std::optional<v3s16>> tops; // per node: the liquid it draws (not flat)
	std::vector<s32> levels;                // per node: its held water's surface, world sixteenths (NONE: none)
	std::vector<float> runs;                // per node: how fast its river runs (nodes/s; 0: still)
	std::vector<std::array<float, 2>> ways; // per node: the way its river runs (zero: by its steps)
	std::vector<Tile> tile;
	std::vector<int> nearest; // tiles, nearest the middle first
	s32 sea_level = INT32_MIN; // world sixteenths, as last seen: water under it beside the sea is the sea's

	// Follows the camera, keeping what it has where they overlap
	struct Placed
	{
		bool moved = false;
		std::vector<v3s16> let_go; // the map blocks of the liquid it let go of (drawn flat again, at once)
	};
	Placed place(v3f camera);

	// a column's neighbour's index by side
	int stepOf(int side) const { return side == 0 ? 1 : side == 1 ? -1 : side == 2 ? cols : -cols; }
	int columnAt(float x, float z) const; // −1: outside
	int tileOf(int column) const { return ((column / cols) / 16) * tiles + (column % cols) / 16; }
	int firstOf(int t) const { return (t / tiles) * 16 * cols + (t % tiles) * 16; }
	// nodes a tile a side
	int per() const { return 16 / res; }
	void wake(int t);
	// Held: its water stands at a level kept for it (the sea's, a river's or
	// a lake's), so what the grid runs is how far it's off that level
	bool held(int c) const { return sea[c] != NONE && floor[c] != NONE; }
	// Its surface (nodes), and how far over its held level (nodes; held only)
	float surface(int c) const { return floor[c] / 16.0f + depth[c]; }
	float off(int c) const { return surface(c) - std::max(sea[c], floor[c]) / 16.0f; }
	// How much higher column c's water stands than n's, as it drives a flow
	// between them: held beside held, by how far each is off its level (a
	// river's sloped surface at rest is still); held beside a bank, only what's
	// over its level (a river at rest never spills over a bank lower than it);
	// otherwise by their surfaces
	float higher(int c, int n) const
	{
		if (held(c))
			return held(n) ? off(c) - off(n) : std::min(off(c), surface(c) - surface(n));
		return surface(c) - surface(n);
	}
	// A tile's columns' held levels from its nodes' (held::corners): whether any
	// changed. Its water moves with them, keeping its disturbance.
	bool relevel(int t);
	// Whether it draws the liquid in node p (it's not drawn flat)
	bool owns(v3s16 p) const;
	// Whether a tile's water doesn't stand level with a known neighbour's
	// along their edge (so it'd run): a tile that does is left asleep
	bool uneven(int t) const;
};

} // namespace liquids
