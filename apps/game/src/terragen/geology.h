// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include "sphere.h"
#include <cstdint>
#include <vector>

/*
	Geology: what the ground is made of, and why. Plates turn on the sphere,
	each about its own pole; where they meet, mountains rise and volcanoes stand; every column
	gets a province (shield, orogen, arc, rift, basin, volcanic field, ocean
	floor) and from it a stack of rock; ores sit where real processes put
	them; caves follow the rock. Fixed point and pure like the rest of
	terragen. See planning/sprint-09-geology.md.
*/
namespace terragen {

struct Grid;

enum class Boundary : uint8_t { None, Convergent, Divergent, Transform };

// Which side of a convergent boundary a cell is on.
enum class Side : uint8_t { None, Overriding, Subducting, Collision };

// (Highland and Mare: a stagnant lid's, planning/sprint-95-maria.md)
enum class Province : uint8_t { Shield, Orogen, Arc, Rift, Basin, Volcanic, OceanFloor, Highland, Mare, Count };
extern const char *const PROVINCE_NAMES[(int)Province::Count];

enum class Rock : uint8_t {
	Granite, Gneiss, Slate, Basalt, Andesite, Tuff, Obsidian, Sandstone, Shale, Limestone, Anorthosite, Breccia, Ice,
	Count
};
extern const char *const ROCK_NAMES[(int)Rock::Count];

enum class Ore : uint8_t { None, Copper, Tin, Lead, Gold, Iron, Magnetite, Coal, Salt, Sulfur, Ilmenite, Kreep, Ice, Flint, Count };
extern const char *const ORE_NAMES[(int)Ore::Count];

struct Plate
{
	Dir centre;
	// how it moves: turning about this pole (a plate's motion on a sphere is
	// always a turn), its speed at 90° from the pole ~0.5..1
	Dir pole;
	fix spin;
	bool oceanic;
};

enum class VolcanoKind : uint8_t { Arc, Rift, Hotspot, Giant, Patera };

struct Volcano
{
	Dir at;                // the vent
	int32_t radius;        // blocks: where its slopes meet the land
	int32_t height;        // blocks above the land around it
	int32_t crater;        // crater radius, blocks
	int32_t depth;         // how far the crater sinks below the rim, blocks
	int32_t base;          // the land's height at its vent (blocks): its lava lake is level from here
	bool active;           // lava in the crater
	VolcanoKind kind;
};

// How a cell's crust came to be (the plates' history, history.cpp).
enum class Formed : uint8_t { Craton, Orogen, Arc, Rift, Ocean };

/*
	The plates' history (planning/sprint-89-plates.md): crust carried about
	the sphere by its plates for HISTORY_STEPS steps of geological time, worn
	by rivers as it goes. Where plates meet, ocean sinks under the other
	side and arcs rise; continents collide into ranges; where they part,
	new ocean floor forms. Per cell, what stands there today.
*/
constexpr int32_t HISTORY_STEPS = 32;
constexpr int32_t STEP_MYR = 15;   // million years a step stands for
struct History
{
	struct Cell
	{
		fix height = 0;      // the surface, before the sea is placed (higher stands out of it)
		fix build = 0;       // on ocean floor: what arcs have piled on it
		fix rate = 0;        // how fast it's being pushed up lately (per step, fading)
		// all the squeezing it has been through (planning/sprint-101-ranges.md),
		// on the side that rode over (thickened behind the front: plateaus) and
		// the side driven under (scraped off ahead of it: folds and thrusts)
		fix over = 0, under = 0;
		Dir home;            // where this crust formed: what it carries (an arc's volcanoes) is drawn from there
		int16_t age = 0;     // steps since it formed or was last built
		uint8_t plate = 0;
		bool continental = false;
		Formed formed = Formed::Ocean;
	};
	enum class Kind : uint8_t { Collision, Opening, Arc };
	struct Event
	{
		int32_t step;
		Kind kind;
		Dir at;
	};
	std::vector<Plate> plates;
	std::vector<Cell> cells;
	std::vector<Event> events; // the first of each kind between each pair of plates
	int64_t continental_start = 0, continental_end = 0; // continental crust's area (fixed cells)
	int64_t made = 0, lost = 0; // …built by arcs, folded away by collisions
};
History history(const WorldCode &code);

/*
	The plates as they stand today (from the history). Per cell: its plate,
	the kind of the nearest boundary and how far it is, which side of a
	convergent boundary it's on, how strongly the land is being pushed up
	there (`belt`, 0..1: what erosion works against), and what the boundary
	adds to the height (`relief`: rift valleys and trenches down).
*/
struct Tectonics
{
	std::vector<Plate> plates;
	std::vector<uint8_t> plate;
	std::vector<Boundary> boundary;
	std::vector<uint8_t> distance; // cells to the nearest boundary (capped)
	std::vector<Side> side;
	std::vector<fix> belt;
	std::vector<fix> relief;
	std::vector<Formed> formed;    // from the history
	std::vector<int16_t> age;      // steps since the crust formed or was last built
	// What built each range (planning/sprint-101-ranges.md), 0..1 unless said
	std::vector<fix> fold;         // how much of it is folds and thrusts
	std::vector<fix> plateau;      // how much is crust thickened into a high plain
	std::vector<fix> oldness;      // how long since it last rose
	std::vector<fix> front;        // cells in from the belt's outer edge: fold ridges are its contours
	// How long the crust has stood still (0..1): 1 on a shield or a platform
	// (old, nothing rising, far from an active edge), 0 in a young belt, a
	// rift or ocean floor. Old land is worn low and smooth (sprint 107).
	std::vector<fix> stable;
};

Tectonics tectonics(const WorldCode &code, const History &h);

// After the land and water exist: each cell's province, and the volcanoes.
std::vector<Province> provinces(const WorldCode &code, const Grid &g);
std::vector<Volcano> volcanoes(const WorldCode &code, const Grid &g);

/*
	A column's geology: its province and what lies under its soil. Rock is
	asked by depth below the bedrock's top; ores and caves by block.
*/
struct Strata
{
	Place at;           // the column's place
	Province province;
	int32_t sediment;   // blocks of sedimentary cover over the basement
	int32_t fold;       // how far beds wander up and down (blocks)
	int32_t tilt;       // this column's offset into the folded beds
	int32_t phase;      // where the bed cycle starts
	bool pluton;        // a granite body under it
	bool crumpled;      // beds folded on themselves (an orogen's, not fold ridges': those are laid out by the ridge)
	bool humid, arid;
	fix placer;         // how rich its river gravels are in gold and tin (0..1; Grid::placer)
	int32_t crater;     // blocks to the nearest active crater (large if none)
	bool lava_province; // deep caverns fill with lava, not water
	// a lid's (planning/sprint-95-maria.md)
	int32_t breccia;    // blocks of impact-shattered rock over the crust
	int32_t mare;       // blocks of mare basalt
	fix titanium;       // its lava's (0..1): ilmenite
	fix thin;           // the crust's thinness (0..1): KREEP under the thin side
	bool cold_trap;     // a crater floor near a pole the sun never reaches: ice
};

} // namespace terragen
