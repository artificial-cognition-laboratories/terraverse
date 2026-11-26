// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <SColor.h>
#include <memory>
#include <vector>

namespace planetarium::glare {

constexpr u32 SIZE = 512;         // the star's glare, pixels a side
constexpr double CORE = 0.035;    // its blown-out middle, a share of its reach

// The glare of a star of this colour: SIZE² A8R8G8B8 pixels. Painted once a
// colour for the whole run (tens of ms: every new view's first frame
// hitched on it, Play and Back in the menu), then shared.
std::shared_ptr<const std::vector<u32>> pixels(video::SColor colour);

} // namespace planetarium::glare
