// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "skyparams.h"
#include <optional>

/*
	The real sun in the client's sky. Visual only, so floats: the same
	formulas as terragen/sun.cpp, which decides gameplay. time_of_day: 0..1,
	0.5 = noon.
*/
namespace solar {

// The local time of day (any number of turns): the body's clock against the engine's.
float localTime(const SkySolar &s, float time_of_day);

// A second star's effect on the light (visual only; the sky sets it each frame,
// where it knows the star). A star only lights the world up to `cap` degrees' worth
// of the stock sun: a faint one is a dawn, not a day.
struct Second
{
	float elevation; // the second's, degrees
	float cap, first_cap;
};
void second(std::optional<Second> s);

// What the air does to daylight's colour (visual only; the sky sets it each frame).
struct Tint
{
	float r = 1, g = 1, b = 1;
};
void tint(const Tint &t);
const Tint &tint();

// The sun's height above the horizon, degrees.
float elevation(const SkySolar &s, float time_of_day);

// The stock time of day at which the stock sky's sun stands as high as the
// real sun does now, on the same side of noon. Everything that colours and
// lights the world is fed this, so the stock look follows the real sun: polar
// night stays dark, the midnight sun stays light.
float apparentTime(const SkySolar &s, float time_of_day);

} // namespace solar
