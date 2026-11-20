// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2015-2018 paramat

#include <cmath>
#include "src/server/world/generate/mapgen.h"
#include "src/shared/map/voxel.h"
#include "src/shared/core/noise.h"
#include "src/shared/core/gamedef.h"
#include "src/server/world/generate/mg_biome.h"
#include "src/shared/map/mapnode.h"
#include "src/shared/map/map.h"
#include "src/shared/content/nodedef.h"
#include "src/server/world/load/emerge.h"
#include "src/shared/map/voxelalgorithms.h"
#include "src/shared/platform/profiler.h"
#include "src/shared/platform/settings.h"
#include "src/server/world/generate/treegen.h"
#include "src/shared/util/numeric.h"
#include "src/shared/util/directiontables.h"
#include "src/shared/platform/log.h"
#include "src/server/world/generate/mapgen_singlenode.h"
#include "src/server/world/generate/mapgen_terra.h"

const FlagDesc flagdesc_mapgen[] = {
	{"caves",       MG_CAVES},
	{"dungeons",    MG_DUNGEONS},
	{"light",       MG_LIGHT},
	{"decorations", MG_DECORATIONS},
	{"biomes",      MG_BIOMES},
	{"ores",        MG_ORES},
	{NULL,          0}
};

const FlagDesc flagdesc_gennotify[] = {
	{"dungeon",          1 << GENNOTIFY_DUNGEON},
	{"temple",           1 << GENNOTIFY_TEMPLE},
	{"cave_begin",       1 << GENNOTIFY_CAVE_BEGIN},
	{"cave_end",         1 << GENNOTIFY_CAVE_END},
	{"large_cave_begin", 1 << GENNOTIFY_LARGECAVE_BEGIN},
	{"large_cave_end",   1 << GENNOTIFY_LARGECAVE_END},
	{"decoration",       1 << GENNOTIFY_DECORATION},
	{"custom",           1 << GENNOTIFY_CUSTOM},
	{NULL,               0}
};

struct MapgenDesc {
	const char *name;
	bool is_user_visible;
};

////
//// Built-in mapgens
////

// Indexed by enum MapgenType (mapgen.h): terra is the game's; singlenode is for checks.
static MapgenDesc g_reg_mapgens[] = {
	{"singlenode", true},
	{"terra",      true},
};

static_assert(
	ARRLEN(g_reg_mapgens) == MAPGEN_INVALID,
	"g_reg_mapgens is wrong size");

////
//// Mapgen
////

Mapgen::Mapgen(int mapgenid, MapgenParams *params, EmergeParams *emerge) :
	gennotify(emerge->createNotifier())
{
	id           = mapgenid;
	water_level  = params->water_level;
	mapgen_limit = params->mapgen_limit;
	flags        = params->flags;
	csize        = params->chunksize * MAP_BLOCKSIZE;

	/*
		We are losing half our entropy by doing this, but it is necessary to
		preserve reverse compatibility.  If the top half of our current 64 bit
		seeds ever starts getting used, existing worlds will break due to a
		different hash outcome and no way to differentiate between versions.

		A solution could be to add a new bit to designate that the top half of
		the seed value should be used, essentially a 1-bit version code, but
		this would require increasing the total size of a seed to 9 bytes (yuck)

		It's probably okay if this never gets fixed.  4.2 billion possibilities
		ought to be enough for anyone.
	*/
	seed = (s32)params->seed;

	m_emerge  = emerge;
	ndef      = emerge->ndef;
}

Mapgen::~Mapgen()
{
	delete m_emerge; // this is our responsibility
}


MapgenType Mapgen::getMapgenType(const std::string &mgname)
{
	for (size_t i = 0; i != ARRLEN(g_reg_mapgens); i++) {
		if (mgname == g_reg_mapgens[i].name)
			return (MapgenType)i;
	}

	return MAPGEN_INVALID;
}


const char *Mapgen::getMapgenName(MapgenType mgtype)
{
	size_t index = (size_t)mgtype;
	if (index == MAPGEN_INVALID || index >= ARRLEN(g_reg_mapgens))
		return "invalid";

	auto &it = g_reg_mapgens[index];
	assert(it.name);
	return it.name;
}


Mapgen *Mapgen::createMapgen(MapgenType mgtype, MapgenParams *params,
	EmergeParams *emerge)
{
	switch (mgtype) {
	case MAPGEN_SINGLENODE:
		return new MapgenSinglenode((MapgenSinglenodeParams *)params, emerge);
	case MAPGEN_TERRA:
		return new MapgenTerra((MapgenTerraParams *)params, emerge);
	default:
		return nullptr;
	}
}


