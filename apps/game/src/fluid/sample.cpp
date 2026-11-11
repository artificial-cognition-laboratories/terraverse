// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "fluid.h"
#include <algorithm>
#include <cmath>

// Reading the layer at a point.
namespace fluid {

static const double DAY_SWING = 5; // K either side of the daily mean, clear sky over land
static const double TROPOSPHERE = 11000; // m: how high the air cools with height

double Layer::lapse() const
{
	return 0.65 * m_opts.planet.gravity / m_cp;
}

double Layer::aloft(double t, double rise) const
{
	return t - lapse() * std::min(rise, TROPOSPHERE);
}

bool Layer::frozen(double t, double p) const
{
	const matter::Substance &c = m_opts.air.condensable;
	return t < c.meltingPoint(std::max(p, c.triple.p));
}

int32_t Layer::nearest(double u, double v) const
{
	const LayerOpts &o = m_opts;
	int32_t col = ((int32_t)std::floor(u * o.width) % o.width + o.width) % o.width;
	int32_t row = std::clamp((int32_t)std::floor(v * o.height), 0, o.height - 1);
	return row * o.width + col;
}

double Layer::climatology(double u, double v) const
{
	return m_f.normal[nearest(u, v)];
}

uint32_t Layer::events(const Sample &s, double u, double v) const
{
	return events(s, nearest(u, v));
}

double Layer::departure(double u, double v, double date) const
{
	const LayerOpts &o = m_opts;
	double x = u * o.width - 0.5, y = std::clamp(v * o.height - 0.5, 0.0, o.height - 1.0);
	int32_t x0 = (int32_t)std::floor(x), y0 = std::min((int32_t)y, o.height - 2);
	double fx = x - x0, fy = y - y0;
	auto at = [&](int32_t c, int32_t r) {
		int32_t i = r * o.width + ((c % o.width) + o.width) % o.width;
		return m_f.t[i] - radiative(i, date);
	};
	return (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) + (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
}

Sample Layer::sample(double u, double v, double height, double hour) const
{
	const LayerOpts &o = m_opts;
	double x = u * o.width - 0.5, y = std::clamp(v * o.height - 0.5, 0.0, o.height - 1.0);
	int32_t x0 = (int32_t)std::floor(x), y0 = std::min((int32_t)y, o.height - 2);
	double fx = x - x0, fy = y - y0;
	auto cell = [&](int32_t c, int32_t r) { return r * o.width + ((c % o.width) + o.width) % o.width; };
	auto at = [&](const std::vector<double> &f) {
		return (f[cell(x0, y0)] * (1 - fx) + f[cell(x0 + 1, y0)] * fx) * (1 - fy)
				+ (f[cell(x0, y0 + 1)] * (1 - fx) + f[cell(x0 + 1, y0 + 1)] * fx) * fy;
	};
	int32_t near = cell((int32_t)std::lround(x), (int32_t)std::lround(y));
	const Ground &g = o.ground[near];
	double ground = std::max(g.height, 0.0);

	Sample s;
	s.cloud = at(m_f.cloud);
	double swing = DAY_SWING * (1 - 0.6 * s.cloud) * (g.water ? 0.4 : 1);
	s.temperature = aloft(at(m_f.t), height - ground)
			+ swing * std::cos(2 * M_PI * (hour - 0.625)); // warmest at 15:00
	s.pressure = (o.air.pressure + at(m_f.pressure)) * std::exp(-height / m_scale);
	s.humidity = std::min(1.0, at(m_f.q) / saturation(s.temperature, s.pressure));
	s.precipitation = at(m_f.rain);
	s.snow = frozen(s.temperature, s.pressure);
	s.wind_u = at(m_f.u);
	s.wind_v = at(m_f.v);
	s.ash = at(m_f.ash);
	s.soil = g.water ? 1 : at(m_f.soil);
	s.events = events(s, near);
	return s;
}

} // namespace fluid
