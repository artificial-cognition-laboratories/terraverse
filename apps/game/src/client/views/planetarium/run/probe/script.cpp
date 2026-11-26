// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/run/probe/script.h"
#include "src/shared/base/exceptions.h"
#include <cmath>
#include <map>

namespace planetarium::probe {

namespace {
double ease(double s); // smoothstep, 0..1 to 0..1
}

// The scripts, by name. Altitudes in radii: 2.67 is the menu's Play
// (shot.h: PLAY_ALTITUDE); 0.02 a region a few kilometres across on a
// 100k-radius world, where every tile on screen is its finest.
static const std::map<std::string, std::vector<Step>> SCRIPTS = {
	{ "zoom", {
		{ "open", 2.67, 2.67, 0, false, 0, 180 },
		{ "to the place", 2.67, 2.67, 0, true, 2, 60 },
		{ "zoom in", 2.67, 0.02, 0, false, 4, 60 },
		{ "pan", 0.02, 0.02, 0.05, false, 3, 60 },
		{ "zoom out", 0.02, 2.67, 0, false, 4, 60 },
		{ "zoom in again", 2.67, 0.02, 0, false, 4, 60 },
	} },
	{ "open", {
		{ "open", 2.67, 2.67, 0, false, 0, 180 },
	} },
};

std::vector<Step> script(const std::string &name)
{
	auto it = SCRIPTS.find(name);
	if (it == SCRIPTS.end())
		throw BaseException("planetarium probe: no script \"" + name + "\" (there are " + scripts() + ")");
	return it->second;
}

std::string scripts()
{
	std::string out;
	for (const auto &[name, steps] : SCRIPTS)
		out += (out.empty() ? "" : ", ") + name;
	return out;
}

Script::Script(Opts opts) : m_opts(std::move(opts)) {}

Script::At Script::at(size_t step, double t) const
{
	// where each step starts: the last's end (its turn, its trip)
	double latitude = m_opts.latitude, longitude = m_opts.longitude;
	for (size_t k = 0; k < step; k++) {
		const Step &done = m_opts.steps[k];
		if (done.travel && m_place)
			std::tie(latitude, longitude) = *m_place;
		longitude += done.turn;
	}
	const Step &s = m_opts.steps[step];
	const double e = s.seconds > 0 ? ease(std::min(1.0, t / s.seconds)) : 1;
	if (s.travel && m_place) {
		latitude += (m_place->first - latitude) * e;
		longitude += std::remainder(m_place->second - longitude, 2 * M_PI) * e;
	}
	return { latitude, longitude + s.turn * e, s.from * std::pow(s.to / s.from, e) };
}

namespace {

double ease(double s)
{
	return s * s * (3 - 2 * s);
}

} // namespace

} // namespace planetarium::probe
