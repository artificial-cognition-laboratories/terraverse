// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/fluid/fluid.h"
#include <algorithm>
#include <cmath>

namespace fluid {

/*
	The circulation a body's spin allows, from two classic results rather
	than a simulated fluid:

	- Held & Hou (1980): air rising at the equator carries its spin poleward
	  and, conserving it, speeds eastward until it can't go further; the
	  Hadley cell ends where that happens. Its edge is
	  √(5/3 · g·H·Δ / (Ω²a²)): narrow on a fast, big world, pole to pole on
	  a slow one (Venus, Titan).
	- Rhines (1975): beyond it, eddies organise into jets spaced about
	  π√(2U/β) apart, β being how fast the spin's effect grows northward.
	  Earth fits one (the westerlies, with the polar cell beyond); a fast
	  giant fits a dozen (Jupiter's stripes).

	Calibrated so Earth gives a Hadley edge near 30° and one jet.
*/
static const double CONTRAST = 0.3;     // equator-to-pole contrast, as a share of the mean (radiative)
static const double TROPOPAUSE = 1.6;   // the weather layer's depth, in scale heights
static const double SURFACE = 1.0 / 8;  // surface wind as a share of the upper wind
static const double LIFT = 0.02;        // m/s: large-scale rising air

Circulation circulate(const Planet &p, const Air &air)
{
	double molar = 0;
	for (const Gas &g : air.gases)
		molar += g.share * g.substance.molar_mass / 1000.0;
	double scale = matter::GAS_CONSTANT * air.mean / (molar * p.gravity);
	double depth = TROPOPAUSE * scale;
	double omega = 2 * M_PI / p.day;
	double a = p.radius;

	double edge = std::sqrt(5.0 / 3.0 * p.gravity * depth * CONTRAST / (omega * omega * a * a));
	edge = std::min(edge, M_PI / 2);
	// the thermal wind aloft, held below what the air's sound speed allows
	double upper = std::min(p.gravity * depth * CONTRAST / (omega * a), 0.3 * std::sqrt(p.gravity * depth));

	int32_t jets = 0;
	double span = a * (M_PI / 2 - edge);
	if (span > 0.05 * a) {
		double beta = 2 * omega * std::cos(M_PI / 4) / a;
		double spacing = M_PI * std::sqrt(2 * upper / beta);
		jets = std::max<int32_t>(1, (int32_t)std::lround(span / spacing));
	}
	double surface = upper * SURFACE;
	return { edge, jets, 0.7 * surface, surface, LIFT };
}

Flow flow(const Circulation &c, double lat, double itcz)
{
	// the Hadley cells, either side of the rising branch, follow it with the seasons
	double north = std::min(M_PI / 2, itcz + c.hadley);
	double south = std::max(-M_PI / 2, itcz - c.hadley);
	if (lat >= south && lat <= north) {
		double side = lat >= itcz ? 1 : -1;
		double edge = side > 0 ? north : south;
		double s = edge == itcz ? 0 : (lat - itcz) / (edge - itcz);
		return {
			-c.trades * std::sin(M_PI * s),           // the trades: easterly
			-side * 0.4 * c.trades * std::sin(M_PI * s), // toward the rising branch
			c.lift * std::cos(M_PI * s),              // up at the ITCZ, down at the edge
		};
	}
	if (c.jets == 0) // the cells reach the poles; only the polar cap is left, sinking
		return { 0, 0, -c.lift };
	// beyond: alternating bands, westerly (eddy-driven, rising at its poleward
	// front) then easterly (sinking poleward), two per jet
	double pole = lat > 0 ? 1 : -1;
	double edge = pole > 0 ? north : -south;
	double x = (std::fabs(lat) - edge) / (M_PI / 2 - edge) * (2 * c.jets);
	int32_t band = std::min<int32_t>((int32_t)x, 2 * c.jets - 1);
	double f = x - band;
	double s = std::sin(M_PI * f);
	if (band % 2 == 0)
		return { c.westerly * s, pole * 0.2 * c.westerly * s, -c.lift * std::cos(M_PI * f) };
	return { -0.5 * c.westerly * s, -pole * 0.1 * c.westerly * s, c.lift * std::cos(M_PI * f) };
}

} // namespace fluid
