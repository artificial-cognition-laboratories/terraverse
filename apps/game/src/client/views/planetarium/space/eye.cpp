// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/space/eye.h"
#include <algorithm>
#include <cmath>

namespace planetarium {

namespace {

constexpr float DARKEST = 0.7f;   // how far it closes, the star itself in view: the band still shows
constexpr double PLANET = 7;      // a sunlit planet this share of the view, inverted, closes it all the way
constexpr double CLOSING = 0.25;  // seconds to close most of the way: a glance at the sun
constexpr double OPENING = 2.2;   // and to open again: eyes adapting to the dark
constexpr double EDGE = 0.35;     // the star still dazzles this far off the screen's edge (a share of it)

using V = Eye::Vec;

double brightness(const Eye::View &view);
double onScreen(const Eye::View &view, const V &at, double angular);

} // namespace

float Eye::look(const View &view)
{
	const float target = 1 - (1 - DARKEST) * (float)std::clamp(brightness(view), 0.0, 1.0);
	const auto now = std::chrono::steady_clock::now();
	if (!m_open) {
		m_open = target;
		m_last = now;
		return *m_open;
	}
	const double dt = std::min(std::chrono::duration<double>(now - m_last).count(), 0.25);
	m_last = now;
	const double tau = target < *m_open ? CLOSING : OPENING;
	*m_open += (target - *m_open) * (float)(1 - std::exp(-dt / tau));
	return *m_open;
}

namespace {

// How bright the view is, 0..1: the star in it is all of it; else the share
// of the screen a lit planet covers, by how much of it faces the star
double brightness(const Eye::View &view)
{
	double total = 0;
	for (const Eye::Thing &t : view.things) {
		const double distance = std::sqrt(t.at.x * t.at.x + t.at.y * t.at.y + t.at.z * t.at.z);
		if (distance <= t.radius)
			continue; // inside it: the ground's light isn't this
		const double angular = std::asin(t.radius / distance);
		const double shown = onScreen(view, t.at, angular);
		if (shown <= 0)
			continue;
		if (t.star) {
			total += shown;
			continue;
		}
		// its phase: 1 seen from the star's side, 0 from its night side
		const V toEye{ -t.at.x / distance, -t.at.y / distance, -t.at.z / distance };
		const V sun{ view.star.x - t.at.x, view.star.y - t.at.y, view.star.z - t.at.z };
		const double sunLength = std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
		const double phase = (1 + (toEye.x * sun.x + toEye.y * sun.y + toEye.z * sun.z) / sunLength) / 2;
		// the share of the view it covers (a disc on the screen, its height 2 tan(fov/2))
		const double half = std::tan(view.fov / 2);
		const double share = M_PI * std::pow(std::tan(std::min(angular, 1.4)) / half, 2) / (4 * view.aspect);
		total += std::min(share, 1.0) * shown * phase * PLANET;
	}
	return total;
}

// How much of something `angular` radians across is in the view: 1 on the
// screen, falling to 0 a little past its edge
double onScreen(const Eye::View &view, const V &at, double angular)
{
	v3f d((f32)at.x, (f32)at.y, (f32)at.z);
	d = view.view.rotateAndScaleVect(d);
	if (d.Z <= 0)
		return 0; // behind the camera
	const double f = 1 / std::tan(view.fov / 2);
	const double x = std::abs(d.X / d.Z * f / view.aspect), y = std::abs(d.Y / d.Z * f);
	const double reach = std::tan(angular) * f; // its radius, in the screen's half-heights
	const double out = std::max(x - 1, y - 1) - reach;
	return 1 - std::clamp(out / EDGE, 0.0, 1.0);
}

} // namespace

} // namespace planetarium
