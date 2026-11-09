// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <optional>

#include "far/palette.h"
#include "far/terrain.h"
#include "irrlichttypes_bloated.h"
#include <S3DVertex.h>
#include <unordered_map>
#include <vector>

namespace far {

// A tile as triangles: in blocks relative to the tile's origin, scaled by
// BS; vertex colours carry each face's colour and shade (as the near mesh's
// do in daylight). Split into parts that fit 16-bit indices, each within a
// quarter: a coarse tile standing in for one finer one is drawn only over it.
struct Mesh
{
	struct Part
	{
		std::vector<video::S3DVertex> vertices;
		std::vector<u16> indices;
		u8 quarter = 0; // which of the tile's children it lies in (TileKey's: bit 0 x, bit 1 z)
	};
	std::vector<Part> parts;
};

// A tile's mesh, flat: in the tile's own cells (a layout bends it onto its planet).
Mesh mesh(const Tile &tile, const Palette &palette);

} // namespace far
