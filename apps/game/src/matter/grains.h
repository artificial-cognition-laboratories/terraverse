// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "matter.h"
#include <array>
#include <vector>

namespace matter {

/*
	Grain size (sprint 111). Loose ground is the same minerals as rock, in
	pieces: what sorts it is how big they are. Sand, silt and clay are sizes,
	not substances (quartz is sand because it lasts; clay is mostly clay
	minerals because that's what weathering leaves, and they're tiny).
	The bands are the soil scientists' (USDA), with stone above gravel.
*/
enum class Band : uint8_t { Stone, Gravel, Sand, Silt, Clay };
constexpr int BANDS = 5;
extern const char *const BAND_NAMES[BANDS];
// The smallest grain each band holds, mm (clay: anything finer than silt).
extern const double BAND_FROM[BANDS];
Band bandOf(double mm);

// The fine earth (under 2 mm) by size: the soil texture triangle.
struct Texture
{
	double sand, silt, clay; // shares, summing to 1 (all 0 with no fine earth)
};

/*
	Grains: kilograms of each substance in each size band, and the water
	held between them. Every operation keeps mass: what it takes out comes
	back as another Grains, and what weathering carries off dissolved is
	returned. The same primitive is a spadeful, a river bar or a soil
	horizon; a sieve, a current and a mill are its splits.

	Organic matter is any substance of carbon and hydrogen (wood, so far):
	it's in the mass and its own share, never the texture.
*/
class Grains
{
public:
	explicit Grains(const Matter &table);
	// A rock broken to one band: its minerals by share (a gravel of
	// granite pebbles is granite's minerals at gravel size).
	static Grains broken(const Matter &table, const RockKind &rock, double kg, Band band);

	const Matter &table() const { return *m_table; }
	double kg(size_t substance, Band band) const { return m_kg[at(substance, band)]; }
	void add(size_t substance, Band band, double kg); // kg ≥ 0
	void add(const Grains &other);                    // its water too
	double water = 0; // kg, between the grains

	double mass() const;          // solids, kg
	double mass(Band band) const; // solids in one band, kg
	double organic() const;       // its share of the solids
	double stones() const;        // share of the solids over 2 mm (stone and gravel)
	Texture texture() const;      // of the mineral fine earth

	// Takes out everything at `band` or finer: what passes a sieve, what a
	// current of that strength carries. The water goes with it in
	// proportion to the mass.
	Grains finer(Band band);
	// Takes out a share of everything (a spadeful of a horizon).
	Grains share(double fraction);
	// Breaks everything coarser than `band` down to it (crushing, milling,
	// a river's wear).
	void crush(Band band);

	/*
		Weathering over `extent`: each mineral that weathers loses
		1 − e^(−extent·rate) of itself, leaving what it weathers into as
		clay-sized grains (feldspar → kaolinite) and the rest dissolved;
		and grains coarser than the rock's own crystals (`crystals`: what
		its grains can fall apart into) break down to them by
		1 − e^(−extent·crumbles), as granite rots to grus (a mineral whose
		crystals never grow that big, clay, comes apart at its own size).
		Returns the kg carried off dissolved (negative where it took more
		from the air than it lost: magnetite rusting).
	*/
	double weather(double extent, Band crystals, double crumbles = 1);

private:
	size_t at(size_t substance, Band band) const { return substance * BANDS + (size_t)band; }
	bool isOrganic(size_t substance) const { return m_organic[substance]; }

	const Matter *m_table;
	std::vector<double> m_kg;
	std::vector<bool> m_organic;
};

} // namespace matter
