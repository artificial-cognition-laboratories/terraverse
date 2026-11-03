// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "air/optics.h"
#include <vector>

/*
	The sums light through air takes, kept in small tables (Hillaire 2020, from
	the paper): what sunlight survives from a height at an angle, and what all
	the scattering after the first adds. Built once per body on the CPU; both
	are small enough to hand to a shader as they are.

	sky() is the reference: the sum per view, which the game's sky image will
	run in a shader (sprint 111) and the PNG tool runs here.
*/
namespace air {

struct Vec
{
	double x, y, z;
};

class Tables
{
public:
	static constexpr int TRANSMIT_W = 256, TRANSMIT_H = 64, SCATTER_SIZE = 32;

	// Throws for a medium with no air: nothing to tabulate.
	explicit Tables(const Medium &medium);

	const Medium &medium() const { return m_medium; }
	// What share of starlight reaches a height (m above the ground), with the
	// star at this cosine from straight up. None past the planet's shadow.
	Rgb transmittance(double height, double cos_sun) const;
	// What later scattering adds per unit scattering coefficient.
	Rgb multiple(double height, double cos_sun) const;

	// The light from a view: from a height, along a unit direction, the star
	// along a unit direction, y up. Star's light as 1: a caller's exposure scales it.
	Rgb sky(double height, const Vec &view, const Vec &sun) const;
	// In air too thick for those sums (Venus's): the share of starlight that diffuses
	// down to the ground, straight from overhead. None in thin air.
	const Rgb &diffuse() const { return m_diffuse; }
	// What share of what's behind that view gets through.
	Rgb through(double height, const Vec &view) const;

private:
	Medium m_medium;
	std::vector<Rgb> m_transmit, m_scatter;
	Rgb m_diffuse{};
};

// An angle above the horizon as seen on the drawn planet, as it is in the real air's
// tables. The air is real-sized and the planet isn't, so from high up their horizons
// are at different angles (`drawn_dip`, `real_dip`: how far below level each is); this
// lines them up, and straight up and down stay put.
double realElevation(double elevation, double drawn_dip, double real_dip);

// A star's sky as a small picture (the game's sky reads it, so the sums run on a
// worker and not per pixel): columns from toward the star round to away from it,
// rows from straight down to straight up by √ of the angle, so the horizon gets
// the most of them. Each pixel's colour is ready to show; its alpha is how much of
// space still shows through (Air fades stars out of a bright sky). RGBA, top row up.
struct SkyImage
{
	static constexpr int W = 192, H = 96;
	std::vector<unsigned char> rgba; // W × H × 4
	Rgb horizon;                     // the row above the horizon's mean, ready to show (0..1)
	Rgb sun;                         // the star's own light after the air, ready to show (0..1)
};
// `light`: the star's brightness as the eye takes it. `tint`: its colour against the air's star.
SkyImage skyImage(const Tables &tables, double height, double elevation, double light, const Rgb &tint);

// The colour daylight on the ground takes at a star's height (radians, as the air
// has it), brightest channel 1. Level with noon's: noon is white, whatever the star's
// own colour does; low it's what the air leaves, lifted a little by the sky's own
// light; under the horizon, the sky's blue dusk. How bright is the sky's job elsewhere.
Rgb sunTint(const Tables &tables, double height, double elevation);

// The air seen from outside, edge on (planetarium/rim): columns by how the star stands
// over the place seen (the cosine, -1..1), rows by how high the sight line passes, from
// the ground to `reach` of the air's depth. RGB, ready to show; each row W pixels.
struct RimImage
{
	static constexpr int W = 64, H = 32;
	std::vector<unsigned char> rgb; // W × H × 3
};
RimImage rimImage(const Tables &tables, double reach, double light);

} // namespace air
