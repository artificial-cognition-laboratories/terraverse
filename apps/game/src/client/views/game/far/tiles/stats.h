// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/land/terrain.h"
#include <set>
#include <tuple>
#include "src/shared/base/irrlichttypes.h"

namespace far {

/*
	What a Tiles does with its tiles, told to the trace as counters each
	frame (library/trace): taken from the workers, let go, taken again after
	being let go (work done twice: what a probe of zooming in and out looks
	for, planetarium/run/probe), and what it holds. Nothing here changes
	what's drawn.
*/
class TileStats
{
public:
	// a tile taken from the workers: a sketch (a stand-in) or the tile itself, its mesh's bytes
	void taken(const TileKey &key, bool sketch, size_t bytes);
	// tiles let go to stay under the memory budget
	void evicted(size_t count);
	// once an update: what's held, wanted and drawn now, and the gaps showing
	// through to what's behind: holes (nothing may stand in) and stand-ins not
	// ready (each Tiles' summed in a frame)
	void now(size_t held, size_t bytes, size_t pending, size_t drawn, size_t holes, size_t unready);

private:
	using Id = std::tuple<s32, s32, s32, s32>;
	std::set<Id> m_ever; // every tile taken whole, ever: one taken again was built twice
};

} // namespace far
