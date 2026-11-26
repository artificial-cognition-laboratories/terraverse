// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/crust.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/base/parallel.h"
#include "library/world/terragen/src/surface/craters.h"
#include "library/world/terragen/src/surface/maria.h"
#include "library/world/terragen/src/surface/lid.h"
#include "library/world/terragen/src/world.h"

namespace terragen::crust {

namespace {

// A plateau's height: some 4.5 km (a unit is 5 km)
const fix PLATEAU = ratio(90, 100);

} // namespace

void lay(const WorldCode &code, Grid &g, const History &past)
{
	const Sphere &sphere = gridSphere();
	const size_t n = (size_t)sphere.size();
	std::vector<fix> raw(n);
	parallelFor((int32_t)n, [&](int32_t i) {
		raw[i] = past.cells[i].height + mul(fbm(sphere.centre(i), code.seed_hash + COAST, code.continents * 7, 3, HALF),
				mul(code.coast, ratio(12, 100)));
	});
	// the ocean floor slopes down from the continents over a few cells (the
	// continental slope), not a cliff from shelf to abyss in one; and floor
	// handed over a step at a time leaves no lines where its ages meet
	for (int pass = 0; pass < 6; pass++) {
		const std::vector<fix> was = raw;
		parallelFor((int32_t)n, [&](int32_t i) {
			if (past.cells[i].continental)
				return;
			int64_t sum = was[i];
			int32_t count = 1;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, count++)
				sum += was[*p];
			raw[i] = std::min<fix>(was[i] + ratio(10, 100), (fix)(sum / count));
		});
	}
	{
		const std::vector<int32_t> by = lowestFirst(raw);
		const int64_t sea = (int64_t)n * (ONE - code.land); // of the planet's n·ONE (each cell's area()), fixed
		int64_t under = 0;
		size_t k = 0;
		while (k + 1 < n && under + sphere.area(by[k]) <= sea)
			under += sphere.area(by[k++]);
		g.sea_offset = raw[by[k]];
	}
	// (the continents' platforms and old shields stand low by the erosion and
	// the crust's floating, not by remapping the history's heights: below)
	g.base.resize(n);
	parallelFor((int32_t)n, [&](int32_t i) {
		const fix land = raw[i] - g.sea_offset;
		const fix relief = g.tectonics.relief[i];
		// trenches and abyssal hills shape the sea floor, never above the sea;
		// rifts sink the land, never below it
		// (on a dry body there's no waterline to hold to: the land just goes on down)
		if (!code.processes.water) {
			g.base[i] = mul(land + relief, code.processes.stiff);
			return;
		}
		g.base[i] = land < 0 ? std::min<fix>(land + relief + fbm(sphere.centre(i), code.seed_hash + DETAIL, 24, 3, HALF) / 25, -ratio(1, 160))
				: std::max<fix>(land + relief, ratio(1, 160));
	});

	// An ice crust flows: over the ages its broad relief sags, the harder its
	// tides warm it the more, and no scarp stands (the old continents' edges)
	if (code.processes.stiff < ONE) {
		const fix sag = div(ONE, ONE + code.processes.flexing);
		for (size_t i = 0; i < n; i++)
			g.base[i] = mul(g.base[i], sag);
		for (int pass = 0; pass < 16; pass++) {
			const std::vector<fix> was = g.base;
			for (size_t i = 0; i < n; i++) {
				int64_t sum = was[i];
				int32_t count = 1;
				for (const int32_t *p = sphere.neighboursBegin((int32_t)i); p != sphere.neighboursEnd((int32_t)i); p++, count++)
					sum += was[*p];
				g.base[i] = (fix)(sum / count);
			}
		}
	}

	// Crust thickened behind a collision stands as a high plain (Tibet): level,
	// and dry in its range's shadow, so it wears slowly (the erosion below)
	{
		std::vector<fix> level = g.base;
		for (int pass = 0; pass < 32; pass++) {
			const std::vector<fix> was = level;
			parallelFor((int32_t)n, [&](int32_t i) {
				int64_t sum = was[i];
				int32_t count = 1;
				for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, count++)
					sum += was[*p];
				level[i] = (fix)(sum / count);
			});
		}
		for (size_t i = 0; i < n; i++)
			if (g.tectonics.plateau[i] > 0 && g.base[i] >= 0)
				g.base[i] += mul(std::max(level[i], PLATEAU) - g.base[i], g.tectonics.plateau[i]);
	}
}

void strike(const WorldCode &code, Grid &g)
{
	const Sphere &sphere = gridSphere();
	const size_t n = (size_t)sphere.size();
	// Impacts: the craters wider than two cells, into the land (the rest are
	// the column's, too small for the grid to hold); the old first, then the
	// maria's lava over them, then the young on top
	{
		const Craters craters(code, World::radiusOf(code), blocksPerMetre(code));
		const int64_t cell = World::cellBlocksOf(code) >> FRAC;
		const fix scale = heightScale(code);
		g.crater_stamp.assign(n, 0);
		g.crater_young.assign(n, 0);
		std::vector<std::vector<uint32_t>> reach(n);
		while (g.crater_from < craters.classes() && craters.widest(g.crater_from) >= 2 * cell) {
			for (const Crater &c : craters.all(g.crater_from)) {
				const uint32_t k = (uint32_t)g.craters.size();
				g.craters.push_back(c);
				std::vector<fix> &stamp = c.young ? g.crater_young : g.crater_stamp;
				// the cells within its ejecta (a diameter out) and a cell more
				const int64_t within = 2 * c.radius + (PLACE_ONE >> FRAC) * sphere.spacing();
				for (int32_t i : sphere.around(sphere.cell(c.at.dir()), [&](int32_t i) {
						return chord(Place(sphere.centre(i)), c.at) <= within; })) {
					reach[i].push_back(k);
					stamp[i] += div(Craters::shape(c, Place(sphere.centre(i))), scale);
				}
			}
			g.crater_from++;
		}
		g.crater_start.assign(n + 1, 0);
		for (size_t i = 0; i < n; i++) {
			g.crater_start[i + 1] = g.crater_start[i] + (uint32_t)reach[i].size();
			g.crater_list.insert(g.crater_list.end(), reach[i].begin(), reach[i].end());
			g.base[i] += g.crater_stamp[i];
		}
		flood(code, g);
		raiseRise(code, g);
		for (size_t i = 0; i < n; i++)
			g.base[i] += g.crater_young[i];
	}
}

} // namespace terragen::crust
