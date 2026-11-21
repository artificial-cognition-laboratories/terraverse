// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include <optional>
#include <vector>

namespace planetarium::hover {

// A body under the mouse: its index (none: the star), where it's drawn and
// how big (window pixels).
struct Hover
{
	std::optional<size_t> body;
	v2f at;
	float radius;
};

// A body as the camera sees it this frame: where it's drawn, how big, and how
// far off it is (none: behind the camera).
struct Seen
{
	v2f at;
	float shown;
	double distance;
};

struct Ask
{
	const std::vector<std::optional<Seen>> &seen; // every body, then the star last
	v2f mouse;            // window pixels
	size_t target;        // the body the camera's round: not offered while it fills the screen
	float height;         // the view's, pixels
	const std::optional<Hover> &was; // last frame's: kept a little past where it'd start
};

// What's under the mouse: a disc it's inside before a dot's reach round one,
// the nearest of those; what's nearest under it hides what's behind.
std::optional<Hover> under(const Ask &ask);

} // namespace planetarium::hover
