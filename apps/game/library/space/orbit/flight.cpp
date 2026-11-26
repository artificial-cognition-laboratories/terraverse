// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/space/orbit/flight.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace orbit {

static const double PI = 3.14159265358979323846;
static const double THICK = 1e-7;    // kg/m³: air worth stepping finely through
static const double FINE = 0.02;     // s: the longest step in the air
static const double COARSE = 1.0;    // s: above it
static const double ESCAPED = 4;     // radii: how far an escaping path is drawn

double Vec::length() const
{
	return std::sqrt(x * x + y * y + z * z);
}

Flight::Flight(const terragen::Planet &planet) : m_planet(planet), m_spin{ 0, planet.spin(), 0 }
{
	if (planet.radius <= 0 || planet.gravity <= 0 || planet.day <= 0 || planet.density < 0 || planet.scale_height <= 0)
		throw std::invalid_argument("flight needs a planet with a size, a pull, a spin and an air (or none)");
}

double Flight::density(double altitude) const
{
	return m_planet.density * std::exp(-std::max(altitude, 0.0) / m_planet.scale_height);
}

Vec Flight::accel(const Vec &pos, const Vec &vel, const Craft &c, const Vec &thrust) const
{
	double r = pos.length();
	Vec gravity = pos * (-m_planet.mu() / (r * r * r));
	Vec turning = m_spin.cross(vel) * -2 - m_spin.cross(m_spin.cross(pos)); // Coriolis, centrifugal
	Vec drag = vel * (-0.5 * density(r - m_planet.radius) * vel.length() * c.drag_area);
	return gravity + turning + (drag + thrust) * (1 / c.mass);
}

// one classic Runge–Kutta step of a state whose acceleration is `a(pos, vel)`
template <typename A>
static State rk4(const State &s, double dt, const A &a)
{
	Vec k1v = a(s.pos, s.vel), k1p = s.vel;
	Vec k2v = a(s.pos + k1p * (dt / 2), s.vel + k1v * (dt / 2)), k2p = s.vel + k1v * (dt / 2);
	Vec k3v = a(s.pos + k2p * (dt / 2), s.vel + k2v * (dt / 2)), k3p = s.vel + k2v * (dt / 2);
	Vec k4v = a(s.pos + k3p * dt, s.vel + k3v * dt), k4p = s.vel + k3v * dt;
	return { s.pos + (k1p + k2p * 2 + k3p * 2 + k4p) * (dt / 6), s.vel + (k1v + k2v * 2 + k3v * 2 + k4v) * (dt / 6) };
}

State Flight::step(const State &s, const Craft &c, const Vec &thrust, double dt) const
{
	if (c.mass <= 0 || c.drag_area < 0 || dt < 0)
		throw std::invalid_argument("flight: a craft needs a mass, a drag area ≥ 0 and a step ≥ 0");
	State out = s;
	auto a = [&](const Vec &p, const Vec &v) { return accel(p, v, c, thrust); };
	while (dt > 0) {
		double longest = density(out.pos.length() - m_planet.radius) > THICK ? FINE : COARSE;
		double h = std::min(dt, longest);
		out = rk4(out, h, a);
		dt -= h;
	}
	return out;
}

State Flight::near(const State &s, const Craft &c, const Vec &thrust, double dt) const
{
	if (c.mass <= 0 || c.drag_area < 0 || dt < 0)
		throw std::invalid_argument("flight: a craft needs a mass, a drag area ≥ 0 and a step ≥ 0");
	auto a = [&](const Vec &p, const Vec &v) {
		double r = m_planet.radius + p.y;
		Vec drag = v * (-0.5 * density(p.y) * v.length() * c.drag_area);
		return Vec{ 0, -m_planet.mu() / (r * r), 0 } + (drag + thrust) * (1 / c.mass);
	};
	State out = s;
	while (dt > 0) {
		double h = std::min(dt, FINE);
		out = rk4(out, h, a);
		dt -= h;
	}
	return out;
}

