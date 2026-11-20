// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <vector>

/*
	Piles (sprint 84): loose ground that moves. A field of columns a
	sixteenth of a node wide, each with bonded ground that stays (its floor)
	and a depth of loose grains over it, in sixteenths. No grains, no
	velocities: where a column stands higher over its neighbour than the
	material rests at, the excess slides downhill and stops. It settles in a
	handful of steps, so it sleeps almost at once.

	Everything is integers in a fixed order, so the server and the client,
	from the same piles, reach the same ground: the server's is the truth
	(it's stood on, driven on, dug), the client runs the same rule to
	predict its own pushes and is corrected.

	Sixteenths, not finer: the world keeps ground as cut shapes, so a slope
	that only rests between sixteenths would slide again each time it's
	read back. A material rests at a whole step a column (or flat).
*/
namespace pile {

constexpr int UNIT = 16; // sixteenths a node

// How a loose material behaves, from its node's groups (data, never per kind)
struct Material
{
	// how far a column may stand over its neighbour at rest, sixteenths
	// (1: 45°, the steepest dry sand or gravel stands in steps of ours)
	int repose;
	// how much further before it gives at all: mud and wet clay hold a
	// steep face till pushed past it; sand gives at once (0)
	int yield;
	// how hard a press sinks it a sixteenth, kPa (soft mud: little)
	int bearing;
	// how far a column packs before it gives no more, sixteenths: a rut
	// deepens pass by pass towards it, then holds
	int give;
};

class Field
{
public:
	Field(int width, int depth, Material material);

	int width() const { return m_w; }
	int depth() const { return m_d; }
	const Material &material() const { return m_material; }

	int floor(int x, int z) const { return m_floor[at(x, z)]; }
	int loose(int x, int z) const { return m_loose[at(x, z)]; }
	int top(int x, int z) const { return floor(x, z) + loose(x, z); }
	void set(int x, int z, int floor, int loose);
	// The world column (sixteenths) its first column is: fields over the
	// same ground with other edges then slide alike.
	void origin(int x, int z) { m_ox = x; m_oz = z; }
	// Something disturbed this column: only stirred columns (and what they
	// slide into) move, so ground standing steeper than it rests at (a
	// generated cliff) stays till something touches it.
	void stir(int x, int z) { m_stirred[at(x, z)] = 1; }

	// All the loose ground, sixteenths: what flow and pushes keep.
	long long total() const;

	// One step of sliding; true if anything moved. Each pair of neighbours
	// moves half its excess, in phases where no two pairs share a column.
	bool step();
	// Steps till nothing moves, at most `most`; the steps taken.
	int settle(int most);

	/*
		Something presses on the ground: a wheel's or a foot's patch, a
		circle (centre and radius in columns) pressing at `kpa`. The patch
		sinks by the pressure over the material's bearing, less as the
		column packs (no more once it's packed its give, never through its
		floor), and what it pushes out goes to the ring just outside,
		evenly: a rut with a ridge. Returns the sixteenths pushed aside.
	*/
	long long press(float cx, float cz, float radius, float kpa);

private:
	int at(int x, int z) const { return z * m_w + x; }

	int m_w, m_d;
	Material m_material;
	std::vector<int> m_floor, m_loose;
	std::vector<int> m_packed; // per column, sixteenths it's been pressed down
	std::vector<char> m_stirred;
	int m_ox = 0, m_oz = 0; // the world column of (0, 0): which pairs a phase takes
};

} // namespace pile
