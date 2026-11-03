// A sheet of skies, for tuning by eye without the game: a row per body with air, a
// column per height of the star, each panel looking toward it from the horizon up.
//   g++ -std=c++17 -O2 -Iapps/game/src apps/game/src/terragen/*.cpp apps/game/src/air/*.cpp scripts/air-check/sky.cpp -o sky -lpthread
//   ./sky <code> <out.png> [real|game]
// real: the air at its true size. game: its height a tenth of the radius, as the far view has it now.
#include "air/tables.h"
#include "terragen/system.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
using namespace air;

static double real(terragen::fix v) { return (double)v / terragen::ONE; }

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
static uint32_t adler(const std::vector<unsigned char> &d)
{
	uint32_t a = 1, b = 0;
	for (unsigned char c : d) { a = (a + c) % 65521; b = (b + a) % 65521; }
	return b << 16 | a;
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
	be32(z, adler(raw));
	FILE *f = fopen(path, "wb");
	if (!f) { std::perror(path); std::exit(1); }
	const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	fwrite(sig, 1, 8, f);
	std::vector<unsigned char> head;
	be32(head, w); be32(head, h);
	head.insert(head.end(), { 8, 2, 0, 0, 0 });
	chunk(f, "IHDR", head);
	chunk(f, "IDAT", z);
	chunk(f, "IEND", {});
	fclose(f);
}

static unsigned char display(double v)
{
	v = 1 - std::exp(-v); // tone map, then sRGB
	v = v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055;
	return (unsigned char)std::lround(std::clamp(v, 0.0, 1.0) * 255);
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::fprintf(stderr, "usage: sky <code> <out.png> [real|game]\n");
		return 2;
	}
	const bool game = argc > 3 && std::string(argv[3]) == "game";
	const terragen::System system(terragen::SystemSpec::forWorld(terragen::WorldCode::decode(argv[1])));
	const double ELEVATIONS[] = { 60, 25, 8, 2, -1, -5 };
	const int PANEL = 150, TALL = 110, GAP = 4;
	const double EXPOSURE = 14;

	std::vector<std::pair<std::string, Medium>> rows;
	for (size_t b = 0; b < system.spec().bodies.size(); b++) {
		Medium m = mediumOf(system.cards()[b], system.radius(b), system.spec().star.type);
		const std::string name = system.spec().bodies[b].name;
		const terragen::BodyCard &card = system.cards()[b];
		std::printf("row? %-12s %-12s %8.3f bar %6.0f K  height %6.1f km  rayleigh g %.2e  particle g %.2e\n", name.c_str(),
				terragen::KIND_NAMES[(int)card.kind], real(card.pressure), real(card.temperature),
				m.rayleigh_height / 1000, m.rayleigh[1], m.particle_scatter[1]);
		if (m.none())
			continue;
		if (game)
			m = m.resized(m.radius, m.radius / 10);
		rows.emplace_back(name, m);
	}
	// Two the fixed system lacks, on Earth's card: thick hot cloud, and the same air under a red star.
	for (size_t b = 0; b < system.spec().bodies.size(); b++)
		if (system.spec().bodies[b].id == "earth") {
			terragen::BodyCard venus = system.cards()[b];
			venus.ocean = false;
			venus.pressure = terragen::toFix(92);
			venus.temperature = terragen::toFix(735);
			for (terragen::fix &g : venus.gases)
				g = 0;
			venus.gases[(int)terragen::Gas::CO2] = terragen::ONE;
			Medium hot = mediumOf(venus, system.radius(b), system.spec().star.type);
			Medium red = mediumOf(system.cards()[b], system.radius(b), 0);
			rows.emplace_back("(thick, hot)", game ? hot.resized(hot.radius, hot.radius / 10) : hot);
			rows.emplace_back("(M star)", game ? red.resized(red.radius, red.radius / 10) : red);
		}
	const int W = 6 * PANEL + 5 * GAP, H = (int)rows.size() * (TALL + GAP);
	std::vector<unsigned char> rgb((size_t)W * H * 3);
	for (size_t row = 0; row < rows.size(); row++) {
		const auto from = std::chrono::steady_clock::now();
		const Tables tables(rows[row].second);
		const double build = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
		// the eye adapts: partway to a dimmer star's light
		const double light = EXPOSURE * std::sqrt(rows[row].second.sunlight);
		for (int col = 0; col < 6; col++) {
			const double e = ELEVATIONS[col] * M_PI / 180;
			const Vec sun{ 0, std::sin(e), std::cos(e) };
			for (int y = 0; y < TALL; y++) {
				const double up = (70.0 * (TALL - 1 - y) / (TALL - 1) - 4) * M_PI / 180;
				for (int x = 0; x < PANEL; x++) {
					const double along = (x - PANEL / 2.0) / PANEL * 2.1; // about 120° across
					const Vec view{ std::cos(up) * std::sin(along), std::sin(up), std::cos(up) * std::cos(along) };
					const Rgb v = tables.sky(1.5, view, sun);
					unsigned char *p = &rgb[((size_t)(row * (TALL + GAP) + y) * W + col * (PANEL + GAP) + x) * 3];
					for (int c = 0; c < 3; c++)
						p[c] = display(v[c] * light);
				}
			}
		}
		const Rgb low = tables.transmittance(0, 0.03);
		std::printf("row %zu: %-12s tables %.0f ms  low sun's light %.2f %.2f %.2f\n", row, rows[row].first.c_str(), build, low[0], low[1], low[2]);
	}
	png(argv[2], W, H, rgb);
	return 0;
}
