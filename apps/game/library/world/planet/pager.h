// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/planet/atlas.h"
#include <functional>
#include <vector>

namespace planet {

/*
	Which of a planet's tiles are in the map's window (planning/sprint-39-tiles.md):
	the server's, the one place the window changes. A tile is put in when
	it's needed (round a player, or asked for), into an empty slot; when
	none is empty, a tile no one has needed for HOLD seconds leaves first.
	A tile leaves only once its blocks are out of the map (saved where they
	are on the planet). Every slot held by a tile needed within HOLD: the
	window is full, and that throws.
*/
class Pager
{
public:
	struct Hooks
	{
		// Take a slot's blocks out of the map, saving them; false when some
		// are in use (the slot isn't emptied, and another is tried).
		std::function<bool(int slot)> empty;
		// A slot holds another tile (or none) now: what's been sent of it is gone.
		std::function<void(int slot)> moved;
	};
	static constexpr double HOLD = 30;     // seconds a needed tile stays
	static constexpr double AROUND = 1024; // blocks round a player kept in the window

	Pager(Atlas &atlas, Hooks hooks);

	// A new world: when every tile of every body fits the window, every
	// tile is put in, needed for ever (a small world is never paged). False
	// when it doesn't fit (the window starts empty).
	bool all();
	// A point of a body (from its centre, blocks).
	struct Point
	{
		int body;
		terragen::Atlas::Vec at;
	};
	// The tiles round each of these points (at the ground under them), at
	// `now` (seconds).
	void keep(const std::vector<Point> &points, double now);
	// A tile in the window, at `now`: its slot.
	int need(const Atlas::TileId &tile, double now);
	// A cell's node, the tiles round it put in the window (AROUND: what's
	// made there steps across its tile's edges).
	Atlas::Node node(const Atlas::Cell &cell, double now);

private:
	Atlas &m_atlas;
	Hooks m_hooks;
	std::vector<double> m_needed; // by slot: when its tile was last needed
};

} // namespace planet
