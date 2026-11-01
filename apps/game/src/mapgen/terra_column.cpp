// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terra_column.h"
#include "mapgen.h" // MAX_MAP_GENERATION_LIMIT
#include "planet/atlas.h"
#include <stdexcept>

using namespace terragen;

namespace terra {

Columns::Columns(const World &world, const Blocks &blocks, s16 water_level, const planet::Atlas &atlas, int body) :
	m_world(world), m_atlas(atlas), m_body(body), m_blocks(blocks), m_water_level(water_level)
{
	atlas.body(body); // throws for no such body
}

Column Columns::at(s16 x, s16 z) const
{
	const std::optional<int> b = m_atlas.bodyAt({ x, 0, z });
	if (b && *b != m_body)
		throw std::logic_error("terra::Columns: storage column (" + std::to_string(x) + ", " + std::to_string(z) +
				") is body " + std::to_string(*b) + "'s, these are body " + std::to_string(m_body) + "'s");
	// the surface shell's column (its storage y is its layer)
	const std::optional<Place> place = m_atlas.place({ x, 0, z });
	return place ? atPlace(*place) : nothing(); // seam: the generator fills nodes by their own place
}

// No world: air all the way up (storage that is no place on an atlas).
Column Columns::nothing() const
{
	Column c{};
	c.space = true;
	c.biome = Biome::Snow;
	c.height = c.water = -MAX_MAP_GENERATION_LIMIT - 1; // every block above it: air
	c.top = CONTENT_AIR;
	c.top_end = c.filler_end = c.ice_end = c.height;
	return c;
}

Column Columns::atPlace(const Place &at) const
{
	Column c{};
	c.at = at;
	Ground g = m_world.ground(at, m_world.sunlight(at, std::nullopt));
	const BiomeBlocks &b = m_blocks.biomes[(int)g.column.biome];
	c.biome = g.column.biome;
	c.ground = g;
	c.height = m_water_level + g.terrain;
	// dry ground has no water surface: put it at the ground
	c.water = g.kind == Water::None ? c.height : m_water_level + g.water;
	c.lava = g.kind == Water::Lava;
	c.water_above = g.kind != Water::None && !c.lava;
	c.strata = m_world.strata(at, g);

	// the soil: rock begins under it. A layer of plain stone isn't soil:
	// there the bedrock shows (peaks)
	bool riverbed = (g.kind == Water::River || g.kind == Water::Lake) && g.water > g.terrain &&
			b.riverbed != CONTENT_IGNORE;
	s32 top_depth = riverbed ? (b.riverbed == m_blocks.stone ? 0 : b.depth_riverbed)
			: (b.top == m_blocks.stone ? 0 : b.depth_top);
	c.soil = top_depth + (b.filler == m_blocks.stone ? 0 : b.depth_filler);
	c.soil_point = soilIndex(m_world.soil(at, g, *c.strata));

	// under a river or lake the top is the biome's riverbed
	bool bed = (g.kind == Water::River || g.kind == Water::Lake) && c.water > c.height;
	bool use_bed = bed && b.riverbed != CONTENT_IGNORE;
	c.top = use_bed ? b.riverbed : b.top;
	// rivers out of the mountains carry gold and tin in their gravel, here and
	// there (by the column's block at the surface)
	const std::array<int32_t, 3> p = m_world.solid(at, 0);
	if (bed && g.kind == Water::River && c.strata->placer &&
			(uint32_t)((p[0] * 73856093) ^ (p[1] * 19349663) ^ (p[2] * 83492791)) % 7 == 0)
		c.top = m_blocks.placer;
	c.top_end = c.height - (use_bed ? b.depth_riverbed : b.depth_top);
	c.filler_end = c.top_end - b.depth_filler;

	c.ice_end = b.water_top != CONTENT_IGNORE && c.height < m_water_level
			? m_water_level - b.depth_water_top : m_water_level;
	return c;
}

MapNode Columns::node(const Column &c, s32 y) const
{
	const Blocks &b = m_blocks;
	if (y > c.height) {
		if (y > c.water)
			return MapNode(CONTENT_AIR);
		if (c.lava)
			return MapNode(b.lava);
		// a frozen sea: the biome's water-top over open water
		if (y > c.ice_end && y <= m_water_level)
			return MapNode(b.biomes[(int)c.biome].water_top);
		return MapNode(b.water);
	}
	s32 depth = c.height - y;
	MapNode n(b.stone); // inside the soil: painted below
	if (depth >= c.soil) {
		Rock rock = m_world.rock(*c.strata, depth - c.soil, y);
		Cave cave = m_world.cave(*c.strata, rock, depth, y, c.water_above);
		if (cave == Cave::Air)
			return MapNode(CONTENT_AIR);
		if (cave == Cave::Water)
			return MapNode(b.water);
		if (cave == Cave::Lava)
			return MapNode(b.lava);
		Ore ore = m_world.ore(*c.strata, rock, depth - c.soil, y);
		// a rock blends toward its neighbour (granite toward gneiss…): param2 is how far
		n = ore == Ore::None ? MapNode(b.rock[(int)rock], 0, m_world.blend(c.at, y)) : MapNode(b.ore[(int)ore]);
	}
	// the soil is painted over plain stone, top then filler
	if (n.getContent() == b.stone && y > c.filler_end) {
		content_t fill = y > c.top_end ? c.top : b.biomes[(int)c.biome].filler;
		if (fill != CONTENT_IGNORE)
			// soil carries what it's made of here as its param2
			return fill == b.soil ? MapNode(fill, 0, c.soil_point) : MapNode(fill);
	}
	return n;
}

} // namespace terra
