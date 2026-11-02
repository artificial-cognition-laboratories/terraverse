// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include <stdexcept>
#include <string>

namespace terragen {

// A world code that can't be read. The message is meant for the player.
struct CodeError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

struct Star
{
	const char *id;
	fix warmth; // shifts the whole planet's climate (Earth's terrain model)
	fix mass;   // solar masses; luminosity follows as mass⁴ (the body model)
};

// Coolest to hottest; the index is what a world code stores. Must match
// ui/src/worldgen/config.ts STARS.
extern const Star STARS[5];

/*
	A decoded world code: TF9.<seed>.<params>, as ui/src/worldgen/config.ts
	writes it. (TF1: sprint 1, without water or exact land; TF2: sprint 2, before
	plates and geology; TF3: sprint 9, before the climate; TF4: sprint 13, before the ocean currents; TF5: sprint 14, on the flat map before the sphere; TF6: sprint 86, places to 1/65 536 of the radius; TF7: sprint 87, before the plates had a history; TF8: sprint 89, before the energy-balance climate.)
	Parameters are kept as the fixed values their quantized bytes stand for,
	so the same code always gives exactly the same numbers.
*/
// What ground is where nothing lives: the home's soil, or another body's
// bare rock, sand or ice by its kind, or regolith (rock ground to dust by
// impacts, where no air stops them).
enum class Surface : uint8_t { Soil, Rock, Sand, Ice, Regolith };

// How a body's crust moves: plates that drift, collide and part, or one shell
// that stays put (a stagnant lid: the Moon, Mars, Venus).
enum class Regime : uint8_t { Plates, Lid };

// Which processes shape a body's land (planning/sprint-92-body-settings.md):
// the body card's, never chosen by the body's name. Not in the code's text:
// a body's come from its card (terragen::bodyWorlds); the home is Earth-like.
struct Processes
{
	Regime regime = Regime::Plates;
	bool water = true; // liquid water at the surface: seas, rain, rivers, lakes, erosion
	bool life = true;  // soil, plants and the climate's biomes
	Surface bare = Surface::Soil;
	// Impacts (planning/sprint-94-impacts.md): the share of the craters the
	// surface keeps (what water, plates and volcanism haven't worn or
	// recycled: Earth's), the smallest that get through the air (metres), and
	// the pull that shapes them (Earth = 1)
	fix craters = ratio(1, 1200); // (as worlds.cpp makes it: water ÷400, plates ÷3)
	int32_t smallest_crater = 100;
	fix gravity = ONE;
	// Flooding (planning/sprint-95-maria.md): the share of a lid's surface
	// lava once covered (maria), and the share of the craters made after it
	// (with none, every crater is as old as the ground: none lie on lava)
	fix flooding = 0;
	fix young = 0;
};

struct WorldCode
{
	std::string text;      // the code itself, exactly as given
	std::string seed;      // as typed (UTF-8)
	uint32_t seed_hash;    // FNV-1a of the seed's bytes: terragen's randomness
	int32_t size;          // blocks per side
	const Star *star;
	int32_t continents;    // 1..8
	fix land, coast, mountains, roughness; // 0..1
	fix temperature, humidity;             // −1..1
	fix rivers, caves, ores;               // 0..1
	fix tilt;              // degrees, 0..45 (whole degrees)
	int32_t year;          // days, 8..128
	Processes processes; // not in the text (see Processes)

	static WorldCode decode(const std::string &code); // throws CodeError
};

} // namespace terragen
