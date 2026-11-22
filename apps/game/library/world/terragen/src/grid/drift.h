// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/grid/geology.h"
#include <vector>

/*
	One step of the plates' motion (the history's, grid/history.cpp). The
	grid's cells stay put and crust is handed between them: each cell asks
	each plate near it which of its crust the plate's turn brings here. Two
	plates bringing crust to one cell meet; a cell no plate reaches is a gap,
	filled with new ocean floor at a ridge.
*/
namespace terragen::drift {

// New ocean floor's height, at the ridge where it forms
constexpr fix RIDGE_TOP = -ratio(8, 100);

// Each plate's turn this step, as the sine and cosine of minus its angle
// (a cell asks where its crust came from)
struct Turn
{
	fix sine, cosine;
};

// Where two plates brought crust to one cell: `win` stays on top, `other`
// went under (or, a continent, was folded in)
struct Met
{
	int32_t at;
	int win, other;
	bool continent;
};
// Where a continent was torn and floor opened between plates `a` and `b`
struct Opened
{
	int32_t at;
	int a, b;
};

struct Moved
{
	std::vector<History::Cell> cells;
	std::vector<Met> met;       // in cell order
	std::vector<Opened> opened;
};

Moved move(const std::vector<History::Cell> &was, const std::vector<Plate> &plates, const std::vector<Turn> &turns);

} // namespace terragen::drift
