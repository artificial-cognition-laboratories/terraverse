// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/game/cut/peel.h"
#include <optional>
#include <vector>

/*
	Stacking (sprint 123): members of one kind laid flat in a neat pile, to keep
	and carry. A stack stands on a footprint of whole blocks (one across, as many
	along as a member's long), filled a layer at a time, each layer in rows, and
	grows up. Taken from the top. Stacked members are placed members (record.h):
	the client picks the slot, the server places it as any other.
*/
namespace stack {

// Where the next member `dims` big (world sixteenths, x y z) goes in the stack whose
// footprint's lowest corner is `base`, round the members already there (`taken`).
peel::Box next(v3s32 base, v3s32 dims, const std::vector<peel::Box> &taken);
// The base of the stack `member` is in: its footprint's block corner, at the bottom
// of the run of `same` (members its size) it rests on.
v3s32 base(const peel::Box &member, const std::vector<peel::Box> &same);
// One of `same` lying on `member` (directly on top, overlapping it), the last if several.
std::optional<peel::Box> above(const peel::Box &member, const std::vector<peel::Box> &same);

} // namespace stack
