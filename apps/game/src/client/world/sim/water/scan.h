// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include <optional>
#include <unordered_set>
#include <vector>

class ClientMap;
class NodeDefManager;

namespace liquids {

class Grid;

/*
	Looks at the map under the grid: per node, the water the server has (the
	highest liquid in reach: the sea's top, a river's held level, a fill) and
	the ground under each column. A tile's looked at when its blocks change,
	nearest the camera first, a few a frame.
*/
class Scan
{
public:
	struct Opts
	{
		const NodeDefManager &ndef;
		Grid &grid;
	};
	explicit Scan(Opts o);

	// Nodes whose liquid the grid took or let go of: drawn flat again
	struct Handover
	{
		std::vector<v3s16> taken, let_go; // map blocks
	};
	// The blocks whose fills (`changed`) or nodes (`blocks`, both) changed mark
	// their tiles; then as many tiles as the frame allows are looked at
	Handover frame(const std::unordered_set<v3s16> &blocks, const std::unordered_set<v3s16> &changed, ClientMap &map,
			const Frame &f, s64 since);

private:
	// What a node's column holds, as looked at
	struct Found
	{
		std::optional<v3s16> top; // its liquid's highest node
		s32 target = NONE;        // where its water stands, world sixteenths
		Kind kind = Kind::None;
		std::array<float, 2> flow{}; // its river's current, nodes/s
		std::array<s32, 256> floor;  // its columns' ground (res × res of them), world sixteenths
		bool coloured = false;
		u8 r = 0, g = 0, b = 0;
	};
	void tile(int t, ClientMap &map, const Frame &f, Handover &handover);
	// Nothing: a block not here yet, before anything was found
	std::optional<Found> node(v3s16 at, s16 hi, s16 lo, ClientMap &map, const Frame &f) const;

	const NodeDefManager &m_ndef;
	Grid &m_grid;
};

} // namespace liquids