MapgenParams *Mapgen::createMapgenParams(MapgenType mgtype)
{
	switch (mgtype) {
	case MAPGEN_SINGLENODE:
		return new MapgenSinglenodeParams;
	case MAPGEN_TERRA:
		return new MapgenTerraParams;
	default:
		return nullptr;
	}
}


void Mapgen::getMapgenNames(std::vector<const char *> *mgnames, bool include_hidden)
{
	for (u32 i = 0; i != ARRLEN(g_reg_mapgens); i++) {
		if (include_hidden || g_reg_mapgens[i].is_user_visible)
			mgnames->push_back(g_reg_mapgens[i].name);
	}
}

void Mapgen::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mg_flags", flagdesc_mapgen,
		 MG_CAVES | MG_DUNGEONS | MG_LIGHT | MG_DECORATIONS | MG_BIOMES | MG_ORES);

	for (int i = 0; i < (int)MAPGEN_INVALID; ++i) {
		MapgenParams *params = createMapgenParams((MapgenType)i);
		params->setDefaultSettings(settings);
		delete params;
	}
}

u32 Mapgen::getBlockSeed(v3s16 p, s32 seed)
{
	return (u32)seed   +
		p.Z * 38134234 +
		p.Y * 42123    +
		p.X * 23;
}


u32 Mapgen::getBlockSeed2(v3s16 p, s32 seed)
{
	// Multiply by unsigned number to avoid signed overflow (UB)
	u32 n = 1619U * p.X + 31337U * p.Y + 52591U * p.Z + 1013U * seed;
	n = (n >> 13) ^ n;
	return (n * (n * n * 60493 + 19990303) + 1376312589);
}


// Returns -MAX_MAP_GENERATION_LIMIT if not found
s16 Mapgen::findGroundLevel(v2s16 p2d, s16 ymin, s16 ymax)
{
	const v3s32 &em = vm->m_area.getExtent();
	u32 i = vm->m_area.index(p2d.X, ymax, p2d.Y);
	s16 y;

	for (y = ymax; y >= ymin; y--) {
		MapNode &n = vm->m_data[i];
		if (ndef->get(n).walkable)
			break;

		VoxelArea::add_y(em, i, -1);
	}
	return (y >= ymin) ? y : -MAX_MAP_GENERATION_LIMIT;
}


// Returns -MAX_MAP_GENERATION_LIMIT if not found or if ground is found first
s16 Mapgen::findLiquidSurface(v2s16 p2d, s16 ymin, s16 ymax)
{
	const v3s32 &em = vm->m_area.getExtent();
	u32 i = vm->m_area.index(p2d.X, ymax, p2d.Y);
	s16 y;

	for (y = ymax; y >= ymin; y--) {
		MapNode &n = vm->m_data[i];
		if (ndef->get(n).walkable)
			return -MAX_MAP_GENERATION_LIMIT;

		if (ndef->get(n).isLiquid())
			break;

		VoxelArea::add_y(em, i, -1);
	}
	return (y >= ymin) ? y : -MAX_MAP_GENERATION_LIMIT;
}


void Mapgen::updateHeightmap(v3s16 nmin, v3s16 nmax)
{
	if (!heightmap)
		return;

	//TimeTaker t("Mapgen::updateHeightmap", NULL, PRECISION_MICRO);
	int index = 0;
	for (s16 z = nmin.Z; z <= nmax.Z; z++) {
		for (s16 x = nmin.X; x <= nmax.X; x++, index++) {
			s16 y = findGroundLevel(v2s16(x, z), nmin.Y, nmax.Y);

			heightmap[index] = y;
		}
	}
}


void Mapgen::getSurfaces(v2s16 p2d, s16 ymin, s16 ymax,
	std::vector<s16> &floors, std::vector<s16> &ceilings)
{
	const v3s32 &em = vm->m_area.getExtent();

	bool is_walkable = false;
	u32 vi = vm->m_area.index(p2d.X, ymax, p2d.Y);
	MapNode mn_max = vm->m_data[vi];
	bool walkable_above = ndef->get(mn_max).walkable;
	VoxelArea::add_y(em, vi, -1);

	for (s16 y = ymax - 1; y >= ymin; y--) {
		MapNode mn = vm->m_data[vi];
		is_walkable = ndef->get(mn).walkable;

		if (is_walkable && !walkable_above) {
			floors.push_back(y);
		} else if (!is_walkable && walkable_above) {
			ceilings.push_back(y + 1);
		}

		VoxelArea::add_y(em, vi, -1);
		walkable_above = is_walkable;
	}
}


