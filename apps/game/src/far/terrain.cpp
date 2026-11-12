// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terragen/worlds.h"
#include "terrain.h"
#include "map.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace terragen;

namespace far {

// A body's world code: the horizon's is the home's; another's is found
// from it (terragen/worlds.h), and then the atlas must hold the system's
// bodies. (A viewer of the home alone, the Planetarium, lays out the home.)
static WorldCode codeOf(const Horizon &horizon, const planet::Atlas &atlas, int body)
{
	if (body == planet::Atlas::HOME)
		return WorldCode::decode(horizon.code);
	const std::vector<terragen::BodyWorld> worlds =
			terragen::bodyWorlds(WorldCode::decode(horizon.code), atlas.body(planet::Atlas::HOME).params());
	if ((int)worlds.size() != atlas.bodies())
		throw BaseException("far: the atlas holds " + std::to_string(atlas.bodies()) + " bodies, the world code's system " +
				std::to_string(worlds.size()));
	return worlds.at(body).code;
}

Generator::Generator(const Horizon &horizon, const planet::Atlas &atlas, int body) :
	m_horizon(horizon),
	m_world(terragen::worldFor(codeOf(horizon, atlas, body))),
	m_atlas(atlas),
	m_body(body),
	m_tiling(std::make_unique<Tiling>(atlas.body(body))),
	m_columns(std::make_unique<terra::Columns>(*m_world, horizon.content.blocks, horizon.water_level, m_atlas, body)),
	m_land(std::make_unique<terra::AtlasLand>(*m_columns, m_atlas))
{
}

Generator::~Generator() = default;

static Cell cellOf(const terra::Columns &columns, const terra::Column &c)
{
	const terra::Blocks &b = columns.blocks();
	Cell cell{};
	if (c.space) {
		cell.space = true;
		cell.ground = cell.water = columns.waterLevel(); // the wall's face toward it runs down to the sea
		return cell;
	}
	auto solid = [&](s32 y) {
		MapNode n = columns.node(c, y);
		content_t id = n.getContent();
		// a cave or spring opening at a cliff: show the rock it's cut in
		return id == CONTENT_AIR || id == b.water || id == b.lava ? MapNode(b.stone) : n;
	};
	cell.ground = c.height;
	cell.top = solid(c.height);
	cell.top_end = c.top_end;
	cell.filler_end = c.filler_end;
	cell.filler = c.top_end > c.filler_end ? solid(c.top_end) : cell.top;
	cell.rock = solid(std::min(c.filler_end, c.height - c.soil));
	cell.water = c.height;
	if (c.water > c.height) {
		cell.water = c.water;
		cell.liquid = columns.node(c, c.water);
	}
	return cell;
}

Cell Generator::seenAt(const terragen::Place &place) const
{
	return cellOf(*m_columns, m_columns->atPlace(place));
}

s32 Generator::surfaceAt(const planet::Atlas::Cell &cell) const
{
	const Cell c = cellOf(*m_columns, m_columns->atPlace(m_atlas.place(cell)));
	return std::max(c.ground, c.water);
}

s32 Generator::groundAt(const planet::Atlas::Cell &cell) const
{
	return cellOf(*m_columns, m_columns->atPlace(m_atlas.place(cell))).ground;
}

Cell Generator::column(s32 x, s32 z) const
{
	const terra::Column c = m_columns->at(x, z);
	if (c.space)
		throw BaseException("far::Generator::column: storage column (" + std::to_string(x) + ", " +
				std::to_string(z) + ") is no place on the planet");
	return cellOf(*m_columns, c);
}

/*
	An atlas tile (far/tiling.h): each cell the column of the atlas cell it
	stands for at the surface layer, as the generator builds it there
	(terra::AtlasLand's rule: the column at the cell's place; noise at its
	stored node), leaving out what's narrower than its cells (World::Coarse).
	A cell the atlas has none of (trimmed, or past the
	grid) is space: nothing drawn, and its neighbours draw no cliff toward it
	(the region across the seam draws that ground).
*/
Tile Generator::build(const TileKey &key) const
{
	return made(key, 1);
}

Tile Generator::sketch(const TileKey &key) const
{
	return made(key, 2);
}

Tile Generator::made(const TileKey &key, s32 stride) const
{
	Tile tile;
	tile.key = key;
	tile.cells.resize((size_t)(TILE + 2) * (TILE + 2));
	const terragen::Atlas &atlas = m_atlas.body(m_body);
	// rivers their own width: a coarse cell shows one where its middle's on it
	// (widened to show on every cell they crossed, they swelled from afar;
	// sprint 114)
	const World::Coarse coarse(*m_world, (int64_t)stride << key.level, false);
	// the cell a sketch copies (itself when it's read): the one before it,
	// counting from the tile's edge, -1
	const auto from = [&](s32 i) { return i - (i + 1) % stride; };
	for (s32 j = -1; j <= TILE; j++)
		for (s32 i = -1; i <= TILE; i++) {
			const terragen::Atlas::Cell cell = m_tiling->cell(key, i, j);
			Cell &out = tile.at(i, j);
			// past its grid's box a region's chart only carries on (and can come
			// back round into its territory): no cell, whatever exists() says
			if (!m_tiling->inGrid(key, cell) || !atlas.exists(cell)) {
				out = Cell{};
				out.space = true;
				out.ground = out.water = std::numeric_limits<s32>::max() / 2;
				continue;
			}
			const Cell &copy = tile.at(from(i), from(j));
			if ((from(i) != i || from(j) != j) && !copy.space) {
				out = copy;
				continue;
			}
			out = cellOf(*m_columns, m_columns->atPlace(m_atlas.place(planet::Atlas::Cell(m_body, cell))));
		}
	return tile;
}

} // namespace far
