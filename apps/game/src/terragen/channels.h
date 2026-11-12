// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "sphere.h"
#include <array>
#include <atomic>
#include <memory>
#include <cstdint>

/*
	The drainage below the grid (planning/sprint-100-channels.md). The grid's
	own tree (every cell to its receiver) is the trunk. Each finer level has
	points hashed on a lattice half as wide as the last, each running to the
	nearest point on the level above that isn't uphill of it: four times the
	streams, each half as long, per level, as Horton's laws have it. A segment
	to a nearest point can't cross another (without the uphill test, never).
	A place's relief comes from how far it is from each level's channels, so
	the ridges are simply what's left between the valleys.
	Pure: a node is a function of its lattice cell; the cache only saves work.
*/
namespace terragen {

struct Grid;
class World;

class Channels
{
public:
	static constexpr int MOST_LEVELS = 7;

	// A channel node: runs from `at` (head) to `join` (foot, on the level above)
	struct Node
	{
		Place at{}, join{};
		fix rain = 0;
		std::array<int32_t, 3> parent{}; // the node it joins (none on the first level: the grid's)
		bool exists = false;
		// it carries water: wet enough, and so is what it runs into, down to
		// a drawn run of the grid's (World::drawn), so every stream reaches one
		bool wet = false;
		fix t = 0;        // how far along what it joins (the level above's segment, from its foot; the grid's run, from its cell)
		int32_t link = -1; // on the first level, the grid cell whose run it joins
		// (the prototype's land) its foot's and head's heights before the flow's
		// known (blocks, fixed): what a finer node joins by, as Shape's need it
		int64_t rough_low = 0, rough_high = 0;
		// (the prototype's land) it runs into another of its own level's head,
		// `into`, not the level above: a tributary of a tributary, so each
		// level branches as a tree, not a comb of lines into the one above
		bool sibling = false;
		std::array<int32_t, 3> into{};
		int64_t cost = INT64_MAX; // what its way down costs (build's: rough height plus the climb to it)
	};

	// A node's shape as the drainage wears it (sprint 108): the area it drains
	// (blocks²), the slope stream power gives its flow, its foot's and head's
	// heights (blocks, fixed), and how steeply the ground rises from it
	struct Shape
	{
		int64_t area = 0;
		fix slope = 0, hill = 0;
		int64_t low = 0, high = 0;
		// a pond on it (none: pond_r 0): centred pond_t along it (0 its foot),
		// its radius and depth, and its level, at its downstream rim (blocks, fixed)
		fix pond_t = 0;
		int64_t pond_r = 0, pond_depth = 0, pond_level = 0;
		// a fan at its mouth (none: 0): its radius (blocks, fixed)
		int64_t fan_r = 0;
		int64_t cirque_r = 0; // a cirque at its head (none: 0): its radius (blocks, fixed)
		fix ice = 0; // how much ice worked it (World::glacial at its head)
	};

	struct Read
	{
		fix relief = 0;       // −1 (a valley floor) .. 1 (a ridge)
		fix trunk = ONE;      // across the largest level's hillside: 0 in its channel
		// the nearest level that can hold water here, if any
		int64_t stream_gap = INT64_MAX; // blocks, fixed
		int stream_from = 0;   // which level (1: the largest)
		// the valleys the channels cut by their flow: 0 (none) .. 1 (the floor of
		// the deepest a level-1 channel cuts), and how wide the floor's water is
		fix cut = 0;
		int64_t stream_width = 0; // blocks, fixed
		// the ground as the drainage shapes it (blocks, fixed; INT64_MAX none):
		// the lowest of each channel near's height plus its hillside
		int64_t land = INT64_MAX;
		// the water there, from the channel whose valley it is (the land's
		// lowest): its level (blocks, fixed), how far off its line, how wide
		int64_t water = INT64_MIN, water_gap = INT64_MAX, water_width = 0;
		int water_from = 0; // the level whose stream it is (1: the largest)
		bool pond = false; // the water's a pond's, standing still
		fix slope = 0;     // how steep the land is here: its hillside's, or on a floor its channel's
		int64_t bed = INT64_MAX; // the height of the channel whose valley it is, here (blocks, fixed)
		int bed_level = -1;      // and its level (0: the trunk)
		// how far the land stands over its channel's bed (blocks, fixed), and
		// continuous where which channel's valley it is changes (the bed jumps
		// there): the least of each one's hillside here and its rise over its bed
		int64_t clear = INT64_MAX;
		bool wash = false;       // on a dry channel's floor: sand, water only in floods
		fix ice = 0;             // how much ice worked that valley (0..1)
	};

	// `land`: shape the ground from the drainage (Read::land; sprint 108's prototype)
	Channels(const WorldCode &code, const Grid &grid, const World &world, int64_t radius, int64_t cell_blocks, bool land);

