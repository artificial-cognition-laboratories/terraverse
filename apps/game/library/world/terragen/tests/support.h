// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

// What several of terragen's test files share: the codes they test on, and
// helpers that read a world.
#pragma once

#include "tests/library/check.h"
#include "library/world/terragen/src/system/body.h"
#include "library/world/terragen/src/system/calendar.h"
#include "library/world/terragen/src/grid/climate.h"
#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/grid/erosion.h"
#include "library/world/terragen/src/base/fixed.h"
#include "library/world/terragen/src/grid/geology.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/base/parallel.h"
#include "library/world/terragen/src/out/preview.h"
#include "library/world/terragen/src/base/sphere.h"
#include "library/world/terragen/src/system/sun.h"
#include "library/world/terragen/src/system/system.h"
#include "library/world/terragen/src/world.h"
#include "library/world/terragen/src/code/worlds.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

using namespace terragen;

// A sample's place, picked as the flat map counted blocks: x east from
// longitude 0, z north from the equator, `size` round and size / 2 pole to pole.
inline Place flat(const World &w, int32_t x, int32_t z)
{
	const int32_t size = w.code().size, half = size / 2;
	return World::place(ratio((((x + half) % size) + size) % size, size), ratio(half - z, size));
}

// Codes for the water tests: default, a supercontinent, an archipelago (as the page encodes them).
inline const char *const WATER_CODES[] = {
	"TF24.dGVycmE",
	"TF24.dGVycmE.AgKAqoCAAACmWZmAgICAgICCVQ",
	"TF24.dGVycmE.AgKAqoCAAP9A2U2ApoCAgICCVQ",
};

inline bool near(fix a, fix b, fix tolerance)
{
	return fabsx(a - b) <= tolerance;
}

inline fix real(double v)
{
	return (fix)(v * ONE);
}

// Stars by class: the Sun's exact, heavy ones short-lived and fierce, poor ones poor in metal.
// A system as the page writes it (config.ts bodyBytes), on a code of default settings
inline std::string codeWith(const SystemSpec &s)
{
	std::vector<uint8_t> b{ (uint8_t)s.bodies.size() };
	const auto text = [&](const std::string &t) { b.push_back((uint8_t)t.size()); b.insert(b.end(), t.begin(), t.end()); };
	const auto logged = [&](fix v) {
		const int16_t w = (int16_t)std::lround(std::log10(v / 65536.0) * 8000);
		b.push_back((uint8_t)(w & 255));
		b.push_back((uint8_t)((uint16_t)w >> 8));
	};
	for (const SystemBody &o : s.bodies) {
		text(o.id);
		text(o.name);
		b.insert(b.end(), { 128, 128, 128 });
		b.push_back(o.causes.parent < 0 ? 255 : (uint8_t)o.causes.parent);
		b.push_back((o.causes.life ? 1 : 0) | (o.playable ? 2 : 0) | (o.land ? 4 : 0) | (uint8_t)o.causes.spin << 3);
		logged(o.causes.orbit);
		logged(o.causes.mass);
		logged(o.causes.day);
		for (const fix v : { o.causes.eccentricity, o.causes.formed, o.causes.volatiles })
			b.push_back((uint8_t)std::lround(v * 255.0 / ONE));
		b.push_back((uint8_t)std::lround(o.phase * 255));
		b.push_back((uint8_t)(int8_t)o.inclination);
		b.push_back((uint8_t)std::lround(o.tilt * 2));
		if (o.land)
			b.insert(b.end(), o.land->begin(), o.land->end());
	}
	static const char ABC[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string out;
	uint32_t acc = 0;
	int bits = 0;
	for (const uint8_t v : b) {
		acc = acc << 8 | v;
		for (bits += 8; bits >= 6; bits -= 6)
			out.push_back(ABC[(acc >> (bits - 6)) & 63]);
	}
	if (bits > 0)
		out.push_back(ABC[(acc << (6 - bits)) & 63]);
	return "TF24.dGVycmE.AAKAqoCAAG1mgICAgICAgICCVQ." + out;
}

// the mean of a world's year at a place, °C
inline fix yearly(const World &w, const Place &at)
{
	fix sum = 0;
	for (int k = 0; k < 4; k++)
		sum += w.temperature(at, ratio(k, 4), ratio(3, 8)) / 4;
	return sum;
}

// Land drawn on the grid's cells by latitude (degrees) and column x (0..128
// eastward round the world), and a field's mean over a patch the same way.
template <typename Height>
inline std::vector<fix> drawn(Height height)
{
	const Sphere &s = gridSphere();
	std::vector<fix> e(s.size());
	for (int32_t i = 0; i < s.size(); i++) {
		const Dir &d = s.centre(i);
		e[i] = height(latitudeOf(d) * 180.0 / PI, World::u(d) * 128.0 / ONE);
	}
	return e;
}

template <typename Where>
inline double meanOver(const std::vector<fix> &f, Where where)
{
	const Sphere &s = gridSphere();
	double sum = 0;
	int n = 0;
	for (int32_t i = 0; i < s.size(); i++) {
		const Dir &d = s.centre(i);
		if (where(latitudeOf(d) * 180.0 / PI, World::u(d) * 128.0 / ONE)) {
			sum += f[i] / 65536.0;
			n++;
		}
	}
	return n ? sum / n : 0;
}
