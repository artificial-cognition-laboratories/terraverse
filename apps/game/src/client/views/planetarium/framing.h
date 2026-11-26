// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/planetarium/camera.h"
#include "src/client/views/planetarium/run/shot.h"

/*
	A composed view as a camera's place, and back: pure. The camera's only
	put somewhere and turned (OrbitCamera::aimed for where the body's centre
	is drawn); nothing about the lens changes, so the sky behind is the same
	sky whatever the framing.
*/
namespace planetarium::framing {

using Vec = OrbitCamera::Vec;

// A shot as its framing on a screen this many pixels (its lens: fovFor):
// the star's place on screen read as its way from the camera, through the aimed view.
Framing of(const Shot &shot, float width, float height);

// The camera's way out from the body (its up) and the screen's up (its north)
// for a framing, the star's way from the body `sun` (unit, the camera's frame).
struct Place
{
	Vec up, north;
};
Place place(const Framing &framing, const Vec &sun);

// The framing a camera's at (place's inverse); `centre` and `sky` as given.
Framing read(const OrbitCamera &camera, const Vec &sun, double sea, v2f centre, float sky);

} // namespace planetarium::framing
