// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/fluid/fluid.h"
#include <algorithm>
#include <cmath>

/*
	Storms: the weather systems a coarse grid can't grow by itself, kept as a
	list instead. Lows and highs form along the jets' fronts and drift east
	with them, deepening for a couple of days and then filling. Tropical
	cyclones form over warm sea in the trades, drift west and poleward,
	strengthen while the sea under them is warm, and die over land. Their
	winds circle them (anticlockwise round a low in the north, clockwise in
	the south) and spiral in; air converging into a low rises and rains.
*/
namespace fluid {

static const double DAY = 86400;
static const int MAX_STORMS = 64;
static const double TROPICAL_SEA = 299; // K: warm enough water to feed a cyclone

// splitmix64: the next random number of the layer's stream
static uint64_t mix(uint64_t x)
{
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
	return x ^ (x >> 31);
}

void Layer::storm(double dt, double date)
{
	const LayerOpts &o = m_opts;
	const Circulation &c = m_circulation;
	uint64_t state = mix(o.seed ^ (m_steps * 0x2545F4914F6CDD1Dull));
	auto roll = [&]() { state = mix(state); return (state >> 11) * (1.0 / 9007199254740992.0); };
	double decl = std::asin(std::sin(o.planet.tilt) * std::sin(2 * M_PI * date));
	double itcz = 0.45 * decl;

	// fronts: where the first westerly band runs, in each hemisphere
	if (c.jets > 0) {
		for (double pole : { 1.0, -1.0 }) {
			if (roll() > dt / (1.5 * DAY) || (int)m_storms.size() >= MAX_STORMS)
				continue;
			double edge = pole > 0 ? std::min(M_PI / 2, itcz + c.hadley) : -std::max(-M_PI / 2, itcz - c.hadley);
			double band = (M_PI / 2 - edge) / (2 * c.jets);
			double lat = pole * (edge + band * (0.5 + 0.6 * (roll() - 0.5)));
			double depth = roll() < 0.6 ? -1600 : 1200; // more lows than highs
			m_storms.push_back({ StormKind::Front, roll() * 2 * M_PI, lat, 0.18 * o.planet.radius, depth, 0 });
		}
	}
	// tropical cyclones: a try over a random cell of warm sea, away from the equator
	if (roll() < dt / (3 * DAY) && (int)m_storms.size() < MAX_STORMS) {
		int32_t i = std::min((int32_t)(roll() * o.width * o.height), o.width * o.height - 1);
		double lat = latitude(i / o.width);
		double away = std::fabs(lat) * 180 / M_PI;
		if (o.ground[i].water && m_f.ts[i] > TROPICAL_SEA && away > 6 && away < 25)
			m_storms.push_back({ StormKind::Tropical, (i % o.width + 0.5) * 2 * M_PI / o.width, lat, 0.06 * o.planet.radius, -800, 0 });
	}

	// steer, deepen, fill
	for (Storm &s : m_storms) {
		Flow f = flow(c, s.lat, itcz);
		double cosl = std::max(std::cos(s.lat), 0.05);
		int32_t row = std::clamp((int32_t)((M_PI / 2 - s.lat) / M_PI * o.height), 0, o.height - 1);
		int32_t col = ((int32_t)(s.lon / (2 * M_PI) * o.width) % o.width + o.width) % o.width;
		int32_t i = row * o.width + col;
		double east, north;
		if (s.kind == StormKind::Front) {
			east = 1.5 * f.u + 2; // steered by the jet aloft, faster than the surface wind
			north = (s.lat > 0 ? 1 : -1) * 0.8;
			double grow = s.age < 2 * DAY ? 1 : -1;
			s.depth *= std::exp(grow * dt / (2 * DAY) * 0.6);
		} else {
			east = f.u;
			north = (s.lat > 0 ? 1 : -1) * 1.5;
			bool fed = o.ground[i].water && m_f.ts[i] > TROPICAL_SEA;
			s.depth = fed ? s.depth - 900 * dt / DAY : s.depth * std::exp(-dt / DAY);
			s.depth = std::max(s.depth, -9000.0);
		}
		s.lon = std::fmod(s.lon + east * dt / (o.planet.radius * cosl) + 2 * M_PI, 2 * M_PI);
		s.lat += north * dt / o.planet.radius;
		s.age += dt;
	}
	m_storms.erase(std::remove_if(m_storms.begin(), m_storms.end(), [](const Storm &s) {
		return std::fabs(s.depth) < 300 || s.age > 12 * DAY || std::fabs(s.lat) > M_PI / 2 - 0.1;
	}), m_storms.end());
}

void Layer::stormWind(int32_t cell, double &u, double &v, double &w, double &dp) const
{
	const LayerOpts &o = m_opts;
	double lat = latitude(cell / o.width);
	double lon = (cell % o.width + 0.5) * 2 * M_PI / o.width;
	double a = o.planet.radius;
	double omega = 2 * M_PI / o.planet.day;
	for (const Storm &s : m_storms) {
		double dlon = std::remainder(lon - s.lon, 2 * M_PI);
		double east = dlon * a * std::cos(0.5 * (lat + s.lat)), north = (lat - s.lat) * a;
		double d = std::hypot(east, north);
		if (d > 2.5 * s.radius)
			continue;
		double shape = std::exp(-(d / s.radius) * (d / s.radius));
		dp += s.depth * shape;
		if (d < 1)
			continue;
		// geostrophic balance: the pressure slope against the spin
		double f = 2 * omega * std::max(std::fabs(std::sin(s.lat)), std::sin(M_PI / 18));
		double speed = std::min(70.0, std::fabs(s.depth) / (1.2 * f * s.radius) * (d / s.radius) * shape * 2.3);
		double turn = (s.depth < 0 ? 1 : -1) * (s.lat > 0 ? 1 : -1); // +1: anticlockwise
		double inward = s.depth < 0 ? 0.3 : -0.3;                   // friction spirals air into a low
		u += speed * (-turn * north / d - inward * east / d);
		v += speed * (turn * east / d - inward * north / d);
		w += -s.depth / 1500 * 2.5 * m_circulation.lift * shape; // a low lifts its air hard enough to stand out from the bands
	}
}

} // namespace fluid