inline bool Mapgen::isLiquidHorizontallyFlowable(u32 vi, v3s32 em)
{
	u32 vi_neg_x = vi;
	VoxelArea::add_x(em, vi_neg_x, -1);
	if (vm->m_data[vi_neg_x].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_nx = ndef->get(vm->m_data[vi_neg_x]);
		if (c_nx.floodable && !c_nx.isLiquid())
			return true;
	}
	u32 vi_pos_x = vi;
	VoxelArea::add_x(em, vi_pos_x, +1);
	if (vm->m_data[vi_pos_x].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_px = ndef->get(vm->m_data[vi_pos_x]);
		if (c_px.floodable && !c_px.isLiquid())
			return true;
	}
	u32 vi_neg_z = vi;
	VoxelArea::add_z(em, vi_neg_z, -1);
	if (vm->m_data[vi_neg_z].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_nz = ndef->get(vm->m_data[vi_neg_z]);
		if (c_nz.floodable && !c_nz.isLiquid())
			return true;
	}
	u32 vi_pos_z = vi;
	VoxelArea::add_z(em, vi_pos_z, +1);
	if (vm->m_data[vi_pos_z].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_pz = ndef->get(vm->m_data[vi_pos_z]);
		if (c_pz.floodable && !c_pz.isLiquid())
			return true;
	}
	return false;
}

void Mapgen::updateLiquid(UniqueQueue<v3s16> *trans_liquid, v3s16 nmin, v3s16 nmax)
{
	bool isignored, isliquid, wasignored, wasliquid, waschecked, waspushed;
	content_t was_n;
	const v3s32 &em = vm->m_area.getExtent();

	isignored = true;
	isliquid = false;
	was_n = CONTENT_IGNORE;

	for (s16 z = nmin.Z + 1; z <= nmax.Z - 1; z++)
	for (s16 x = nmin.X + 1; x <= nmax.X - 1; x++) {
		wasignored = true;
		wasliquid = false;
		waschecked = false;
		waspushed = false;

		u32 vi = vm->m_area.index(x, nmax.Y, z);
		for (s16 y = nmax.Y; y >= nmin.Y; y--) {
			const content_t is_n = vm->m_data[vi].getContent();
			if (is_n != was_n) {
				isignored = is_n == CONTENT_IGNORE;
				isliquid = ndef->get(is_n).isLiquid();
			}

			if (isignored || wasignored || isliquid == wasliquid) {
				// Neither topmost node of liquid column nor topmost node below column
				waschecked = false;
				waspushed = false;
			} else if (isliquid) {
				// This is the topmost node in the column
				bool ispushed = false;
				if (isLiquidHorizontallyFlowable(vi, em)) {
					trans_liquid->push_back(v3s16(x, y, z));
					ispushed = true;
				}
				// Remember waschecked and waspushed to avoid repeated
				// checks/pushes in case the column consists of only this node
				waschecked = true;
				waspushed = ispushed;
			} else {
				// This is the topmost node below a liquid column
				u32 vi_above = vi;
				VoxelArea::add_y(em, vi_above, 1);
				if (!waspushed && (ndef->get(vm->m_data[vi]).floodable ||
						(!waschecked && isLiquidHorizontallyFlowable(vi_above, em)))) {
					// Push back the lowest node in the column which is one
					// node above this one
					trans_liquid->push_back(v3s16(x, y + 1, z));
				}
			}

			was_n = is_n;
			wasliquid = isliquid;
			wasignored = isignored;
			VoxelArea::add_y(em, vi, -1);
		}
	}
}


void Mapgen::setLighting(u8 light, v3s16 nmin, v3s16 nmax)
{
	ScopeProfiler sp(g_profiler, "EmergeThread: update lighting", SPT_AVG);
	VoxelArea a(nmin, nmax);

	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++) {
			u32 i = vm->m_area.index(a.MinEdge.X, y, z);
			for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++, i++)
				vm->m_data[i].param1 = light;
		}
	}
}


void Mapgen::lightSpread(VoxelArea &a, std::queue<std::pair<v3s16, u8>> &queue,
	const v3s16 &p, u8 light)
{
	if (light <= 1 || !a.contains(p))
		return;

	u32 vi = vm->m_area.index(p);
	MapNode &n = vm->m_data[vi];

	// Decay light in each of the banks separately
	u8 light_day = light & 0x0F;
	if (light_day > 0)
		light_day -= 0x01;

	u8 light_night = light & 0xF0;
	if (light_night > 0)
		light_night -= 0x10;

	// Bail out only if we have no more light from either bank to propagate, or
	// we hit a solid block that light cannot pass through.
	if ((light_day  <= (n.param1 & 0x0F) &&
			light_night <= (n.param1 & 0xF0)) ||
			!ndef->getLightingFlags(n).light_propagates)
		return;

	// MYMAX still needed here because we only exit early if both banks have
	// nothing to propagate anymore.
	light = MYMAX(light_day, n.param1 & 0x0F) |
			MYMAX(light_night, n.param1 & 0xF0);

	n.param1 = light;

	// add to queue
	queue.emplace(p, light);
}


