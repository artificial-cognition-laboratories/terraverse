// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Which nodes round a block are cells, found a box at a time: a seam block's
// mesh and the blocks across from it both start from them.

#include "atlas.h"
#include <cmath>

namespace planet {

std::vector<bool> Atlas::cellsAround(Block block) const
{
	constexpr int32_t W = BLOCK + 2;
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	std::vector<bool> isCell((size_t)W * W * W);
	markCells(isCell, low, { 0, 0, 0 }, { W, W, W });
	return isCell;
}

// The nodes of [from, to) (offsets from low) that are cells, into isCell,
// a box at a time where the box can be answered whole, else halved down to a
// few nodes asked one by one. (Asking every node of every seam block's rim
// was most of choosing a view's blocks near a cap's edge.)
void Atlas::markCells(std::vector<bool> &isCell, Node low, std::array<int32_t, 3> from, std::array<int32_t, 3> to) const
{
	constexpr int32_t W = BLOCK + 2;
	const auto fill = [&](bool value) {
		for (int32_t z = from[2]; z < to[2]; z++)
			for (int32_t y = from[1]; y < to[1]; y++)
				for (int32_t x = from[0]; x < to[0]; x++)
					isCell[((size_t)z * W + y) * W + x] = value;
	};
	const std::optional<bool> whole = boxCells({ low.x + from[0], low.y + from[1], low.z + from[2] },
			{ low.x + to[0] - 1, low.y + to[1] - 1, low.z + to[2] - 1 });
	if (whole)
		return fill(*whole);
	const std::array<int32_t, 3> size{ to[0] - from[0], to[1] - from[1], to[2] - from[2] };
	const int longest = size[0] >= size[1] && size[0] >= size[2] ? 0 : size[1] >= size[2] ? 1 : 2;
	if (size[longest] <= 4) {
		for (int32_t z = from[2]; z < to[2]; z++)
			for (int32_t y = from[1]; y < to[1]; y++)
				for (int32_t x = from[0]; x < to[0]; x++)
					isCell[((size_t)z * W + y) * W + x] = cell({ low.x + x, low.y + y, low.z + z }).has_value();
		return;
	}
	std::array<int32_t, 3> middle = to;
	middle[longest] = from[longest] + size[longest] / 2;
	markCells(isCell, low, from, middle);
	std::array<int32_t, 3> rest = from;
	rest[longest] = middle[longest];
	markCells(isCell, low, rest, to);
}

std::optional<bool> Atlas::boxCells(Node lo, Node hi) const
{
	// one slot (a slot is a square, so its two corners decide), and in it no
	// tile, or the box clear of the tile's: no cells
	const std::optional<int> slot = slotAt(lo);
	if (!slot || slotAt(hi) != slot)
		return std::nullopt;
	const std::optional<Tile> &tile = layout().slots[*slot];
	if (!tile || hi.x < tile->rect.x || lo.x >= tile->rect.x + tile->rect.width || hi.z < tile->rect.z ||
			lo.z >= tile->rect.z + tile->rect.depth || hi.y < tile->k0 || lo.y > tile->k1)
		return false;
	if (lo.x < tile->rect.x || hi.x >= tile->rect.x + tile->rect.width || lo.z < tile->rect.z ||
			hi.z >= tile->rect.z + tile->rect.depth || lo.y < tile->k0 || hi.y > tile->k1)
		return std::nullopt;
	// wholly the tile's: a cell exists by how far its centre is inside the
	// territory (terragen's retained: wholly past CLEAR either way), and the
	// box's middle decides for all of it when it's far enough in or out for
	// how fast that changes over the box: terragen's inside is a min of arc
	// lengths v (from a segment's edge at r cos lat, a cap's circle, the
	// core), changing by at most sqrt(1 + (v / ρ)²) a block, ρ the distance
	// from the axis (a segment's) or the centre. (TestPlanetAtlas holds it to
	// asking each node.)
	constexpr double CLEAR = 0.9;
	const auto cellAt = [&](Node n) {
		return Cell(tile->body, { tile->kind, tile->index, tile->i0 + (n.x - tile->rect.x), tile->j0 + (n.z - tile->rect.z), n.y });
	};
	const Cell middle = cellAt({ lo.x + (hi.x - lo.x) / 2, lo.y + (hi.y - lo.y) / 2, lo.z + (hi.z - lo.z) / 2 });
	const terragen::Atlas &shape = shapeOf(middle);
	const terragen::Atlas::Vec m = chart(middle, 0, 0, 0);
	double reach = 0;
	for (int k = 0; k < 8; k++) {
		const terragen::Atlas::Vec c = chart(cellAt({ k & 1 ? hi.x : lo.x, k & 2 ? hi.y : lo.y, k & 4 ? hi.z : lo.z }), 0, 0, 0);
		reach = std::max(reach, std::sqrt((c.x - m.x) * (c.x - m.x) + (c.y - m.y) * (c.y - m.y) + (c.z - m.z) * (c.z - m.z)));
	}
	reach += 1; // a node's centre between the corners' bulges no further
	const double clear = shape.clearance(middle);
	const double rho = (tile->kind == terragen::Atlas::Region::Segment ? std::hypot(m.x, m.z) : std::sqrt(m.x * m.x + m.y * m.y + m.z * m.z)) - reach;
	if (rho <= 0)
		return std::nullopt;
	// accepted only when the change over the box is under |clear|, so every v there is under 2|clear|
	const double v = 2 * std::abs(clear) / rho, steepest = std::sqrt(1 + v * v);
	if (std::abs(clear) < CLEAR + steepest * reach)
		return std::nullopt;
	if (clear < 0)
		return false;
	if (tile->kind == terragen::Atlas::Region::Core || hi.y < shape.params().height)
		return true;
	return std::nullopt;
}

} // namespace planet
