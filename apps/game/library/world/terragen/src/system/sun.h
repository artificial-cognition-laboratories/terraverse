// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/base/fixed.h"

/*
	The solar model, in fixed point: the same formulas as
	ui/src/worldgen/sun.ts. date: 0..1 through the year, 0 = northern spring
	equinox. latitude: radians, positive north. hour: 0..1 through the day,
	0.5 = noon.
*/
namespace terragen {

// The latitude under the noon sun on `date`, for a tilt in degrees.
fix declination(fix tilt_degrees, fix date);

// The sun's height above the horizon, radians (negative below it).
fix elevation(fix latitude, fix declination, fix hour);

// The share of the day the sun is up: 0 polar night .. 1 midnight sun.
fix dayShare(fix latitude, fix declination); // (not "daylight": libc has one)

// Mean sunlight over a day, relative to the equator at equinox (= 1).
fix insolation(fix latitude, fix declination);

// Mean sunlight over the whole year at a latitude.
fix annualInsolation(fix latitude, fix tilt_degrees);

} // namespace terragen
