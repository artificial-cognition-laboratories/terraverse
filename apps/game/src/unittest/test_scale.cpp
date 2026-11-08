// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "planet/atlas.h"
#include "terragen/code.h"
#include "terragen/world.h"
#include <chrono>
#include <cmath>
#include <vector>
#include <cstdio>

/*
	Planets from today's largest to the Earth's size (planning/sprint-39-tiles.md):
	what the world model costs to build and to ask, how rough its land is
	block to block, and whether the atlas lays it out. Prints a SCALE line
	a radius; asserts only what must hold at every size.
*/
class TestScale : public TestBase {
public:
	TestScale() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestScale"; }

	void runTests(IGameDef *gamedef);

	void testEverySize();
};

static TestScale g_test_instance;

void TestScale::runTests(IGameDef *gamedef)
{
	TEST(testEverySize);
}

void TestScale::testEverySize()
{
	using Clock = std::chrono::steady_clock;
	const auto ms = [](Clock::time_point a) { return std::chrono::duration<double, std::milli>(Clock::now() - a).count(); };
	std::vector<int32_t> spans;
	for (double radius : { 2607.0, 10000.0, 50000.0, 250000.0, 1000000.0, 6371000.0 }) {
		terragen::WorldCode code = terragen::WorldCode::decode("TF19.dGVycmE");
		code.size = (int32_t)std::lround(2 * M_PI * radius);
		// its own text: the grid is cached by a code's text, and a grid is built
		// for its size (volcanoes' reach, in blocks)
		code.grid_key += "@" + std::to_string(code.size);

		Clock::time_point t = Clock::now();
		const terragen::World world(code);
		const double built = ms(t);

		// a walk east along 40° north, a block of longitude at a time (cos 40°
		// of a block along the ground), in 64 stretches round the world
		// places made as the atlas makes them: from the point, to the block
		const double lat = 40 * M_PI / 180;
		const auto at = [&](int32_t x) {
			const double lon = 2 * M_PI * x / code.size, r = 1 << 30;
			return terragen::placeOf(std::llround(r * std::cos(lat) * std::cos(lon)), std::llround(r * std::sin(lat)),
					std::llround(r * std::cos(lat) * std::sin(lon)));
		};
		const terragen::fix sun = world.sunlight(at(0), std::nullopt);
		int32_t low = INT32_MAX, high = INT32_MIN;
		double steps = 0, steep = 0;
		size_t asked = 0;
		t = Clock::now();
		for (int s = 0; s < 64; s++) {
			const int32_t x0 = (int32_t)((int64_t)code.size * s / 64);
			int32_t was = world.ground(at(x0), sun).terrain;
			for (int32_t x = x0 + 1; x < x0 + 256; x++) {
				const int32_t h = world.ground(at(x), sun).terrain;
				low = std::min(low, h);
				high = std::max(high, h);
				steps += std::abs(h - was);
				steep = std::max(steep, (double)std::abs(h - was));
				was = h;
				asked++;
			}
		}
		const double perColumn = ms(t) * 1000 / asked;

		std::string laid;
		t = Clock::now();
		try {
			const planet::Atlas atlas = planet::Atlas::resident(terragen::Atlas::standard(radius));
			laid = "fits (" + std::to_string((int)ms(t)) + " ms)";
		} catch (const std::invalid_argument &e) {
			laid = e.what();
		}
		std::printf("SCALE radius=%.0f round=%d built=%.0fms column=%.2fus heights=%d..%d step=%.3f steepest=%.0f atlas=%s\n",
				radius, code.size, built, perColumn, low, high, steps / asked, steep, laid.c_str());
		UASSERT(high > low);
		// the ground is ground at every size: a block of longitude at 40° is
		// under a block along it, so no step is a cliff (a radius past
		// 32 767 blocks round overflowed a fix: steps of 32 000 blocks)
		UASSERT(steep < 40);
		spans.push_back(high - low);
	}
	// heights in metres (sprint 93): the land grows with the planet, as
	// blocks per metre does (√ of the radius), not the same everywhere
	for (size_t k = 1; k < spans.size(); k++)
		UASSERT(spans[k] > spans[k - 1]);
	UASSERT(spans.back() > spans.front() * 10);

	// the largest planet whose every tile is resident at once
	double fits = 1000, refused = 100000;
	while (refused - fits > 50) {
		const double r = (fits + refused) / 2;
		try {
			const planet::Atlas atlas = planet::Atlas::resident(terragen::Atlas::standard(r));
			fits = r;
		} catch (const std::invalid_argument &) {
			refused = r;
		}
	}
	const planet::Atlas largest = planet::Atlas::resident(terragen::Atlas::standard(fits));
	std::printf("SCALE resident up to radius %.0f (%zu tiles of %d)\n", fits, largest.tiles().size(),
			planet::Atlas::SLOTS * planet::Atlas::SLOTS);
}