	bool active() const { return m_levels > 0; }
	int levels() const { return m_levels; }
	// its own, of every Channels made: what the threads' caches know it by
	uint64_t owner() const { return m_owner; }
	int64_t spacing(int level) const { return m_lattice[level]; } // place units
	// `warped` is the place read for channels; `range` how much it's a rising range
	// `least`: no level with a lattice finer than this (place units): what a
	// far tile's cells are too wide to show, and a whole search per cell to find
	Read read(const Place &warped, fix range, int64_t least = 0) const;
	// a node, or one that doesn't exist (level 1..levels())
	const Node &node(int level, int32_t i, int32_t j, int32_t l) const;
	// its shape (a node that exists)
	const Shape &shape(int level, int32_t i, int32_t j, int32_t l) const;
	int32_t latticeOf(int level, int32_t c) const;
	// A stream's width for what drains to it (blocks² of average rain; World::riverWidth)
	int64_t streamWidth(int64_t flow) const;

	~Channels();
	Channels(const Channels &) = delete;
	Channels &operator=(const Channels &) = delete;

private:
	// What's near a place's lattice cell, as read() gathers it: a channel, or
	// a piece of a grid run's (the trunk's)
	struct Segment
	{
		Place join, at;
		bool wet;
		fix rain;
		int64_t low = 0, high = 0; // heights at its foot and head (blocks, fixed)
		fix hill = 0, grade = 0; // its sides' slope, and its own
		int64_t flow = 0; // what runs down it, blocks² of average rain (the grid's runs: none drawn here)
		int64_t floor = 0; // its flat floor's half width (blocks, fixed)
		Place pond; // its pond's centre, if pond_r > 0 (Shape's)
		bool wash = false; // a dry channel's floor (WASH_RAIN)
		fix wall = 0;      // a trough's walls' slope, rising wall_run off its floor (blocks, fixed), then hill
		int64_t wall_run = 0;
		fix ice = 0;       // how much ice worked it
		int64_t cirque = 0; // a cirque's radius at its head (blocks, fixed; 0 none)
		Place fan;  // its fan's centre, half fan_r up from its foot, if fan_r > 0
		int64_t fan_r = 0;
		int64_t pond_r = 0, pond_depth = 0, pond_level = 0;
	};
	struct Pieces; // a grid run's trunk pieces, as read() traces them
	// each grid cell's run's pieces, traced once and kept for every thread (none yet: null)
	std::unique_ptr<std::atomic<Pieces *>[]> m_pieces;

	Node build(int level, int32_t i, int32_t j, int32_t l) const;
	// a node joining only the level above (or the grid's runs): what build()
	// weighs its own level's lower heads against (cached)
	const Node &down(int level, int32_t i, int32_t j, int32_t l) const;
	Node toLevelAbove(int level, int32_t i, int32_t j, int32_t l) const;
	Shape wear(int level, int32_t i, int32_t j, int32_t l) const;
	// everything upstream of a node: its own cell and its children's, each
	// as much as a child of its level drains on average (blocks², cached;
	// heights not needed, so no cycle with wear)
	int64_t drains(int level, int32_t i, int32_t j, int32_t l) const;
	int64_t steepness(int32_t c) const;
	fix slopeFor(int32_t c, int64_t flow, fix most) const;
	// how hard the rock is at a place, 0..1: its province's, varying between
	// beds and joints (the land's texture: sprint 109 P3)
	fix hardness(const Place &p) const;
	void toGrid(const Place &p, fix range, Read &out) const;
	void beds(const Place &p, Read &out) const;

	const Grid *m_grid;
	const World *m_world;
	uint32_t m_seed;
	uint64_t m_owner;
	int64_t m_radius;
	bool m_water;
	bool m_land;
	int m_levels = 0;
	int m_area_levels = 0; // the levels what a channel drains is counted over (the finer are side valleys)
	int m_wet_levels = 0;  // the levels that can carry water, 1..this (by their size: STREAM_LEAST_*)
	std::array<fix, MOST_LEVELS + 1> m_wet{};              // the rain each needs to (the grid's: 1 average)
	std::array<int64_t, MOST_LEVELS + 1> m_stream_width{}; // its stream's width as the noise land draws it (blocks, fixed)
	std::array<int64_t, MOST_LEVELS + 1> m_lattice{}; // [0]: the grid's spacing
	std::array<int32_t, MOST_LEVELS + 1> m_waves{};
	// (the land's) how far round a trunk window's middle its runs are
	// gathered (place units): every run within its reach of any place in it
	int64_t m_trunk_ball = 0;
	int32_t m_roll_waves = 8; // the gentle land's swells, round the world
	// beds and rolls as tall as on the planet they were tuned on (100 000 round), less on a smaller one,
	// whose heights are less (sprint 112): this planet's height scale against that one's, at most 1
	fix m_small = ONE;
	fix m_steep = ONE; // how many times steeper hillsides stand than the Earth's (STEEP_MOST at most)
	int m_tarn_level = 0;     // the level whose channels are a cirque's size (none: 0)
	// a level's lattice spacing (whole blocks)
	int64_t blocksOf(int level) const { return mulShift(m_lattice[level], m_radius, PLACE_FRAC) >> FRAC; }
};

} // namespace terragen
