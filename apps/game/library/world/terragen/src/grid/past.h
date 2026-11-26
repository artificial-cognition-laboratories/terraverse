// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/grid/geology.h"
#include <vector>

/*
	What happens to crust inside a plate, through the history (sprint 120,
	.agents/planning/sprint-120-land-with-a-history.md). Edges build and tear
	crust; plumes rising from deep in the mantle are what change a plate's
	middle. A plume is fixed beneath the plates. When its head arrives it
	floods the land over it with basalt (the Deccan Traps) and splits the
	crust into three rift arms; one or two open, the rest fail and fill
	with sediment (aulacogens). As long as it lasts it holds the land above
	it up in a broad swell (Ahaggar, Tibesti). Land carried off it cools and
	sinks into a sag basin (the Michigan basin, the Congo's).
*/
namespace terragen::past {

struct Plume
{
	Dir at;
	int32_t arrives; // the step its head reaches the plates (before 0: it was already there)
	int32_t ends;    // the step it dies
	fix turn;        // the angle of its first rift arm, radians
};

// The planet's plumes, fixed in the mantle.
std::vector<Plume> plumes(const WorldCode &code);

// Before the history: continents are older pieces (terranes) sewn together
// along belts long since worn down. Each cell's piece, and each piece's middle
// (the plates take continents a piece at a time, so they part along old seams).
struct Terranes
{
	std::vector<Dir> centres;
	std::vector<uint8_t> of; // each cell's
	uint32_t seed;           // (what else is drawn per piece: its cover)
};
Terranes terranes(const WorldCode &code);
// Each cell's plate at the start, grown out from the plates' centres each at
// its own pace (and the small ones set turning slower, so they don't trail
// their own new floor round the world in a strip)
std::vector<uint8_t> grow(std::vector<Plate> &plates, const WorldCode &code);

// Marks the seams between the continents' pieces and the belts round them.
void inherit(std::vector<History::Cell> &cells, const Terranes &terranes);

// One step older: every record ages with its crust.
void age(std::vector<History::Cell> &cells);

// What the plumes do to the crust over them this step.
void lift(std::vector<History::Cell> &cells, const std::vector<Plume> &plumes, int32_t step);

} // namespace terragen::past
