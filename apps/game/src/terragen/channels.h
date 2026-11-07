// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "sphere.h"
#include <array>
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
	static constexpr int MOST_LEVELS = 6;
	static constexpr int STREAM_LEVELS = 3; // the levels that can carry water

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
	};

	Channels(const WorldCode &code, const Grid &grid, const World &world, int64_t radius, int64_t cell_blocks);

	bool active() const { return m_levels > 0; }
	int levels() const { return m_levels; }
	int64_t spacing(int level) const { return m_lattice[level]; } // place units
	// `warped` is the place read for channels; `range` how much it's a rising range
	// `least`: no level with a lattice finer than this (place units): what a
	// far tile's cells are too wide to show, and a whole search per cell to find
	Read read(const Place &warped, fix range, int64_t least = 0) const;
	// a node, or one that doesn't exist (level 1..levels())
	const Node &node(int level, int32_t i, int32_t j, int32_t l) const;
	int32_t latticeOf(int level, int32_t c) const;

private:
	Node build(int level, int32_t i, int32_t j, int32_t l) const;

	const Grid *m_grid;
	const World *m_world;
	uint32_t m_seed;
	uint64_t m_owner;
	int64_t m_radius;
	bool m_water;
	int m_levels = 0;
	std::array<int64_t, MOST_LEVELS + 1> m_lattice{}; // [0]: the grid's spacing
	std::array<int32_t, MOST_LEVELS + 1> m_waves{};
};

} // namespace terragen