Vec Flight::inertial(const State &s) const
{
	return s.vel + m_spin.cross(s.pos);
}

Elements Flight::elements(const State &s) const
{
	double mu = m_planet.mu(), R = m_planet.radius;
	Vec v = inertial(s);
	double r = s.pos.length(), speed = v.length();
	double energy = speed * speed / 2 - mu / r;
	Vec h = s.pos.cross(v);
	Vec e = v.cross(h) * (1 / mu) - s.pos * (1 / r);
	double ecc = e.length(), hl = h.length();
	Elements out{};
	out.altitude = r - R;
	out.speed = s.vel.length();
	out.orbital_speed = speed;
	out.bound = energy < 0;
	double p = hl * hl / mu; // the conic's semi-latus rectum
	out.periapsis = p / (1 + ecc) - R;
	out.inclination = hl > 0 ? std::acos(std::clamp(h.y / hl, -1.0, 1.0)) : 0;
	if (out.bound) {
		double a = -mu / (2 * energy);
		out.apoapsis = a * (1 + ecc) - R;
		out.period = 2 * PI * std::sqrt(a * a * a / mu);
	}
	return out;
}

std::vector<Vec> Flight::path(const State &s, int points, double reach) const
{
	const double mu = m_planet.mu();
	const Vec v = inertial(s);
	const double r = s.pos.length();
	const Vec h = s.pos.cross(v);
	const double hl = h.length();
	if (hl < 1e-9 || points < 2)
		return { s.pos }; // straight up or down: no conic to draw
	const Vec e = v.cross(h) * (1 / mu) - s.pos * (1 / r);
	const double ecc = e.length(), p = hl * hl / mu;
	// the orbit's plane: a way along it from the focus, and square to that
	// ahead (with the motion); from periapsis, or from here for a circle
	const Vec x = ecc > 1e-9 ? e * (1 / ecc) : s.pos * (1 / r);
	const Vec z = h * (1 / hl);
	const Vec y = z.cross(x);
	const double now = std::atan2(s.pos.dot(y), s.pos.dot(x));
	// how far round it goes: once round bound, else till it's `reach` out
	double sweep = 2 * PI;
	if (ecc >= 1) {
		const double out = std::acos(std::clamp((p / reach - 1) / ecc, -1.0, 1.0)); // the anomaly at `reach`
		sweep = std::max(out - now, 0.0);
	}
	std::vector<Vec> out;
	out.reserve(points);
	for (int n = 0; n < points; n++) {
		// closest together at the ends (where it is, and back round to it),
		// where it's looked at closely
		const double t = 0.5 - 0.5 * std::cos(PI * n / (points - 1));
		const double nu = now + sweep * t;
		const double rho = p / (1 + ecc * std::cos(nu));
		out.push_back((x * std::cos(nu) + y * std::sin(nu)) * rho);
	}
	return out;
}

Frame Flight::frame(const State &s) const
{
	Vec up = s.pos * (1 / s.pos.length());
	Vec east{ up.z, 0, -up.x }; // the spin's axis (+y) × up
	if (east.length() < 1e-9)
		throw std::invalid_argument("flight: over a pole, east isn't one direction");
	east = east * (1 / east.length());
	return { east, up, up.cross(east) };
}

Directions Flight::directions(const State &s) const
{
	Vec v = inertial(s);
	Vec up = s.pos * (1 / s.pos.length());
	if (v.length() < 1e-9) // still against the stars: straight up, then east
		return { up, up, Vec{ 0, 1, 0 } };
	Vec pro = v * (1 / v.length());
	Vec h = s.pos.cross(v);
	Vec normal = h.length() > 1e-9 ? h * (1 / h.length()) : Vec{ 0, 1, 0 };
	Vec radial = pro.cross(normal); // in the plane, away from the planet
	if (radial.dot(up) < 0)
		radial = radial * -1;
	return { pro, radial, normal };
}