void Mapgen::calcLighting(v3s16 nmin, v3s16 nmax, v3s16 full_nmin, v3s16 full_nmax,
	bool propagate_shadow)
{
	ScopeProfiler sp(g_profiler, "EmergeThread: update lighting", SPT_AVG);

	propagateSunlight(nmin, nmax, propagate_shadow);
	spreadLight(full_nmin, full_nmax);
}


void Mapgen::propagateSunlight(v3s16 nmin, v3s16 nmax, bool propagate_shadow)
{
	//TimeTaker t("propagateSunlight");
	VoxelArea a(nmin, nmax);
	bool block_is_underground = (water_level >= nmax.Y);
	const v3s32 &em = vm->m_area.getExtent();

	// NOTE: Direct access to the low 4 bits of param1 is okay here because,
	// by definition, sunlight will never be in the night lightbank.

	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++) {
			// see if we can get a light value from the overtop
			u32 i = vm->m_area.index(x, a.MaxEdge.Y + 1, z);
			if (vm->m_data[i].getContent() == CONTENT_IGNORE) {
				if (block_is_underground)
					continue;
			} else if ((vm->m_data[i].param1 & 0x0F) != LIGHT_SUN &&
					propagate_shadow) {
				continue;
			}
			VoxelArea::add_y(em, i, -1);

			for (int y = a.MaxEdge.Y; y >= a.MinEdge.Y; y--) {
				MapNode &n = vm->m_data[i];
				if (!ndef->getLightingFlags(n).sunlight_propagates)
					break;
				n.param1 = LIGHT_SUN;
				VoxelArea::add_y(em, i, -1);
			}
		}
	}
	//printf("propagateSunlight: %dms\n", t.stop());
}


void Mapgen::spreadLight(const v3s16 &nmin, const v3s16 &nmax)
{
	//TimeTaker t("spreadLight");
	std::queue<std::pair<v3s16, u8>> queue;
	VoxelArea a(nmin, nmax);

	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++) {
			u32 i = vm->m_area.index(a.MinEdge.X, y, z);
			for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++, i++) {
				MapNode &n = vm->m_data[i];
				if (n.getContent() == CONTENT_IGNORE)
					continue;

				ContentLightingFlags cf = ndef->getLightingFlags(n);
				if (!cf.light_propagates)
					continue;

				// TODO(hmmmmm): Abstract away direct param1 accesses with a
				// wrapper, but something lighter than MapNode::get/setLight

				u8 light_produced = cf.light_source;
				if (light_produced)
					n.param1 = light_produced | (light_produced << 4);

				u8 light = n.param1;
				if (light) {
					const v3s16 p(x, y, z);
					// spread to all 6 neighbor nodes
					for (const auto &dir : g_6dirs)
						lightSpread(a, queue, p + dir, light);
				}
			}
		}
	}

	while (!queue.empty()) {
		const auto &i = queue.front();
		// spread to all 6 neighbor nodes
		for (const auto &dir : g_6dirs)
			lightSpread(a, queue, i.first + dir, i.second);
		queue.pop();
	}

	//printf("spreadLight: %lums\n", t.stop());
}


////
//// MapgenBasic
////

MapgenBasic::MapgenBasic(int mapgenid, MapgenParams *params, EmergeParams *emerge)
	: Mapgen(mapgenid, params, emerge)
{
	this->m_bmgr   = emerge->biomemgr;

	//// Here, 'stride' refers to the number of elements needed to skip to index
	//// an adjacent element for that coordinate in noise/height/biome maps
	//// (*not* vmanip content map!)

	// Note there is no X stride explicitly defined.  Items adjacent in the X
	// coordinate are assumed to be adjacent in memory as well (i.e. stride of 1).

	// Number of elements to skip to get to the next Y coordinate
	this->ystride = csize.X;

	// Number of elements to skip to get to the next Z coordinate
	this->zstride = csize.X * csize.Y;

	// Z-stride value for maps oversized for 1-down overgeneration
	this->zstride_1d = csize.X * (csize.Y + 1);

	// Z-stride value for maps oversized for 1-up 1-down overgeneration
	this->zstride_1u1d = csize.X * (csize.Y + 2);

	//// Allocate heightmap
	this->heightmap = new s16[csize.X * csize.Z];

	//// Initialize biome generator
	biomegen = emerge->biomegen;
	biomegen->assertChunkSize(csize);
	biomemap = biomegen->biomemap;

	//// Look up some commonly used content
	c_stone              = ndef->getId("mapgen_stone");
	c_water_source       = ndef->getId("mapgen_water_source");
	c_river_water_source = ndef->getId("mapgen_river_water_source");
	c_lava_source        = ndef->getId("mapgen_lava_source");
	c_cobble             = ndef->getId("mapgen_cobble");

	// Fall back to more basic content if not defined.
	// Lava falls back to water as both are suitable as cave liquids.
	if (c_lava_source == CONTENT_IGNORE)
		c_lava_source = c_water_source;

	if (c_stone == CONTENT_IGNORE)
		errorstream << "Mapgen: Mapgen alias 'mapgen_stone' is invalid!" << std::endl;
	if (c_water_source == CONTENT_IGNORE)
		errorstream << "Mapgen: Mapgen alias 'mapgen_water_source' is invalid!" << std::endl;
	if (c_river_water_source == CONTENT_IGNORE)
		warningstream << "Mapgen: Mapgen alias 'mapgen_river_water_source' is invalid!" << std::endl;
}


