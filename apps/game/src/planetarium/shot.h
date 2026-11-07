// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <irrlichttypes_bloated.h>

namespace planetarium {

/*
	A composed view of a body and its star: where they stand on screen, not
	where the camera is. The camera's worked out from the star's direction
	(Planetarium::compose), so the picture holds on any window shape. It's
	a moment too: the clock stands at `date`.
*/
struct Shot
{
	const char *code; // the world
	double date;      // 0..1 through the year
	double altitude;  // over the sea, in the body's radii
	v2f centre, star; // on screen: −1..1, +x right, +y up
	double around;    // radians: which side of the body, among those that frame it so
	float sky;        // how bright the galaxy behind it, 0..1: enough to set the title against
	float fov;        // radians, vertical. Long: framed this far off-centre, a wide lens stretches the sky
};

// The main menu's. Tuned by eye: `luanti --planetarium-shot <png>` renders it.
inline constexpr Shot HOME_SHOT{ "TF18.dGVycmE", 0.25, 1.13, { -0.55f, -0.75f }, { 0.62f, 0.55f }, 0, 0.7f, 0.58f };

} // namespace planetarium
