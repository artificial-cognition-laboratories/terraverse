// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "planet/atlas.h"
#include "mapgen/atlas_land.h"
#include "planet/atlas.h"
#include <optional>

#include "mapgen.h"
#include "terragen/world.h"
#include "terragen/worlds.h"
#include <memory>
#include <mutex>
#include <vector>
#include "mapgen/terra_column.h"
#include <string>

class Biome;
class BiomeManager;

// The game's blocks for everything the terra generator places (and the far
// view draws). Throws if the game lacks one.
terra::Blocks terraBlocks(const BiomeManager *bmgr, const NodeDefManager *ndef);

struct MapgenTerraParams : public MapgenParams
{
	// The world code (TF10.…): everything terragen needs. Written into the
	// world's map_meta.txt when the world is created.
	std::string code;

	void readParams(const Settings *settings);
	void writeParams(Settings *settings) const;
};

/*
	Terra Firma's generator: terrain, climate and biome come from terragen, a
	pure function of the world code (src/terragen). The engine's own passes —
	ores, decorations, dust, liquids, lighting — run on top, driven by the
	biome terragen chose for each column.
*/
class MapgenTerra : public MapgenBasic
{
public:
	MapgenTerra(MapgenTerraParams *params, EmergeParams *emerge);
	~MapgenTerra() = default;

	virtual MapgenType getType() const { return MAPGEN_TERRA; }

	virtual void makeChunk(BlockMakeData *data);
	int getSpawnLevelAtPoint(v2s16 p);
	int getGroundLevelAtPoint(v2s16 p);
	// Terra Firma: the same at a place on a body (radians), with no node of
	// the map: a spawn is looked for before its tile is in the window.
	// MAX_MAP_GENERATION_LIMIT where no one spawns. Safe from another thread.
	int spawnLevelAt(int body, double latitude, double longitude) const;

private:
public:
	// What grows on column `index` of the current chunk (z outer, x inner)
	struct SurfaceCover
	{
		double density, heat, wet, elevation;
		bool bank;
		u32 roll;
		bool alive; // on a body where life arose: nothing at all grows on the rest
	};
	SurfaceCover surfaceCover(size_t index) const;

private:
	// Fills stone, water and air from terragen; records each column. Returns
	// the highest stone placed.
	s16 generateTerrain();
	// Fills the biome map (the engine's passes read it). The soil is painted
	// as the terrain is filled: terra::Columns::node.
	void paintSurface();
	// The finished column at storage column (x, z), or false for seam.
	bool groundAt(s16 x, s16 z, terragen::Ground &ground) const;

	terragen::World m_world;
	planet::Atlas m_atlas;                  // the world's (planet_atlas)
	content_t c_seam = CONTENT_IGNORE;      // an atlas's storage that is no cell
	MapNode atlasNode(s16 x, s16 y, s16 z);
	// The debug sphere's block for a cell: what it is, painted.
	content_t atlasPaint(const planet::Atlas::Cell &cell) const;
	// An atlas's light, from the planet, not the map: sunlit above its
	// column's ground (storage columns are no columns of the planet).
	void lightAtlas();
	bool m_atlasDebug = false;
	struct AtlasPaints
	{
		content_t segmentA, segmentB, cap, core, segmentSeam, capEdge, cut, coreEdge, line;
	} m_paint{};
	terra::Blocks m_blocks;
	// An atlas's bodies, each a world of its own (terragen/worlds.h): its
	// columns, its cells (shared with the far view) and this generator's
	// reading of them. Built when a chunk first needs them.
	struct BodyLand
	{
		terragen::World world;
		terra::Columns columns;
		terra::AtlasLand land;
		terra::AtlasLand::Reader reader;
		BodyLand(const terragen::WorldCode &code, const terra::Blocks &blocks, s16 water_level,
				const planet::Atlas &atlas, int body) :
			world(code), columns(world, blocks, water_level, atlas, body), land(columns, atlas), reader(land)
		{
		}
	};
	std::vector<terragen::BodyWorld> m_bodyWorlds;
	// by body; built under m_building: spawnLevelAt asks from the main
	// thread while this generator's emerge thread makes chunks
	mutable std::vector<std::unique_ptr<BodyLand>> m_bodies;
	mutable std::mutex m_building;
	BodyLand &bodyLand(int body) const;
	// The land a storage column is of: its body's (seam's is seam in any)
	BodyLand &landAt(s16 x, s16 z) const;
	Biome *m_biomes[(int)terragen::Biome::Count];

	// per column of the current chunk (z outer, x inner)
	std::vector<terra::Column> m_column;
};
