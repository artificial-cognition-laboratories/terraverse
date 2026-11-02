// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "preview.h"
#include <algorithm>

namespace terragen {

template <typename T>
static T quantize(fix v, int32_t scale, int32_t lo, int32_t hi)
{
	int64_t q = ((int64_t)v * scale) >> FRAC;
	return (T)(q < lo ? lo : q > hi ? hi : q);
}

Preview preview(const World &world, std::optional<fix> date, int32_t resolution)
{
	Preview p;
	p.resolution = resolution;
	size_t n = (size_t)resolution * resolution;
	p.elevation.resize(n);
	p.heat.resize(n);
	p.wet.resize(n);
	p.season.resize(n);
	p.fertility.resize(n);
	p.current.resize(n);
	p.biome.resize(n);
	p.lake.resize(n);
	p.province.resize(n);
	p.plate.resize(n);
	p.boundary.resize(n);
	const Grid &g = world.grid();
	// the plates' story: one line per kind of thing a step saw
	for (const History::Event &e : g.history) {
		const int32_t ago = (HISTORY_STEPS - e.step) * STEP_MYR;
		bool told = false;
		for (const Preview::Moment &m : p.history)
			told |= m.ago == ago && m.kind == e.kind;
		if (!told)
			p.history.push_back({ ago, e.kind });
	}

	for (int32_t y = 0; y < resolution; y++) {
		fix v = ratio(2 * y + 1, 2 * resolution); // cell centres
		for (int32_t x = 0; x < resolution; x++) {
			// the picture is equirectangular; what it shows is the place there
			const Place at = World::place(ratio(x, resolution), v);
			Column c = world.column(at, world.sunlight(at, date));
			size_t i = (size_t)y * resolution + x;
			const int32_t cell = world.cell(at);
			p.elevation[i] = quantize<int16_t>(c.elevation, 10000, -32767, 32767);
			p.heat[i] = quantize<int8_t>(c.heat, 100, -127, 127);
			p.wet[i] = quantize<int8_t>(c.wet, 100, -127, 127);
			p.season[i] = quantize<int8_t>(c.season, 100, -127, 127);
			p.biome[i] = (uint8_t)c.biome;
			p.lake[i] = g.lake[cell];
			p.current[i] = quantize<int8_t>(world.between(g.sea_heat, at), 100, -127, 127);
			p.fertility[i] = c.elevation < 0 ? 0 : (uint8_t)std::clamp(roundInt(g.fertility[cell] * 255), 0, 255);
			p.province[i] = (uint8_t)g.province[cell];
			p.plate[i] = g.tectonics.plate[cell];
			p.boundary[i] = g.tectonics.distance[cell] == 0 ? (uint8_t)g.tectonics.boundary[cell] : 0;
		}
	}
	// a place as u and v, 0..65535 across the map, little-endian
	auto put = [&](std::vector<uint8_t> &out, const Dir &d) {
		const uint32_t u = (uint32_t)(((int64_t)World::u(d) * 65535) >> FRAC);
		const uint32_t vv = (uint32_t)std::clamp<int64_t>(((int64_t)World::v(d) * 65535) >> FRAC, 0, 65535);
		out.push_back(u & 255);
		out.push_back((u >> 8) & 255);
		out.push_back(vv & 255);
		out.push_back(vv >> 8);
	};
	const Sphere &sphere = gridSphere();
	for (int32_t i = 0; i < g.size(); i++) {
		int32_t r = g.receiver[i];
		if (!g.river(i) || r < 0)
			continue;
		int32_t strength = roundInt(sqrtWide64((g.drained[i] << FRAC) / g.river_threshold) * 16);
		put(p.rivers, sphere.centre(i));
		put(p.rivers, sphere.centre(r));
		p.rivers.push_back((uint8_t)std::clamp(strength, 1, 255));
	}
	for (const Volcano &v : g.volcanoes) {
		put(p.volcanoes, v.at);
		p.volcanoes.push_back(v.active ? 1 : 0);
	}
	return p;
}

} // namespace terragen
