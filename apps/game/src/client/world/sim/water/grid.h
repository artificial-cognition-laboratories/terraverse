// Terraverse
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
	in tiles of 16×16. Water here is a base plus a disturbance (sprint 122):
	each column's level is the server's (eased toward where it says, never
	snapped), and its η is how far the client has it off that level right
	now (a wake, a splash). Only η is simulated. The state the water layer's
	leaves share; it only knows its own shape, and how it follows the camera.
*/
namespace liquids {

// Whose water a node's is: what its level comes from, and how fast a disturbance on it dies
enum class Kind : u8 { None, Sea, River, Lake, Fill };

// A tile: 16×16 of the grid's columns
struct Tile
{
	bool known = false;    // looked at whole once: it owns every liquid top in it
	bool scanned = false;  // its ground and water looked at since they last changed
	bool stale = false;    // its ground changed while it was dry: looked at when water comes
	bool awake = false;    // some η or flow in it: run
	bool easing = false;   // some level in it isn't at its target yet
	bool retarget = false; // its columns' targets to be found again from its nodes' and its neighbours'
	bool snap = false;     // first seen: its levels put at their targets, not eased (nothing settles in front of you)
	bool drawn = false;    // its texels are as its water is
	float quiet = 0;       // s still
	bool coloured = false;
	u8 r = 0, g = 0, b = 0;
	bool wet = false;     // any top shown
	bool level = false;   // every top shown, at one height: drawn as one quad where it can be
	float mean = 0;       // its tops' mean, sixteenths over the grid's base
	float shade = 1;      // its light
	float low = 0, high = 0; // its lowest side's foot and highest top, sixteenths over the base (to cull it)
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

	// per column
	std::vector<s32> floor;   // world sixteenths: its ground (NONE: none in reach, a wall)
	std::vector<s32> target;  // world sixteenths: where the server has its water stand (NONE: dry)
	std::vector<float> level; // world sixteenths: its base as drawn, easing toward target (at floor: dry)
	std::vector<float> eta;   // nodes: how far it's off its level (the disturbance)
	std::vector<std::array<float, 4>> out; // η's flow out of each side (+x −x +z −z), node²/s
	std::vector<std::array<float, 2>> vel; // its water's current (x, z), nodes/s: what carries η
	std::vector<Kind> kind;
	std::vector<s32> shown;   // world sixteenths: its top as drawn (NONE: none)

	// per node
	std::vector<std::optional<v3s16>> tops;  // the liquid it draws (not flat)
	std::vector<s32> targets;                // its water's surface, world sixteenths (NONE: none)
	std::vector<Kind> kinds;
	std::vector<std::array<float, 2>> flows; // its river's current, nodes/s (zero: still, or not known)

	std::vector<Tile> tile;
	std::vector<int> nearest; // tiles, nearest the middle first

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
	int nodeOf(int column) const { return ((column / cols) / res) * nodes + (column % cols) / res; }
	// nodes a tile a side
	int per() const { return 16 / res; }
	void wake(int t);

	// Water stands in it: its level's over its ground (a disturbance runs only between wet columns,
	// so a wake never climbs a bank)
	bool wet(int c) const { return floor[c] != NONE && level[c] > floor[c] + 1e-3f; }
	// Its surface (nodes), base and disturbance
	float surface(int c) const { return level[c] / 16.0f + eta[c]; }
	// How deep its water is (nodes)
	float depth(int c) const { return floor[c] == NONE ? 0.0f : std::max(0.0f, surface(c) - floor[c] / 16.0f); }
	// Its base's depth (nodes): how deep it is at rest
	float rest(int c) const { return floor[c] == NONE ? 0.0f : std::max(0.0f, (level[c] - floor[c]) / 16.0f); }
	// Whether it draws the liquid in node p (it's not drawn flat)
	bool owns(v3s16 p) const;
};

} // namespace liquids
