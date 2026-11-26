// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace planetarium {

/*
	Whether the camera's zooming too fast for its tiles to be worth building
	(far::Eye::hurried): zoomed from orbit to the ground, every level on the
	way was built, thousands of tiles for the fifty it ended on (bun
	perf:planetarium). Fast is the altitude changing by more than RATE of
	itself a second; it stays fast till it's been slow for SETTLE, so a
	wheel's steps, a frame or two apart, are one zoom. And whether it's
	coming down: then the next level's built ahead of it.
*/
class Zooming
{
public:
	struct Motion
	{
		bool fast;       // too fast to build what's seen (far::Eye::hurried)
		bool descending; // coming down: the next level's wanted soon (far::Eye::ahead)
	};
	// a frame: the camera's altitude now (blocks), the seconds since the last
	Motion at(double altitude, double dt);

private:
	static constexpr double RATE = 1.0;    // ln(altitude) a second: a factor of e
	static constexpr double DOWN = 0.05;   // coming down at least this (ln a second) is descending
	static constexpr double SETTLE = 0.25; // seconds either holds after it stops
	double m_altitude = 0;
	double m_slow = SETTLE; // seconds it's been slow for
	double m_still = SETTLE; // seconds it hasn't come down for
};

} // namespace planetarium
