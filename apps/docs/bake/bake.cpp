// Bakes the site's planets from the real generator: an equirectangular map per
// world, height in red, rain in green, water in blue. The page lights them itself.
//   bun run --cwd apps/docs bake
#include "terragen/world.h"
#include "terragen/worlds.h"
#include "terragen/sphere.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
using namespace terragen;

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::fprintf(stderr, "usage: bake <code>[@body] <out.ppm>\n");
		return 2;
	}
	const std::string arg = argv[1];
	const size_t at = arg.find('@');
	WorldCode code = WorldCode::decode(arg.substr(0, at));
	if (at != std::string::npos) {
		bool found = false;
		for (const BodyWorld &b : bodyWorlds(code, Atlas::standard(World::radiusOf(code) / 65536.0)))
			if (b.id == arg.substr(at + 1)) {
				code = b.code;
				found = true;
			}
		if (!found)
			throw std::invalid_argument("bake: no body " + arg.substr(at + 1));
	}
	std::shared_ptr<const Grid> g = gridFor(code);
	World world(code);

	const int W = 1536, H = 768;
	FILE *f = std::fopen(argv[2], "wb");
	if (!f)
		throw std::runtime_error("bake: can't write " + std::string(argv[2]));
	std::fprintf(f, "P6\n%d %d\n255\n", W, H);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			int i = world.cell(World::place(ratio(2 * x + 1, 2 * W), ratio(2 * y + 1, 2 * H)));
			double e = g->eroded[i] / 65536.0;
			double rain = std::clamp((std::log2(std::max(g->rain(i), fix(1)) / 65536.0) + 2.5) / 4.5, 0.0, 1.0);
			unsigned char rgb[3] = {
				(unsigned char)std::clamp(127.5 + e * 127.5, 0.0, 255.0),
				(unsigned char)(rain * 255),
				(unsigned char)(g->lake[i] ? 255 : g->river(i) ? 128 : 0),
			};
			std::fwrite(rgb, 1, 3, f);
		}
	std::fclose(f);

	double land = 0, all = 0;
	int rivers = 0, lakes = 0;
	const Sphere &sphere = gridSphere();
	for (int i = 0; i < g->size(); i++) {
		all += sphere.area(i);
		land += g->base[i] >= 0 ? sphere.area(i) : 0;
		rivers += g->river(i);
		lakes += g->lake[i];
	}
	std::printf("{\"land\":%.0f,\"rivers\":%d,\"lakes\":%d,\"water\":%s}\n", 100 * land / all, rivers, lakes, code.processes.water ? "true" : "false");
	return 0;
}
