// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include "geology.h"
#include "grid.h"
#include "sphere.h"
#include <array>
#include <memory>
#include <optional>
#include <string>

namespace terragen {

// The biome classes terragen assigns. The game registers a Lua biome with
// each name (games/terrafirma/mods/tf_world); the order is what the preview
// sends, with the names alongside.
enum class Biome : uint8_t {
	Deep, Ocean, SeaIce, Beach, Desert, Savanna, Grassland, Forest, Jungle,
	Taiga, Tundra, Rock, Snow, Monsoon, Mediterranean, Regolith, Mare,
	Count
};

extern const char *const BIOME_NAMES[(int)Biome::Count];

struct Column
{
	fix elevation; // −1..~1.2, 0 = sea level; erosion included
	fix heat;      // ~−1 frozen .. 1 scorching
	fix wet;       // ~−1 arid .. 1 soaked: log₂ of `rain`, halved
	fix rain;      // the year's rain, relative to the median over land (1)
	fix season;    // when it falls: 1 all in summer .. −1 all in winter (0 even)
	Biome biome;
};

enum class Water : uint8_t { None, Sea, Lake, River, Lava };

// What can grow on a column (World::cover): how densely, and the ground's
// lie. Which plants grow is the game's choice.
struct Cover
{
	fix density;   // 0..1: bare to closed forest
	fix slope;     // rise over run
	fix fertility; // 0..1: what the ground supplies, less what rain washes out
	bool bank;     // beside a river or lake
	bool tree;     // a tree stands here (one spot per small cell, if the roll allows)
	uint32_t roll; // a per-column random number, for the game's choices
};

// Soil as a composition (soil.cpp): its mineral part on the texture
// triangle, and organic matter as a share of the whole.
struct Soil
{
	fix sand, silt, clay; // shares of the mineral part, summing to ~1
	fix organic;          // 0..1 of the soil
};
// The compositions a soil block can hold (its param2): sand and clay in
// tenths, silt the rest, at three levels of organic matter.
constexpr int32_t SOIL_ORGANIC = 3;
constexpr int32_t SOIL_POINTS = 66 * SOIL_ORGANIC;
uint8_t soilIndex(const Soil &s);
Soil soilPoint(uint8_t index); // throws std::invalid_argument past SOIL_POINTS
// How many steps a rock blends toward its neighbour (its param2).
constexpr int32_t ROCK_BLENDS = 16;

// What a cave holds where it's carved.
enum class Cave : uint8_t { None, Air, Water, Lava };

// A column as it is built: ground and water heights in blocks, relative to
// sea level (the map generator adds its water level).
struct Ground
{
	int32_t terrain; // the top solid block
	int32_t water;   // the water surface, if `kind` isn't None
	Water kind;
	Column column;
	int32_t mare = 0; // blocks of lava under the top (a flooded lid's maria), 0 off them
};

/*
	The world model: a pure function of the world code. A column is asked for
	by its place on the planet (a direction from the centre), a block by its
	point in the planet (blocks from the centre, solid()). It knows nothing of
	the map's blocks, regions or seams: the atlas hands each cell its place.
*/
class World
{
public:
	explicit World(const WorldCode &code);

	const WorldCode &code() const { return m_code; }
	const Grid &grid() const { return *m_grid; }

	// Sunlight reaching a place's latitude: on `date` (0..1), or the yearly mean.
	fix sunlight(const Place &d, std::optional<fix> date) const;
	Column column(const Place &d, fix sunlight) const;

	// The finished column at a place, water included. `sunlight` for its
	// latitude (see sunlight()).
	Ground ground(const Place &d, fix sunlight) const;

	// The air temperature in °C at a place on `date`, at `hour` of the day
	// (0..1): the season's climate there plus a daily swing that peaks
	// mid-afternoon and is wider where it's dry.
	fix temperature(const Place &d, fix date, fix hour) const;

	// (u, v) → the place on the planet, and back: how a flat picture of it
	// (the preview's map) counts, u east from longitude −π, v south from the
	// north pole.
	static Place place(fix u, fix v);
	static fix u(const Dir &d);
	static fix v(const Dir &d);
	static fix u(const Place &p) { return u(p.dir()); }
	static fix v(const Place &p) { return v(p.dir()); }
	// The planet the model is of, in blocks: its size round the equator / 2π.
	// Fixed point in 64 bits: past 32 767 blocks round a radius doesn't fit
	// a fix, and every distance in blocks came out wrong.
	int64_t radius() const { return m_radius; }
	static int64_t radiusOf(const WorldCode &code);
	// Blocks (fixed, 64 bits) spanned by `unit` radii: a chord or an offset
	// on the unit sphere, made the planet's size.
	int64_t blocks(fix unit) const { return blocksOf(unit, m_radius); }
	// the same for a length at a place's precision (PLACE_ONE a radius)
	int64_t blocksFine(int64_t unit) const { return mulShift(unit, m_radius, PLACE_FRAC); }
	static int64_t blocksOf(fix unit, int64_t radius) { return ((int64_t)unit * radius) >> FRAC; }
	// The angle (radians) `blocks` of ground spans.
	fix angle(int32_t blocks) const { return (fix)(((int64_t)blocks << (2 * FRAC)) / m_radius); }
	// The coarse grid's mean spacing between cell centres, in blocks (fixed, 64 bits).
	int64_t cellBlocks() const { return m_cell_blocks; }
	static int64_t cellBlocksOf(const WorldCode &code);
	// Blocks between two places, straight through (the same as along the
	// ground, this near).
	int32_t blocksBetween(const Place &a, const Place &b) const;
	// A block's point in the planet, in blocks from its centre: `y` blocks
	// over the model's radius (size / 2π) at a place. Where 3D noise is read.
	std::array<int32_t, 3> solid(const Place &d, int32_t y) const;

