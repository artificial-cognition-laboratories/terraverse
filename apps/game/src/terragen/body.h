// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include <vector>

/*
	The body model: a world is a short history, not a list of settings. A
	few causes — the star, the orbit, the body's mass, where it formed, what
	it formed with, how it spins, whether life arose — are stepped through
	time from formation to the system's age; what the body is today emerges
	(its air, its temperature, its field, its seas, its kind), along with the
	story of how it got there. Units are relative to Earth (= 1) unless
	noted. Fixed point and pure, like the rest of terragen: the menu's
	preview and the game read the same numbers. See
	planning/systems-and-bodies.md.
*/
namespace terragen {

struct StarCauses
{
	int32_t type;  // index into STARS: its class
	fix mass;      // solar masses, in its class's range
	fix metallicity; // [Fe/H]: dex against the Sun's, −1..0.5
	fix age;       // the system's age, billions of years
	fix activity;  // 0..1: how hard its wind blows
	Companion companion{};
};

// What holds a planet: both stars of a close pair, the first of a wide one.
fix starsPull(const StarCauses &star);

// How a body turns: its own day, or held to what it orbits. Stored in a world
// code's body flags, in this order; must match ui/src/worldgen/config.ts SPINS.
enum class Spin : uint8_t { Free, Partner, Star, Res3_2, Res2_1, Res5_2, Count };

struct BodyCauses
{
	int32_t parent;   // −1 orbits the star; else the index of the planet it orbits (listed before it), round their balance point
	fix orbit;        // AU around the star; parent radii around a planet
	fix eccentricity; // 0..0.9
	fix mass;         // Earth masses
	fix formed;       // 0..1: where it formed, hot and close (0) to beyond the frost line (1)
	fix volatiles;    // 0..1: the water and gases it formed with
	fix day;          // rotation period, hours
	bool life;        // whether life arises where it can
	Spin spin = Spin::Free; // a lock sets the day; `day` is only a free body's
};

extern const char *const SPIN_NAMES[(int)Spin::Count];
fix resonance(Spin spin);

// The other half of a double planet, its index, or −1: a moon whose balance point
// with its planet lies outside the planet. Its mass and distance decide it, not a
// kind of body. Must match ui/src/system/model.ts partnerOf.
int32_t partnerOf(const std::vector<BodyCauses> &all, size_t body);

enum class Kind : uint8_t {
	Temperate, ColdDesert, Hothouse, Volcanic, IceShell, Hydrocarbon, Airless, Giant,
	Count
};
extern const char *const KIND_NAMES[(int)Kind::Count];

enum class Gas : uint8_t { CO2, N2, O2, CH4, H2, He, Count };
extern const char *const GAS_NAMES[(int)Gas::Count];

// What can happen in a body's history.
enum class Event : uint8_t {
	FieldOn, FieldFades, OceansForm, OceansBoil, OceansFreeze, SeasComeAndGo,
	PlatesBegin, PlatesStop, VolcanoesSilent, AirLost, Runaway, LifeAppears, OxygenRises,
	Count
};
extern const char *const EVENT_TEXT[(int)Event::Count];

struct Moment
{
	fix age;   // billions of years after formation
	Event event;
};

// One shell of a body, from the centre out.
struct Layer
{
	const char *name; // "core", "mantle", "crust", "ocean", "ice", "metallic hydrogen", "envelope"
	fix outer;        // its outer edge, as a share of the radius
	bool molten;      // liquid (a core that runs a field, an ocean, an envelope)
};

struct BodyCard
{
	Kind kind;
	bool giant;
	fix metal, rock, ice;   // composition shares
	fix radius, gravity;    // Earth = 1
	fix escape;             // km/s
	fix sunlight;           // starlight received, Earth = 1
	fix temperature;        // surface (cloud tops for a giant), kelvin
	fix equilibrium;        // without an atmosphere, kelvin
	fix pressure;           // bar at the surface; −1 for a giant (no surface)
	fix gases[(int)Gas::Count]; // shares of the air, summing to 1 (all 0 when there's none)
	fix field;              // magnetic field strength
	fix heat;               // interior heat flow, tides included
	fix tidal;              // the tidal part of it
	bool ocean, plates, volcanism, locked; // locked: to its star or what it orbits, by choice
	Spin spin;
	fix day; // hours a turn takes, as it spins (at most 32 000: all the same to what reads it)
	std::vector<Layer> layers; // centre outward
	std::vector<Moment> history;
};

// Every body's card, in the order given. Throws std::invalid_argument on a
// system that can't exist (a moon of a moon, a parent listed after its moon,
// out-of-range causes).
std::vector<BodyCard> bodies(const StarCauses &star, const std::vector<BodyCauses> &causes);

} // namespace terragen
