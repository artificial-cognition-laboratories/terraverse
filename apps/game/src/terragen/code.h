// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include <array>
#include <stdexcept>
#include <cstdint>
#include <string>
#include <vector>

namespace terragen {

// A world code that can't be read. The message is meant for the player.
struct CodeError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

struct Star
{
	const char *id;
	fix least, mass, most; // solar masses: the class's range, and its usual one
};

// Spectral classes, coolest to hottest; the index is what a world code stores.
// Must match ui/src/system/stars.ts STAR_CLASSES.
constexpr int STAR_COUNT = 7;
extern const Star STARS[STAR_COUNT];

// How bright a star of this mass is (the Sun = 1). Saturates at 32 000 Suns,
// which only the heaviest O stars reach: nothing is left to tell apart by then.
fix luminosity(fix mass);
// How long it shines, billions of years: heavy stars burn out fast.
fix lifetime(fix mass);

/*
	A decoded world code: TF16.<seed>.<params>[.<bodies>], as ui/src/worldgen/config.ts
	writes it. (TF1: sprint 1, without water or exact land; TF2: sprint 2, before
	plates and geology; TF3: sprint 9, before the climate; TF4: sprint 13, before the ocean currents; TF5: sprint 14, on the flat map before the sphere; TF6: sprint 86, places to 1/65 536 of the radius; TF7: sprint 87, before the plates had a history; TF8: sprint 89, before the energy-balance climate; TF9: before stars had classes; TF10: before the code carried the system; TF11: before the home followed its card; TF12: before each body had its own land; TF13: before the winds followed a body's day.)
	Parameters are kept as the fixed values their quantized bytes stand for,
	so the same code always gives exactly the same numbers.
*/
// What ground is where nothing lives: the home's soil, or another body's
// bare rock, sand or ice by its kind, or regolith (rock ground to dust by
// impacts, where no air stops them).
// Rust: dust reddened where water once rusted the iron. Sulfur: what eruptions
// snow over a body's plains.
enum class Surface : uint8_t { Soil, Rock, Sand, Ice, Regolith, Rust, Sulfur };

// How a body's crust moves: plates that drift, collide and part, or one shell
// that stays put (a stagnant lid: the Moon, Mars, Venus).
enum class Regime : uint8_t { Plates, Lid };

// Which processes shape a body's land (planning/sprint-92-body-settings.md):
// the body card's, never chosen by the body's name. Not in the code's text:
// every body's come from its card (processesOf), the home's as the code is decoded.
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
	// An ice crust (planning/sprint-98-ice.md): ice flows, so its relief sags
	// (the share of a rock crust's it keeps; ONE is rock, and rock underfoot),
	// and how hard tides flex it, which cracks and breaks the shell
	fix stiff = ONE;
	fix flexing = 0;
	// A rock lid's own (planning/sprint-99-lids.md): how long plumes rose from a
	// thick mantle (Gyr; none: no rise, no shields), how long it held seas that
	// are gone (Gyr), and how fast eruptions bury it (interior heat, Earth = 1)
	fix plumes = 0;
	fix ancient_water = 0;
	fix eruptions = 0;
	// Kelvin its surface is above Earth's on its card: star, orbit and air
	// together, what the climate is pushed by (planning/arc-systems.md)
	fix warming = 0;
	// How it turns (planning/arc-systems.md): hours a turn takes, what sets its
	// winds; one face to the star, its climate centred on the point under it
	// (Dir +x, longitude 0); and how well its air carries heat round to the night
	fix day = toFix(24);
	bool facing_star = false;
	fix carrying = ONE;
};

// A second star. Close: the planets go round both. Wide: round the first, the
// other a brilliant star. Stored in a code's params after the style, in this order;
// must match ui/src/system/stars.ts.
enum class Pairing : uint8_t { None, Close, Wide, Count };
struct Companion
{
	Pairing kind = Pairing::None;
	int32_t type = 0;   // index into STARS
	fix mass = 0;       // solar masses, in its class's range
	fix separation = 0; // AU between the stars
};

// How the seed lays out a system with no bodies of its own in the code. Home
// is the one fixed system, what a code with no settings means; must match
// ui/src/worldgen/config.ts SYSTEM_STYLES.
enum class SystemStyle : uint8_t { Home, SunLike, Random, Binary, Double, Harsh, Count };

// A body's land settings as a code stores them (continents .. ores), in order.
constexpr size_t LAND_FIELDS = 10;
using LandBytes = std::array<uint8_t, LAND_FIELDS>;
// What a body has when nobody has set its land.
LandBytes defaultLand();
// FNV-1a: the same on every machine, unlike std::hash.
uint32_t hashOf(const std::string &bytes);

struct WorldCode
{
	std::string text;      // the code itself, exactly as given
	std::string grid_key;  // what the world model's grid is kept by: what decides the land, not the whole system
	std::string seed;      // as typed (UTF-8)
	uint32_t seed_hash;    // FNV-1a of the seed's bytes: terragen's randomness
	int32_t size;          // blocks per side
	const Star *star;
	fix star_mass;         // solar masses, in its class's range
	fix metallicity;       // [Fe/H]: dex against the Sun's, −1..0.5
	fix age;               // the system's, billions of years; no more than the star's life
	fix activity;          // 0..1: how hard its wind blows
	SystemStyle style = SystemStyle::Home;
	Companion companion; // as the code says; none and style Binary: the seed's (SystemSpec::forWorld)
	std::vector<uint8_t> bodies; // the system as edited (SystemSpec::read); empty: as the style lays it out
	int32_t continents;    // 1..8
	fix land, coast, mountains, roughness; // 0..1
	fix temperature, humidity;             // −1..1
	fix rivers, caves, ores;               // 0..1
	fix tilt;              // degrees: the code's own for a laid-out system, an edited system's home's otherwise
	int32_t year;          // days, 8..128: a calendar's compression of the real orbit
	Processes processes; // not in the text (see Processes)

	static WorldCode decode(const std::string &code); // throws CodeError
	void setLand(const LandBytes &bytes);
	// Sets grid_key from what decides the land: the seed, settings and card,
	// so an edit elsewhere in the system doesn't rebuild this body.
	void keepGrid();
};

} // namespace terragen
