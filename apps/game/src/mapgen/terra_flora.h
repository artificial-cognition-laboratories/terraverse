// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "mapnode.h"
#include "flora/forest.h"
#include "shape/shape.h"
#include <functional>
#include <map>

class MMVManip;
class NodeDefManager;

/*
	Plants in the map (sprint 113): a grown plant (flora::Plant) written as
	the game's plant blocks. Its voxels fall in nodes: each node is the part
	most of it is (wood wins: it holds the rest up), cut to the sixteenths
	its voxels fill, and coloured its lineage's (param2: an index into the
	plant blocks' palette).
*/
namespace terra {

// The game's plant blocks, by flora::Part. Throws if the game lacks one.
struct PlantBlocks
{
	content_t wood, leaf, flesh, flower;
	content_t growth; // a small plant's foot: the plant's a sprig, drawn by the client
	content_t of(flora::Part part) const;
};
PlantBlocks plantBlocks(const NodeDefManager *ndef);

// The plant blocks' palette: 256 colours (also drawn into tf_world's texture by scripts/flora-palette.py).
u32 paletteColour(u8 index);
u8 paletteIndex(u32 rgb);

// Where a plant may put a node: (x, y, z) is open. Whole nodes only; shapes beside.
using Open = std::function<bool(v3s16)>;

// `plant` at `foot` (the node over the ground), turned a quarter `turn` times and mirrored if `mirror`:
// its part between the nodes `lo` and `hi`, where `open` lets it.
void place(const flora::Plant &plant, const flora::Colours &colours, v3s16 foot, int turn, bool mirror, v3s16 lo, v3s16 hi,
		const PlantBlocks &blocks, MMVManip &vm, const Open &open, std::map<v3s16, shape::Shape> &shapes);

} // namespace terra
