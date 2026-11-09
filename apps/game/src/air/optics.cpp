// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "air/optics.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace air {

using terragen::Gas;

namespace {

double real(terragen::fix v) { return (double)v / terragen::ONE; }

const double WAVELENGTH[3] = { 680e-9, 550e-9, 440e-9 }; // m
const double BOLTZMANN = 1.380649e-23;
const double GAS_CONSTANT = 8.314;
const double STP_NUMBER = 2.687e25; // molecules per m³ at 0 °C, 1 atm: what refractivity is quoted at
const double BELOW_THIS_NONE = 1e-7; // bar: the Moon's kind of "air" scatters nothing to see

// Per gas (terragen::Gas order): n − 1 at STP in visible light, the King
// factor (how far from a sphere the molecule is) and the molar mass (g/mol).
// Handbook values; the spread across the visible is small beside λ⁻⁴.
struct GasOptics
{
	double refractivity, king, molar;
};
const GasOptics GASES[(int)Gas::Count] = {
	{ 4.49e-4, 1.15, 44.01 },  // CO2
	{ 2.98e-4, 1.034, 28.01 }, // N2
	{ 2.71e-4, 1.096, 32.00 }, // O2
	{ 4.44e-4, 1.00, 16.04 },  // CH4
	{ 1.39e-4, 1.00, 2.016 },  // H2
	{ 0.35e-4, 1.00, 4.003 },  // He
};

// m² a molecule, by Rayleigh's formula
double crossSection(const GasOptics &g, double wavelength)
{
	const double k = g.refractivity / (STP_NUMBER * wavelength * wavelength);
	return 32 * M_PI * M_PI * M_PI / 3 * k * k * g.king;
}

// Earth's ozone (Bruneton's fit to measured profiles), per m at its peak, with
// the layer 25 km up and 15 km either side. Made from oxygen, so it scales with it.
const Rgb OZONE = { 0.650e-6, 1.881e-6, 0.085e-6 };
const double EARTH_OXYGEN = 0.21;
const double EARTH_AIR_HEIGHT = 8500; // m
// Methane's bands in the red and a little in the green, m² a molecule: the order
// of Karkoschka's measured coefficients, averaged across each colour's width.
const Rgb METHANE = { 3e-30, 2e-31, 0 };

// What hangs in the air, chosen by what the card says the world is like. Each is
// a depth of air seen straight up (τ) with a share of light kept at each colour.
struct Particles
{
	double depth;    // τ straight up, in green
	Rgb kept;        // of the light that meets one, the share scattered, not absorbed
	double height;   // m, in the air's heights
	double g;
};
// (g 0.95: real haze sends most of its forward light within a few degrees of
// the star, so its disc stays sharp in a small halo; 0.8's one wide lobe
// whitened a third of the sky round it)
const Particles SEA_HAZE = { 0.1, { 0.9, 0.9, 0.9 }, 0.14, 0.95 };        // Earth's aerosols
const Particles DRY_DUST = { 0.5, { 0.97, 0.85, 0.55 }, 1.0, 0.65 };     // iron-stained, as Mars' is
const Particles DUSTY_AIR = { 0.2, { 0.95, 0.88, 0.7 }, 0.3, 0.7 };      // dry land under thicker air
const Particles ORGANIC = { 3.0, { 0.95, 0.72, 0.35 }, 1.5, 0.6 };       // sunlit methane's tholins, as Titan's
const Particles ACID_CLOUD = { 25.0, { 0.999, 0.995, 0.93 }, 2.5, 0.75 }; // Venus's, a little blue taken out

const Particles *particlesOf(const terragen::BodyCard &card, double bar, double methane, double oxygen)
{
	if (methane > 0.005 && oxygen < 0.001 && bar > 0.01)
		return &ORGANIC;
	if (bar >= 10 && real(card.temperature) > 500)
		return &ACID_CLOUD;
	if (card.ocean)
		return &SEA_HAZE;
	return bar < 0.3 ? &DRY_DUST : &DUSTY_AIR;
}

double planck(double wavelength, double kelvin)
{
	const double second = 1.4388e-2; // m K
	return 1 / (std::pow(wavelength, 5) * (std::exp(second / (wavelength * kelvin)) - 1));
}

} // namespace

