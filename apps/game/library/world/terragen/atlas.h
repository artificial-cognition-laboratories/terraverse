// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace terragen {

/*
	The planet as blocks: layout v2 (planet.paper.md, section 2).

	A handful of regions, each one Cartesian grid from the surface down to
	the core:
	- Segments: N wedges between meridians, cap circle to cap circle. A row
	  follows the centre meridian exactly (its latitude is j / radius); east,
	  a row runs along the great circle square to that meridian, rounded:
	  partway (`rounding`) from a flat line to the circle itself, so blocks
	  keep their width going down and neighbouring grids overlap at the seam.
	- Caps: poleward of capLatitude, a grid on each pole, rounded the same
	  way out from the pole.
	- Core: inside `core` blocks of the centre, one plain Cartesian grid.
	Every place belongs to one region (its territory: by radius, latitude and
	longitude); a grid's cells are those that reach into its territory, and
	the rest are trimmed. Cells the territory's edge cuts are cut cells.

	Planet frame: y to the north pole (the spin axis), x to longitude 0 on
	the equator, z to 90° east. Layer k is about the radii
	[radius + k, radius + k + 1).
*/
class Atlas
{
public:
	struct Vec
	{
		double x, y, z;
	};

	struct Params
	{
		double radius;      // blocks from the centre to the surface
		int segments;       // round the planet
		double capLatitude; // degrees: poleward of this, a cap
		double core;        // blocks from the centre: inside, the core
		double rounding;    // 0 flat .. 1 round: segments' east, caps' out from the pole
		int height;         // layers above the surface
	};

	enum class Region : uint8_t
	{
		Segment,
		Cap,
		Core,
	};

	/*
		A cell of a region's grid: i east, j north, k up (a layer; the core's
		k is its own y, from the centre). `index`: a segment's number from
		longitude 0 eastward, a cap's +1 north or −1 south, the core's 0.
		Every grid is right-handed with its up.
	*/
	struct Cell
	{
		Region region;
		int index;
		int64_t i, j, k;

		bool operator==(const Cell &o) const
		{
			return region == o.region && index == o.index && i == o.i && j == o.j && k == o.k;
		}
	};

	// The seams a cell touches.
	enum Seam : uint8_t
	{
		SEGMENT = 1, // a neighbouring segment
		CORE = 2,    // the core's sphere
		CAP = 4,     // a cap's circle
		CUT = 8,     // trimmed by its territory's edge
	};

	// Signed stretches (edge length − 1, the worst of the four parallel
	// edges), angles in degrees, retained in 0–1.
	struct Quality
	{
		double east, north, up;
		double skew;    // worst departure from 90° between the three axes
		double gravity; // between the cell's up and radial
		double retained;
		uint8_t seams;
	};

	// A region's grid: its kind and index, and the cells it can have (every
	// cell that reaches its territory is within low..high, inclusive).
	struct Grid
	{
		Region region;
		int index;
		std::array<int64_t, 3> low, high; // i, j, k
	};

	explicit Atlas(const Params &params); // throws std::invalid_argument for a layout that can't be

	// The layout a planet of this radius gets unless told otherwise: 24
	// segments (every block inside a region within 1% of a cube, gravity
	// within 0.4°: planet.paper.md), caps poleward of 85°, the core the inner
	// quarter, rounding 0.95, 256 layers of sky.
	static Params standard(double radius) { return { radius, 24, 85, radius / 4, 0.95, 256 }; }

	const Params &params() const { return m_params; }
	// Every region's grid: the segments, the caps (north, south), the core.
	const std::vector<Grid> &grids() const { return m_grids; }

	// The cell of a point's territory's grid there (it may not be one: a
	// sliver at a cut). And the cell the point belongs to: that one; or where
	// it's a sliver, the neighbouring grid's that reaches over; or where
	// neither keeps it, the nearest cell round it (slivers merged into a
	// neighbour). Throw std::out_of_range above the atlas.
	Cell owner(const Vec &point) const;
	Cell locate(const Vec &point) const;
	// Whether a grid cell is one: some of it (a 64th) is in its territory.
	bool exists(const Cell &cell) const;
	// How far a cell's centre is inside its territory (blocks to the
	// nearest edge, along the surface it's on; negative outside).
	double clearance(const Cell &cell) const { return inside(cell.region, cell.index, centre(cell)); }
	// How far inside a cell's territory any point is (negative outside).
	double territory(const Cell &cell, const Vec &point) const { return inside(cell.region, cell.index, point); }
	// The same, edge by edge: how far inside each of its territory's edges
	// a point is (a segment's west and east meridians, its cap circle, the
	// core's sphere; infinite for an edge it hasn't). territory() is the
	// least. Each is smooth where the territory, at a corner, isn't.
	std::array<double, 4> edges(const Cell &cell, const Vec &point) const;

	// A point of a cell's grid, continued past it: (di, dj, dk) from its
	// centre along i, j, k. The cell's true geometry; corners() are its
	// points at (±½, ±½, ±½).
	Vec at(const Cell &cell, double di, double dj, double dk) const;
	// And back: where a point is in a region's grid, in cells (cell (i, j, k)
	// spans [i, i + 1) and so on).
	std::array<double, 3> grid(Region region, int index, const Vec &point) const;

	// bit 0 east (i + 1), bit 1 north (j + 1), bit 2 up (k + 1).
	std::array<Vec, 8> corners(const Cell &cell) const;
	Vec centre(const Cell &cell) const;
	Quality quality(const Cell &cell) const;
	double retained(const Cell &cell) const;

	// Faces 0–5: −east, +east, −north, +north, −up, +up. Cells across a face:
	// the grid's own neighbour inside a territory; at a seam or a cut, every
	// cell the face's samples fall in (a T-junction's several); none past the
	// top of the atlas.
	std::vector<Cell> neighbours(const Cell &cell, int face) const;
	// Of those, the one straight through the face's middle (just past it, or
	// past the cut's edge where a cut cell's territory ends first); where
	// that line of the cell isn't in its territory at all, the one nearest
	// the face's middle. None past the top of the atlas.
	std::optional<Cell> through(const Cell &cell, int face) const;

	// The mapping as a world records it ("atlas-v2 radius=… rounding=…"),
	// and back. parse throws on another version or a missing, extra or
	// malformed field.
	static constexpr const char *VERSION = "atlas-v2";
	std::string code() const;
	static Params parse(const std::string &code);

private:
	struct Frame
	{
		Vec up, east, north;
	};
	Frame frame(Region region, int index) const;
	// How far inside a region's territory a point is (blocks to its nearest
	// edge; negative outside).
	double inside(Region region, int index, const Vec &point) const;
	bool above(const Vec &point) const; // past the atlas's top
	std::optional<Vec> pastFace(const Cell &cell, int axis, double d, double s, double t) const;
	std::optional<Cell> beyond(const Cell &cell, int axis, double d, double s, double t) const;
	uint8_t seams(const Cell &cell) const;
	Grid extent(Region region, int index) const;

	Params m_params;
	std::vector<Grid> m_grids;

	// Cut cells' retained volume, sampled once (shared by copies, and
	// between threads)
	struct Kept
	{
		using Key = std::tuple<int, int, int64_t, int64_t, int64_t>;
		std::mutex lock;
		std::map<Key, double> retained;
	};
	std::shared_ptr<Kept> m_kept = std::make_shared<Kept>();
};

} // namespace terragen
