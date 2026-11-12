// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "channels.h"
#include "code.h"
#include "fixed.h"
#include "geology.h"
#include "grid.h"
#include "sphere.h"
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace terragen {

// The biome classes terragen assigns. The game registers a Lua biome with
// each name (games/terrafirma/mods/tf_world); the order is what the preview
// sends, with the names alongside.
enum class Biome : uint8_t {
	Deep, Ocean, SeaIce, Beach, Desert, Savanna, Grassland, Forest, Jungle,
	Taiga, Tundra, Rock, Snow, Monsoon, Mediterranean, Regolith, Mare, Dust, Sulfur,
	// landforms: where water left the ground (a closed basin's dried floor, low
	// wet ground that silt built, a big river's plain)
	SaltFlat, Marsh, Floodplain,
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

// Soil as a composition (soil.cpp, sprint 111): what weathering made of
// the rock under it, moved and sorted by slopes and water. Its fine earth
// (under 2 mm) on the texture triangle; stones over that; organic matter.
struct Soil
{
	fix sand, silt, clay; // shares of the mineral fine earth, summing to ~1
	fix organic;          // 0..1 of the soil
	fix stones = 0;       // 0..1 of the mineral part: gravel and stones over 2 mm
	int32_t depth = 0;    // blocks of it over the rock
	Rock parent = Rock::Granite; // what its grains weathered from
	bool laid = false;    // water or ice laid it (a bed, a bank, a floodplain, till): one deposit all the way down
};
// Soil kinds (sprint 111): a soil block's name says what its grains came
// from and how stony it is; its param2 is its fine earth (soilIndex).
enum class SoilParent : uint8_t { Granitic, Basaltic, Limy, Shaly, Count };
extern const char *const SOIL_PARENT_NAMES[(int)SoilParent::Count];
SoilParent soilParent(Rock rock);
// how stony: stones under 15% of it, to 35%, to 60%, more (gravel)
constexpr int SOIL_STONY = 4;
extern const char *const SOIL_STONY_NAMES[SOIL_STONY]; // "" (none), "stony", "gravelly", "gravel"
int soilStony(fix stones);
fix soilStonesOf(int stony); // the share a class stands for
// What a column's soil holds `below` blocks under its top (0: the top):
// the topsoil is the column's; the subsoil under it keeps a quarter of its
// organic matter (peat all of it) and the clay rain washed down; its last
// block, over the rock, is the rock breaking up. What water laid is one
// deposit all the way down, its organic matter in the top alone.
Soil soilLayer(const Soil &column, int32_t below);
// Whether a river braids (sprint 112): steep for what it carries, a wide
// shallow bed of bars split among many channels, against one that winds in
// a single channel. Leopold and Wolman's line, slope > 0.012·Q^−0.44 (Q in
// m³/s, drawn ten times higher for the grid's runs: soil.cpp), Q and the slope in the planet's own metres as soil reads them;
// glacial meltwater, laden with outwash, braids at a third of the slope.
// `width` its single channel's (blocks, fixed), `glacial` 0..1.
bool braids(int64_t width, fix slope, fix rain, int64_t radius, fix metre, fix glacial);
// The minerals each rock weathers by (soil.cpp): its own copy of
// assets/matter (rocks.json minerals and grain, substances.json weathers),
// since terragen reads no files; TestTerragen holds it to the data.
constexpr int GRAIN_BANDS = 5; // stone, gravel, sand, silt, clay (matter::Band)
struct ParentMineral
{
	const char *id; // the substance
	fix share;      // of the rock's mass
	fix rate;       // how fast it weathers (0: it lasts)
	fix keep;       // kg it leaves a kg weathered (as clay); the rest dissolves
	int finest;     // the band its crystals come apart at, if finer than the rock's (clay minerals: clay), else −1
};
struct ParentRock
{
	std::array<ParentMineral, 4> minerals; // unused ones have no id
	int crystals; // the band its grains fall apart into
	fix crumbles; // how fast, against granite
};
const ParentRock &parentRock(Rock rock);
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
	// The nearest of the grid's rivers, as soil reads what it laid (sprint
	// 111): its width and how far its line is (blocks, fixed; no width:
	// none near), its water's level (blocks), how steeply it runs there
	// (rise over run), whether this is the bar inside its bend, which side
	// of the bend it's on (1 inside, −1 outside, the cut bank), and whether
	// the river braids (sprint 112)
	int64_t river_width = 0, river_dist = INT64_MAX;
	int32_t river_level = 0;
	fix river_slope = 0;
	bool river_bar = false;
	int8_t river_bend = 0;
	bool river_braided = false;
	int32_t dune = 0; // blocks of wind-blown sand on top (sprint 112)
	bool bare = false; // too steep for the biome's loose top: its filler shows
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
	// (the channels below the grid keep a pointer to it: never copied)
	World(const World &) = delete;
	World &operator=(const World &) = delete;

	const WorldCode &code() const { return m_code; }
	const Grid &grid() const { return *m_grid; }

	// Sunlight reaching a place's latitude: on `date` (0..1), or the yearly mean.
	fix sunlight(const Place &d, std::optional<fix> date) const;
	Column column(const Place &d, fix sunlight) const;

	// The finished column at a place, water included. `sunlight` for its
	// latitude (see sunlight()).
	Ground ground(const Place &d, fix sunlight) const;
	// While one stands, this thread's columns are for cells this many blocks
	// wide (far tiles): valleys and craters narrower than a cell are left out.
	// They'd only alias, and finding them costs 20× a column near its
	// neighbour. One block wide (a generated block, a level-0 tile) leaves out nothing.
	// A map (the Planetarium's) reads at its texels' width, and draws a river
	// by how much of a texel it covers: `widened` false keeps rivers their
	// own width, where a far tile widens them to show on every cell they cross.
	class Coarse
	{
	public:
		Coarse(const World &world, int64_t cell_blocks, bool widened = true);
		~Coarse();
		Coarse(const Coarse &) = delete;
		Coarse &operator=(const Coarse &) = delete;
	};

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
	// Whether a grid cell's run to its receiver is drawn as water: a river,
	// or a creek with a share of a river's flow (sprint 107)
	bool drawn(int32_t c) const;
	// The place a drawn run's line passes at `t` (0 at the cell, 1 at its
	// receiver), as ground() draws it: in the rivers' warp, bowed and swung.
	// Where a stream below the grid ends on it, so it meets the water.
	Place onRun(int32_t c, fix t) const;
	// The place whose warped reading (the rivers') lands about here: where on
	// the ground a place on a run's line is
	Place unwarped(const Place &warped) const;
	// The grid's water surface along a cell's run to its receiver at `t` (0 at
	// the cell), blocks over the sea, fixed: what the channels below it join
	int64_t linkHeight(int32_t c, fix t) const;
	// The grid's ground at a place, between cell centres (blocks over the sea,
	// fixed): what the land there stands at on average
	int64_t gridHeight(const Place &p) const;
	// How much the ice ages' glaciers worked a cell's ground at this height
	// (blocks, fixed): 0 never .. 1 (cold enough now, so colder then)
	fix glacial(int32_t c, int64_t height) const;
	// Whether a cell's run braids (sprint 112; see braids())
	bool braided(int32_t c) const { return !m_runs.empty() && m_runs[(size_t)c].braided; }
	// A drawn run's water level (blocks) at PROFILE + 1 points along it, from
	// the cell to its receiver (none: nullptr). Worked out the first time it's asked for.
	static constexpr int PROFILE = 8;
	const int32_t *profile(int32_t c) const;
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
	// Karst (sprint 112): how much a place is a limestone platform's (0..1)
	// in a climate this wet: its beds lime, its small streams sunk
	fix karst(const Place &at, fix wet) const;
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
	// The soil each grid cell's land mostly has (sprint 111), for maps: from
	// the grid alone (its height, climate, rock and rivers; no streams below
	// it, no detail), as fields `between` reads. Made the first time it's
	// asked for, in parallel; the sea's cells hold nothing.
	struct RegionalSoil
	{
		std::vector<fix> sand, silt, clay, organic, stones;
	};
	const RegionalSoil &regionalSoil() const;
	uint8_t blend(const Place &d, int32_t y) const;

	// What can grow here (vegetation.cpp).
	Cover cover(const Place &d, const Ground &ground) const;
	// Whether a tree may stand here: its spot in its small cell (cover's
	// `tree` is this and the density). Cheap: no column.
	bool treeSpot(const Place &d) const;

public:
	// A river's width (blocks, fixed, at least one) for what drains to it
	// (blocks² of average rain): its streams' and the grid's runs' alike
	int64_t riverWidth(int64_t area) const;
	// blocks a metre of height is (blocksPerMetre)
	fix metre() const { return m_metre; }
	// the drainage below the grid, if this world has one (for tools that draw it)
	const Channels *channels() const { return m_channels ? &*m_channels : nullptr; }
	// where a place lies on its fold ridge (0 valley .. 1 crest; 0 outside a belt)
	fix ridgesAt(const Place &at) const { return folds(at).crest; }

private:
	uint32_t spot(const Place &at, const std::array<int32_t, 3> &solid) const;
	// how much every nearby volcano raises a place, and a crater's lava surface
	// (absolute blocks) if any
	// `floor` is the crater's floor (absolute) inside a crater, built level from the vent
	// a volcano's relief at a place: its cone (raise), a crater's floor and its
	// lava lake's level, a crater lake's level (pool, where it's wet
	// enough to fill), and how thick a young lava plain lies (sprint 112);
	// INT32_MIN where there's none
	struct Volcanic
	{
		int32_t raise = 0, lava = INT32_MIN, floor = INT32_MIN, pool = INT32_MIN, plain = 0;
		bool fresh = false; // the plain is the youngest's: bare lava
	};
	Volcanic volcanic(const Place &at, fix wet) const;

	WorldCode m_code;
	std::shared_ptr<const Grid> m_grid;
	int64_t m_radius, m_cell_blocks;
	int32_t m_meander;     // rivers' meanders: waves round the equator
	fix m_height;          // blocks per unit of elevation (heightScale)
	fix m_metre;           // blocks per metre (blocksPerMetre)
	std::vector<Crater> m_rayed; // the young craters still bright with rays (sprint 96)
	// Deltas: where a big river meets the sea, the fan its silt builds out
	// into it, split among distributaries (sprint 106)
	struct Delta
	{
		Place apex;            // the river's last cell on land
		fix sea[3], side[3];   // unit: seaward, and across (to its left)
		int64_t radius;        // the fan's reach, blocks, fixed
		int64_t width;         // the river's width there, blocks, fixed
		uint32_t hash;
		int arms;              // its distributaries
	};
	std::vector<int32_t> m_upstream; // each drawn cell's main stem in: the one draining most into it (−1 none)
	std::vector<uint8_t> m_sea_cells; // each cell's steps to the sea (0 at sea; capped at 255)
	// A drawn run's shape, the same for every column that reads it
	struct RunShape
	{
		int64_t width = 0;  // blocks, fixed
		int64_t run = 0;    // its length, blocks, fixed (0: not drawn)
		int64_t loops = 1;  // meander loops along it
		bool braided = false; // a braided river's: wide, straight, split round bars
		int64_t swing = 0, reach_side = 0; // how far they swing (signed), and at most (blocks, fixed)
		fix m0 = 0, m1 = 0; // its slopes across the run at each end (Hermite's)
		// the line's offset across the run at t (blocks, fixed), the meander's
		// part of it, and its rate (per unit t)
		void line(fix t, int64_t &side, int64_t &off, int64_t &rate) const;
	};
	std::vector<RunShape> m_runs;
	void shapeRuns();
	// Each drawn run's water level (blocks) at PROFILE + 1 points from its
	// cell to its receiver: falling downstream; the land's carved to it (the
	// water's cut into the land, never banked up over it). Its ends are the
	// network's (profileRuns, for every run: each is held by those up and
	// down it); the points between are its own, from its ends and the rock
	// it runs over (profileRun), filled in the first time it's asked for.
	// Never the land read along it: a full read each, somewhere new on a
	// large planet, its channels built from nothing (sprint 110: 20 s to make
	// a world, then 9 s for its coarse planet)
	mutable std::vector<int32_t> m_profile;
	std::unique_ptr<std::once_flag[]> m_profiled; // a run's points between filled in
	void profileRuns();
	void profileRun(int32_t c) const;

	// the soil from what the column says and, if any, the channel nearest (soil.cpp)
	Soil soilWith(const Place &at, const Ground &ground, const Strata &strata, const Channels::Read *read) const;
	mutable RegionalSoil m_regional_soil;
	mutable std::once_flag m_regional_soiled;

	std::vector<Delta> m_deltas;
	std::vector<int16_t> m_delta_at; // each cell's delta, if one reaches it (−1 none)
	void deltas();
	// a delta's fan in a column, if one's here (before the rivers, which run
	// on over it), or its distributaries and marsh (after)
	void delta(const Place &at, Ground &out, bool arms) const;
	fix m_detail;          // the detail's amplitude at a cell's wavelength, blocks, where ruggedness is 1
	int m_detail_octaves;  // from a cell's wavelength down to 16 blocks
	std::optional<Channels> m_channels; // the drainage below the grid, where water ever ran
	bool m_land = false; // the land from the drainage (sprint 108; TERRAGEN_LAND=noise: the old)
	Craters m_craters;     // impacts: the grid holds the wide ones, a column finds the rest
	// the ground at a place before volcanoes and water (blocks): the land
	// and its detail, the old craters, the maria's lava over them, the young
	// craters on top
	// the top solid block; `mare` gets how deep lava lies under it
	int32_t terrain(const Place &at, fix elevation, int32_t &mare) const;
	// What the lava did after it set (sprint 96): wrinkle ridges, rilles and
	// domes on a mare, in blocks over its level
	int32_t mareRelief(const Place &at) const;
	// what tides do to an ice crust: cracks and chaos (blocks)
	int32_t flexed(const Place &at) const;
	// what a canyon from a lid's rise cuts here (blocks, none or negative)
	int32_t canyoned(const Place &at) const;
	// a lifeless body's biome: its bare ground, lava, caps
	Biome bareBiome(const Dir &d, bool flooded) const;
	// Under a young crater's rays (bright ejecta thrown across the dark lava).
	bool rayed(const Place &at) const;
	// How hard the coast's rock is, 0 (soft or buried in sediment: beaches)
	// to 1 (a shield, a range's core: cliffs), between the cells (sprint 97)
	fix hardness(const Dir &d) const;
	// Whether the sea is within a cell or so of a place (its cells or theirs)
	bool bySea(const Dir &d) const;
	// one age of craters at a place, exactly (blocks, fixed): the grid's wide
	// ones and the column's own
	fix craters(const Place &at, Age age) const;
	// the channels read at a place: its warp is the rivers', so a river lies
	// in its valley (the last place's is kept: ground asks twice)
	struct ChannelsAt
	{
		uint64_t from = 0; // the Channels' owner(): an address is reused once one's freed
		int64_t least = 0;
		Place at{};
		Channels::Read read;
		std::array<fix, 3> warp{}; // the rivers' warp there, in cells
	};
	const ChannelsAt &channelsAt(const Place &at) const;
	// Gullies: the drainage finer than the grid, as erosion leaves it on a
	// slope (blocks, fixed, about 0); `strength` 0..1 how deeply it's cut
	fix gullies(const Place &at, fix strength) const;
	// wind-blown sand: raises `out` by its dunes and makes them sand
	void dunes(const Place &at, fix surf, int32_t strand, Ground &out) const;
	// small relief on steep hillsides, blocks (sprint 112)
	int32_t crags(const Place &at, fix slope) const;
	// its sinkholes' depth at a place, blocks (karst())
	int32_t dolines(const Place &at, fix karst) const;
	// fold ridges at a place (planning/sprint-101-ranges.md)
	struct Folds
	{
		fix crest = 0;  // across a ridge: 0 in the weak beds' valley, 1 on the hard bed's crest
		fix height = 0; // of a ridge here, blocks; none outside a fold belt
	};
	Folds folds(const Place &at) const;
	int32_t m_fold_waves = 0; // ridges a cell; none where a cell can't hold them
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

// The world a code makes, shared: the server's generators, the far view and
// the menu's planet all read one, built once (a big one takes seconds). A few
// recent kept; asked for while it's being built, the asker waits for that build.
std::shared_ptr<const World> worldFor(const WorldCode &code);
// Blocks per unit of elevation, land and sea alike.
inline fix heightScale(const WorldCode &code) { return mul(blocksPerMetre(code), toFix(METRES_PER_UNIT)); }

} // namespace terragen
