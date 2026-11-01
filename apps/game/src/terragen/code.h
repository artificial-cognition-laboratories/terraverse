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
	A decoded world code: TF7.<seed>.<params>, as ui/src/worldgen/config.ts
	writes it. (TF1: sprint 1, without water or exact land; TF2: sprint 2, before
	plates and geology; TF3: sprint 9, before the climate; TF4: sprint 13, before the ocean currents; TF5: sprint 14, on the flat map before the sphere; TF6: sprint 86, places to 1/65 536 of the radius.)
	Parameters are kept as the fixed values their quantized bytes stand for,
	so the same code always gives exactly the same numbers.
*/
// What a body's land is (sprint 41): the home's lives (seas, rivers, plants,
// the climate's biomes); another body's is bare rock, sand or ice by its kind,
// with no water and nothing growing (terragen::bodyWorlds picks it).
enum class Surface : uint8_t { Living, Rock, Sand, Ice };

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
	Surface surface = Surface::Living; // not in the text: a body's, from its kind

	static WorldCode decode(const std::string &code); // throws CodeError
};

} // namespace terragen
