// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/tiling.h"
#include "planet/atlas.h"
#include <optional>

#include "mapgen/atlas_land.h"
#include "mapgen/terra_column.h"
#include "mapgen/terra_species.h"
#include "terragen/world.h"
#include <memory>
#include <string>
#include <vector>

/*
	The far view's world: what lies beyond the loaded map blocks, as blocks.
	Not a picture of the land but the land itself at a lower resolution: each
	cell is a column (or 2ᵏ×2ᵏ columns) made of the blocks the generator
	would place there, decided by the generator's own rule
	(terra::Columns::node). So the far view and the real chunks can't
	disagree, and the seam between them is only a change of resolution.
	No client dependencies: built on worker threads, and in the unit tests.
*/
namespace far {


// The game's part of a planet's land: every block the generator places, by
// role, and the tree species. The same for every world of the game (only
// their content ids are a server's).
struct Content
{
	terra::Blocks blocks;
	std::vector<terra::Species> species;
};

// What the server sends a client of a terra world (TOCLIENT_HORIZON): all
// the far view needs to build it. The world's part (its code and sea) and
// the game's.
struct Horizon
{
	std::string code;
	s16 water_level;
	Content content;
};


// A vertical run of one block, low to high inclusive.
struct Run
{
	s32 low = 0, high = -1;
	MapNode node;
	bool empty() const { return high < low; }
};

// What a column (or cell of columns) shows from afar.
struct Cell
{
	bool space;      // no cell of the planet here: nothing to draw
	s32 ground;      // the top solid block
	MapNode top;     // that block
	// a cliff shows `top` down to top_end, `filler` down to filler_end, then `rock`
	s32 top_end, filler_end;
	MapNode filler, rock;
	s32 water;       // the liquid's surface, if above the ground
	MapNode liquid;
	Run trunk, canopy; // the trees standing here
};

// Cells per tile side. A tile at level k has cells 2ᵏ blocks wide.
constexpr s32 TILE = 32;

struct TileKey
{
	s32 level, x, z; // tile coordinates at its level
	s32 face = 0;    // on an atlas (far/tiling.h), the region: its grid in terragen::Atlas::grids
	s32 cell() const { return 1 << level; }
	s32 span() const { return TILE << level; }
	bool operator==(const TileKey &o) const { return level == o.level && x == o.x && z == o.z && face == o.face; }
};

struct Tile
{
	TileKey key;
	// (TILE + 2)² cells: one of border on every side, for the faces between tiles
	std::vector<Cell> cells;
	const Cell &at(s32 i, s32 j) const { return cells[(size_t)(j + 1) * (TILE + 2) + (i + 1)]; }
	Cell &at(s32 i, s32 j) { return cells[(size_t)(j + 1) * (TILE + 2) + (i + 1)]; }
};

/*
	Builds tiles of one body of a planet. Const and thread-safe: workers
	share one. Each cell is the column of the atlas cell it stands for.
*/
class Generator
{
public:
	// One body of an atlas world: its tiles are its regions' (far/tiling.h),
	// each cell the column the generator builds in the atlas cell it stands for
	Generator(const Horizon &horizon, const planet::Atlas &atlas, int body);
	~Generator();

	const Horizon &horizon() const { return m_horizon; }
	const terragen::World &world() const { return *m_world; }

	Tile build(const TileKey &key) const;
	// A rough tile, built in about a quarter of the time: the land read at
	// every other cell (each odd one a copy of the one before it), as land
	// twice as coarse. It stands in till the tile itself is built.
	Tile sketch(const TileKey &key) const;

	// The top solid block of an atlas cell's column, or the liquid's surface if that's higher.
	s32 surfaceAt(const planet::Atlas::Cell &cell) const;
	// The top solid block of an atlas cell's column, under any liquid.
	s32 groundAt(const planet::Atlas::Cell &cell) const;

	// The storage column at (x, z) (throws for seam).
	Cell column(s32 x, s32 z) const;
	// What the column at a place on the body shows from afar (as a tile's cell
	// there would; planetarium/globe.h)
	Cell seenAt(const terragen::Place &place) const;

	// The planet's cells as the generator fills them.
	const terra::AtlasLand &land() const { return *m_land; }

private:
	Tile made(const TileKey &key, s32 stride) const; // every stride-th cell read, the rest copied
	Horizon m_horizon;
	std::shared_ptr<const terragen::World> m_world; // (terragen::worldFor: the game's own, in one process)
	planet::Atlas m_atlas;
	int m_body; // the atlas's body drawn
	std::unique_ptr<Tiling> m_tiling;
	std::unique_ptr<terra::Columns> m_columns;
	std::unique_ptr<terra::AtlasLand> m_land;
};

} // namespace far
