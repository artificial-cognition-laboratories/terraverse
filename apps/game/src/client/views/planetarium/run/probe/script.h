// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace planetarium::probe {

/*
	What the probe does with the camera, step by step: a move (an altitude
	eased from one to another, geometrically, as a wheel feels; a turn in
	longitude; a trip to the place worth looking at), then a wait for the
	land to settle. Every step's timed apart (record.h).
*/
struct Step
{
	std::string name;
	double from, to;       // altitude, in the body's radii
	double turn = 0;       // radians of longitude, east, over the move
	bool travel = false;   // over the move, from where the camera is to the place (Script::place)
	double seconds = 0;    // the move (0: none, only the wait)
	double settle = 60;    // the longest the wait may be (seconds); past it the step's marked unsettled
};

// The steps a script's name gives; throws for one there isn't.
//   zoom:  open the world, go to the place, zoom in, pan, zoom out, zoom in again
//   open:  only open the world, till it settles (what a new world costs)
std::vector<Step> script(const std::string &name);
// The scripts there are, for the option's help
std::string scripts();

// Where the camera is on a step `t` seconds into its move, the place it's
// over for the steps from the second (until then: where it opened).
class Script
{
public:
	struct Opts
	{
		std::vector<Step> steps;
		double latitude, longitude; // where it opens, radians
	};
	explicit Script(Opts opts);

	struct At
	{
		double latitude, longitude, radii;
	};
	At at(size_t step, double t) const;
	// the place the travelling step goes to (found once the world's open: finding it makes the world)
	void place(double latitude, double longitude) { m_place = { latitude, longitude }; }
	const std::vector<Step> &steps() const { return m_opts.steps; }

private:
	Opts m_opts;
	std::optional<std::pair<double, double>> m_place;
};

} // namespace planetarium::probe
