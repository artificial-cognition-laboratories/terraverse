// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapgen_terra.h"
#include "mapgen/terra_column.h"
#include <optional>
#include "emerge.h"
#include "exceptions.h"
#include "mg_biome.h"
#include "mg_decoration.h"
#include "mg_ore.h"
#include "nodedef.h"
#include "settings.h"
#include "voxel.h"

using namespace terragen;

void MapgenTerraParams::readParams(const Settings *settings)
{
	settings->getNoEx("mgterra_code", code);
}

void MapgenTerraParams::writeParams(Settings *settings) const
{
	settings->set("mgterra_code", code);
}

static WorldCode decodeOrFail(const std::string &code)
{
	if (code.empty())
		throw BaseException("terra map generator: this world has no world code (mgterra_code in map_meta.txt)");
	try {
		return WorldCode::decode(code);
	} catch (const CodeError &e) {
		throw BaseException(std::string("terra map generator: ") + e.what());
	}
}

// The game's blocks for everything the generator places. Throws if the game
// lacks one: every rock and ore terragen can place must be a block.
terra::Blocks terraBlocks(const BiomeManager *bmgr, const NodeDefManager *ndef)
{
	content_t stone = ndef->getId("mapgen_stone"), water = ndef->getId("mapgen_water_source");
	if (stone == CONTENT_IGNORE || water == CONTENT_IGNORE)
		throw BaseException("terra map generator: the game has no 'mapgen_stone' or 'mapgen_water_source' alias");
	terra::Blocks blocks;
	for (int i = 0; i < (int)terragen::Biome::Count; i++) {
		auto *b = (const ::Biome *)bmgr->getByName(BIOME_NAMES[i]);
		if (!b)
			throw BaseException(std::string("terra map generator: the game registers no biome called '") +
					BIOME_NAMES[i] + "'");
		blocks.biomes[i] = { b->c_top, b->c_filler, b->c_riverbed, b->c_water_top,
			b->depth_top, b->depth_filler, b->depth_riverbed, b->depth_water_top };
	}
	auto need = [&](const std::string &alias) {
		content_t c = ndef->getId(alias);
		if (c == CONTENT_IGNORE)
			throw BaseException("terra map generator: the game has no '" + alias + "' alias");
		return c;
	};
	blocks.stone = stone;
	blocks.water = water;
	blocks.lava = need("mapgen_lava_source");
	blocks.placer = need("mapgen_placer");
	blocks.soil = need("mapgen_soil");
	for (int i = 0; i < (int)terragen::Rock::Count; i++)
		blocks.rock[i] = need(std::string("mapgen_rock_") + ROCK_NAMES[i]);
	blocks.ore[0] = CONTENT_IGNORE;
	for (int i = 1; i < (int)terragen::Ore::Count; i++)
		blocks.ore[i] = need(std::string("mapgen_ore_") + ORE_NAMES[i]);
	return blocks;
}

// The server's atlas, its window shared (a copy is the same window). A
// terra world is a planet: it has one.
static planet::Atlas sharedAtlas(const MapgenParams &params)
{
	if (params.planet_atlas.empty())
		throw BaseException("mapgen terra: the world has no planet_atlas (a terra world is a planet; worlds made before sprint 30 aren't)");
	if (!params.planet)
		throw BaseException("mapgen terra: a planet's mapgen needs the server's atlas (MapgenParams::planet)");
	if (params.planet->body(planet::Atlas::HOME).code() != terragen::Atlas(terragen::Atlas::parse(params.planet_atlas)).code())
		throw BaseException("mapgen terra: the server's atlas isn't this world's");
	return *params.planet;
}

MapgenTerra::MapgenTerra(MapgenTerraParams *params, EmergeParams *emerge) :
	MapgenBasic(MAPGEN_TERRA, params, emerge),
	m_world(decodeOrFail(params->code)),
	m_atlas(sharedAtlas(*params)),
	m_blocks(terraBlocks(m_bmgr, ndef))
{
	m_bodyWorlds = terragen::bodyWorlds(m_world.code(), terragen::Atlas::parse(params->planet_atlas));
	if ((int)m_bodyWorlds.size() != m_atlas.bodies())
		throw BaseException("mapgen terra: the server's atlas holds " + std::to_string(m_atlas.bodies()) +
				" bodies, the world code's system " + std::to_string(m_bodyWorlds.size()));
	m_bodies.resize(m_bodyWorlds.size());
	c_seam = ndef->getId("mapgen_seam");
	if (c_seam == CONTENT_IGNORE)
		throw BaseException("mapgen terra: an atlas world needs the mapgen_seam alias (tf_planet)");
	m_atlasDebug = params->planet_atlas_debug;
	if (m_atlasDebug) {
		const auto paint = [&](const char *name) {
			const content_t c = ndef->getId(std::string("tf_planet:debug_") + name);
			if (c == CONTENT_IGNORE)
				throw BaseException(std::string("mapgen terra: the debug sphere needs tf_planet:debug_") + name);
			return c;
		};
		m_paint = { paint("segment_a"), paint("segment_b"), paint("cap"), paint("core"), paint("segment_seam"),
			paint("cap_edge"), paint("cut"), paint("core_edge"), paint("line") };
	}
	for (int i = 0; i < (int)terragen::Biome::Count; i++)
		m_biomes[i] = (::Biome *)m_bmgr->getByName(BIOME_NAMES[i]);
	c_lava_source = m_blocks.lava;
	m_column.resize((size_t)csize.X * csize.Z);
}

