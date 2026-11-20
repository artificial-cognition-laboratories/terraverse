// Standalone terragen: a world's land against the Earth's, at the grid's
// scale (sprint 116, phase 2: mountains as uplift against erosion):
//   scripts/terragen-tool.sh /tmp/hypsometry scripts/terragen-check/hypsometry.cpp
//   /tmp/hypsometry <code>[:<setting>=<0..1>]...   (setting: mountains, land, roughness, humidity)
// What it reports, and the Earth's (land by area, heights above sea level):
//   the share of the land in each band of height: 0–200 m 28%, 200–500 24%,
//     500–1000 21%, 1000–2000 16%, 2000–3000 6%, over 3000 5%
//   mean ~840 m; the highest cell (a cell is a broad area: the Earth's
//     highest ~90 km square stands ~5–6 km, its peaks 8.8)
//   where land rises now (belt over ½), its median and highest
//   concavity, river slope ~ area^−θ: θ 0.4–0.6 (of the finished land: its
//     filled hollows and sunk basins read higher than the erosion left it)
//   how long the grid took
#include "library/world/terragen/grid.h"
#include "library/world/terragen/world.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
using namespace terragen;

static const double METRES = 5000; // a unit of height

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "usage: hypsometry <code>...\n");
		return 2;
	}
	const Sphere &sphere = gridSphere();
	static const double BANDS[] = { 200, 500, 1000, 2000, 3000 };
	static const double EARTH[] = { 28, 24, 21, 16, 6, 5 };
	std::printf("%-40s %5s %5s %5s %5s %5s %5s · %5s %5s · %11s · %5s · %6s\n", "", "<200", "<500", "<1k", "<2k", "<3k", ">3k",
			"mean", "top", "belts p50/top", "θ", "ms");
	std::printf("%-40s %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f · %5d %5s · %11s · %5s\n", "the Earth", EARTH[0], EARTH[1], EARTH[2], EARTH[3],
			EARTH[4], EARTH[5], 840, "~5.5k", "", "0.4–0.6");
	for (int a = 1; a < argc; a++) {
		const auto t0 = std::chrono::steady_clock::now();
		// <code>:<setting>=<value>: the code with one setting moved
		const std::string arg = argv[a];
		const size_t colon = arg.find(':');
		WorldCode code = WorldCode::decode(arg.substr(0, colon));
		if (colon != std::string::npos) {
			const size_t eq = arg.find('=', colon);
			const std::string name = arg.substr(colon + 1, eq - colon - 1);
			const fix value = (fix)std::lround(std::stod(arg.substr(eq + 1)) * 65536);
			fix *field = name == "mountains" ? &code.mountains : name == "land" ? &code.land : name == "roughness" ? &code.roughness
					: name == "humidity" ? &code.humidity : nullptr;
			if (!field) {
				std::fprintf(stderr, "hypsometry: no setting %s\n", name.c_str());
				return 2;
			}
			*field = value;
			code.keepGrid();
		}
		const std::shared_ptr<const Grid> kept = gridFor(code);
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		const Grid &g = *kept;
		double band[6] = {}, land = 0, sum = 0, top = 0;
		struct Fit
		{
			double sx = 0, sy = 0, sxx = 0, sxy = 0;
			int n = 0;
			void add(double x, double y) { sx += x; sy += y; sxx += x * x; sxy += x * y; n++; }
			double theta() const { return -(n * sxy - sx * sy) / (n * sxx - sx * sx); }
		} all;
		std::vector<double> belts;
		for (int32_t i = 0; i < g.size(); i++) {
			if (g.base[i] < 0)
				continue;
			const double area = sphere.area(i) / 65536.0, m = g.eroded[i] / 65536.0 * METRES;
			land += area;
			sum += m * area;
			top = std::max(top, m);
			int b = 0;
			while (b < 5 && m >= BANDS[b])
				b++;
			band[b] += area;
			if (g.tectonics.belt[i] > HALF)
				belts.push_back(m);
			const int32_t r = g.receiver[i];
			if (!g.stream(i) || r < 0 || g.lake[i] || g.eroded[r] < 0 || g.filled[i] <= g.filled[r])
				continue;
			double gap = 0;
			const fix *d = sphere.distancesBegin(i);
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, d++)
				if (*p == r)
					gap = *d;
			const double x = std::log((double)g.drained[i]), y = std::log((g.filled[i] - g.filled[r]) / gap);
			all.add(x, y);
		}
		std::sort(belts.begin(), belts.end());
		std::printf("%-40s %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f · %5.0f %5.0f · %5.0f %5.0f · %5.2f · %6.0f\n", argv[a],
				100 * band[0] / land, 100 * band[1] / land, 100 * band[2] / land, 100 * band[3] / land, 100 * band[4] / land,
				100 * band[5] / land, sum / land, top, belts.empty() ? 0 : belts[belts.size() / 2], belts.empty() ? 0 : belts.back(),
				all.theta(), ms);
	}
}
