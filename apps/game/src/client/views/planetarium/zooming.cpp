// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/zooming.h"
#include <cmath>

namespace planetarium {

Zooming::Motion Zooming::at(double altitude, double dt)
{
	const double was = m_altitude;
	m_altitude = altitude;
	if (was > 0 && altitude > 0 && dt > 0) {
		const double rate = std::log(altitude / was) / dt; // below 0: coming down
		m_slow = std::abs(rate) > RATE ? 0 : m_slow + dt;
		m_still = rate < -DOWN ? 0 : m_still + dt;
	}
	return { m_slow < SETTLE, m_still < SETTLE };
}

} // namespace planetarium
