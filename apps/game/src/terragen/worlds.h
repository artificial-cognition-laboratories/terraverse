// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "atlas.h"
#include "body.h"
#include "code.h"
#include "planet.h"
#include <string>
#include <vector>

/*
	A world's bodies that can be stood on (planning/sprint-40-bodies.md):
	the home (the system's playable body, as the world code makes it) first,
	then every other planet and moon with a surface and room for the
	smallest world, at the home's scale (its blocks per km). Each is a world
	of its own: its code is the home's at its size, with a seed of its own
	and the land of its kind (Surface: bare rock, sand or ice).
*/
namespace terragen {

// What a body's surface is like to stand on (planning/sprint-96-moon-in-play.md),
// from its card: what falls, breathes and burns there reads it.
struct Surroundings
{
	double gravity;     // m/s²
	double pressure;    // Pa at the surface (0: none)
	double temperature; // K, the surface's mean
	// the air by mole share, as the matter table names its gases (those it
	// lacks left out: helium); empty when there's no air
	std::vector<std::pair<std::string, double>> gases;
};

struct BodyWorld
{
	size_t system;  // its index in the world's System
	std::string id; // the system's (SystemBody::id)
	WorldCode code; // its own text and seed: nothing built from it is another body's
	Atlas::Params atlas;
	Surroundings surroundings;
};

// The body as flight and the body physics see it: its pull, its air's
// density at the ground, thinning a tenth of its radius up (as sphere()'s).
Planet planetOf(const BodyWorld &body);

// The smallest radius a body is stood on at (blocks): the smallest world size's.
constexpr double SMALLEST_BODY = 2607;

// A body's processes from its card, and whether life arises where it can.
Processes processesOf(const BodyCard &card, bool life);

// `homeAtlas`: the world's own (its planet_atlas). Throws for a system with
// no playable body, or a home without a surface.
std::vector<BodyWorld> bodyWorlds(const WorldCode &home, const Atlas::Params &homeAtlas);

} // namespace terragen
