// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/system/planet.h"
#include <cmath>

namespace terragen {

Planet home()
{
	// the air: sea-level density, and the scale height of the lower air
	return { EARTH_RADIUS / 10, 9.81, 86400.0 / 10, 1361, 1.225, 7200 };
}

Planet sphere(double radius)
{
	const double g = 9.81, orbital = std::sqrt(g * radius);
	const double EQUATOR_SHARE = 465.1 / 7905.0; // Earth's equator against its low orbit
	const double day = 2 * 3.14159265358979323846 * radius / (EQUATOR_SHARE * orbital);
	return { radius, g, day, 1361, 1.225, radius / 10 };
}

Planet resized(const Planet &p, double radius)
{
	Planet r = p;
	r.radius = radius;
	r.day = p.day * (radius / p.radius);
	return r;
}

} // namespace terragen
