// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "solar.h"
#include "sky.h"
#include <cmath>

namespace solar {

static Tint s_tint;
void tint(const Tint &t) { s_tint = t; }
const Tint &tint() { return s_tint; }

static float radians(float degrees)
{
	return degrees * (float)M_PI / 180.0f;
}

float localTime(const SkySolar &s, u32 day, float time_of_day)
{
	const double t = s.at + (double)s.turns * ((double)day + time_of_day);
	return (float)(t - std::floor(t));
}

float elevation(const SkySolar &s, float time_of_day)
{
	float lat = radians(s.latitude), decl = radians(s.declination);
	float hour = 2.0f * (float)M_PI * (time_of_day - 0.5f);
	float up = std::sin(lat) * std::sin(decl) + std::cos(lat) * std::cos(decl) * std::cos(hour);
	return std::asin(std::fmax(-1.0f, std::fmin(1.0f, up))) * 180.0f / (float)M_PI;
}

static std::optional<Second> g_second;

void second(std::optional<Second> s)
{
	g_second = s;
}

float apparentTime(const SkySolar &s, u32 day, float time_of_day)
{
	const float t = localTime(s, day, time_of_day);
	float e = elevation(s, t);
	if (g_second)
		e = std::fmax(std::fmin(e, g_second->first_cap), std::fmin(g_second->elevation, g_second->cap));
	// the stock sun stands at (wicked − 0.25) · 360° in the morning
	float wicked = t < 0.5f ? 0.25f + e / 360.0f : 0.75f - e / 360.0f;
	return getTimeOfDayFromWicked(wicked);
}

} // namespace solar
