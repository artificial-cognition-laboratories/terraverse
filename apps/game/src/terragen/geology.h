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

enum class Province : uint8_t { Shield, Orogen, Arc, Rift, Basin, Volcanic, OceanFloor, Count };
extern const char *const PROVINCE_NAMES[(int)Province::Count];

enum class Rock : uint8_t {
	Granite, Gneiss, Slate, Basalt, Andesite, Tuff, Obsidian, Sandstone, Shale, Limestone,
	Count
};
extern const char *const ROCK_NAMES[(int)Rock::Count];

enum class Ore : uint8_t { None, Copper, Tin, Lead, Gold, Iron, Magnetite, Coal, Salt, Sulfur, Count };
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

enum class VolcanoKind : uint8_t { Arc, Rift, Hotspot };

struct Volcano
{
	Dir at;                // the vent
	int32_t radius;        // blocks: where its slopes meet the land
	int32_t height;        // blocks above the land around it
	int32_t crater;        // crater radius, blocks
	int32_t base;          // the land's height at its vent (blocks): its lava lake is level from here
	bool active;           // lava in the crater
	VolcanoKind kind;
};

/*
	The plates, worked out before the land's height (mountains follow them).
	Per cell: its plate, the kind of the nearest boundary and how far it is,
	which side of a convergent boundary it's on, how strongly mountains rise
	there (`belt`, 0..1), and what the boundary adds to the height (`relief`:
	rift valleys and trenches down, mid-ocean ridges up).
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
};

// `mass` is the landmass field per cell, `sea_offset` where the sea is.
Tectonics tectonics(const WorldCode &code, const std::vector<fix> &mass, fix sea_offset);

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
	bool humid, arid;
	bool placer;        // its river gravels carry gold and tin
	int32_t crater;     // blocks to the nearest active crater (large if none)
	bool lava_province; // deep caverns fill with lava, not water
};

} // namespace terragen