MapgenTerra::BodyLand &MapgenTerra::bodyLand(int body) const
{
	std::lock_guard<std::mutex> hold(m_building);
	std::unique_ptr<BodyLand> &land = m_bodies.at(body);
	if (!land)
		land = std::make_unique<BodyLand>(m_bodyWorlds[body].code, m_blocks, water_level, m_atlas, body);
	return *land;
}

MapgenTerra::BodyLand &MapgenTerra::landAt(s16 x, s16 z) const
{
	return bodyLand(m_atlas.bodyAt({ x, 0, z }).value_or(planet::Atlas::HOME));
}

bool MapgenTerra::groundAt(s16 x, s16 z, terragen::Ground &ground) const
{
	const std::optional<terragen::Place> at = m_atlas.place({ x, 0, z });
	if (!at)
		return false; // seam at the surface: no ground
	const World &world = landAt(x, z).world;
	ground = world.ground(*at, world.sunlight(*at, std::nullopt));
	return true;
}

int MapgenTerra::spawnLevelAt(int body, double latitude, double longitude) const
{
	if (m_atlasDebug)
		return -1; // as getSpawnLevelAtPoint: the debug sphere's ground's top
	const World &world = bodyLand(body).world; // only its world: what's const
	const terragen::Place at = terragen::dirAt((fix)std::llround(latitude * ONE), (fix)std::llround(longitude * ONE));
	const Ground g = world.ground(at, world.sunlight(at, std::nullopt));
	if (g.kind != Water::None)
		return MAX_MAP_GENERATION_LIMIT;
	return water_level + g.terrain + 2;
}

void MapgenTerra::makeChunk(BlockMakeData *data)
{
	assert(data->vmanip);
	assert(data->nodedef);

	this->generating = true;
	this->vm = data->vmanip;
	this->ndef = data->nodedef;

	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	blockseed = getBlockSeed2(full_node_min, seed);
	// on a planet, by where the chunk is on its body, not where the window
	// put it (the engine's ores and decorations are seeded by it)
	if (const std::optional<planet::Atlas::Spot> at = m_atlas.spot({ node_min.X, node_min.Y, node_min.Z }))
			blockseed = (u32)(seed ^ ((u64)(u32)at->body * 0x9E3779B97F4A7C15ull) ^ ((u64)(u32)at->x * 1619) ^
					((u64)(u32)at->y * 31337) ^ ((u64)(u32)at->z * 52591));

	generateTerrain();
	paintSurface();
	updateHeightmap(node_min, node_max);

	// Caves and dungeons arrive with terragen's geology (a later sprint).

	if (flags & MG_ORES)
		m_emerge->oremgr->placeAllOres(this, blockseed, node_min, node_max);
	if (flags & MG_DECORATIONS)
		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);
	dustTopNodes();

	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);
	if (flags & MG_LIGHT)
		lightAtlas();

	this->generating = false;
}

MapgenTerra::SurfaceCover MapgenTerra::surfaceCover(size_t index) const
{
	const terra::Column &c = m_column.at(index);
	if (c.space)
		return { 0, 0, 0, 0, false, 0, false };
	const terragen::World &world = landAt(node_min.X + (s16)(index % csize.X), node_min.Z + (s16)(index / csize.X)).world;
	const terragen::Cover cover = world.cover(c.at, c.ground);
	const terragen::Column &g = c.ground.column;
	const auto real = [](fix v) { return (double)v / ONE; };
	return { real(cover.density), real(g.heat), real(g.wet), real(g.elevation), cover.bank, cover.roll, world.code().processes.life };
}

s16 MapgenTerra::generateTerrain()
{
	const v3s32 &em = vm->m_area.getExtent();
	s16 stone_max = -MAX_MAP_GENERATION_LIMIT;
	size_t index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++) {
		for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
			// through the reader: the nodes below ask it for this same column
			BodyLand &land = landAt(x, z);
			const terra::Column *read = land.reader.column({ x, 0, z });
			const terra::Column &c = m_column[index] = read ? *read : land.columns.at(x, z);
			u32 vi = vm->m_area.index(x, node_min.Y - 1, z);
			for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
				if (vm->m_data[vi].getContent() == CONTENT_IGNORE) {
					// every block from the one rule the far view also draws with
					vm->m_data[vi] = atlasNode(x, y, z);
					if (y <= c.height && y > stone_max)
						stone_max = y;
				}
				VoxelArea::add_y(em, vi, 1);
			}
		}
	}
	return stone_max;
}

