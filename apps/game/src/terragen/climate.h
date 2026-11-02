// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include "planet.h"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

/*
	The climate: where rain falls and when, from the air rather than from
	noise. The same physics as the live weather (src/fluid), solved for its
	long-run state instead of stepped through time, in fixed point so a world
	code gives the same climate everywhere. On the grid's own cells.

	- The circulation by rule: Held & Hou's Hadley cells, Rhines' jets
	  beyond them (circulate, shared with the fluid layer).
	- Ocean currents: wind-driven gyres, warm poleward off east coasts and
	  cold toward the equator off west coasts, warming or chilling the sea
	  and the coasts, and drying the air over cold water.
	- Temperature from a balance of energy (Budyko, Sellers, North): sunlight
	  absorbed against heat radiated and carried poleward, with ice that
	  brightens a cold world further, and the sea damping the seasons.
	- Two seasons, each with the rising branch (the ITCZ) following the sun,
	  and thermal lows over summer continents drawing sea air in (monsoons).
	- Moisture evaporates from warm sea (and is recycled by wet land),
	  travels with the mean winds, and rains out where air rises (the ITCZ,
	  the polar front, slopes facing the wind, hot land) and is held where it
	  sinks (the dry belts, the lee of mountains).
*/
namespace terragen {

// The large-scale flow a spin allows. Plain doubles from + − × ÷ √ only,
// which IEEE rounds exactly, so it is deterministic like the fixed-point code.
struct Circulation
{
	double hadley;   // the Hadley cells' poleward edge, radians
	int32_t jets;    // eddy-driven jets per hemisphere beyond them
	double trades;   // surface wind scales, m/s
	double westerly;
	double lift;     // vertical wind scale, m/s
};
// radius m, gravity m/s², day s, depth: the weather layer's depth, m
Circulation circulate(double radius, double gravity, double day, double depth);
// Earth's (terra worlds are Earth-sized and spin once a day).
// The circulation of a planet's air.
Circulation circulation(const Planet &p);

// The shape of the mean flow at a latitude (radians), with the rising branch
// at `itcz`: u in westerly units (trades blow at −0.7), v in westerly units,
// w in lift units (+1 rising at the ITCZ).
struct Flow
{
	fix u, v, w;
};
Flow flow(const Circulation &c, fix latitude, fix itcz);

struct Climate
{
	// Per cell of the grid (gridSphere()):
	// rain in each season, relative to the median over land (1): [0] while the
	// north has summer (the ITCZ north), [1] while the south does
	std::vector<fix> rain[2];
	// how much warmer (or colder) the ocean currents make it, in heat units;
	// carried a few cells onto the coasts
	std::vector<fix> sea;
	// the yearly mean heat at sea level (currents included), and how much
	// it gains per unit of sunlight above the yearly mean (the seasons:
	// fierce in continental interiors, mild where the sea damps them)
	std::vector<fix> heat;
	std::vector<fix> gain;
	fix ice; // the share of the planet's area frozen over (yearly mean below −10 °C)
};

// The climate over `elevation` (per cell of gridSphere(), 0 = sea level),
// for the world's sun, star, Temperature and Humidity.
Climate climate(const WorldCode &code, const std::vector<fix> &elevation);

// Sunlight reaching row v (World::sunlight): on `date` (0..1), or the yearly mean.
fix sunlight(const WorldCode &code, fix v, std::optional<fix> date);

// The temperature field the climate and the world share is in heat units:
// ~−1 frozen .. 1 scorching, 11 °C at 0, 37 °C a unit (fitted to the biomes).
inline fix celsiusOf(fix heat) { return toFix(11) + heat * 37; }
inline fix heatOfCelsius(fix celsius) { return (celsius - toFix(11)) / 37; }
// Air cools with height: 0.9 heat units (33 °C) per unit of elevation.
constexpr fix LAPSE = ratio(9, 10);

} // namespace terragen
