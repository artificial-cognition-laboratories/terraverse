// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/camera.h"
#include "constants.h"
#include <algorithm>
#include <cmath>

namespace planetarium {

static constexpr double LOWEST = 100;  // blocks over the sea
static constexpr double TURN = 0.004;  // radians a pixel, from a body radius up
static constexpr double STEP = 0.85;   // a wheel step's share of the altitude
static constexpr float CELL = 6;       // pixels a tile's cell may span before it splits: finer cost frames (a ninth the cells of 2)

OrbitCamera::OrbitCamera(double sea, double farthest) : m_sea(sea), m_farthest(farthest), m_altitude(2.5 * sea)
{
	focus(0.4, 0.3);
}

void OrbitCamera::drag(double dx, double dy)
{
	const double k = TURN * std::clamp(m_altitude / m_sea, 0.002, 1.0);
	// the ground follows the hand: the camera goes the other way, across the
	// screen about its up, along it about its right
	const Vec u = m_up, n = m_north;
	const Vec east{ u.y * n.z - u.z * n.y, u.z * n.x - u.x * n.z, u.x * n.y - u.y * n.x };
	const double a = -dx * k, ca = std::cos(a), sa = std::sin(a);
	const Vec u1{ u.x * ca + east.x * sa, u.y * ca + east.y * sa, u.z * ca + east.z * sa };
	const double b = dy * k, cb = std::cos(b), sb = std::sin(b);
	m_up = { u1.x * cb + n.x * sb, u1.y * cb + n.y * sb, u1.z * cb + n.z * sb };
	m_north = { n.x * cb - u1.x * sb, n.y * cb - u1.y * sb, n.z * cb - u1.z * sb };
	// kept unit and square to each other as the rounding drifts
	const auto unit = [](Vec v) { const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); return Vec{ v.x / l, v.y / l, v.z / l }; };
	m_up = unit(m_up);
	const double d = m_north.x * m_up.x + m_north.y * m_up.y + m_north.z * m_up.z;
	m_north = unit({ m_north.x - d * m_up.x, m_north.y - d * m_up.y, m_north.z - d * m_up.z });
}

void OrbitCamera::zoom(double steps)
{
	altitude(m_altitude * std::pow(STEP, steps));
}

void OrbitCamera::focus(double latitude, double longitude, std::optional<double> altitude)
{
	const double lat = std::clamp(latitude, -1.55, 1.55), lon = longitude;
	m_up = { std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
	// north along the ground under it: the screen's up
	m_north = { -std::sin(lat) * std::cos(lon), std::cos(lat), -std::sin(lat) * std::sin(lon) };
	if (altitude)
		this->altitude(*altitude);
}

void OrbitCamera::altitude(double altitude)
{
	m_altitude = std::clamp(altitude, LOWEST, m_farthest);
}

void OrbitCamera::place(const Vec &up, const Vec &north, double altitude)
{
	m_up = up;
	m_north = north;
	this->altitude(altitude);
}

void OrbitCamera::resize(double sea, double farthest)
{
	const double radii = m_altitude / m_sea;
	m_sea = sea;
	m_farthest = farthest;
	altitude(radii * sea);
}

double OrbitCamera::latitude() const
{
	return std::asin(std::clamp(m_up.y, -1.0, 1.0));
}

double OrbitCamera::longitude() const
{
	return std::atan2(m_up.z, m_up.x);
}

OrbitCamera::Vec OrbitCamera::ray(double x, double y, double width, double height, v2f shift) const
{
	// the pixel as if the screen's middle were where the view's is
	x -= shift.X * width / 2;
	y += shift.Y * height / 2;
	// in the camera's own axes (left-handed: x right, y up, z ahead), then turned
	// into the planet's by the view's inverse
	const double t = std::tan(FOV / 2), aspect = width / std::max(1.0, height);
	core::vector3df d((float)((2 * x / width - 1) * t * aspect), (float)((1 - 2 * y / height) * t), 1);
	core::matrix4 inverse;
	view().getInverse(inverse);
	d = inverse.rotateAndScaleVect(d);
	d.normalize();
	return { d.X, d.Y, d.Z };
}

OrbitCamera::Vec OrbitCamera::position() const
{
	const Vec &u = m_up;
	const double d = m_sea + m_altitude;
	return { u.x * d, u.y * d, u.z * d };
}

far::Eye OrbitCamera::eyeAt(const Vec &p, double sea, float height)
{
	const double d = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	const Vec u{ p.x / d, p.y / d, p.z / d };
	const double h = std::max(d - sea, 0.0);
	// what it sees of the body: the cap within the horizon's angle, as the
	// chord from the point under it (far::Tiles measures so); a little
	// over, for the land standing above the sea
	const double horizon = std::acos(sea / (sea + h));
	const float range = (float)(2 * sea * std::sin(std::min(horizon * 1.15, M_PI) / 2));
	// a tile of span s, seen from d, splits while d < s · split: its cells
	// (s / TILE) are wider than CELL pixels at d
	const float focal = height / (2 * std::tan(FOV / 2));
	const float split = focal / (far::TILE * CELL);
	return { { u.x * sea, u.y * sea, u.z * sea }, (float)h, 0, range, split };
}

OrbitCamera::Vec OrbitCamera::turn(const Vec &p, double spin)
{
	const double c = std::cos(spin), s = std::sin(spin);
	return { p.x * c - p.z * s, p.y, p.x * s + p.z * c };
}

core::matrix4 OrbitCamera::turned(const core::matrix4 &view, double spin)
{
	// turn() as a matrix (x' = x·M0 + y·M4 + z·M8, and so on), then the view
	const f32 c = (f32)std::cos(spin), s = (f32)std::sin(spin);
	core::matrix4 r;
	r[0] = c;
	r[8] = -s;
	r[2] = s;
	r[10] = c;
	return view * r;
}

core::matrix4 OrbitCamera::view() const
{
	const Vec &u = m_up;
	const core::vector3df north((float)m_north.x, (float)m_north.y, (float)m_north.z);
	core::matrix4 m;
	m.buildCameraLookAtMatrixLH(core::vector3df(0), core::vector3df((float)-u.x, (float)-u.y, (float)-u.z), north);
	return m;
}

core::matrix4 OrbitCamera::perspective(float aspect, float near, float far, v2f shift, float fov)
{
	core::matrix4 m;
	m.buildProjectionMatrixPerspectiveFovLH(fov, aspect, near, far);
	// clip x and y gain shift · w (w is the depth): the drawing moves by shift
	m[8] += shift.X;
	m[9] += shift.Y;
	return m;
}

core::matrix4 OrbitCamera::projection(float aspect, v2f shift, float fov) const
{
	// near: well short of the highest ground under it; far: past the body's
	// back, and round the far side of an orbit drawn out to five radii
	const float near = (float)std::max(1.0, m_altitude * 0.2) * BS;
	const float far = (float)(m_altitude + 6.2 * m_sea) * BS;
	return perspective(aspect, near, far, shift, fov);
}

} // namespace planetarium
