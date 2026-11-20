// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include "sphere.h"
#include <cstdint>
#include <vector>

namespace terragen {

struct Grid;

/*
	The ground below the grid's cells, gullied as water and soil leave it
	(planning/sprint-116-land-by-the-book.md, phase 6): an erosion filter on
	the grid's smooth surface, after runevision's erosion filter and Phacelle
	noise (2025–26) and clayjohn's eroded terrain noise (2018), from their
	descriptions, not their code.

	Octaves from half a cell down to a hillside's length; in each, stripes
	run down the slope as it stands so far: each point of a jittered lattice
	(in three dimensions, so the sphere has no seam) sends a wave across the
	slope, the waves blended as a phasor (normalised: even amplitude, no
	beating), so the ground is grooved downhill. Each octave steers by the
	slope the ones before made, so small gullies bend into bigger ones and
	branch as drainage does.

	How deep: as stream power at its balance cuts, a drainage L across falls
	k_s · cell / L a block (k_s the grid's channel steepness, S·√A), so each
	octave adds about RELIEF · k_s · cell, less for the smaller ones (FALL),
	and never steeper than a hillside stands (CRITICAL). Below a hillside's
	length nothing's gullied: soil creeps smooth faster than water cuts.

	A pure function of the place: the same on every machine, seamless.
*/
class Amplifier
{
public:
	// radius: blocks; cell_blocks: blocks, fixed; height: blocks per unit of elevation; metre: blocks a metre
	Amplifier(const Grid &grid, int64_t radius, int64_t cell_blocks, fix height, fix metre, uint32_t seed);

	// A grid field at a place, smooth (C², and so its slope) between cells:
	// a Wendland kernel over the cells within REACH spacings
	fix smooth(const std::vector<fix> &field, const Place &p) const;
	// The grid's channel steepness (S·√A, A in cells) at a place, smooth
	fix steepness(const Place &p) const { return smooth(m_ks, p); }
	const std::vector<fix> &steepnesses() const { return m_ks; } // each cell's
	// The relief at a place over the grid's smooth surface, blocks (fixed)
	// (`below`: only the octaves finer than this, blocks; 0 all)
	int64_t relief(const Place &p, int64_t below = 0) const;

private:
	const Grid &m_grid;
	int64_t m_radius;      // blocks
	int64_t m_cell_blocks; // blocks, fixed
	fix m_height;          // blocks per unit of elevation
	uint32_t m_seed;
	std::vector<fix> m_ks; // each cell's channel steepness (S·√A, A in cells)
	std::vector<int64_t> m_octaves; // the lattices' spacings, blocks, coarsest first
};

} // namespace terragen
