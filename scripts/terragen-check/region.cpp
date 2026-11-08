// Standalone terragen: a region of the land at block scale, as the game
// builds its columns (World::ground), hillshaded from the north-west with
// contours, and the land's hypsometry (how much of it stands how high), whole
// planet and region. For tuning the shape of the land by eye:
//   g++ -std=c++17 -O2 -I apps/game/src apps/game/src/terragen/*.cpp scripts/terragen-check/region.cpp -o /tmp/region
//   [SIZE=628320] [BIOMES=1 | KINDS=1 | SEABED=1] [COARSE=1] /tmp/region <code> out.png [u,v | peak | belt | plain | lake | pan | mouth | fjord | river] [span km] [pixels]
// u, v: the region's middle (0..1 round, north to south); span: across it.
// peak: the highest land; belt: the most-rising range; plain: the lowest
// broad interior (the land furthest from both sea and range); lake: the biggest lake; river: the biggest river, inland.
#include "terragen/world.h"
#include "terragen/sphere.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
using namespace terragen;
static double real(fix v) { return v / (double)ONE; }

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


static void hypsometry(const char *what, std::vector<double> h)
{
	if (h.empty())
		return;
	std::sort(h.begin(), h.end());
	std::printf("%s: ", what);
	for (int p : { 5, 25, 50, 75, 90, 95, 99 })
		std::printf("p%d %.0f  ", p, h[std::min(h.size() - 1, h.size() * p / 100)]);
	std::printf("max %.0f\n", h.back());
	// land by height band, a bar each
	const double top = std::max(1.0, h.back());
	const int BANDS = 10;
	std::vector<size_t> count(BANDS, 0);
	for (double v : h)
		count[std::min(BANDS - 1, (int)(std::max(0.0, v) / top * BANDS))]++;
	for (int b = BANDS - 1; b >= 0; b--) {
		std::printf("  %5.0f–%5.0f ", top * b / BANDS, top * (b + 1) / BANDS);
		for (size_t k = 0; k < count[b] * 60 / h.size(); k++)
			std::putchar('#');
		std::printf(" %.1f%%\n", 100.0 * count[b] / h.size());
	}
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::fprintf(stderr, "usage: region <code> <out.png> [u,v|peak|belt|plain|lake|river] [span km] [pixels]\n");
		return 2;
	}
	WorldCode code = WorldCode::decode(argv[1]);
	// SIZE=<blocks round>: the code's land on a planet that size (as a new world's 628320)
	if (const char *size = std::getenv("SIZE"))
		code.size = std::stoi(size);
	const std::string where = argc > 3 ? argv[3] : "peak";
	const double km = argc > 4 ? std::stod(argv[4]) : 20;
	const int N = argc > 5 ? std::stoi(argv[5]) : 512;
	std::shared_ptr<const Grid> g = gridFor(code);
	World world(code);
	const Sphere &sphere = gridSphere();
	const double R = World::radiusOf(code) / 65536.0; // blocks
	const double perM = real(blocksPerMetre(code));

	// the grid's land, in blocks over the sea (each cell weighted alike: they're near equal)
	const auto blocksOf = [&](int32_t i) {
		const Dir d = sphere.centre(i);
		return (double)world.ground(Place(d), world.sunlight(Place(d), std::nullopt)).terrain;
	};
	std::vector<double> land;
	int32_t pick = -1;
	double best = -1e18;
	for (int32_t i = 0; i < g->size(); i++) {
		if (g->base[i] < 0)
			continue;
		const double b = real(g->base[i]);
		land.push_back(b);
		double score = 0;
		if (where == "peak") score = b;
		else if (where == "belt") score = real(g->tectonics.belt[i]);
		else if (where == "shield") score = b > 0.03 && b < 0.12 && std::abs(gridSphere().centre(i).y) < ONE / 2 ? real(g->tectonics.stable[i]) : -1;
		else if (where == "lake") {
			// the middle of the biggest lake: the most lake cells within two rings
			score = -1e18;
			if (g->lake[i]) {
				score = 0;
				for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
					for (const int32_t *q = sphere.neighboursBegin(*p); q != sphere.neighboursEnd(*p); q++)
						score += g->lake[*q];
			}
		}
		else if (where == "pan") {
			score = -1e18;
			if (!g->pan.empty() && g->pan[i]) {
				score = 0;
				for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
					for (const int32_t *q = sphere.neighboursBegin(*p); q != sphere.neighboursEnd(*p); q++)
						score += g->pan[*q];
			}
		}
		else if (where == "fjord") { // the coldest high coast: where the ice cut fjords
			bool sea = false;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
				sea |= g->base[*p] < 0;
			score = sea ? b * 4 - real(g->heat[i]) : -1e18;
		}
		else if (where == "mouth") // the biggest river into the sea
			score = g->river(i) && !g->lake[i] && g->receiver[i] >= 0 && g->base[g->receiver[i]] < 0 && !g->lake[g->receiver[i]] ? (double)g->drained[i] : -1e18;
		else if (where == "river") score = g->river(i) && !g->lake[i] && g->base[i] > ratio(2, 100) ? (double)g->drained[i] : -1e18; // the biggest river, off the coast
		else if (where == "dry") score = g->base[i] > ratio(5, 100) && g->rain(i) < ratio(30, 100) ? real(g->tectonics.belt[i]) : -1; // a range in dry country: fans
		else if (where == "bedded") score = (g->province[i] == terragen::Province::Basin || g->province[i] == terragen::Province::Rift) && g->base[i] > ratio(5, 100) ? real(g->tectonics.belt[i]) * (1 - real(g->tectonics.fold[i])) : -1; // flat beds rising: mesas
		else if (where == "plain") score = real(g->tectonics.distance[i]) - 4 * real(g->tectonics.belt[i]) - b * 10;
		if (score > best) { best = score; pick = i; }
	}
	std::printf("land %.0f%% of cells · %.2f blocks a metre · radius %.0f blocks\n", 100.0 * land.size() / g->size(), perM, R);
	{
		std::vector<double> blocks;
		for (int32_t i = 0; i < g->size(); i += 7)
			if (g->base[i] >= 0)
				blocks.push_back(blocksOf(i));
		hypsometry("planet's land, blocks over sea (cell centres)", blocks);
	}

	double u0, v0;
	if (where.find(',') != std::string::npos) {
		u0 = std::stod(where.substr(0, where.find(',')));
		v0 = std::stod(where.substr(where.find(',') + 1));
	} else {
		const Dir d = sphere.centre(pick);
		u0 = real(World::u(d));
		v0 = real(World::v(d));
	}
	std::printf("region %s at %.4f,%.4f · %.0f km across\n", where.c_str(), u0, v0, km);

	// heights over an N × N lattice, `span` blocks across
	const double span = km * 1000 * perM, step = span / N;
	const double lat = (0.5 - v0) * M_PI;
	std::vector<double> h((size_t)N * N), wet((size_t)N * N);
	std::vector<uint8_t> biome((size_t)N * N), kind((size_t)N * N);
	// COARSE=1: as the far view reads it, leaving out what's narrower than a pixel
	std::unique_ptr<World::Coarse> coarse;
	if (std::getenv("COARSE"))
		coarse = std::make_unique<World::Coarse>(world, (int64_t)step);
	const auto started = std::chrono::steady_clock::now();
	for (int y = 0; y < N; y++)
		for (int x = 0; x < N; x++) {
			const double dv = (y - N / 2) * step / (M_PI * R), du = (x - N / 2) * step / (2 * M_PI * R * std::max(0.05, std::cos(lat)));
			const double u = u0 + du - std::floor(u0 + du), v = std::clamp(v0 + dv, 0.0, 1.0);
			// to the place's full precision (World::place is a Dir's: blocks apart)
			const double la = (0.5 - v) * M_PI, lo = (u - 0.5) * 2 * M_PI;
			const Place at((int32_t)std::llround(std::cos(la) * std::cos(lo) * PLACE_ONE), (int32_t)std::llround(std::sin(la) * PLACE_ONE),
					(int32_t)std::llround(std::cos(la) * std::sin(lo) * PLACE_ONE));
			const Ground gr = world.ground(at, world.sunlight(at, std::nullopt)); // (the year's mean sun)
			h[(size_t)y * N + x] = gr.terrain;
			biome[(size_t)y * N + x] = (uint8_t)gr.column.biome;
			kind[(size_t)y * N + x] = (uint8_t)gr.kind;
			wet[(size_t)y * N + x] = gr.kind != Water::None && gr.water > gr.terrain ? gr.water : -1e9;
		}
	std::printf("columns: %.1f µs each (one thread, the region's %d²)\n",
		std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() / ((double)N * N), N);
	hypsometry("region, blocks", h);
	// slopes: how steep the region is, in degrees between lattice points
	{
		std::vector<double> s;
		for (int y = 1; y < N - 1; y++)
			for (int x = 1; x < N - 1; x++) {
				const double gx = (h[(size_t)y * N + x + 1] - h[(size_t)y * N + x - 1]) / (2 * step);
				const double gy = (h[(size_t)(y + 1) * N + x] - h[(size_t)(y - 1) * N + x]) / (2 * step);
				s.push_back(std::atan(std::sqrt(gx * gx + gy * gy)) * 180 / M_PI);
			}
		std::sort(s.begin(), s.end());
		std::printf("region slope, degrees: p50 %.1f  p90 %.1f  p99 %.1f\n", s[s.size() / 2], s[s.size() * 9 / 10], s[s.size() * 99 / 100]);
	}

	// hillshade from the north-west, 45° up, slopes as they are; height tints; contours
	const double lo = *std::min_element(h.begin(), h.end()), hi = *std::max_element(h.begin(), h.end());
	const double contour = std::max(10.0, std::pow(10, std::floor(std::log10(std::max(1.0, hi - lo) / 4))) / 2);
	std::printf("contours every %.0f blocks (bold ×5) · %0.f to %.0f\n", contour, lo, hi);
	std::vector<unsigned char> rgb((size_t)N * N * 3);
	const double L[3] = { -0.5, 0.7071, -0.5 }; // x east, y up, z south: from the north-west
	for (int y = 0; y < N; y++)
		for (int x = 0; x < N; x++) {
			const auto at = [&](int xx, int yy) { return h[(size_t)std::clamp(yy, 0, N - 1) * N + std::clamp(xx, 0, N - 1)]; };
			const double gx = (at(x + 1, y) - at(x - 1, y)) / (2 * step), gz = (at(x, y + 1) - at(x, y - 1)) / (2 * step);
			const double nl = std::sqrt(gx * gx + 1 + gz * gz);
			const double shade = std::clamp((-gx * L[0] + L[1] - gz * L[2]) / nl / L[1], 0.2, 1.5);
			const double e = h[(size_t)y * N + x];
			double r, gg, b;
			// KINDS=1: water by its kind (sea dark, lake cyan, river red)
			if (std::getenv("KINDS") && kind[(size_t)y * N + x] == (uint8_t)Water::River) { r = 200; gg = 40; b = 40; }
			else if (std::getenv("KINDS") && kind[(size_t)y * N + x] == (uint8_t)Water::Lake) { r = 40; gg = 190; b = 200; }
			// SEABED=1: the ground under the sea, shaded, by its depth
			else if (std::getenv("SEABED") && e < 0) {
				const double t = std::clamp(-e / std::max(200.0, -lo), 0.0, 1.0);
				r = 120 - 90 * t; gg = 170 - 110 * t; b = 200 - 80 * t;
			}
			else if (wet[(size_t)y * N + x] > -1e8) { r = 60; gg = 110; b = 170; }
			else if (e < 0) { r = 40; gg = 70; b = 120; }
			else if (std::getenv("BIOMES")) {
				// BIOMES=1: the ground's biome (landforms too), as the planetarium's overlay
				static const unsigned char BC[(int)Biome::Count][3] = { { 18, 52, 96 }, { 30, 84, 140 }, { 200, 220, 235 },
					{ 214, 200, 140 }, { 222, 190, 118 }, { 176, 170, 84 }, { 104, 160, 72 }, { 52, 116, 56 }, { 28, 120, 60 },
					{ 60, 100, 80 }, { 150, 160, 140 }, { 120, 116, 110 }, { 236, 240, 244 }, { 96, 140, 40 }, { 160, 150, 96 },
					{ 168, 166, 160 }, { 84, 84, 88 }, { 164, 83, 44 }, { 216, 194, 60 }, { 245, 240, 230 }, { 40, 90, 70 }, { 150, 200, 60 } };
				const unsigned char *bc = BC[biome[(size_t)y * N + x]];
				r = bc[0]; gg = bc[1]; b = bc[2];
			} else {
				const double t = std::clamp(e / std::max(200.0, hi), 0.0, 1.0);
				r = 110 + 120 * t; gg = 150 + 50 * t - 40 * t * t; b = 90 + 110 * t * t;
			}
			r *= shade; gg *= shade; b *= shade;
			const bool line = std::floor(at(x + 1, y) / contour) != std::floor(e / contour) || std::floor(at(x, y + 1) / contour) != std::floor(e / contour);
			const bool bold = std::floor(at(x + 1, y) / (contour * 5)) != std::floor(e / (contour * 5)) || std::floor(at(x, y + 1) / (contour * 5)) != std::floor(e / (contour * 5));
			if (e >= 0 && line) { const double k = bold ? 0.45 : 0.75; r *= k; gg *= k; b *= k; }
			unsigned char *p = &rgb[((size_t)y * N + x) * 3];
			p[0] = (unsigned char)std::clamp(r, 0.0, 255.0); p[1] = (unsigned char)std::clamp(gg, 0.0, 255.0); p[2] = (unsigned char)std::clamp(b, 0.0, 255.0);
		}
	png(argv[2], N, N, rgb);
	return 0;
}
