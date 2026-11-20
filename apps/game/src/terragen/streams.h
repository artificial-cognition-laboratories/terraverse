// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include "sphere.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace terragen {

struct Grid;
class Amplifier;
class World;

/*
	The land below the grid's cells, cut from its streams
	(planning/sprint-116-land-by-the-book.md, phase 6b; after Dendry,
	Gaillard, Peytavie, Galin et al. 2019, from the paper).

	A tree of channels. Level 0 is the grid's: every cell's run to its
	receiver (a drawn river's line as ground() draws it, others smooth
	between the centres), its bed the grid's water. Each finer level is a
	lattice of cubes half as wide (four times the streams, each half as long):
	a hashed point in each cube lying on the ground walks down the land the
	coarser levels carved till a coarser channel takes it, and that walk is
	its channel, its bed falling all the way. Then the land: from the grid's
	smooth surface, each level's channels cut their valleys into what the
	coarser left, the land the lowest of them, so ridges stand where valleys
	meet. A channel carries water where what it drains, by the rain there,
	passes one threshold at every level.

	A pure function of the place: the same on every machine, seamless.
	What it builds is kept only to save the work.
*/
class Streams
{
public:
	struct Opts
	{
		const Grid &grid;
		const World &world;          // the rivers' runs (level 0)
		const Amplifier &amplifier;  // the grid's surface and channel steepness, smooth
		int64_t radius;              // blocks
		int64_t cell_blocks;         // blocks, fixed
		fix height;                  // blocks per unit of elevation
		fix metre;                   // blocks a metre
		uint32_t seed;
	};
	explicit Streams(Opts o);
	~Streams();
	Streams(const Streams &) = delete;
	Streams &operator=(const Streams &) = delete;

	struct Read
	{
		int64_t land = 0;            // blocks over the sea, fixed: the ground, cut
		// the widest wet channel here, if any
		int level = -1;              // 0 the grid's .. levels()
		int64_t bed = 0;             // its water's level, blocks, fixed
		int64_t width = 0;           // blocks, fixed
		fix hillside = 0;            // −1 a valley floor .. 1 a ridge
	};
	// The land at a place, from `above` (the land there before its valleys,
	// blocks, fixed). `least`: no level finer than this (blocks, fixed): what
	// a far tile's cells are too wide to show.
	Read read(const Place &p, int64_t above, int64_t least = 0) const;
	// A cell's valley floor (blocks over the sea, fixed): its share of the
	// grid's surface is the cell's mean, its trunk runs a valley's depth at the
	// cell's scale under it; falling downstream, none under a lake or the sea
	int64_t floor(int32_t c) const { return m_floor[(size_t)c]; }
	int levels() const { return (int)m_spacing.size() - 1; }
	// its own, of every Streams made: what the threads' caches know it by
	uint64_t owner() const { return m_owner; }
	int64_t spacing(int level) const { return m_spacing[(size_t)level]; } // blocks

	struct Branch;
	struct Kept;
	struct V
	{
		int64_t x, y, z; // blocks, at detail::SUB fractional bits
	};

private:
	// the land carved by the levels below `upto` (blocks, fixed), and the
	// widest wet channel within `within` blocks (fixed) of the place
	// (`above` FROM_GRID: the grid's surface there, a little over it)
	static constexpr int64_t FROM_GRID = INT64_MIN;
	int64_t carve(const V &p, int64_t above, int upto, Read *wet, int64_t within) const;
	// the grid's fields at a place, straight across its triangle of cells
	// (the walks read them a hundred times a branch: the amplifier's smooth
	// kernel was most of their time)
	struct Here
	{
		std::array<int32_t, 3> cells;
		std::array<fix, 3> weights;
		fix of(const std::vector<fix> &field) const;
	};
	Here here(const V &p) const;
	int64_t depth(const Here &h, int level) const; // a valley's depth at a level's scale
	fix rain(const Here &h) const;
	std::shared_ptr<const std::unordered_map<uint64_t, std::vector<std::shared_ptr<const Branch>>>> indexed(int level, int64_t bi, int64_t bj, int64_t bl) const;
	std::shared_ptr<const Branch> grown(int level, int64_t i, int64_t j, int64_t l) const;
	std::shared_ptr<const Branch> walk(int level, int64_t i, int64_t j, int64_t l) const; // (streams_grow.cpp)
	std::shared_ptr<const Branch> run(int32_t c) const;
	// the channels near a place on a level
	void near(const V &p, const Here &h, int level, const std::function<void(const Branch &)> &f) const;
	V toV(const Place &p) const;
	Place toPlace(const V &v) const;
	V ground(V v) const; // onto the sphere's surface

	const Grid &m_grid;
	const World &m_world;
	const Amplifier &m_amplifier;
	int64_t m_radius, m_cell_blocks;
	fix m_height;
	uint32_t m_seed;
	uint64_t m_owner;
	std::vector<int64_t> m_spacing;  // blocks, [0] the grid's
	std::vector<fix> m_scale;        // each level's depth, of the grid's: (L / half a cell)^FALL
	std::vector<int32_t> m_main;     // each cell's biggest donor (−1 none)
	std::vector<int64_t> m_floor;    // blocks, fixed (floor())
	std::unique_ptr<Kept> m_kept;
};

} // namespace terragen
