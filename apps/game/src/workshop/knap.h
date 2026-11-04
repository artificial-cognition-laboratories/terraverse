// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "piece/machine.h"
#include <optional>
#include <string>
#include <vector>

class IGameDef;

namespace workshop {

/*
	Knapping (sprint 130): one small thing struck with another. Near an edge
	a long thin flake comes off, running on from where it's hit along the blow; in the middle of
	a face it only thuds. How much is by the striker's weight, and how neatly
	by how clean the material breaks, so a stone worked at its edges thins to
	an edge of its own. A cell's "sharp" when little is round it.
*/
struct Struck
{
	size_t removed = 0;
	// What's left, biggest first (more than one: it split or a piece came
	// away); none: nothing left big enough to keep. Unchanged when it thudded.
	std::vector<piece::Machine> left;
};
// `cell`: sixteenths on its grid; `in`: the way the blow goes, on its grid.
// `clean`: 0 to 1, how neatly it breaks. Throws std::invalid_argument on
// a thing that isn't one small part.
Struck strike(const piece::Machine &m, v3s32 cell, v3f in, float striker_kg, float clean, u32 seed, IGameDef *gamedef);

// How good an edge it has: 0 none, 1 rough, 2 fine.
int edgeOf(const piece::Machine &m);
// The cells of a piece that make its edge (drawn lit, to read it by eye), as
// sixteenths on its grid. Short runs that aren't an edge aren't among them.
std::vector<v3s32> edgeCells(const piece::Piece &built);

} // namespace workshop
