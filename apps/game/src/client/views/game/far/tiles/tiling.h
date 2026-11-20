// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include "library/world/terragen/src/out/atlas.h"
#include <vector>

namespace far {

struct TileKey;
struct Mesh;

/*
	The far view's tiling of an atlas planet (planning/sprint-32-atlas-lod.md):
	each region's own surface grid (a segment's, a cap's) as a quadtree, so a
	tile's cells are the atlas's cells, 2ᵏ×2ᵏ of them at level k, and at
	level 0 exactly the game's. A tile's key is its region (`face`: the
	grid's index in terragen::Atlas::grids), its level, and its place in the
	grid at that level, from the grid's low corner. Within a tile, as on the
	map, positions are in finest cells from the tile's corner, a cell centred
	on whole numbers; y is the layer. Geometry is the region's own chart
	(terragen::Atlas::at), so rows bend as the grid does.
*/
class Tiling
{
public:
	using Vec = terragen::Atlas::Vec;

	explicit Tiling(const terragen::Atlas &atlas);

	const terragen::Atlas &atlas() const { return m_atlas; }
	double radius() const { return m_atlas.params().radius; }

	// The coarsest tiles: each region with a surface as a row of squares
	// as wide as its grid's short side (a segment's grid is long and thin).
	// A square can still reach past its grid's far edge.
	const std::vector<TileKey> &roots() const { return m_roots; }

	// The atlas cell a tile's cell stands for at the surface layer (the
	// middle fine cell of a coarse one), (i, j) from the tile's corner.
	terragen::Atlas::Cell cell(const TileKey &key, s32 i, s32 j) const;

	// Whether a cell of a tile's region is inside the region's grid's box.
	bool inGrid(const TileKey &key, const terragen::Atlas::Cell &cell) const;
	// Whether any of a tile is in its region's grid (a quadtree's square
	// reaches past a grid that isn't).
	bool within(const TileKey &key) const;

	// A tile's position on the planet, from its centre.
	Vec point(const TileKey &key, double x, double y, double z) const;
	// A tile's middle at the sea: where its mesh is drawn from, and how far
	// it is. Both of the part of it in its grid (past the grid its chart
	// only carries on, somewhere else on the planet).
	Vec middle(const TileKey &key) const;
	// How far across a tile is (blocks, along its diagonal at the sea).
	double diagonal(const TileKey &key) const;
	// A tile's mesh (far/mesh.h, flat in its cells) bent onto the planet:
	// each vertex at its place in the tile's region, round its middle (BS
	// units), its normal turned with it.
	void place(Mesh &mesh, const TileKey &key) const;

private:
	// The part of a tile in its grid: [x0, x1] × [z0, z1], finest cells from
	// the tile's corner (cell edges).
	struct Extent
	{
		double x0, x1, z0, z1;
	};
	Extent extent(const TileKey &key) const;

	const terragen::Atlas m_atlas;
	std::vector<TileKey> m_roots;
};

} // namespace far
