// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include "sphere.h"
#include <cstdint>
#include <vector>

/*
	Impacts (planning/sprint-94-impacts.md): craters of every size, hashed on
	a 3D lattice per size class, so any place finds the craters round it
	without a list. Classes are a factor of two apart, from half the body's
	radius down to a few blocks, each covering the same share of the
	surface (counts ∝ D⁻²); what the body keeps of them is
	Processes::craters. Shapes in the body's metres, by its gravity (Pike
	1977, Melosh 1989; papers only).
*/
namespace terragen {

struct Crater
{
	Place at;
	int64_t radius; // place units (PLACE_ONE a body radius)
	int32_t depth, rim, peak; // blocks, after wear
	bool complex, basin;
	bool young; // made after the body's lava flooded (Processes::young): on top of the maria
};

// Which craters a column asks for: the old lie under the maria's lava, the
// young on it.
enum class Age : uint8_t { All, Old, Young };

class Craters
{
public:
	// `radius`: the body's, in blocks (fixed, 64 bits: World::radiusOf);
	// `blocksPerMetre`: its scale (world.h)
	Craters(const WorldCode &code, int64_t radius, fix blocksPerMetre);

	// Classes from the largest: is class k wider than `blocks` (whole, so
	// the grid takes it instead of the column)?
	int classes() const { return (int)m_classes.size(); }
	int64_t widest(int k) const { return m_classes[k].widest; } // blocks across, the most
	// Every crater of class k (for the grid, which stamps the wide ones).
	std::vector<Crater> all(int k) const;
	// How far class k ≥ `from` of an age raise or sink a place, in blocks (fixed).
	fix height(const Place &at, int from, Age age, int64_t least = 0) const; // classes down to a lattice of `least`
	// One crater's shape at a place (blocks, fixed): 0 past its ejecta.
	static fix shape(const Crater &c, const Place &at);

private:
	struct Class
	{
		int64_t lattice;  // place units a cell
		int64_t smallest; // diameter, place units
		int64_t widest;   // blocks
		uint32_t keep;    // a cell holds one if its hash's low 16 bits are under this
	};
	bool candidate(int k, int32_t i, int32_t j, int32_t l, Crater &out) const;

	uint32_t m_seed;
	int64_t m_radius;   // blocks, fixed
	fix m_scale;        // blocks per metre
	int64_t m_simple;   // the transition diameter, place units
	bool m_water;
	uint32_t m_young; // a crater is young if its age hash's 16 bits are under this
	fix m_stiff; // what the crust keeps of a crater's relief (Processes::stiff)
	std::vector<Class> m_classes;
};

} // namespace terragen