Rgb starColour(int type)
{
	// M to O: a typical surface temperature for each class
	static const double KELVIN[terragen::STAR_COUNT] = { 3300, 4400, 5778, 6800, 8500, 18000, 35000 };
	if (type < 0 || type >= terragen::STAR_COUNT)
		throw std::invalid_argument("air::starColour: no such star class");
	Rgb c;
	for (int i = 0; i < 3; i++)
		c[i] = planck(WAVELENGTH[i], KELVIN[type]) / planck(WAVELENGTH[i], 5778);
	const double green = c[1];
	for (double &v : c)
		v /= green;
	return c;
}

Medium mediumOf(const terragen::BodyCard &card, double radius_km, int star_type)
{
	Medium m;
	m.radius = radius_km * 1000;
	m.top = m.radius;
	m.star = starColour(star_type);
	m.sunlight = real(card.sunlight);
	// (a giant's card has no surface: its cloud tops, at about a bar, are the ground here)
	const double bar = card.giant ? 1 : real(card.pressure);
	const double kelvin = real(card.temperature);
	double kept = 0;
	for (int g = 0; g < (int)Gas::Count; g++)
		kept += real(card.gases[g]);
	if (bar < BELOW_THIS_NONE || kept <= 0 || kelvin <= 0)
		return m;

	const double number = bar * 1e5 / (BOLTZMANN * kelvin);
	double molar = 0;
	for (int g = 0; g < (int)Gas::Count; g++) {
		const double share = real(card.gases[g]) / kept;
		molar += share * GASES[g].molar;
		for (int c = 0; c < 3; c++)
			m.rayleigh[c] += share * number * crossSection(GASES[g], WAVELENGTH[c]);
	}
	const double pull = real(card.gravity) * 9.81;
	if (pull <= 0)
		throw std::invalid_argument("air::mediumOf: air on a body with no pull");
	m.rayleigh_height = GAS_CONSTANT * kelvin / (molar / 1000 * pull);
	m.top = m.radius + 8 * m.rayleigh_height; // (as the far view's air: all but 0.03% is below this)

	const double oxygen = real(card.gases[(int)Gas::O2]) / kept;
	const double methane = real(card.gases[(int)Gas::CH4]) / kept;
	// ozone: its column follows the oxygen; its layer stays at Earth's share of the air's height
	const double thin = m.rayleigh_height / EARTH_AIR_HEIGHT;
	m.ozone_at = 25000 * thin;
	m.ozone_width = 15000 * thin;
	const double ozone_share = std::min(oxygen / EARTH_OXYGEN, 1.0) / thin * std::min(bar, 1.0);
	for (int c = 0; c < 3; c++) {
		m.ozone[c] = OZONE[c] * ozone_share;
		m.gas_absorb[c] = METHANE[c] * methane * number;
	}

	const Particles *p = particlesOf(card, bar, methane, oxygen);
	m.particle_height = p->height * m.rayleigh_height;
	m.particle_g = p->g;
	const double total = p->depth / m.particle_height;
	for (int c = 0; c < 3; c++) {
		m.particle_scatter[c] = total * p->kept[c];
		m.particle_absorb[c] = total * (1 - p->kept[c]);
	}
	return m;
}

Medium Medium::resized(double radius_m, double rayleigh_height_m) const
{
	Medium m = *this;
	m.radius = radius_m;
	if (none()) {
		m.top = radius_m;
		return m;
	}
	const double k = rayleigh_height_m / rayleigh_height;
	m.rayleigh_height = rayleigh_height_m;
	m.top = radius_m + (top - radius) * k;
	m.ozone_at *= k;
	m.ozone_width *= k;
	m.particle_height *= k;
	for (int c = 0; c < 3; c++) {
		m.rayleigh[c] /= k;
		m.gas_absorb[c] /= k;
		m.ozone[c] /= k;
		m.particle_scatter[c] /= k;
		m.particle_absorb[c] /= k;
	}
	return m;
}

} // namespace air
