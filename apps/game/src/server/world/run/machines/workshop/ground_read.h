// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include "src/shared/map/block/mapnode.h"
#include "library/geometry/shape/shape.h"
#include <optional>
#include <unordered_map>
#include <vector>

class Map;
class NodeDefManager;
namespace planet { class Atlas; }

namespace workshop {

/*
	A map block's ground read in two halves, so the slow one runs off the
	frame's thread: a snapshot of the block and the margin round it that
	smoothing reaches (taken on the map's thread: a copy a block at a time,
	cheap), then its boxes worked out from the snapshot alone (anywhere: the
	smoothing, the cut checks and the node boxes, up to tens of ms a block in
	a forest or by a shore, which was the player's physics spiking flying fast).
*/
// The planet's seam (core:seam, between its grids): a wall, or open to a mover
// the atlas carries across it (Atlas::carry), which must reach it to cross
enum class Seam { Wall, Open };

class GroundSnapshot
{
public:
	// None: the block isn't loaded, or it holds a node not yet known.
	static std::optional<GroundSnapshot> take(Map &map, v3s16 block);

	// Its boxes: nodes, from the block's least node's centre. `ndef` and
	// `atlas` (none: not a planet) outlive the call; both are read only.
	std::vector<aabb3f> boxes(const NodeDefManager *ndef, const planet::Atlas *atlas, Seam seam) const;

	// The margin round the block it holds: what a loose node's smoothing reads.
	static constexpr int MARGIN = 6; // 1 + ground::REACH + 2
	static constexpr int SIDE = 16 + 2 * MARGIN;

private:
	GroundSnapshot(v3s16 block) : m_block(block) {}

	// What it holds at a node (map coordinates); past its margin, unknown.
	MapNode node(v3s16 p) const;
	const shape::Shape *shape(v3s16 p) const;
	int index(v3s16 p) const; // -1 past the margin

	v3s16 m_block;
	std::vector<MapNode> m_nodes; // SIDE³, from the margin's least corner
	std::unordered_map<int, shape::Shape> m_shapes; // by index: the cut nodes
};

} // namespace workshop
