// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/base/fixed.h"
#include <vector>

namespace terragen {

struct Grid;

// How fertile each cell's ground is (0..1), on the finished land
std::vector<fix> fertility(const WorldCode &code, const Grid &g);

} // namespace terragen
