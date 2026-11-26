// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/game/cut/record.h"
#include <vector>

/*
	Snapping (sprint 123): a member being placed follows the cursor, and is
	nudged, along each axis but the face's it's laid on, to line up with what's
	built near it: its ends or sides flush with another's, butted against one,
	or centred on one. Only within NEAR; past that it stays where it's aimed.
*/
namespace snap {

constexpr s32 NEAR = 4; // sixteenths: how far a member's nudged to line up

// `box` (world sixteenths) nudged to line up with `members`, along every axis but `normal`.
peel::Box align(const peel::Box &box, const std::vector<record::Member> &members, int normal);

// `box` moved out along `face` (toward them) until it meets none of `members`:
// a board laid across two posts it's longer than the gap between rests on their faces.
peel::Box clear(const peel::Box &box, const std::vector<record::Member> &members, v3s16 face);

} // namespace snap
