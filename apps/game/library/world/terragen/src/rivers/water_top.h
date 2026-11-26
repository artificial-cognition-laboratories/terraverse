// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

// Where a river's surface stands, to a sixteenth (planning/sprint-122-water.md):
// a river runs down its run's profile smoothly, its top node filled to the
// sixteenth its level reaches, not stepped a whole block at a time.
#pragma once

#include "library/world/terragen/src/base/fixed.h"
#include <cstdint>

namespace terragen {

struct WaterTop
{
	int32_t node; // the node the surface lies in (blocks over the sea)
	int32_t part; // sixteenths of it under the surface: 1..16 (16: full)
};

// The water `level` (blocks over the sea, fixed) stands at: a whole level is
// that node full, as a level of whole blocks always was; one between whole
// blocks fills the node over it to the nearest sixteenth.
inline WaterTop waterTop(int64_t level)
{
	const int64_t top = ((level * 16 + HALF) >> FRAC) + 16; // sixteenths: the top of node 0 is 16
	const int32_t node = (int32_t)((top - 1) >> 4);
	return { node, (int32_t)(top - (int64_t)node * 16) };
}

} // namespace terragen
