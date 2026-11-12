// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace terragen {

// A direction from a planet's centre, a unit vector in fixed point: fine
// enough for the grid, coarse for a column (1/65 536 of the radius).
struct Dir
{
	fix x, y, z;
};

/*
	A place on the planet to the block (sprint 87): its direction from the
	centre as a unit vector to 30 fractional bits, so a column is told from
	its neighbour on a planet Earth's size. What works at cell scale takes
	its coarse dir(); distances in blocks, block noise and fine detail read
	it whole. A Dir is a place exactly (its components widened).
*/
constexpr int PLACE_FRAC = 30;
constexpr int64_t PLACE_ONE = (int64_t)1 << PLACE_FRAC;
struct Place
{
	int32_t x = 0, y = 0, z = 0;
	Place() = default;
	Place(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
	Place(const Dir &d) :
		x(d.x * (1 << (PLACE_FRAC - FRAC))), y(d.y * (1 << (PLACE_FRAC - FRAC))), z(d.z * (1 << (PLACE_FRAC - FRAC))) {}
	// rounded to the grid's precision
	Dir dir() const;
	bool operator==(const Place &o) const { return x == o.x && y == o.y && z == o.z; }
};
// A vector made a place, any length but zero.
Place placeOf(int64_t x, int64_t y, int64_t z);
// Straight-line distance between two places on the unit sphere, at PLACE_ONE.
int64_t chord(const Place &a, const Place &b);

/*
	The world model's grid on the sphere (planning/sprint-86-sphere.md): a
	subdivided icosahedron, its vertices the cells (hexagons, and twelve
	pentagons at the icosahedron's corners), all about one size, so no
	place on the planet is special. Built from integers and fixed point
	alone: the same cells, in the same order, on every machine.

	`frequency` n cuts each icosahedron edge into n; the sphere has
	10n² + 2 cells.
*/
class Sphere
{
public:
	explicit Sphere(int32_t frequency);

	int32_t frequency() const { return m_n; }
	int32_t size() const { return (int32_t)m_centre.size(); }

	const Dir &centre(int32_t cell) const { return m_centre[cell]; }
	// A cell's neighbours, 5 or 6, in turn round it.
	const int32_t *neighboursBegin(int32_t cell) const { return &m_neighbours[m_start[cell]]; }
	const int32_t *neighboursEnd(int32_t cell) const { return &m_neighbours[m_start[cell + 1]]; }
	int32_t neighbourCount(int32_t cell) const { return m_start[cell + 1] - m_start[cell]; }
	// A cell's area against the mean cell's (1: the mean), for what sums over
	// area (rain gathered as it drains).
	fix area(int32_t cell) const { return m_area[cell]; }
	// How far each neighbour's centre is (straight through, on the unit
	// sphere), alongside neighboursBegin.
	const fix *distancesBegin(int32_t cell) const { return &m_distance[m_start[cell]]; }
	// The mean distance between neighbours' centres, on the unit sphere.
	fix spacing() const { return m_spacing; }

	// The cells reached from `start` by neighbours while `keep(cell)` holds
	// (start first, then outward): a patch of the sphere round a place.
	// (What's been looked at is marked with a stamp per cell, the thread's,
	// made once: a set made afresh each call, an allocation a cell, was a
	// twentieth of a far column's time. Not from inside `keep`.)
	template <typename Keep>
	std::vector<int32_t> around(int32_t start, Keep keep) const
	{
		thread_local std::vector<uint32_t> marks;
		thread_local uint32_t stamp = 0;
		thread_local bool busy = false;
		if (busy)
			throw std::logic_error("terragen::Sphere::around: called from its own keep");
		busy = true;
		struct Done
		{
			bool &busy;
			~Done() { busy = false; }
		} done{ busy };
		if (marks.size() < (size_t)size())
			marks.assign((size_t)size(), 0);
		if (++stamp == 0) { // wrapped round: an old mark could pass for this call's
			std::fill(marks.begin(), marks.end(), 0);
			stamp = 1;
		}
		std::vector<int32_t> out{ start };
		marks[start] = stamp;
		for (size_t head = 0; head < out.size(); head++)
			for (const int32_t *p = neighboursBegin(out[head]); p != neighboursEnd(out[head]); p++) {
				if (marks[*p] == stamp)
					continue;
				marks[*p] = stamp;
				if (keep(*p))
					out.push_back(*p);
			}
		return out;
	}

	// The cell whose centre is nearest a direction (a unit one, as Dir is).
	int32_t cell(const Dir &d) const;

	// A direction as the three cells round it and how much each counts
	// (summing to ONE): a field read between cell centres, continuous
	// everywhere, across the icosahedron's edges too.
	struct Spot
	{
		std::array<int32_t, 3> cells;
		std::array<fix, 3> weights;
	};
	Spot spot(const Dir &d) const;

private:
	Spot spotAt(const Dir &d) const; // spot(), not kept

public:

private:
	// Where a direction falls on the icosahedron: its face and its place in
	// the face's lattice (fixed: i, j toward its second and third corner).
	struct Onface
	{
		int32_t face;
		fix i, j;
	};
	Onface onFace(const Dir &d) const;
	int32_t lattice(int32_t face, int32_t i, int32_t j) const;

	int32_t m_n;
	std::vector<Dir> m_centre;
	std::vector<int32_t> m_start, m_neighbours;
	std::vector<fix> m_area;
	std::vector<fix> m_distance; // per neighbour slot
	fix m_spacing = 0;
	std::vector<int32_t> m_lattice; // per face, its lattice points' cells, row by row (j), i along
	std::array<std::array<std::array<int64_t, 3>, 3>, 20> m_cross; // per face: B×C, C×A, A×B of its corners
};

// A direction from latitude and longitude (radians): +y north, longitude 0
// toward +x, east toward +z.
Dir dirAt(fix latitude, fix longitude);
fix latitudeOf(const Dir &d);  // −π/2 .. π/2
fix longitudeOf(const Dir &d); // (−π, π]
// Straight-line distance between two directions on the unit sphere.
fix chord(const Dir &a, const Dir &b);
// A vector made unit, any length but zero (fixed components).
Dir normalise(int64_t x, int64_t y, int64_t z);

} // namespace terragen
