// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/camera.h"
#include "src/shared/base/constants.h"
#include <algorithm>
#include <cmath>

namespace planetarium {

static constexpr double LOWEST = 100;  // blocks over the sea
static constexpr double TURN = 0.004;  // radians a pixel, from a body radius up
static constexpr double STEP = 0.85;   // a wheel step's share of the altitude

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

float OrbitCamera::fovFor(float width, float height)
{
	const float k = std::clamp((width - NARROW_BELOW) / (WIDE_FROM - NARROW_BELOW), 0.f, 1.f);
	const float across = NARROWEST + (WIDEST - NARROWEST) * k * k * (3 - 2 * k);
	const float aspect = width / std::max(height, 1.f);
	return std::min(TALLEST, 2 * std::atan(std::tan(across / 2) / std::max(aspect, 1e-3f)));
}

OrbitCamera::Vec OrbitCamera::ray(const core::matrix4 &view, double x, double y, double width, double height, float fov)
{
	const v2f at((f32)(2 * x / width - 1), (f32)(1 - 2 * y / height));
	core::vector3df d = through(at, (float)(width / std::max(1.0, height)), fov);
	// into the looked-at frame by the view's inverse (its turn's transpose)
	core::matrix4 inverse;
	view.getInverse(inverse);
	d = inverse.rotateAndScaleVect(d);
	d.normalize();
	return { d.X, d.Y, d.Z };
}

core::vector3df OrbitCamera::through(v2f at, float aspect, float fov)
{
	const float t = std::tan(fov / 2);
	core::vector3df d(at.X * t * aspect, at.Y * t, 1);
	d.normalize();
	return d;
}

core::matrix4 OrbitCamera::aimed(v2f at, float aspect, float fov)
{
	// Rodrigues: the turn about ahead × n taking ahead (z) to n
	const core::vector3df n = through(at, aspect, fov);
	const double c = n.Z; // cos of the angle
	const core::vector3df axis(-n.Y, n.X, 0); // z × n, its length the sine
	const double s = axis.getLength();
	core::matrix4 m;
	if (s < 1e-9)
		return m;
	const double kx = axis.X / s, ky = axis.Y / s, kz = 0, v = 1 - c;
	const double r[3][3] = {
		{ c + kx * kx * v, kx * ky * v - kz * s, kx * kz * v + ky * s },
		{ ky * kx * v + kz * s, c + ky * ky * v, ky * kz * v - kx * s },
		{ kz * kx * v - ky * s, kz * ky * v + kx * s, c + kz * kz * v },
	};
	// (transformVect: out_i = Σ_j in_j · M[j*4 + i])
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			m[j * 4 + i] = (f32)r[i][j];
	return m;
}

OrbitCamera::Vec OrbitCamera::position() const
{
	const Vec &u = m_up;
	const double d = m_sea + m_altitude;
	return { u.x * d, u.y * d, u.z * d };
}

far::Eye OrbitCamera::eyeAt(const Vec &p, double sea, float focal)
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
	// (s / TILE) are wider than `cell` pixels at d
	const float split = focal / (far::TILE * cell);
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

core::matrix4 OrbitCamera::perspective(float aspect, float near, float far, float fov)
{
	core::matrix4 m;
	m.buildProjectionMatrixPerspectiveFovLH(fov, aspect, near, far);
	return m;
}

core::matrix4 OrbitCamera::projection(float aspect, float fov) const
{
	// near: well short of the highest ground under it; far: past the body's
	// back, and round the far side of an orbit drawn out to five radii
	const float near = (float)std::max(1.0, m_altitude * 0.2) * BS;
	const float far = (float)(m_altitude + 6.2 * m_sea) * BS;
	return perspective(aspect, near, far, fov);
}

} // namespace planetarium
