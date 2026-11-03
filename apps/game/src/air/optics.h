// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/body.h"
#include <array>

/*
	A body's air as light meets it (planning/arc-atmosphere.md): what scatters
	and absorbs at each height, from its card. The gases are physics; the
	particles (dust, haze, cloud) are estimates from what the card says the
	world is like. Only ever drawn, so floats, and no part of a world code.
*/
namespace air {

// Red, green, blue: 680, 550 and 440 nm.
using Rgb = std::array<double, 3>;

struct Medium
{
	double radius = 0, top = 0;  // m: the ground's and the air's edge, from the centre
	Rgb rayleigh{};              // per m at the ground
	double rayleigh_height = 1;  // m: thins e-fold per this
	Rgb gas_absorb{};            // per m at the ground, thinning as the air does (methane)
	Rgb ozone{};                 // per m at the layer's peak
	double ozone_at = 0, ozone_width = 1; // m: the peak's height, the layer's half width
	Rgb particle_scatter{}, particle_absorb{}; // per m at the ground
	double particle_height = 1;  // m
	double particle_g = 0;       // how far forward particles scatter, -1..1
	Rgb star{ 1, 1, 1 };         // the star's colour (green = 1)
	double sunlight = 1;         // the star's light here, Earth = 1

	bool none() const { return top <= radius; }
	// The same air on a planet of another size, its height another share of
	// that: the same amount overhead, so straight up looks the same; it's the
	// long ways through (sunsets, the horizon) that change.
	Medium resized(double radius_m, double rayleigh_height_m) const;
};

// A star's colour by class (terragen::STARS), against the Sun's as the eye takes white.
Rgb starColour(int type);

// A body's air, at its real size. No air: none().
Medium mediumOf(const terragen::BodyCard &card, double radius_km, int star_type);

} // namespace air