std::vector<Vec> Flight::path(const State &s, int points) const
{
	if (points < 2)
		throw std::invalid_argument("flight: a path needs at least two points");
	double mu = m_planet.mu();
	Vec v = inertial(s);
	double r = s.pos.length();
	Vec h = s.pos.cross(v);
	double hl = h.length();
	if (hl <= 0) // straight up or down: the line it moves along
		return { s.pos, s.pos * (ESCAPED * m_planet.radius / r) };
	Vec e = v.cross(h) * (1 / mu) - s.pos * (1 / r);
	double ecc = e.length(), p = hl * hl / mu;
	// the orbit's plane: P toward periapsis (or where it is now, if circular), Q 90° on
	Vec P = ecc > 1e-9 ? e * (1 / ecc) : s.pos * (1 / r);
	Vec Q = h.cross(P) * (1 / hl);
	// a closed orbit all the way round; an open one as far as ESCAPED radii
	double reach = PI;
	if (ecc >= 1) {
		double far = ESCAPED * m_planet.radius;
		reach = std::acos(std::clamp((p / far - 1) / ecc, -1.0, 1.0));
	}
	std::vector<Vec> out;
	out.reserve(points);
	for (int i = 0; i < points; i++) {
		double th = -reach + 2 * reach * i / (points - 1);
		double rr = p / (1 + ecc * std::cos(th));
		out.push_back((P * std::cos(th) + Q * std::sin(th)) * rr);
	}
	return out;
}

Arc Flight::coast(const State &s, const Craft &c, double seconds, double every) const
{
	if (seconds <= 0 || every <= 0)
		throw std::invalid_argument("flight: coast needs a time and a sampling interval");
	Arc arc{ {}, 0, Arc::End::Time };
	State now = s;
	double swept = 0, t = 0, R = m_planet.radius;
	// the stars' frame at the start: the turning frame, turned back by the spin since
	auto fixed = [&](const Vec &p, double at) {
		double a = m_planet.spin() * at, ca = std::cos(a), sa = std::sin(a);
		return Vec{ ca * p.x + sa * p.z, p.y, -sa * p.x + ca * p.z };
	};
	Vec last = fixed(now.pos, 0);
	arc.points.push_back(last);
	while (t < seconds) {
		now = step(now, c, {}, std::min(every, seconds - t));
		t = std::min(seconds, t + every);
		Vec here = fixed(now.pos, t);
		swept += std::acos(std::clamp(here.dot(last) / (here.length() * last.length()), -1.0, 1.0));
		last = here;
		if (now.pos.length() <= R) { // down: where it met the datum
			arc.points.push_back(here * (R / here.length()));
			arc.end = Arc::End::Ground;
			break;
		}
		arc.points.push_back(here);
		if (swept >= 2 * PI) {
			arc.end = Arc::End::Around;
			break;
		}
	}
	arc.seconds = t;
	return arc;
}

Where Flight::where(const State &s) const
{
	double r = s.pos.length();
	Vec up = s.pos * (1 / r);
	double lat = std::asin(std::clamp(up.y, -1.0, 1.0)), lon = std::atan2(up.x, up.z);
	Vec east{ std::cos(lon), 0, -std::sin(lon) };
	Vec north = up.cross(east);
	return { lat, lon, r - m_planet.radius, { s.vel.dot(east), s.vel.dot(up), s.vel.dot(north) } };
}

State Flight::at(const Where &w) const
{
	double cl = std::cos(w.latitude), sl = std::sin(w.latitude);
	Vec up{ cl * std::sin(w.longitude), sl, cl * std::cos(w.longitude) };
	Vec east{ std::cos(w.longitude), 0, -std::sin(w.longitude) };
	Vec north = up.cross(east);
	return { up * (m_planet.radius + w.altitude), east * w.velocity.x + up * w.velocity.y + north * w.velocity.z };
}

} // namespace orbit
