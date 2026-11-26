// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/base/progress.h"
#include "library/world/terragen/src/grid/grid.h"
#include <functional>
#include <optional>
#include <vector>

/*
	The land worn (grid.cpp builds it): the ice ages' scour, then rounds of
	uplift against rivers and creep, with what they carry laid down where
	they slow. Each cell's setting (setting.h) says how it rises, sags and
	wears.
*/
namespace terragen::wear {

// Glacial scour: hollows ground into the land that's cold now, into the base
void scour(const WorldCode &code, Grid &g);
// The erosion, from the base to the eroded heights, with the sediment and
// placers it leaves (and its account: uplifted, to the deep); `step` is
// moved on through its own stages
void erode(const WorldCode &code, Grid &g, const std::vector<fix> &rain, std::optional<Step> &step,
		const std::function<void(const ErosionRound &)> *watch);

} // namespace terragen::wear
