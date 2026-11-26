// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/parts.h"
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
	void tile(int t, ClientMap &map, const Frame &f, Handover &handover);
	void besideSea(int t, bool sea);

	const NodeDefManager &m_ndef;
	Grid &m_grid;
};

} // namespace liquids
