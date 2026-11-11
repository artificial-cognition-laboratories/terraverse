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
};

/*
	A shot as the camera takes it: what eases from one to the next (each a
	number, so any mix of two is a shot too, the camera rebuilt from it each
	frame: no blend of two cameras to twist). Round the tiled body, whichever
	it is: the star's way, not its place on screen, so a shot holds on any.
*/
struct Framing
{
	double altitude; // over the sea, in the body's radii
	v2f centre;      // where the body's centre is drawn: −1..1, +x right, +y up
	double off;      // radians between the way to the body and the way to the star: π − the phase
	double roll;     // radians: the way across the screen the star lies from the body's centre, +x 0, +y π/2
	double around;   // as Shot's
	float sky;       // as Shot's
};

// The menu's Play: the selected world in full view, three quarters of it lit
// (a phase of 60°), the star out of frame behind the camera. The roll and
// around are the home shot's, so going there from it is a turn and a swing
// east round the body, nothing else.
inline constexpr double PLAY_ALTITUDE = 2.2, PLAY_OFF = 2.0944; // radii; 120°

// The main menu's. Tuned by eye: `luanti --planetarium-shot <png>` renders it.
// Through the orbit view's own lens: a long one magnified the sky's picture
// past its texels and flattened the depth. Far enough out that the star, in
// the corner, lights a crescent of the near side; turned so the dusty rim
// crosses behind, not the galaxy's core.
inline constexpr Shot HOME_SHOT{ "TF22.dGVycmE", 0.25, 2.0, { -0.7f, -0.5f }, { 0.88f, 0.82f }, 1.9, 0.5f };

} // namespace planetarium
