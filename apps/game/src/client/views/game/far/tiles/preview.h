// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/land/terrain.h"
#include "src/client/views/game/far/tiles/tiling.h"
#include <functional>
#include <utility>
#include <vector>

namespace far {

/*
	What to build while the eye's passing through (Eye::hurried): not the
	tiles it wants (gone in a moment, thousands of them zooming from orbit to
	the ground), and not nothing (stopped, it found no tile near enough to
	stand in, and showed space till its own were built), but each one's
	ancestor PREVIEW levels up: a few dozen, there to stand in when it
	stops, then refined level by level.
*/
namespace preview {

constexpr int LEVELS = 3; // an eighth of the detail: a sixty-fourth of the tiles

// `want`: what's wanted, by priority (lower first); the roots' (below 0) are kept as they are.
// Each other one's ancestor LEVELS up instead (no higher than its region's
// roots: Tiling::top), at the best priority of those it stands for; none already `ready`.
std::vector<std::pair<float, TileKey>> of(const std::vector<std::pair<float, TileKey>> &want, const Tiling &tiling,
		const std::function<bool(const TileKey &)> &ready);

} // namespace preview

} // namespace far
