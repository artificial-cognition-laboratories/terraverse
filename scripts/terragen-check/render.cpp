// Standalone terragen (no engine): renders a world code's grid as a PNG —
// shaded relief, lakes, rivers by flow — and prints how long the grid took and
// what share is land. For tuning generation by eye:
//   g++ -std=c++17 -O2 -I src src/terragen/*.cpp scripts/terragen-check/render.cpp -o render
//   ./render TF19.dGVycmE out.png [scale] [relief|plates|provinces|rain|season|biomes|fertility|currents|sediment]
#include "terragen/world.h"
#include "terragen/worlds.h"
#include "terragen/sphere.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
using namespace terragen;

// A minimal PNG writer: RGB, deflate "stored" blocks (no compression).
static uint32_t crc(const unsigned char *p, size_t n, uint32_t c = 0xffffffffu)
{
	for (size_t i = 0; i < n; i++) {
		c ^= p[i];
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
	}
	return c;
}
static void be32(std::vector<unsigned char> &o, uint32_t v) { for (int s = 24; s >= 0; s -= 8) o.push_back((v >> s) & 255); }
static void chunk(FILE *f, const char *type, const std::vector<unsigned char> &data)
{
	std::vector<unsigned char> b;
	be32(b, data.size());
	b.insert(b.end(), type, type + 4);
	b.insert(b.end(), data.begin(), data.end());
	be32(b, ~crc(b.data() + 4, b.size() - 4));
	fwrite(b.data(), 1, b.size(), f);
}
static void png(const char *path, int w, int h, const std::vector<unsigned char> &rgb)
{
	std::vector<unsigned char> raw;
	for (int y = 0; y < h; y++) {
		raw.push_back(0);
		raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3);
	}
	std::vector<unsigned char> z = { 0x78, 0x01 };
	for (size_t i = 0; i < raw.size(); i += 65535) {
		size_t n = std::min<size_t>(65535, raw.size() - i);
		z.push_back(i + n == raw.size());
		z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
		z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
	}
	uint32_t a = 1, b = 0;
	for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
	be32(z, (b << 16) | a);
	std::vector<unsigned char> ihdr;
	be32(ihdr, w); be32(ihdr, h);
	ihdr.insert(ihdr.end(), { 8, 2, 0, 0, 0 });
	FILE *f = fopen(path, "wb");
	const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	fwrite(sig, 1, 8, f);
	chunk(f, "IHDR", ihdr);
	chunk(f, "IDAT", z);
	chunk(f, "IEND", {});
	fclose(f);
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::fprintf(stderr, "usage: render <code>[@body] <out.png> [scale] [mode]\n");
		return 2;
	}
	int scale = argc > 3 ? std::stoi(argv[3]) : 3;
	std::string mode = argc > 4 ? argv[4] : "relief";
	auto t0 = std::chrono::steady_clock::now();
	// <code>@<body>: another body of the world's system, as the game builds it
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
			throw std::invalid_argument("render: no body " + arg.substr(at + 1) + " to stand on");
	}
	std::shared_ptr<const Grid> g = gridFor(code);
	World world(code);
	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

	const Sphere &sphere = gridSphere();
	double land = 0, all = 0;
	int lakes = 0, rivers = 0;
	for (int i = 0; i < g->size(); i++) {
		all += sphere.area(i);
		land += g->base[i] >= 0 ? sphere.area(i) : 0;
		lakes += g->lake[i];
		rivers += g->river(i);
	}
	// hypsometry: how much of the land stands under 500 m and over 2 km (a unit is 5 km)
	double low = 0, high = 0;
	for (int i = 0; i < g->size(); i++)
		if (g->eroded[i] >= 0) {
			low += g->eroded[i] < ONE / 10 ? sphere.area(i) : 0;
			high += g->eroded[i] > ONE * 2 / 5 ? sphere.area(i) : 0;
		}
	std::printf("land under 500 m %.0f%%, over 2 km %.0f%%\n", 100.0 * low / std::max(1.0, land), 100.0 * high / std::max(1.0, land));
	std::printf("grid %.0f ms · land %.1f%% (asked %.0f%%) · lakes %d cells · rivers %d cells\n",
		ms, 100.0 * land / all, 100.0 * code.land / ONE, lakes, rivers);

	// the map, equirectangular: each pixel the cell at its place
	const int MAP = 256;
	int W = MAP * scale;
	std::vector<unsigned char> rgb((size_t)W * W * 3);
	for (int y = 0; y < W; y++)
		for (int x = 0; x < W; x++) {
			int cx = x / scale, cz = y / scale;
			const fix pu = ratio(2 * cx + 1, 2 * MAP), pv = ratio(2 * cz + 1, 2 * MAP);
			const Place at = World::place(pu, pv);
			int i = world.cell(at);
			double e = g->eroded[i] / 65536.0;
			int ie = world.cell(World::place(pu < ratio(1, MAP) ? pu + ONE - ratio(1, MAP) : pu - ratio(1, MAP), std::max<fix>(pv - ratio(1, MAP), ratio(1, 2 * MAP))));
			double shade = 1 + (g->eroded[ie] - g->eroded[i]) / 65536.0 * 18;
			double r, gg, b;
			if (e < 0) { double d = std::max(0.0, 1 + e * 1.4); r = 20 * d; gg = 50 + 50 * d; b = 110 + 70 * d; }
			else if (e < 0.15) { r = 90 + 200 * e; gg = 150 + 100 * e; b = 70; }
			else if (e < 0.45) { r = 120 + 100 * (e - 0.15); gg = 160 - 120 * (e - 0.15); b = 80; }
			else { double s = std::min(1.0, (e - 0.45) * 2); r = 140 + 100 * s; gg = 125 + 115 * s; b = 115 + 125 * s; }
			if (e >= 0) { r *= shade; gg *= shade; b *= shade; }
			if (!code.processes.water) { // a dry body: grey by height, all of it shaded; the maria dark
				r = gg = b = std::clamp(150 + 160 * e, 40.0, 230.0) * shade * (g->mare[i] > 0 ? 0.5 : 1.0);
			}
			if (mode == "plates") {
				// each plate its own tint; boundaries red (converging), cyan (spreading), yellow (sliding)
				const Tectonics &t = g->tectonics;
				unsigned k = t.plate[i] * 2654435761u;
				double tint = e < 0 ? 0.45 : 0.85;
				r = (60 + (k >> 8) % 160) * tint; gg = (60 + (k >> 16) % 160) * tint; b = (60 + (k >> 24) % 160) * tint;
				if (t.distance[i] == 0) {
					if (t.boundary[i] == Boundary::Convergent) { r = 240; gg = 40; b = 40; }
					else if (t.boundary[i] == Boundary::Divergent) { r = 40; gg = 220; b = 240; }
					else { r = 240; gg = 220; b = 40; }
				}
			} else if (mode == "provinces") {
				static const unsigned char PC[7][3] = { { 150, 140, 130 }, { 170, 80, 70 }, { 210, 140, 60 },
					{ 90, 170, 160 }, { 120, 170, 90 }, { 60, 40, 40 }, { 40, 70, 130 } };
				const unsigned char *pc = PC[(int)g->province[i]];
				r = pc[0] * shade; gg = pc[1] * shade; b = pc[2] * shade;
			} else if (mode == "rain") {
				// the year's rain: brown (dry) through green to deep blue (soaked), log scale
				double t = std::clamp((std::log2(g->rain(i) / 65536.0) + 2.5) / 4.5, 0.0, 1.0);
				r = 200 - 180 * t; gg = 160 + 40 * std::sin(t * 3.14) - 60 * t; b = 70 + 170 * t;
				r *= shade; gg *= shade; b *= shade;
			} else if (mode == "season") {
				// when it rains: red summer, blue winter, grey even; darker where dry
				Column c = world.column(at, world.sunlight(at, std::nullopt));
				double s = c.season / 65536.0, amount = std::clamp(0.4 + c.wet / 65536.0 * 0.6, 0.2, 1.0);
				r = (140 + 110 * std::max(0.0, s) - 100 * std::max(0.0, -s)) * amount;
				gg = (140 - 70 * std::fabs(s)) * amount;
				b = (140 + 110 * std::max(0.0, -s) - 100 * std::max(0.0, s)) * amount;
			} else if (mode == "biomes") {
				static const unsigned char BC[(int)Biome::Count][3] = { { 18, 52, 96 }, { 30, 84, 140 }, { 200, 220, 235 },
					{ 214, 200, 140 }, { 222, 190, 118 }, { 176, 170, 84 }, { 104, 160, 72 }, { 52, 116, 56 }, { 28, 120, 60 },
					{ 60, 100, 80 }, { 150, 160, 140 }, { 120, 116, 110 }, { 236, 240, 244 }, { 96, 140, 40 }, { 160, 150, 96 },
					{ 168, 166, 160 }, { 84, 84, 88 }, { 164, 83, 44 }, { 216, 194, 60 }, { 232, 226, 214 }, { 74, 110, 84 }, { 132, 170, 70 } };
				const unsigned char *bc = BC[(int)world.column(at, world.sunlight(at, std::nullopt)).biome];
				r = bc[0] * shade; gg = bc[1] * shade; b = bc[2] * shade;
			} else if (mode == "currents") {
				// what the currents do to the heat: red warmer than the latitude, blue colder
				double a = std::clamp(g->sea_heat[i] / 65536.0 / 0.25, -1.0, 1.0);
				double base = e < 0 ? 70 : 150;
				r = base + 170 * std::max(0.0, a) - 40 * std::max(0.0, -a);
				gg = base - 40 * std::fabs(a);
				b = base + 170 * std::max(0.0, -a) - 40 * std::max(0.0, a);
				if (e >= 0) { r *= shade; gg *= shade; b *= shade; }
			} else if (mode == "sediment") {
				// what rivers laid down: grey nothing, through yellow to deep orange (ten blocks or more)
				double a = alluvium(g->sediment[i]) / 65536.0;
				if (e >= 0) { r = (110 + 130 * a) * shade; gg = (110 + 40 * a) * shade; b = (110 - 90 * a) * shade; }
				else { r = 20 + 120 * a; gg = 50 + 60 * a; b = 130 - 60 * a; }
				// placers (gold, tin) in red, by how rich
				const double pl = g->placer[i] / 65536.0;
				if (pl > 0) { r = 160 + 95 * pl; gg = 40; b = 40; }
			} else if (mode == "age") {
				// how long the crust has stood still: shields deep violet, young belts and rifts orange
				const double st = g->tectonics.stable[i] / 65536.0;
				if (e >= 0) { r = (230 - 150 * st) * shade; gg = (140 - 80 * st) * shade; b = (60 + 120 * st) * shade; }
			} else if (mode == "fertility") {
				double f = g->fertility[i] / 65536.0;
				if (e >= 0) { r = (90 + 80 * (1 - f)) * shade; gg = (70 + 120 * f) * shade; b = 50 * shade; }
			}
			if (mode != "relief") {
			} else if (g->lake[i]) { r = 60; gg = 150; b = 210; }
			else if (g->river(i)) {
				double s = std::min(1.0, std::sqrt((double)g->drained[i] / g->river_threshold) / 6);
				r = 40 - 20 * s; gg = 110 + 40 * s; b = 220;
			}
			size_t o = ((size_t)y * W + x) * 3;
			rgb[o] = (unsigned char)std::clamp(r, 0.0, 255.0);
			rgb[o + 1] = (unsigned char)std::clamp(gg, 0.0, 255.0);
			rgb[o + 2] = (unsigned char)std::clamp(b, 0.0, 255.0);
		}
	// volcanoes: white (active) and grey rings
	if (mode == "plates" || mode == "provinces") {
		for (const Volcano &v : g->volcanoes) {
			int cx = (int)(((int64_t)World::u(v.at) * MAP) >> FRAC), cz = (int)(((int64_t)World::v(v.at) * MAP) >> FRAC);
			for (int dy = -2; dy <= 2; dy++)
				for (int dx = -2; dx <= 2; dx++) {
					int px = cx * scale + dx, py = cz * scale + dy;
					if (px < 0 || py < 0 || px >= W || py >= W) continue;
					size_t o = ((size_t)py * W + px) * 3;
					unsigned char c = v.active ? 255 : 150;
					rgb[o] = c; rgb[o + 1] = c; rgb[o + 2] = c;
				}
		}
		std::printf("volcanoes %zu (%d active)\n", g->volcanoes.size(),
			(int)std::count_if(g->volcanoes.begin(), g->volcanoes.end(), [](const Volcano &v) { return v.active; }));
	}
	png(argv[2], W, W, rgb);
}
