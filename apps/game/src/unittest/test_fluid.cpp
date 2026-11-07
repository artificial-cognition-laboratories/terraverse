// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "fluid/fluid.h"
#include "porting.h"
#include "filesys.h"
#include "weather/weather.h"
#include <cmath>

using namespace fluid;

class TestFluid : public TestBase {
public:
	TestFluid() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFluid"; }

	void runTests(IGameDef *gamedef);

	void testSpinShapesTheBands();
	void testCondensable();
	void testEarthClimateEmerges();
	void testRainShadow();
	void testStormsAndEvents();
	void testSaveAndDeterminism();
	void testTerraWeather();
};

static TestFluid g_test_instance;

static const matter::Matter &table()
{
	static matter::Matter m = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	return m;
}

static const double DAY = 86400;
static const double YEAR = 48 * DAY;
static const int N = 64;

static Planet earth(double day = DAY)
{
	return { 6.371e6, 9.81, day, 23.4 * M_PI / 180, YEAR, 1361 };
}

static Air earthAir()
{
	const matter::Matter &m = table();
	return { { { m.substance("nitrogen"), 0.79 }, { m.substance("oxygen"), 0.21 } }, 101325, m.substance("water"), 288 };
}

// An ocean world, with land where `land` says (height in m).
template <typename F>
static LayerOpts world(F land)
{
	LayerOpts o{ earth(), earthAir(), N, N, {}, {}, 42 };
	for (int r = 0; r < N; r++)
		for (int c = 0; c < N; c++) {
			double h = land(c, r);
			o.ground.push_back({ h, h <= 0, 0.5, 0, 0 });
		}
	return o;
}

// Run a year to settle, then average the next half year.
static Layer::Fields climate(Layer &layer)
{
	double dt = 3 * 3600;
	int steps = (int)(YEAR / dt);
	for (int k = 0; k < steps; k++)
		layer.step(dt, std::fmod(layer.time() / YEAR, 1.0));
	Layer::Fields mean = layer.fields();
	for (auto *f : { &mean.rain, &mean.u })
		std::fill(f->begin(), f->end(), 0.0);
	for (int k = 0; k < steps / 2; k++) {
		layer.step(dt, std::fmod(layer.time() / YEAR, 1.0));
		for (size_t i = 0; i < mean.rain.size(); i++) {
			mean.rain[i] += layer.fields().rain[i] / (steps / 2);
			mean.u[i] += layer.fields().u[i] / (steps / 2);
		}
	}
	return mean;
}

// The mean of a field over the rows within `lo..hi` degrees of latitude (both hemispheres).
static double band(const std::vector<double> &f, double lo, double hi)
{
	double sum = 0;
	int n = 0;
	for (int r = 0; r < N; r++) {
		double lat = std::fabs(90 - (r + 0.5) * 180.0 / N);
		if (lat < lo || lat > hi)
			continue;
		for (int c = 0; c < N; c++, n++)
			sum += f[r * N + c];
	}
	return sum / n;
}

void TestFluid::runTests(IGameDef *gamedef)
{
	TEST(testSpinShapesTheBands);
	TEST(testCondensable);
	TEST(testEarthClimateEmerges);
	TEST(testRainShadow);
	TEST(testStormsAndEvents);
	TEST(testSaveAndDeterminism);
	TEST(testTerraWeather);
}

void TestFluid::testSpinShapesTheBands()
{
	Air air = earthAir();
	Circulation e = circulate(earth(), air);
	double edge = e.hadley * 180 / M_PI;
	UASSERT(edge > 25 && edge < 36); // Earth: Hadley cells to about 30°
	UASSERTEQ(int, e.jets, 1);       // and one jet beyond: the westerlies
	// spin it faster and more bands fit; slow it down and the cells reach the poles
	Circulation fast = circulate(earth(DAY / 4), air);
	UASSERT(fast.hadley < e.hadley && fast.jets > e.jets);
	Circulation slow = circulate(earth(243 * DAY), air);
	UASSERT(slow.hadley >= M_PI / 2 - 1e-9 && slow.jets == 0);
	// a fast giant is striped
	Planet jupiter{ 6.99e7, 24.8, 9.9 * 3600, 0.05, YEAR, 50 };
	const matter::Matter &m = table();
	Air h2{ { { m.substance("hydrogen"), 1.0 } }, 1e5, m.substance("ammonia"), 165 };
	UASSERT(circulate(jupiter, h2).jets >= 5);
	// the trades blow east to west, the westerlies west to east
	UASSERT(flow(e, 15 * M_PI / 180, 0).u < 0);
	UASSERT(flow(e, 45 * M_PI / 180, 0).u > 0);
	UASSERT(flow(e, -45 * M_PI / 180, 0).u > 0);
	UASSERT(flow(e, 0, 0).w > 0);                // rising at the ITCZ
	UASSERT(flow(e, 30 * M_PI / 180, 0).w < 0);  // sinking at the edge: the deserts
}

void TestFluid::testCondensable()
{
	const matter::Matter &m = table();
	std::vector<const matter::Substance *> all;
	for (const char *id : { "nitrogen", "oxygen", "carbon_dioxide", "methane", "water" })
		all.push_back(&m.substance(id));
	UASSERT(pickCondensable(all, 288, 101325).id == "water");   // Earth
	UASSERT(pickCondensable(all, 94, 146700).id == "methane");  // Titan
	EXCEPTION_CHECK(FluidError, pickCondensable({ &m.substance("nitrogen") }, 288, 101325));
}