MapgenBasic::~MapgenBasic()
{
	delete []heightmap;
}


void MapgenBasic::generateBiomes()
{
	// can't generate biomes without a biome generator!
	assert(biomegen);
	assert(biomemap);

	const v3s32 &em = vm->m_area.getExtent();
	u32 index = 0;

	noise_filler_depth->noiseMap2D(node_min.X, node_min.Z);

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = NULL;
		biome_t water_biome_index = 0;
		u16 depth_top = 0;
		u16 base_filler = 0;
		u16 depth_water_top = 0;
		u16 depth_riverbed = 0;
		u32 vi = vm->m_area.index(x, node_max.Y, z);

		s16 biome_y_next = biomegen->getNextTransitionY(node_max.Y);

		// Check node at base of mapchunk above, either a node of a previously
		// generated mapchunk or if not, a node of overgenerated base terrain.
		content_t c_above = vm->m_data[vi + em.X].getContent();
		bool air_above = c_above == CONTENT_AIR;
		bool river_water_above = c_above == c_river_water_source;
		bool water_above = c_above == c_water_source || river_water_above;

		biomemap[index] = BIOME_NONE;

		// If there is air or water above enable top/filler placement, otherwise force
		// nplaced to stone level by setting a number exceeding any possible filler depth.
		u16 nplaced = (air_above || water_above) ? 0 : U16_MAX;

		for (s16 y = node_max.Y; y >= node_min.Y; y--) {
			content_t c = vm->m_data[vi].getContent();
			const bool biome_outdated = !biome || y <= biome_y_next;
			// Biome is (re)calculated:
			// 1. At the surface of stone below air or water.
			// 2. At the surface of water below air.
			// 3. When stone or water is detected but biome has not yet been calculated.
			// 4. When stone or water is detected just below a biome's lower limit.
			bool is_stone_surface = (c == c_stone) &&
				(air_above || water_above || biome_outdated); // 1, 3, 4

			bool is_water_surface =
				(c == c_water_source || c == c_river_water_source) &&
				(air_above || biome_outdated); // 2, 3, 4

			if (is_stone_surface || is_water_surface) {
				if (biome_outdated) {
					// (Re)calculate biome
					biome = biomegen->getBiomeAtIndex(index, v3s16(x, y, z));
					biome_y_next = biomegen->getNextTransitionY(y);

					if (x == node_min.X && z == node_min.Z && false) {
						dstream << "biomegen: biome at " << y << " is " << biome->name
							<< ", next at " << biome_y_next << std::endl;
					}
				}

				// Add biome to biomemap at first stone surface detected
				if (biomemap[index] == BIOME_NONE && is_stone_surface)
					biomemap[index] = biome->index;

				// Store biome of first water surface detected, as a fallback
				// entry for the biomemap.
				if (water_biome_index == 0 && is_water_surface)
					water_biome_index = biome->index;

				depth_top = biome->depth_top;
				base_filler = MYMAX(depth_top +
					biome->depth_filler +
					noise_filler_depth->result[index], 0.0f);
				depth_water_top = biome->depth_water_top;
				depth_riverbed = biome->depth_riverbed;
			}

			if (c == c_stone) {
				content_t c_below = vm->m_data[vi - em.X].getContent();

				// If the node below isn't solid, make this node stone, so that
				// any top/filler nodes above are structurally supported.
				// This is done by aborting the cycle of top/filler placement
				// immediately by forcing nplaced to stone level.
				if (c_below == CONTENT_AIR
						|| c_below == c_water_source
						|| c_below == c_river_water_source)
					nplaced = U16_MAX;

				if (river_water_above) {
					if (nplaced < depth_riverbed) {
						vm->m_data[vi] = MapNode(biome->c_riverbed);
						nplaced++;
					} else {
						nplaced = U16_MAX;  // Disable top/filler placement
						river_water_above = false;
					}
				} else if (nplaced < depth_top) {
					vm->m_data[vi] = MapNode(biome->c_top);
					nplaced++;
				} else if (nplaced < base_filler) {
					vm->m_data[vi] = MapNode(biome->c_filler);
					nplaced++;
				} else {
					vm->m_data[vi] = MapNode(biome->c_stone);
					nplaced = U16_MAX;  // Disable top/filler placement
				}

				air_above = false;
				water_above = false;
			} else if (c == c_water_source) {
				vm->m_data[vi] = MapNode((y > (s32)(water_level - depth_water_top))
						? biome->c_water_top : biome->c_water);
				nplaced = 0;  // Enable top/filler placement for next surface
				air_above = false;
				water_above = true;
			} else if (c == c_river_water_source) {
				vm->m_data[vi] = MapNode(biome->c_river_water);
				nplaced = 0;  // Enable riverbed placement for next surface
				air_above = false;
				water_above = true;
				river_water_above = true;
			} else if (c == CONTENT_AIR) {
				nplaced = 0;  // Enable top/filler placement for next surface
				air_above = true;
				water_above = false;
			} else {  // Possible various nodes overgenerated from neighboring mapchunks
				nplaced = U16_MAX;  // Disable top/filler placement
				air_above = false;
				water_above = false;
			}

			VoxelArea::add_y(em, vi, -1);
		}
		// If no stone surface detected in mapchunk column and a water surface
		// biome fallback exists, add it to the biomemap. This avoids water
		// surface decorations failing in deep water.
		if (biomemap[index] == BIOME_NONE && water_biome_index != 0)
			biomemap[index] = water_biome_index;
	}
}


