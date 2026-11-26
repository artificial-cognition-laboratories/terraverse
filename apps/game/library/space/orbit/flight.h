// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/system/planet.h"
#include <vector>

/*
	Flight on a planet: anything with mass moving under its gravity and
	through its air, pushed by thrust.

	Positions are planet-centred, in metres, in the frame that turns with the
	planet: a point on the ground stays put, a speed is over the ground, and
	the air (which turns with the ground) is still. +y is the north pole; at
	latitude φ, longitude λ the ground's normal is (cos φ sin λ, sin φ,
	cos φ cos λ), the same as the UI's globe. The turning frame adds its
	Coriolis and centrifugal pulls, so a launch eastward is helped by the
	spin, as on Earth.

	Near the ground, the game's blocks are a flat patch where a block is a
	metre (`near`): the same gravity and air, no curvature.
*/
namespace orbit {

struct Vec
{
	double x = 0, y = 0, z = 0;
	Vec operator+(const Vec &o) const { return { x + o.x, y + o.y, z + o.z }; }
	Vec operator-(const Vec &o) const { return { x - o.x, y - o.y, z - o.z }; }
	Vec operator*(double k) const { return { x * k, y * k, z * k }; }
	double dot(const Vec &o) const { return x * o.x + y * o.y + z * o.z; }
	Vec cross(const Vec &o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
	double length() const;
};

// What moves, as gravity and the air see it.
struct Craft
{
	double mass;      // kg
	double drag_area; // m²: drag coefficient × the area facing the air
};

struct State
{
	Vec pos, vel; // m, m/s (see above)
};

// The orbit a state is on, if nothing pushes it.
struct Elements
{
	double altitude, speed;   // m above the datum, m/s over the ground
	double orbital_speed;     // m/s, against the stars
	bool bound;               // it comes back around (below escape)
	double apoapsis;          // m above the datum (bound only)
	double periapsis;         // m above the datum; negative: the orbit meets the ground
	double period;            // s (bound only)
	double inclination;       // rad: the orbit's tilt to the equator
};

// A place and its motion over the ground, as someone standing there sees it.
struct Where
{
	double latitude, longitude, altitude; // rad (north, east), m above the datum
	Vec velocity;                         // m/s: east (x), up (y), north (z)
};

// The directions a burn is aimed by, unit vectors in the turning frame:
// along the motion against the stars, away from the planet (square to the
// motion), and square to the orbit's plane (north for an eastward orbit).
struct Directions
{
	Vec prograde, radial, normal;
};

// The ground's own directions at a place, unit vectors in the turning frame:
// what someone standing there (or a bubble of space riding over it) calls
// east, up and north. The game's x, y and z there.
struct Frame
{
	Vec east, up, north;
	Vec out(const Vec &local) const { return east * local.x + up * local.y + north * local.z; }
	Vec in(const Vec &v) const { return { v.dot(east), v.dot(up), v.dot(north) }; }
};

// A path flown without thrust (Flight::coast).
struct Arc
{
	std::vector<Vec> points; // in the frame of the stars at the start (the planet turns under it)
	double seconds;          // how long it flew
	enum class End { Ground, Around, Time } end; // came down, went all the way around, ran out of time
};

class Flight
{
public:
	explicit Flight(const terragen::Planet &planet);
	const terragen::Planet &planet() const { return m_planet; }

	double density(double altitude) const; // kg/m³

	// Move a state by dt seconds under gravity, the air's drag and `thrust` (N).
	State step(const State &s, const Craft &c, const Vec &thrust, double dt) const;
	// The same over the flat patch near the ground: positions are the game's
	// metres (x east, y up from the datum, z north), the planet's gravity and air.
	State near(const State &s, const Craft &c, const Vec &thrust, double dt) const;

	// What moves it at a place: gravity, the turning frame's pulls, the
	// air's drag and `thrust` (N), in m/s².
	Vec accel(const Vec &pos, const Vec &vel, const Craft &c, const Vec &thrust) const;

	Elements elements(const State &s) const;
	// The path it coasts on (the conic against the stars, gravity alone),
	// as points round it in the turning frame as it's turned now (m from
	// the centre): `points` round a bound orbit from where it is, closest
	// together near it; an
	// escape out to `reach` m from the centre. Where it meets the ground
	// it goes on through (whoever draws it hides that).
	std::vector<Vec> path(const State &s, int points, double reach) const;
	Directions directions(const State &s) const;
	// East, up and north over where a state is. Throws over a pole, where east isn't one direction.
	Frame frame(const State &s) const;
	// The orbit as it is now (no drag, no thrust): `points` around it (the
	// part within a few radii if it escapes), in the turning frame at this instant.
	std::vector<Vec> path(const State &s, int points) const;
	// Fly without thrust, sampled every `every` seconds, until it comes down,
	// goes all the way around, or `seconds` have passed.
	Arc coast(const State &s, const Craft &c, double seconds, double every) const;

	Where where(const State &s) const;
	State at(const Where &w) const;

private:
	Vec inertial(const State &s) const; // the velocity against the stars

	terragen::Planet m_planet;
	Vec m_spin; // rad/s, about +y
};

} // namespace orbit