void TestFluid::testEarthClimateEmerges()
{
	Layer layer(world([](int, int) { return -1000.0; }));
	Layer::Fields f = climate(layer);
	double equator = band(f.rain, 0, 8), subtropics = band(f.rain, 20, 32), mid = band(f.rain, 40, 60);
	infostream << "fluid: rain mm/h equator " << equator << " subtropics " << subtropics << " mid " << mid << " all " << band(f.rain, 0, 90)
			<< "; wind 10-20° " << band(f.u, 10, 20) << " 40-55° " << band(f.u, 40, 55) << std::endl;
	UASSERT(equator > 2 * subtropics); // wet tropics, dry belts at ~30°
	UASSERT(mid > subtropics);         // the storm track rains again
	UASSERT(band(f.u, 10, 20) < 0);    // trades
	UASSERT(band(f.u, 40, 55) > 0);    // westerlies
	for (double v : layer.fields().q)
		UASSERT(std::isfinite(v) && v >= 0 && v < 0.05);
}

void TestFluid::testRainShadow()
{
	// a north–south range in the westerlies, rows 8..24 (~25..65° N), columns 30..33
	Layer layer(world([](int c, int r) {
		if (r < 6 || r > 26 || c < 26 || c > 40)
			return -1000.0;
		return c >= 31 && c <= 32 ? 3500.0 : 200.0;
	}));
	Layer::Fields f = climate(layer);
	double windward = 0, leeward = 0;
	for (int r = 9; r < 22; r++) {
		windward += f.rain[r * N + 29] + f.rain[r * N + 30];
		leeward += f.rain[r * N + 34] + f.rain[r * N + 35];
	}
	infostream << "fluid: windward " << windward << " leeward " << leeward << std::endl;
	UASSERT(windward > 1.5 * leeward);
}

void TestFluid::testStormsAndEvents()
{
	Layer layer(world([](int, int) { return -1000.0; }));
	bool front = false, tropical = false;
	for (int k = 0; k < 400; k++) {
		layer.step(3 * 3600, std::fmod(layer.time() / YEAR, 1.0));
		for (const Storm &s : layer.storms())
			(s.kind == StormKind::Front ? front : tropical) = true;
	}
	UASSERT(front && tropical);
	// somewhere it is raining hard enough for a thunderstorm
	bool thunder = false;
	for (int r = 0; r < N && !thunder; r++)
		for (int c = 0; c < N && !thunder; c++)
			thunder = layer.sample((c + 0.5) / N, (r + 0.5) / N, 0, 0.6).events & (1u << (int)Event::Thunderstorm);
	UASSERT(thunder);
	// air higher up is colder and thinner
	Sample low = layer.sample(0.5, 0.5, 0, 0.5), high = layer.sample(0.5, 0.5, 3000, 0.5);
	UASSERT(high.temperature < low.temperature - 15 && high.pressure < low.pressure * 0.8);
}

void TestFluid::testSaveAndDeterminism()
{
	auto make = [] { return Layer(world([](int c, int r) { return (c / 8 + r / 8) % 3 == 0 ? 400.0 : -800.0; })); };
	Layer a = make(), b = make();
	for (int k = 0; k < 40; k++) {
		a.step(3 * 3600, 0.1);
		b.step(3 * 3600, 0.1);
	}
	UASSERT(a.save() == b.save());
	Layer c = make();
	c.load(a.save());
	a.step(3 * 3600, 0.1);
	c.step(3 * 3600, 0.1);
	UASSERT(a.save() == c.save());
	EXCEPTION_CHECK(FluidError, c.load("nonsense"));
}

void TestFluid::testTerraWeather()
{
	terragen::World world(terragen::WorldCode::decode("TF18.dGVycmE"));
	std::string path = fs::TempPath() + DIR_DELIM "tf_weather_test.bin";
	fs::DeleteSingleFileOrEmptyDirectory(path);
	double day = 86400, now = weather::clock(3, 0.5);
	auto w = weather::Weather::open({ world, table(), path }, now);
	UASSERT(w->now() <= now && w->now() > now - 3 * 3600);
	// on the equator at longitude 0, near the ground and 3 km up
	const terragen::Place at = terragen::World::place(terragen::HALF, terragen::HALF);
	fluid::Sample s = w->sample(at, 1, now), up = w->sample(at, 120, now);
	// the weather swings about terragen's climate, which anchors it
	double climate = world.temperature(at, 0, terragen::ratio(3, 8)) / 65536.0 + 273.15;
	UASSERT(std::fabs(s.temperature - climate) < 10);
	UASSERT(s.pressure > 95000 && s.pressure < 107000);
	UASSERT(up.temperature < s.temperature);
	// a day on, saved; reopened from disk it carries on exactly as if never closed
	w->advance(now + day);
	auto again = weather::Weather::open({ world, table(), path }, now + day);
	w->advance(now + 2.5 * day);
	again->advance(now + 2.5 * day);
	UASSERT(w->layer().save() == again->layer().save());
	// another world's file is refused
	terragen::World other(terragen::WorldCode::decode("TF18.YWJj"));
	EXCEPTION_CHECK(FluidError, weather::Weather::open({ other, table(), path }, now));
	fs::DeleteSingleFileOrEmptyDirectory(path);

	// the create screen's year: a map per snapshot, and the seasons in it
	auto year = weather::Weather::year(world, table(), 4);
	UASSERTEQ(size_t, year.size(), 4u);
	UASSERTEQ(size_t, year[0].t.size(), 64u * 64u);
	double north_summer = 0, north_winter = 0;
	for (int c = 0; c < 64; c++) {
		north_summer += year[1].t[12 * 64 + c]; // ~56° N, a quarter through the year
		north_winter += year[3].t[12 * 64 + c];
	}
	UASSERT(north_summer > north_winter);
}