void MapgenBasic::dustTopNodes()
{
	if (node_max.Y < water_level)
		return;

	const v3s32 &em = vm->m_area.getExtent();
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = (Biome *)m_bmgr->getRaw(biomemap[index]);

		if (biome->c_dust == CONTENT_IGNORE)
			continue;

		// Check if mapchunk above has generated, if so, drop dust from 16 nodes
		// above current mapchunk top, above decorations that will extend above
		// the current mapchunk. If the mapchunk above has not generated, it
		// will provide this required dust when it does.
		u32 vi = vm->m_area.index(x, full_node_max.Y, z);
		content_t c_full_max = vm->m_data[vi].getContent();
		s16 y_start;

		if (c_full_max == CONTENT_AIR) {
			y_start = full_node_max.Y - 1;
		} else if (c_full_max == CONTENT_IGNORE) {
			vi = vm->m_area.index(x, node_max.Y + 1, z);
			content_t c_max = vm->m_data[vi].getContent();

			if (c_max == CONTENT_AIR)
				y_start = node_max.Y;
			else
				continue;
		} else {
			continue;
		}

		vi = vm->m_area.index(x, y_start, z);
		for (s16 y = y_start; y >= node_min.Y - 1; y--) {
			if (vm->m_data[vi].getContent() != CONTENT_AIR)
				break;

			VoxelArea::add_y(em, vi, -1);
		}

		content_t c = vm->m_data[vi].getContent();
		NodeDrawType dtype = ndef->get(c).drawtype;
		// Only place on cubic, walkable, non-dust nodes.
		// Dust check needed due to avoid double layer of dust caused by
		// dropping dust from 16 nodes above mapchunk top.
		if ((dtype == NDT_NORMAL ||
				dtype == NDT_ALLFACES ||
				dtype == NDT_ALLFACES_OPTIONAL ||
				dtype == NDT_GLASSLIKE ||
				dtype == NDT_GLASSLIKE_FRAMED ||
				dtype == NDT_GLASSLIKE_FRAMED_OPTIONAL) &&
				ndef->get(c).walkable && c != biome->c_dust) {
			VoxelArea::add_y(em, vi, 1);
			vm->m_data[vi] = MapNode(biome->c_dust);
		}
	}
}

////
//// GenerateNotifier
////

GenerateNotifier::GenerateNotifier(u32 notify_on,
	const std::set<u32> *notify_on_deco_ids,
	const std::set<std::string> *notify_on_custom)
{
	m_notify_on = notify_on;
	m_notify_on_deco_ids = notify_on_deco_ids;
	m_notify_on_custom = notify_on_custom;
}


bool GenerateNotifier::addEvent(GenNotifyType type, v3s16 pos)
{
	assert(type != GENNOTIFY_DECORATION && type != GENNOTIFY_CUSTOM);
	if (!shouldNotifyOn(type))
		return false;

	GenNotifyEvent gne;
	gne.type = type;
	gne.pos  = pos;
	m_notify_events.emplace_back(std::move(gne));
	return true;
}


