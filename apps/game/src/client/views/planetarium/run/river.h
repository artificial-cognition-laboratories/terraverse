// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include <utility>

namespace planetarium {

// A place worth looking at on a world, for the runs that take pictures or
// time a camera over it (the ladder, the probe): over the widest river on
// land, within a radian of the noon longitude `sun` (radians), out of the
// polar caps, on `date` (0..1 through the year). Latitude and longitude,
// radians. Throws if there's no river on land.
std::pair<double, double> riverToShow(const terragen::WorldCode &code, double date, double sun);

} // namespace planetarium