/*
	An atlas's node: seam where no cell is stored, else what the land is
	there (terra::AtlasLand: the far view asks it too).
*/
MapNode MapgenTerra::atlasNode(s16 x, s16 y, s16 z)
{
	if (m_atlasDebug) {
		const std::optional<planet::Atlas::Cell> cell = m_atlas.cell({ x, y, z });
		if (!cell)
			return MapNode(c_seam);
		return MapNode(cell->k >= 0 && cell->region != terragen::Atlas::Region::Core ? CONTENT_AIR : atlasPaint(*cell));
	}
	const std::optional<MapNode> n = landAt(x, z).reader.node({ x, y, z });
	return n ? *n : MapNode(c_seam); // no cell: seam
}

/*
	The debug sphere: a cell painted by its seams (cut, the cap's circle,
	the core's sphere, a segment's edge), else its region (segments
	alternating, the caps, the core), with a line every 15° of latitude and
	30° of longitude.
*/
content_t MapgenTerra::atlasPaint(const planet::Atlas::Cell &cell) const
{
	using Region = terragen::Atlas::Region;
	const terragen::Atlas &atlas = m_atlas.body(cell.body);
	const uint8_t seams = atlas.quality(cell).seams;
	if (seams & terragen::Atlas::CUT)
		return m_paint.cut;
	if (seams & terragen::Atlas::CAP)
		return m_paint.capEdge;
	if (seams & terragen::Atlas::CORE)
		return m_paint.coreEdge;
	if (seams & terragen::Atlas::SEGMENT)
		return m_paint.segmentSeam;
	if (cell.region == Region::Cap)
		return m_paint.cap;
	if (cell.region == Region::Core)
		return m_paint.core;
	// on a line: the nearest 15° of latitude, or 30° of longitude, passes
	// within half a block of this cell's centre
	const terragen::Atlas::Vec p = atlas.centre(cell);
	const double r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	const double lat = std::asin(p.y / r), lon = std::atan2(p.z, p.x);
	const double latStep = M_PI / 12, lonStep = M_PI / 6;
	const double offLat = std::abs(lat - std::round(lat / latStep) * latStep) * r;
	const double offLon = std::abs(lon - std::round(lon / lonStep) * lonStep) * r * std::cos(lat);
	if (offLat < 0.5 || offLon < 0.5)
		return m_paint.line;
	return cell.index % 2 ? m_paint.segmentA : m_paint.segmentB;
}

void MapgenTerra::lightAtlas()
{
	// y innermost: the land's reader keeps one column, and a whole column
	// built per node is most of what an open block costs
	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++)
	for (s16 y = node_min.Y; y <= node_max.Y; y++) {
		MapNode &n = vm->m_data[vm->m_area.index(x, y, z)];
		const ContentFeatures &f = ndef->get(n);
		u8 light = 0;
		const std::optional<planet::Atlas::Cell> cell = m_atlas.cell({ x, y, z });
		if (!cell) {
			light = LIGHT_SUN; // seam: lit, so a face beside it isn't black
		} else if (cell->region != terragen::Atlas::Region::Core && (f.sunlight_propagates || f.liquid_type != LIQUID_NONE)) {
			const s32 ground = m_atlasDebug ? -1 : landAt(x, z).reader.column({ x, y, z })->height;
			if (cell->k > ground) {
				if (f.liquid_type == LIQUID_NONE) {
					light = LIGHT_SUN;
				} else {
					// under water, dimmer with depth below its surface
					const s32 surface = m_atlasDebug ? ground : landAt(x, z).reader.column({ x, y, z })->water;
					light = (u8)std::max<s32>(0, LIGHT_SUN - 1 - 2 * (surface - (s32)cell->k));
				}
			}
		}
		n.setLight(LIGHTBANK_DAY, light, f.getLightingFlags());
		n.setLight(LIGHTBANK_NIGHT, 0, f.getLightingFlags());
	}
}

void MapgenTerra::paintSurface()
{
	size_t index = 0;
	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++)
		biomemap[index] = m_biomes[(int)m_column[index].biome]->index;
}


int MapgenTerra::getGroundLevelAtPoint(v2s16 p)
{
	if (m_atlasDebug)
		return -1; // the debug sphere's ground: layer −1, at every place
	Ground g;
	if (!groundAt(p.X, p.Y, g))
		return MAX_MAP_GENERATION_LIMIT;
	return water_level + g.terrain;
}

int MapgenTerra::getSpawnLevelAtPoint(v2s16 p)
{
	if (m_atlasDebug)
		return -1; // on the debug sphere, its ground's top (core.get_spawn_level adds one)
	Ground g;
	if (!groundAt(p.X, p.Y, g) || g.kind != Water::None)
		return MAX_MAP_GENERATION_LIMIT; // in the sea, a lake or a river: the engine looks elsewhere
	return water_level + g.terrain + 2; // above the top node and any dust
}
