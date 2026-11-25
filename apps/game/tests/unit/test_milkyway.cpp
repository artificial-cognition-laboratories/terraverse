// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "library/space/milkyway/milkyway.h"
#include <cmath>

/*
	The galaxy's sky (milkyway/): baked small, it's the picture the game
	shows. A band along the galaxy's plane over a dark sky, brightest and
	warmest toward its middle, the same however many threads drew it.
*/
class TestMilkyway : public TestBase {
public:
	TestMilkyway() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMilkyway"; }

	void runTests(IGameDef *gamedef);

	void testTheSameEachTime();
	void testABandOverADarkSky();
	void testBrightestTowardTheMiddle();
	void testTheBulgeIsWarm();
};

static TestMilkyway g_test_instance;

void TestMilkyway::runTests(IGameDef *gamedef)
{
	TEST(testTheSameEachTime);
	TEST(testABandOverADarkSky);
	TEST(testBrightestTowardTheMiddle);
	TEST(testTheBulgeIsWarm);
}

namespace {

// Each pixel's way in the galaxy's axes, as bake() works it out: its middle along
// −x from us, z its north. And the picture's light, by where it looks.
struct Sky
{
	milkyway::Picture picture;
	milkyway::Opts opts;

	struct Light
	{
		double r = 0, g = 0, b = 0;
		int n = 0;
		double grey() const { return (r + g + b) / 3 / n; }
	};
	template <typename F>
	Light where(F wanted) const;
};

Sky baked();
float dot(const milkyway::Way &a, const milkyway::Way &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

} // namespace

// A picture is a pure function of its options: rows are shared among
// threads as they come free, so a race would show as two pictures differing
void TestMilkyway::testTheSameEachTime()
{
	const Sky a = baked(), b = baked();
	UASSERT(a.picture.width == 256 && a.picture.height == 128);
	UASSERT(a.picture.rgb.size() == 256u * 128 * 3);
	UASSERT(a.picture.rgb == b.picture.rgb);
}

// Within 10° of the plane the sky's lit; past 45° from it, it's nearly black
void TestMilkyway::testABandOverADarkSky()
{
	const Sky s = baked();
	const auto band = s.where([](const milkyway::Way &g) { return std::abs(g.z) < std::sin(10 * M_PI / 180); });
	const auto dark = s.where([](const milkyway::Way &g) { return std::abs(g.z) > std::sin(45 * M_PI / 180); });
	UASSERT(band.n > 0 && dark.n > 0);
	UASSERT(dark.grey() < 30);
	UASSERT(band.grey() > 3 * dark.grey() + 20);
}

// Along the band, the half facing the galaxy's middle outshines the half
// facing away (where the bulge and most of the disc lie)
void TestMilkyway::testBrightestTowardTheMiddle()
{
	const Sky s = baked();
	const auto in = [](const milkyway::Way &g) { return std::abs(g.z) < std::sin(10 * M_PI / 180); };
	const auto toward = s.where([&](const milkyway::Way &g) { return in(g) && g.x < -0.5f; });
	const auto away = s.where([&](const milkyway::Way &g) { return in(g) && g.x > 0.5f; });
	UASSERT(toward.grey() > 1.5 * away.grey());
}

// The middle's old stars are warm: more red than blue toward it
void TestMilkyway::testTheBulgeIsWarm()
{
	const Sky s = baked();
	const auto bulge = s.where([](const milkyway::Way &g) { return g.x < -0.9f; });
	UASSERT(bulge.n > 0 && bulge.r > bulge.b);
}

namespace {

Sky baked()
{
	milkyway::Opts opts;
	opts.width = 256;
	opts.height = 128;
	opts.exposure = 0.45f; // as the planetarium bakes it
	return { milkyway::bake(opts), opts };
}

template <typename F>
Sky::Light Sky::where(F wanted) const
{
	// the galaxy's axes in the picture's, as bake() makes them
	const auto unit = [](milkyway::Way a) {
		const float k = 1 / std::sqrt(dot(a, a));
		return milkyway::Way{ a.x * k, a.y * k, a.z * k };
	};
	const milkyway::Way pole = unit(opts.pole);
	const float c = dot(opts.centre, pole);
	const milkyway::Way centre = unit({ opts.centre.x - pole.x * c, opts.centre.y - pole.y * c, opts.centre.z - pole.z * c });
	const milkyway::Way side{ pole.y * centre.z - pole.z * centre.y, pole.z * centre.x - pole.x * centre.z, pole.x * centre.y - pole.y * centre.x };
	Light out;
	for (uint32_t y = 0; y < picture.height; y++) {
		const double latitude = (0.5 - (y + 0.5) / picture.height) * M_PI;
		for (uint32_t x = 0; x < picture.width; x++) {
			const double longitude = (x + 0.5) / picture.width * 2 * M_PI;
			const milkyway::Way d{ (float)(std::cos(latitude) * std::cos(longitude)), (float)std::sin(latitude),
					(float)(std::cos(latitude) * std::sin(longitude)) };
			if (!wanted(milkyway::Way{ -dot(d, centre), dot(d, side), dot(d, pole) }))
				continue;
			const uint8_t *p = &picture.rgb[((size_t)y * picture.width + x) * 3];
			out.r += p[0];
			out.g += p[1];
			out.b += p[2];
			out.n++;
		}
	}
	return out;
}

} // namespace