	// Surface height in blocks for an elevation, with fine detail at a place.
	int32_t surface(fix elevation, const Place &d) const;

	// Geology (geology.cpp). A column's strata, then by block, at height `y`
	// in the column (its noise read at solid(s.at, y)): the rock at a depth
	// below the bedrock's top, the ore there if any, and whether a cave is
	// carved there (`water_above`: under a sea, lake or river).
	Strata strata(const Place &d, const Ground &ground) const;
	Rock rock(const Strata &s, int32_t depth, int32_t y) const;
	Ore ore(const Strata &s, Rock rock, int32_t depth, int32_t y) const;
	Cave cave(const Strata &s, Rock rock, int32_t depth, int32_t y, bool water_above) const;
	// A grid field (Grid's per-cell vectors) at a place, between cell centres.
	fix between(const std::vector<fix> &field, const Dir &d) const;
	fix between(const std::vector<fix> &field, const Place &p) const { return between(field, p.dir()); }
	// The coarse cell a place falls in.
	int32_t cell(const Dir &d) const;
	int32_t cell(const Place &p) const { return cell(p.dir()); }

	// What the soil is made of here (soil.cpp), and how far a rock at height
	// `y` blends toward its neighbour (0 .. ROCK_BLENDS − 1).
	Soil soil(const Place &d, const Ground &ground, const Strata &strata) const;
	uint8_t blend(const Place &d, int32_t y) const;

	// What can grow here (vegetation.cpp).
	Cover cover(const Place &d, const Ground &ground) const;

private:
	// how much every nearby volcano raises a place, and a crater's lava surface
	// (absolute blocks) if any
	// `floor` is the crater's floor (absolute) inside a crater, built level from the vent
	void volcanic(const Place &at, int32_t &raise, int32_t &lava, int32_t &floor) const;

	WorldCode m_code;
	std::shared_ptr<const Grid> m_grid;
	int64_t m_radius, m_cell_blocks;
	int64_t m_river_scale; // fixed: how much wider rivers are than on the default planet
	int32_t m_meander;     // rivers' meanders: waves round the equator
	fix m_height;          // blocks per unit of elevation (heightScale)
	fix m_metre;           // blocks per metre (blocksPerMetre)
	std::vector<Crater> m_rayed; // the young craters still bright with rays (sprint 96)
	fix m_detail;          // the detail's amplitude at a cell's wavelength, blocks, where ruggedness is 1
	int m_detail_octaves;  // from a cell's wavelength down to 16 blocks
	Craters m_craters;     // impacts: the grid holds the wide ones, a column finds the rest
	// the ground at a place before volcanoes and water (blocks): the land
	// and its detail, the old craters, the maria's lava over them, the young
	// craters on top
	// the top solid block; `mare` gets how deep lava lies under it
	int32_t terrain(const Place &at, fix elevation, int32_t &mare) const;
	// What the lava did after it set (sprint 96): wrinkle ridges, rilles and
	// domes on a mare, in blocks over its level
	int32_t mareRelief(const Place &at) const;
	// Under a young crater's rays (bright ejecta thrown across the dark lava).
	bool rayed(const Place &at) const;
	// How hard the coast's rock is, 0 (soft or buried in sediment: beaches)
	// to 1 (a shield, a range's core: cliffs), between the cells (sprint 97)
	fix hardness(const Dir &d) const;
	// one age of craters at a place, exactly (blocks, fixed): the grid's wide
	// ones and the column's own
	fix craters(const Place &at, Age age) const;
	// a water surface at the grid's filled level, in blocks
	int32_t waterBlocks(fix filled) const { return std::max<int32_t>(0, floorInt(mul(filled, m_height))); }
};

/*
	Heights in metres (planning/sprint-93-scale.md). A unit of the grid's
	elevation is 5 km (the lapse rate's: 0.9 of heat, ~33 °C, is Earth's
	6.5 °C/km over 5 km). A body is blocks per metre √(R / R⊕) (R in blocks,
	R⊕ Earth's radius in metres; at most 1): an Earth-sized world is a block
	a metre, and a smaller one's mountains are steeper than real by 1/that
	(proportional heights would leave small worlds flat, real ones make
	needles).
*/
constexpr int32_t METRES_PER_UNIT = 5000;
constexpr int64_t EARTH_RADIUS_M = 6371000;
fix blocksPerMetre(const WorldCode &code);
// Blocks per unit of elevation, land and sea alike.
inline fix heightScale(const WorldCode &code) { return mul(blocksPerMetre(code), toFix(METRES_PER_UNIT)); }

} // namespace terragen
