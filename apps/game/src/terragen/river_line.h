// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include <cstdint>
#include <vector>

namespace terragen {

/*
	A grid run's river line: its meanders (planning/sprint-116-land-by-the-
	book.md, phase 6e). A meander isn't a wave across a line: the river turns,
	its direction swinging by an angle that rises and falls along it
	(Kinoshita's curve: θ = θ₀ sin φ, skewed by θ₀ times a third harmonic),
	so at a small angle it wanders, at a large one it folds past square into
	loops and U-turns. Each loop's angle and length its own (near the
	river's: LOOP widths free, its valley's spurs' spacing held by the hills);
	scaled and sheared so it runs from the run's start to its end, bowed (Hermite) so it leaves and arrives along
	the way the river runs through each end, where it joins the next run.

	In the run's frame: x along it from its start, y across (blocks, fixed).
*/
class RiverLine
{
public:
	struct Opts
	{
		int64_t run;   // its length, blocks, fixed
		int64_t loop;  // a loop's length along the river, blocks, fixed (its width's LOOP, or its valley's spurs')
		fix angle;     // its loops' greatest turn (radians)
		fix skew;      // how far its loops lean downstream (−: upstream)
		fix m0, m1;    // its slopes across the run at each end
		uint32_t seed;
	};
	static constexpr int64_t LOOP = 20; // a free loop's length in its widths (11 down its valley at a sinuosity of 1.8)
	RiverLine() = default; // (a run with no river: no line)
	explicit RiverLine(Opts o);

	struct Near
	{
		fix t = 0;          // how far along the line, of its length
		int64_t dist = 0;   // blocks, fixed
		int64_t across = 0; // signed: + on the left of the way it runs
		int64_t along = 0;  // its x there
		fix curve = 0;      // how it turns there, of its sharpest: + to the left
		int32_t loop = 0;
	};
	Near nearest(int64_t x, int64_t y) const;
	struct At
	{
		int64_t x, y; // blocks, fixed
	};
	At at(fix t) const;
	int32_t loops() const { return m_loops; }

private:
	// points at 1/256 of a block, each one's turn (of the sharpest, ±32767)
	// and how far along the line it is (fixed)
	std::vector<int32_t> m_x, m_y;
	std::vector<int16_t> m_curve;
	std::vector<fix> m_t;
	// each chunk of points' box, for the nearest's search
	struct Box
	{
		int32_t x0, y0, x1, y1;
	};
	std::vector<Box> m_boxes;
	int32_t m_loops = 0;
};

} // namespace terragen
