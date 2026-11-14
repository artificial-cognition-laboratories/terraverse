// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "landbake.h"
#include "exceptions.h"
#include "terragen/code.h"
#include "terragen/grid.h"
#include <cmath>
#include <map>

namespace far {

using namespace terragen;

static constexpr int SIDE = 64;        // columns a side
static constexpr int MOST_PATH = 40;   // cells from the range to the sea: the whole river fits in the view
static constexpr int BLOCKS = 160;     // of the grid's height unit

static double toDouble(fix v) { return (double)v / ONE; }
static fix fromDouble(double v) { return (fix)std::lround(v * ONE); }

struct V { double x, y, z; };
static V vec(const Dir &d) { return { toDouble(d.x), toDouble(d.y), toDouble(d.z) }; }
static V cross(V a, V b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
static double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V unit(V a) { const double l = std::sqrt(dot(a, a)); return { a.x / l, a.y / l, a.z / l }; }

Json::Value bakeRiverPatch()
{
	const WorldCode code = WorldCode::decode("TF23.dGVycmE");
	std::vector<std::vector<fix>> land, sediment;
	std::vector<std::vector<int64_t>> flow;
	const std::shared_ptr<const Grid> built = watchErosion(code, [&](const ErosionRound &r) {
		land.push_back(r.land);
		sediment.push_back(r.sediment);
		flow.push_back(r.area);
	});
	const Grid &g = *built;
	const Sphere &sphere = gridSphere();

	// the biggest river that starts in a range's core and is short enough to see whole
	int32_t source = -1;
	int64_t best = -1;
	for (int32_t c = 0; c < g.size(); c++) {
		if (g.base[c] < 0 || g.tectonics.belt[c] <= VEIN_CORE)
			continue;
		int length = 0;
		int32_t at = c;
		while (at >= 0 && g.eroded[at] >= 0 && length <= MOST_PATH) {
			at = g.receiver[at];
			length++;
		}
		// (to the sea, long enough to be a river)
		if (at < 0 || g.eroded[at] >= 0 || length > MOST_PATH || length < 16)
			continue;
		if (g.drained[c] > best) {
			best = g.drained[c];
			source = c;
		}
	}
	if (source < 0)
		throw BaseException("bake: no river from a range to the sea on the home world");
	std::vector<int32_t> path{ source };
	while (g.eroded[path.back()] >= 0)
		path.push_back(g.receiver[path.back()]);

	// a square on the tangent plane at its middle, the river running along it
	const V middle = vec(sphere.centre(path[path.size() / 2]));
	const V from = vec(sphere.centre(path.front())), to = vec(sphere.centre(path.back()));
	const V along0{ to.x - from.x, to.y - from.y, to.z - from.z };
	const double span = std::sqrt(dot(along0, along0)) * 1.5;
	const V along = unit(cross(cross(middle, along0), middle)), across = unit(cross(middle, along));

	std::map<int32_t, int> index;
	Json::Value out, cells(Json::arrayValue);
	std::vector<int32_t> ids;
	for (int z = 0; z < SIDE; z++)
		for (int x = 0; x < SIDE; x++) {
			const double u = ((x + 0.5) / SIDE - 0.5) * span, v = ((z + 0.5) / SIDE - 0.5) * span;
			const V p = unit({ middle.x + across.x * u + along.x * v, middle.y + across.y * u + along.y * v, middle.z + across.z * u + along.z * v });
			const int32_t c = sphere.cell(Dir{ fromDouble(p.x), fromDouble(p.y), fromDouble(p.z) });
			const auto found = index.try_emplace(c, (int)ids.size());
			if (found.second)
				ids.push_back(c);
			cells.append(found.first->second);
		}
	out["side"] = SIDE;
	out["columns"] = cells;

	const auto blocks = [](fix v) { return (int)std::lround(toDouble(v) * BLOCKS); };
	const auto streamsAt = [&](int64_t a) { return a >= g.stream_threshold; };
	for (size_t r = 0; r < land.size(); r++) {
		Json::Value l(Json::arrayValue), s(Json::arrayValue), f(Json::arrayValue);
		for (const int32_t c : ids) {
			l.append(blocks(land[r][c]));
			s.append(blocks(sediment[r][c]));
			f.append(streamsAt(flow[r][c]) ? 1 : 0);
		}
		out["land"].append(l);
		out["sediment"].append(s);
		out["stream"].append(f);
	}
	Json::Value core(Json::arrayValue), placer(Json::arrayValue), lake(Json::arrayValue), river(Json::arrayValue);
	std::map<int32_t, bool> on;
	for (const int32_t c : path)
		on[c] = true;
	for (const int32_t c : ids) {
		core.append(g.tectonics.belt[c] > VEIN_CORE ? 1 : 0);
		placer.append((int)std::lround(toDouble(g.placer[c]) * 100));
		lake.append(g.lake[c] ? 1 : 0);
		river.append(on.count(c) ? 1 : 0);
	}
	out["core"] = core;
	out["placer"] = placer;
	out["lake"] = lake;
	out["river"] = river;
	return out;
}

} // namespace far
