// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "maria.h"
#include "grid.h"
#include "noise.h"
#include "plates.h"
#include <algorithm>
#include <queue>

namespace terragen {

using plates::mix;
using plates::unit;

namespace {

enum MariaStream : uint32_t { CRUST = 229, TITANIUM = 233 };

// How much of a pool's share a basin's thin crust lets up: lava rises
// through thin crust far more readily (the Moon's near side holds nearly
// all its maria)
fix weight(const Crater &c, fix thin)
{
	const fix r = (fix)std::min<int64_t>(ONE, c.radius >> (PLACE_FRAC - FRAC));
	return mul(mul(r, r), mul(mul(thin, thin), mul(thin, thin)));
}

} // namespace

void flood(const WorldCode &code, Grid &g)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	g.mare.assign(n, 0);
	g.mare_level = g.base;
	g.titanium.assign(n, 0);
	g.thin.assign(n, 0);

	// the crust thinner on one side (a body's halves cooled unevenly), with
	// broad patches
	const uint32_t s = code.seed_hash + CRUST;
	const Dir axis = normalise(unit(mix(s, 1, s)) - HALF, unit(mix(s, 2, s)) - HALF, unit(mix(s, 3, s)) - HALF);
	for (int32_t i = 0; i < n; i++) {
		const Dir &c = sphere.centre(i);
		const fix side = mul(c.x, axis.x) + mul(c.y, axis.y) + mul(c.z, axis.z);
		g.thin[i] = clamp(HALF + mul(side, ratio(45, 100)) + noise(c, s, 3) / 4, 0, ONE);
	}
	if (code.processes.flooding <= 0)
		return;

	// the sources: the complex craters and basins the grid holds, the most
	// open to the mantle first
	struct Source
	{
		int32_t cell;
		fix weight;
		uint32_t k;
	};
	std::vector<Source> sources;
	fix total = 0;
	for (uint32_t k = 0; k < g.craters.size(); k++) {
		const Crater &c = g.craters[k];
		if (!c.complex || c.young)
			continue;
		const int32_t i = sphere.cell(c.at.dir());
		const fix w = weight(c, g.thin[i]);
		if (w > 0) {
			sources.push_back({ i, w, k });
			total += w;
		}
	}
	if (sources.empty()) // no basin to fill: the lowest thin ground wells up
		sources.push_back({ (int32_t)(std::min_element(g.base.begin(), g.base.end(), [&](fix a, fix b) { return a < b; }) - g.base.begin()), ONE, 0 });
	if (total == 0)
		total = ONE;
	std::stable_sort(sources.begin(), sources.end(), [](const Source &a, const Source &b) { return a.weight > b.weight; });

	// (areas against the mean cell's: the sum is the cell count, past a fix)
	int64_t surface = 0;
	for (int32_t i = 0; i < n; i++)
		surface += sphere.area(i);
	const int64_t share = surface * code.processes.flooding >> FRAC;

	// each pours like water rising in its basin: the lowest cell at its edge
	// next, level to the highest taken, until it covers its share; a full
	// basin spills into the ground round it
	std::vector<uint32_t> seen(n, 0);
	using Edge = std::pair<fix, int32_t>;
	for (uint32_t p = 0; p < sources.size(); p++) {
		const Source &src = sources[p];
		const int64_t quota = (int64_t)((__int128)share * src.weight / total);
		if (quota <= 0)
			continue;
		std::priority_queue<Edge, std::vector<Edge>, std::greater<Edge>> edge;
		std::vector<int32_t> pool;
		edge.push({ g.base[src.cell], src.cell });
		seen[src.cell] = p + 1;
		fix level = INT32_MIN;
		int64_t covered = 0;
		while (!edge.empty() && covered < quota) {
			const auto [h, i] = edge.top();
			edge.pop();
			level = std::max(level, h);
			pool.push_back(i);
			if (g.mare[i] == 0) // (an earlier pool's lava it merges with isn't new)
				covered += sphere.area(i);
			for (const int32_t *q = sphere.neighboursBegin(i); q != sphere.neighboursEnd(i); q++)
				if (seen[*q] != p + 1) {
					seen[*q] = p + 1;
					edge.push({ g.base[*q], *q });
				}
		}
		// each pool's lava its own: some rich in titanium (ilmenite), most not
		const fix ti = mul(unit(mix(src.k, p, code.seed_hash + TITANIUM)), unit(mix(src.k, p + 7, code.seed_hash + TITANIUM)));
		for (int32_t i : pool) {
			g.mare[i] += level - g.base[i];
			g.base[i] = level;
			g.mare_level[i] = level;
			g.titanium[i] = std::max(g.titanium[i], ti);
		}
	}
	// a cell the lava only met (level with it) isn't under any
	for (int32_t i = 0; i < n; i++)
		if (g.mare[i] == 0)
			g.mare_level[i] = g.base[i];
}

} // namespace terragen