bool GenerateNotifier::addDecorationEvent(v3s16 pos, u32 id)
{
	if (!shouldNotifyOn(GENNOTIFY_DECORATION))
		return false;
	// check if data relating to this decoration was requested
	assert(m_notify_on_deco_ids);
	if (m_notify_on_deco_ids->find(id) == m_notify_on_deco_ids->cend())
		return false;

	GenNotifyEvent gne;
	gne.type = GENNOTIFY_DECORATION;
	gne.pos  = pos;
	gne.id   = id;
	m_notify_events.emplace_back(std::move(gne));
	return true;
}


bool GenerateNotifier::setCustom(const std::string &key, const std::string &value)
{
	if (!shouldNotifyOn(GENNOTIFY_CUSTOM))
		return false;
	// check if this key was requested to be saved
	assert(m_notify_on_custom);
	if (m_notify_on_custom->count(key) == 0)
		return false;

	m_notify_custom[key] = value;
	return true;
}


void GenerateNotifier::getEvents(
	std::map<std::string, std::vector<v3s16>> &event_map) const
{
	for (auto &gn : m_notify_events) {
		assert(gn.type != GENNOTIFY_CUSTOM); // never stored in this list

		std::string name = (gn.type == GENNOTIFY_DECORATION) ?
			"decoration#"+ itos(gn.id) :
			flagdesc_gennotify[gn.type].name;

		event_map[name].push_back(gn.pos);
	}
}


void GenerateNotifier::clearEvents()
{
	m_notify_events.clear();
	m_notify_custom.clear();
}


////
//// MapgenParams
////


MapgenParams::~MapgenParams()
{
	delete bparams;
}


void MapgenParams::readParams(const Settings *settings)
{
	// should always be used via MapSettingsManager
	assert(settings != g_settings);

	std::string seed_str;
	if (settings->getNoEx("seed", seed_str)) {
		if (!seed_str.empty())
			seed = read_seed(seed_str.c_str());
		else
			myrand_bytes(&seed, sizeof(seed));
	}

	std::string mg_name;
	if (settings->getNoEx("mg_name", mg_name)) {
		mgtype = Mapgen::getMapgenType(mg_name);
		// a world made by a generator this game no longer has: generating on
		// in another would join two lands at the edge of what's saved
		if (mgtype == MAPGEN_INVALID)
			throw BaseException("this world was made by mapgen '" + mg_name +
					"', which Terra Firma doesn't have (only terra)");
	}

	settings->getS16NoEx("water_level", water_level);
	settings->getS16NoEx("mapgen_limit", mapgen_limit);
	settings->getNoEx("planet_atlas", planet_atlas);
	planet_atlas_debug = settings->exists("planet_atlas_debug") && settings->getBool("planet_atlas_debug");
	settings->getFlagStrNoEx("mg_flags", flags, flagdesc_mapgen);

	std::string chunksize_str;
	settings->getNoEx("chunksize", chunksize_str);
	if (is_number(chunksize_str)) {
		chunksize = v3s16(stoi(chunksize_str, 1, 999));
	} else if (auto tmp = str_to_v3f(chunksize_str); tmp.has_value()) {
		chunksize = v3s16(
			rangelim(tmp->X, 1, 999),
			rangelim(tmp->Y, 1, 999),
			rangelim(tmp->Z, 1, 999)
		);
	} else if (!chunksize_str.empty()) {
		errorstream << "MapgenParams: invalid chunksize \"" << chunksize_str
			<< "\"" << std::endl;
	}
	// Finally check the volume limit
	if (u32 v = chunksize.X * chunksize.Y * chunksize.Z; v > MAX_CHUNK_VOLUME) {
		errorstream << "MapgenParams: chunksize " << chunksize
			<< " is too big (volume > " << MAX_CHUNK_VOLUME
			<< "), falling back to the default." << std::endl;
		chunksize = v3s16(5);
	}

	delete bparams;
	bparams = BiomeManager::createBiomeParams(BIOMEGEN_ORIGINAL);
	if (bparams) {
		bparams->readParams(settings);
		bparams->seed = seed;
	}
}


void MapgenParams::writeParams(Settings *settings) const
{
	settings->set("mg_name", Mapgen::getMapgenName(mgtype));
	settings->setU64("seed", seed);
	settings->setS16("water_level", water_level);
	settings->setS16("mapgen_limit", mapgen_limit);
	if (!planet_atlas.empty())
		settings->set("planet_atlas", planet_atlas);
	if (planet_atlas_debug)
		settings->setBool("planet_atlas_debug", true);
	settings->setFlagStr("mg_flags", flags, flagdesc_mapgen);

	// Write as number if cubic, for backwards-compatibility
	if (chunksize.X == chunksize.Y && chunksize.Y == chunksize.Z) {
		settings->setS16("chunksize", chunksize.X);
	} else {
		settings->setV3F("chunksize", v3f::from(chunksize));
	}

	if (bparams)
		bparams->writeParams(settings);
}


