// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/framing.h"
#include <algorithm>
#include <cmath>

namespace planetarium::framing {

namespace {

double dot(const Vec &a, const Vec &b);
Vec cross(const Vec &a, const Vec &b);
Vec unit(const Vec &v);
Vec mix(const Vec &a, double p, const Vec &b, double q);
// a way square to the star's, and a third square to both: what `around` turns between
std::pair<Vec, Vec> across(const Vec &sun);

} // namespace

Framing of(const Shot &shot, float width, float height)
{
	const float aspect = width / std::max(height, 1.f);
	// The star's way in the camera's own space once it's turned to draw the
	// body's centre at shot.centre; turned back (the transpose), its way
	// from the body's middle as compose places it
	const float fov = OrbitCamera::fovFor(width, height);
	const core::matrix4 aim = OrbitCamera::aimed(shot.centre, aspect, fov);
	core::vector3df star = OrbitCamera::through(shot.star, aspect, fov), base;
	core::matrix4 back;
	aim.getTransposed(back);
	base = back.rotateAndScaleVect(star);
	base.normalize();
	return Framing{ shot.altitude, shot.centre, std::acos(std::clamp((double)base.Z, -1.0, 1.0)),
		std::atan2((double)base.Y, (double)base.X), shot.around, shot.sky };
}

Place place(const Framing &framing, const Vec &sun)
{
	const auto [side, other] = across(sun);
	const double off = framing.off, dx = std::cos(framing.roll), dy = std::sin(framing.roll);
	const Vec ahead = mix(sun, std::cos(off), mix(side, std::cos(framing.around), other, std::sin(framing.around)), std::sin(off));
	// the screen's up, so the star's on the way (dx, dy) from the centre
	const Vec toward = unit(mix(sun, 1, ahead, -dot(sun, ahead)));
	return Place{ { -ahead.x, -ahead.y, -ahead.z }, mix(toward, dy, cross(ahead, toward), dx) };
}

Framing read(const OrbitCamera &camera, const Vec &sun, double sea, v2f centre, float sky)
{
	const auto [side, other] = across(sun);
	const Vec ahead{ -camera.up().x, -camera.up().y, -camera.up().z };
	const double off = std::acos(std::clamp(dot(ahead, sun), -1.0, 1.0));
	const Vec toward = unit(mix(sun, 1, ahead, -std::cos(off)));
	const Vec &north = camera.north();
	return Framing{ camera.altitude() / sea, centre, off, std::atan2(dot(north, toward), dot(north, cross(ahead, toward))),
		std::atan2(dot(ahead, other), dot(ahead, side)), sky };
}

namespace {

double dot(const Vec &a, const Vec &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec cross(const Vec &a, const Vec &b)
{
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

Vec unit(const Vec &v)
{
	const double l = std::sqrt(dot(v, v));
	return { v.x / l, v.y / l, v.z / l };
}

Vec mix(const Vec &a, double p, const Vec &b, double q)
{
	return { a.x * p + b.x * q, a.y * p + b.y * q, a.z * p + b.z * q };
}

std::pair<Vec, Vec> across(const Vec &sun)
{
	const Vec side = unit(cross(sun, std::abs(sun.y) < 0.9 ? Vec{ 0, 1, 0 } : Vec{ 1, 0, 0 }));
	return { side, cross(sun, side) };
}

} // namespace

} // namespace planetarium::framing
