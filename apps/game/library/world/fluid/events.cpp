// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/fluid/fluid.h"
#include <cmath>

/*
	Events are thresholds on the weather, as data: each is a few ranges that
	must all hold. Temperatures are measured from where the condensable
	melts, so a thunderstorm on Titan is one in methane.
*/
namespace fluid {

const char *const EVENT_NAMES[(int)Event::Count] = {
	"thunderstorm", "blizzard", "fog", "hurricane", "drought", "ashfall",
};

namespace {

enum class Probe : uint8_t { AboveMelt, Humidity, Precipitation, Snow, Wind, Ash, Dryness, None };

struct Range
{
	Probe probe;
	double min, max;
};

struct Rule
{
	Event event;
	Range all[3];
};

const double ANY = 1e30;

const Rule RULES[] = {
	{ Event::Thunderstorm, { { Probe::Precipitation, 3, ANY }, { Probe::AboveMelt, 15, ANY }, { Probe::None, 0, 0 } } },
	{ Event::Blizzard, { { Probe::Snow, 1, ANY }, { Probe::Precipitation, 0.5, ANY }, { Probe::Wind, 10, ANY } } },
	{ Event::Fog, { { Probe::Humidity, 0.97, ANY }, { Probe::Wind, 0, 3 }, { Probe::Precipitation, 0, 0.1 } } },
	{ Event::Hurricane, { { Probe::Wind, 30, ANY }, { Probe::AboveMelt, 20, ANY }, { Probe::Precipitation, 2, ANY } } },
	{ Event::Drought, { { Probe::Dryness, 0.7, ANY }, { Probe::None, 0, 0 }, { Probe::None, 0, 0 } } },
	{ Event::Ashfall, { { Probe::Ash, 0.3, ANY }, { Probe::None, 0, 0 }, { Probe::None, 0, 0 } } },
};

} // namespace

uint32_t Layer::events(const Sample &s, int32_t cell) const
{
	const matter::Substance &c = m_opts.air.condensable;
	double normal = m_f.normal[cell];
	double probes[] = {
		s.temperature - c.triple.t,
		s.humidity,
		s.precipitation,
		s.snow ? 1.0 : 0.0,
		std::hypot(s.wind_u, s.wind_v),
		s.ash,
		normal > 0.02 ? 1 - m_f.recent[cell] / normal : 0, // a wet place gone dry
	};
	uint32_t out = 0;
	for (const Rule &r : RULES) {
		bool holds = true;
		for (const Range &range : r.all)
			if (range.probe != Probe::None) {
				double v = probes[(int)range.probe];
				holds = holds && v >= range.min && v <= range.max;
			}
		if (holds)
			out |= 1u << (int)r.event;
	}
	return out;
}

} // namespace fluid
