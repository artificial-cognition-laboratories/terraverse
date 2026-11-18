// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "sphere.h"
#include <array>
#include <cstdint>

/*
	The drainage below the grid (planning/sprint-100-channels.md): where the
	small streams run and the valleys they cut. The grid's own tree (every
	cell to its receiver) is the trunk. Each finer level has points hashed on
	a lattice half as wide as the last, each running to the nearest point on
	the level above that isn't uphill of it: four times the streams, each half
	as long, per level, as Horton's laws have it. A place's relief comes from
	how far it is from each level's channels, so the ridges are simply what's
	left between the valleys.
	(The land itself is the erosion filter's, amplify.h; sprint 116.)
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
	};

	struct Read
	{
		fix relief = 0; // −1 (a valley floor) .. 1 (a ridge)
		// the nearest level that can hold water here, if any
		int64_t stream_gap = INT64_MAX; // blocks, fixed
		int stream_from = 0;            // which level (1: the largest)
		int64_t stream_width = 0;       // blocks, fixed
		// the valleys the channels cut by their flow: 0 (none) .. 1 (the floor
		// of the deepest a level-1 channel cuts)
		fix cut = 0;
	};

	Channels(const WorldCode &code, const Grid &grid, const World &world, int64_t radius, int64_t cell_blocks);

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
	// the lattice cell a place's coordinate (place units) lies in, on a level
	int32_t latticeOf(int level, int32_t c) const;

	Channels(const Channels &) = delete;
	Channels &operator=(const Channels &) = delete;

private:
	// What's near a place's lattice cell, as read() gathers it: a channel, or
	// a grid run (the trunk)
	struct Segment
	{
		Place join, at;
		bool wet;
		fix rain;
	};

	Node build(int level, int32_t i, int32_t j, int32_t l) const;

	const Grid *m_grid;
	const World *m_world;
	uint32_t m_seed;
	uint64_t m_owner;
	int64_t m_radius;
	bool m_water;
	int m_levels = 0;
	int m_wet_levels = 0; // the levels that can carry water, 1..this (by their size: STREAM_LEAST_*)
	std::array<fix, MOST_LEVELS + 1> m_wet{};              // the rain each needs to (the grid's: 1 average)
	std::array<int64_t, MOST_LEVELS + 1> m_stream_width{}; // its stream's width (blocks, fixed)
	std::array<int64_t, MOST_LEVELS + 1> m_lattice{};      // [0]: the grid's spacing
	std::array<int32_t, MOST_LEVELS + 1> m_waves{};
};

} // namespace terragen
