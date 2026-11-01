// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/*
	The planet as the things that move on it see it: its size, its pull, its
	spin and its air. One card, read by the climate, the weather layer and
	flight (src/orbit), so they agree on the body they share.

	The world's blocks are a map of it (World::u, v): the map wraps the whole
	planet in `WorldCode::size` blocks, so a block across is far more than a
	metre of planet. Heights and speeds near the ground are the game's own
	(a block is a metre); above the flight ceiling, flight runs on the card.
*/
namespace terragen {

constexpr double EARTH_RADIUS = 6.371e6; // m
// Height above the sea where space begins, m: above it there's no weather.
constexpr double SPACE_BOUNDARY = 30000;

struct Planet
{
	double radius;       // m
	double gravity;      // m/s² at the surface
	double day;          // s per turn
	double sunlight;     // W/m² at the top of the air, facing the star
	double density;      // kg/m³: the air at the datum (0: no air)
	double scale_height; // m: the air thins e-fold per this much height

	double mu() const { return gravity * radius * radius; }       // GM, m³/s²
	double spin() const { return 2 * 3.14159265358979323846 / day; } // rad/s
};

// The world's planet: Earth's pull and air at a tenth of its size, turning
// ten times as fast, so its equator moves as Earth's does and its air flows
// as Earth's (low orbit is ~2.3 km/s instead of 7.8).
Planet home();

// The planet a world is (src/planet/atlas.h), at its radius:
// Earth's pull and air at the ground; turning so its equator moves at the
// share of orbital speed Earth's does (keep Earth's own 465 m/s and a planet
// of a few km flings everything off); its air thinning over a tenth of the
// radius, so the mountains breathe and a low orbit is clear of it.
Planet sphere(double radius);

// The same planet at another size, with its surface gravity and the speed of
// its equator kept: its air's flow is the same (the same winds give the same
// Rossby number), so a model tuned at one size runs on another.
Planet resized(const Planet &p, double radius);

} // namespace terragen