s32 MapgenParams::getSpawnRangeMax()
{
	auto [emin, emax] = get_mapgen_edges(mapgen_limit, chunksize);
	s32 min_xz = std::max(emin.X, emin.Z), max_xz = std::min(emax.X, emax.Z);
	return std::min(-min_xz, max_xz);
}


std::pair<v3s16, v3s16> get_mapgen_edges(s16 mapgen_limit, v3s16 chunksize)
{
	// Effective mapgen limit, in blocks
	// Uses same calculation as ServerMap::blockpos_over_mapgen_limit(v3s16 p)
	s16 mapgen_limit_b = rangelim(mapgen_limit,
		0, MAX_MAP_GENERATION_LIMIT) / MAP_BLOCKSIZE;
	// Effective mapgen limits, in nodes
	s16 mapgen_limit_min = -mapgen_limit_b * MAP_BLOCKSIZE;
	s16 mapgen_limit_max = (mapgen_limit_b + 1) * MAP_BLOCKSIZE - 1;

	const auto &calculate = [&] (s16 cs) -> std::pair<s16, s16> {
		// Central chunk offset, in blocks
		s16 ccoff_b = -cs / 2;
		// Chunksize, in nodes
		s32 csize_n = cs * MAP_BLOCKSIZE;
		// Minp/maxp of central chunk, in nodes
		s16 ccmin = ccoff_b * MAP_BLOCKSIZE;
		s16 ccmax = ccmin + csize_n - 1;
		// Fullminp/fullmaxp of central chunk, in nodes
		s16 ccfmin = ccmin - MAP_BLOCKSIZE;
		s16 ccfmax = ccmax + MAP_BLOCKSIZE;
		// Number of complete chunks from central chunk fullminp/fullmaxp
		// to effective mapgen limits.
		s16 numcmin = std::max((ccfmin - mapgen_limit_min) / csize_n, 0);
		s16 numcmax = std::max((mapgen_limit_max - ccfmax) / csize_n, 0);
		return {ccmin - numcmin * csize_n, ccmax + numcmax * csize_n};
	};

	// Mapgen edges, in nodes
	v3s16 emin, emax;
	std::tie(emin.X, emax.X) = calculate(chunksize.X);
	std::tie(emin.Y, emax.Y) = calculate(chunksize.Y);
	std::tie(emin.Z, emax.Z) = calculate(chunksize.Z);
	return {emin, emax};
}

// Terra Firma: the list of positions, in this chunk, of loose ground (group
// `loose`) under water, or open above with water beside it at its level.
std::vector<v3s16> mapgenShore(Mapgen &mg)
{
	const NodeDefManager *ndef = mg.ndef;
	const content_t water = ndef->getId("mapgen_water_source");
	MMVManip &vm = *mg.vm;
	const VoxelArea &area = vm.m_area;
	const s16 margin = MAP_BLOCKSIZE;
	const v3s16 lo = area.MinEdge + v3s16(margin, margin, margin), hi = area.MaxEdge - v3s16(margin, margin, margin);
	thread_local const NodeDefManager *known_of = nullptr;
	thread_local std::vector<s8> known;
	if (known_of != ndef) {
		known_of = ndef;
		known.assign(65536, -1);
	}
	const auto loose = [&](content_t c) {
		s8 &k = known[c];
		if (k < 0)
			k = itemgroup_get(ndef->get(c).groups, "loose") > 0;
		return k > 0;
	};
	static const v2s16 SIDES[8] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
	std::vector<v3s16> out;
	if (water == CONTENT_IGNORE)
		return out; // a world with no water has no shore
	for (s16 z = lo.Z; z <= hi.Z; z++)
	for (s16 y = lo.Y; y <= hi.Y; y++)
	for (s16 x = lo.X; x <= hi.X; x++) {
		if (!loose(vm.m_data[area.index(x, y, z)].getContent()))
			continue;
		const content_t above = vm.m_data[area.index(x, y + 1, z)].getContent();
		bool wet = above == water;
		if (!wet && !loose(above))
			for (const v2s16 &d : SIDES)
				if (area.contains(v3s16(x + d.X, y, z + d.Y)) && vm.m_data[area.index(x + d.X, y, z + d.Y)].getContent() == water) {
					wet = true;
					break;
				}
		if (wet)
			out.push_back(v3s16(x, y, z));
	}
	return out;
}

