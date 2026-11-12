// Standalone terragen: what a column costs once the world's made (the lazy
// path: far tiles, the Planetarium), part by part, at the widths far tiles
// read at (World::Coarse). `bun perf:lazy` runs it (scripts/perf-lazy.sh):
//   scripts/terragen-tool.sh /tmp/lazy scripts/terragen-check/lazy.cpp
//   [KEEP=<dir>] /tmp/lazy <code>
// A square of 64×64 columns, each width apart, round the widest river on land
// (a place with everything in it: land, a river, a coast), and the same
// square again (what a tile's neighbour or a second look costs).
#include "terragen/world.h"
#include "terragen/grid.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace terragen;

using Clock = std::chrono::steady_clock;
static double since(Clock::time_point t) { return std::chrono::duration<double, std::micro>(Clock::now() - t).count(); }

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "usage: [KEEP=<dir>] lazy <code>\n");
		return 2;
	}
	const WorldCode code = WorldCode::decode(argv[1]);
	if (const char *keep = std::getenv("KEEP"))
		keepGridsIn(keep, "lazy", "lazy");
	auto t = Clock::now();
	const World world(code);
	std::printf("world made in %.2f s\n", since(t) / 1e6);

	// the widest river on land, on a coarse lattice (as the ladder looks;
	// read coarse, so finding it isn't what's measured)
	Place centre = World::place(HALF, HALF);
	int64_t widest = -1;
	{
	const World::Coarse rough(world, 4096);
	for (int j = 20; j < 100; j++)
		for (int i = 0; i < 240; i++) {
			const Place at = World::place(ratio(2 * i + 1, 480), ratio(2 * j + 1, 240));
			const Ground g = world.ground(at, world.sunlight(at, std::nullopt));
			if (g.terrain > 0 && g.river_width > widest && g.river_dist <= g.river_width) {
				widest = g.river_width;
				centre = at;
			}
		}
	}
	const fix cu = World::u(centre), cv = World::v(centre);
	const double round = 2 * M_PI * (double)world.radius() / ONE; // blocks round

	std::printf("%8s %9s %9s %9s %9s %9s %9s   (µs a column)\n", "width", "sunlight", "ground", "strata", "soil", "solid", "again");
	constexpr int N = 64;
	const char *only = std::getenv("WIDTH");
	for (const int64_t width : { 1, 8, 64, 512, 4096 }) {
		if (only && std::stoll(only) != width)
			continue;
		const World::Coarse coarse(world, width);
		double sun = 0, ground = 0, strata = 0, soil = 0, solid = 0, again = 0;
		for (int pass = 0; pass < 2; pass++)
			for (int j = 0; j < N; j++)
				for (int i = 0; i < N; i++) {
					const fix u = cu + (fix)((double)(i - N / 2) * width / round * ONE);
					const fix v = cv + (fix)((double)(j - N / 2) * width / (round / 2) * ONE);
					const Place at = World::place(u & (ONE - 1), std::clamp<fix>(v, 0, ONE - 1));
					auto s = Clock::now();
					const fix light = world.sunlight(at, std::nullopt);
					const double a = since(s);
					s = Clock::now();
					const Ground g = world.ground(at, light);
					const double b = since(s);
					if (pass == 1) {
						again += a + b;
						continue;
					}
					sun += a;
					ground += b;
					s = Clock::now();
					const Strata st = world.strata(at, g);
					strata += since(s);
					s = Clock::now();
					world.soil(at, g, st);
					soil += since(s);
					s = Clock::now();
					world.solid(at, 0);
					solid += since(s);
				}
		const double n = N * N;
		std::printf("%8lld %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n", (long long)width, sun / n, ground / n, strata / n, soil / n,
				solid / n, again / n);
	}
	return 0;
}
