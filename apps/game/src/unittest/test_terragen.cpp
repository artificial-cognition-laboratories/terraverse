// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "terragen/body.h"
#include "terragen/calendar.h"
#include "terragen/code.h"
#include "terragen/fixed.h"
#include "terragen/geology.h"
#include "terragen/noise.h"
#include "terragen/preview.h"
#include "terragen/sun.h"
#include "terragen/world.h"
#include "terragen/grid.h"
#include "terragen/sphere.h"
#include "terragen/climate.h"
#include <algorithm>
#include <string>

using namespace terragen;

class TestTerragen : public TestBase {
public:
	TestTerragen() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestTerragen"; }

	void runTests(IGameDef *gamedef);

	void testDecodeDefaults();
	void testDecodeParams();
	void testDecodeRejects();
	void testTrig();
	void testNoiseOnTheSphere();
	void testNoPoleOrSeam();
	void testSun();
	void testSunPath();
	void testCalendar();
	void testTemperature();
	void testLogExp();
	void testBodies();
	void testCoordinates();
	void testLandShareExact();
	void testWaterDrainsToSea();
	void testWaterFlowsDownhill();
	void testRiversOnLand();
	void testLakesAreFlat();
	void testSediment();
	void testPlates();
	void testVolcanoes();
	void testStrata();
	void testOresAndCaves();
	void testCover();
	void testClimate();
	void testClimateFromLand();
	void testOceanCurrents();
	void testSoil();
	void testDeterminismGolden();
};

static TestTerragen g_test_instance;

// A sample's place, picked as the flat map counted blocks: x east from
// longitude 0, z north from the equator, `size` round and size / 2 pole to pole.
static Place flat(const World &w, int32_t x, int32_t z)
{
	const int32_t size = w.code().size, half = size / 2;
	return World::place(ratio((((x + half) % size) + size) % size, size), ratio(half - z, size));
}

void TestTerragen::runTests(IGameDef *gamedef)
{
	TEST(testDecodeDefaults);
	TEST(testDecodeParams);
	TEST(testDecodeRejects);
	TEST(testTrig);
	TEST(testNoiseOnTheSphere);
	TEST(testNoPoleOrSeam);
	TEST(testSun);
	TEST(testSunPath);
	TEST(testCalendar);
	TEST(testTemperature);
	TEST(testLogExp);
	TEST(testBodies);
	TEST(testCoordinates);
	TEST(testLandShareExact);
	TEST(testWaterDrainsToSea);
	TEST(testWaterFlowsDownhill);
	TEST(testSediment);
	TEST(testRiversOnLand);
	TEST(testLakesAreFlat);
	TEST(testPlates);
	TEST(testVolcanoes);
	TEST(testStrata);
	TEST(testOresAndCaves);
	TEST(testCover);
	TEST(testClimate);
	TEST(testClimateFromLand);
	TEST(testOceanCurrents);
	TEST(testSoil);
	TEST(testDeterminismGolden);
}

// Codes for the water tests: default, a supercontinent, an archipelago (as the page encodes them).
static const char *const WATER_CODES[] = {
	"TF7.dGVycmE",
	"TF7.dGVycmE.AgIAplmZgICAgICAglU",
	"TF7.dGVycmE.AgL_QNlNgKaAgICAglU",
};

static bool near(fix a, fix b, fix tolerance)
{
	return fabsx(a - b) <= tolerance;
}

// Codes produced by the page's encodeWorldCode (ui/src/worldgen/config.ts).
void TestTerragen::testDecodeDefaults()
{
	WorldCode w = WorldCode::decode("TF7.YWJj");
	UASSERTEQ(std::string, w.seed, "abc");
	UASSERTEQ(int, w.size, 16384); // as ui/src/worldgen/config.ts DEFAULTS
	UASSERTEQ(std::string, w.star->id, "yellow");
	UASSERTEQ(int, w.continents, 4);
	UASSERTEQ(int, roundInt(w.tilt), 23);
	UASSERTEQ(int, w.year, 48);
	UASSERT(near(w.land, ratio(4, 10), 256));
	UASSERT(near(w.temperature, 0, 600));
}

void TestTerragen::testDecodeParams()
{
	// size 1 (62832 round: radius 10k), red dwarf, continents 6, tilt 40,
	// everything else default
	WorldCode w = WorldCode::decode("TF7.YWJj.AQC2ZoCAgICAgICA41U");
	UASSERTEQ(int, w.size, 62832);
	UASSERTEQ(std::string, w.star->id, "red");
	UASSERTEQ(int, w.continents, 6);
	UASSERTEQ(int, roundInt(w.tilt), 40);
	UASSERTEQ(int, w.year, 48);
}

void TestTerragen::testDecodeRejects()
{
	EXCEPTION_CHECK(CodeError, WorldCode::decode("XX.YWJj"));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF1.YWJj")); // an older generator: refused, not reinterpreted
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF2.YWJj")); // before plates and geology: refused too
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF7."));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF7.YWJj.AAA"));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF7.YW*j"));
}

void TestTerragen::testTrig()
{
	const fix tol = ONE / 500;
	UASSERT(near(terragen::sin(HALF_PI), ONE, tol));
	UASSERT(near(terragen::cos(0), ONE, tol));
	UASSERT(near(terragen::sin(PI), 0, tol));
	UASSERT(near(terragen::cos(PI), -ONE, tol));
	UASSERT(near(terragen::sin(-HALF_PI), -ONE, tol));
	UASSERT(near(terragen::sin(TWO_PI + HALF_PI), ONE, tol)); // reduction
	UASSERT(near(terragen::acos(0), HALF_PI, tol));
	UASSERT(near(terragen::acos(ONE), 0, tol));
	UASSERT(near(terragen::acos(-ONE), PI, tol));
	UASSERT(near(terragen::atan2(ONE, -ONE), PI - PI / 4, tol));
	UASSERT(near(terragen::atan2(-ONE, -ONE), -(PI - PI / 4), tol));
	UASSERT(near(terragen::sqrt(toFix(4)), toFix(2), 2));
}

void TestTerragen::testNoiseOnTheSphere()
{
	// across the east–west seam it carries on, and at a pole every
	// longitude is one place
	for (fix v = ONE / 16; v < ONE; v += ONE / 7)
		UASSERT(near(fbm(World::place(0, v), 1234, 5, 6, HALF), fbm(World::place(ONE - 1, v), 1234, 5, 6, HALF), ONE / 64));
	for (fix u = 0; u < ONE; u += ONE / 9) {
		// (cos 90° in fixed point is a hair over 0: the same place within a unit)
		UASSERT(near(noise(World::place(u, 0), 99, 3), noise(World::place(0, 0), 99, 3), ONE / 1024));
		UASSERT(near(noise(World::place(u, ONE), 99, 3), noise(World::place(0, ONE), 99, 3), ONE / 1024));
	}
}

void TestTerragen::testSun()
{
	const fix tol = ONE / 50;
	UASSERT(near(insolation(0, 0), ONE, tol));                       // equator at equinox
	fix lat60 = mul(PI, ratio(1, 3));
	UASSERT(annualInsolation(0, toFix(23)) > annualInsolation(lat60, toFix(23)));
	fix lat80 = mul(PI, ratio(80, 180));
	fix winter = declination(toFix(23), ratio(3, 4));               // northern winter
	UASSERT(near(insolation(lat80, winter), 0, tol));                // polar night
	fix summer = declination(toFix(23), ratio(1, 4));
	UASSERT(insolation(lat80, summer) > ONE);                        // midnight sun
}

static fix degrees(int32_t d)
{
	return (fix)(((int64_t)d * PI) / 180);
}

void TestTerragen::testSunPath()
{
	const fix tol = degrees(1);
	fix equinox = 0, summer = declination(toFix(23), ratio(1, 4)), winter = -summer;
	UASSERT(near(elevation(0, equinox, HALF), HALF_PI, tol));            // overhead at the equator
	UASSERT(near(elevation(degrees(60), equinox, HALF), degrees(30), tol));
	UASSERT(near(elevation(degrees(60), summer, HALF), degrees(53), tol)); // 90 − 60 + 23
	UASSERT(near(elevation(0, equinox, ratio(1, 4)), 0, tol));           // sunrise at six
	UASSERT(elevation(degrees(45), equinox, 0) < 0);                      // midnight
	UASSERT(elevation(degrees(80), winter, HALF) < 0);                    // polar night: no sunrise
	UASSERT(elevation(degrees(80), summer, 0) > 0);                       // midnight sun
	UASSERT(near(elevation(-degrees(60), winter, HALF), degrees(53), tol)); // the south mirrors

	const fix share = ONE / 100;
	UASSERT(near(dayShare(degrees(50), equinox), HALF, share));
	UASSERT(dayShare(degrees(50), summer) > dayShare(degrees(50), equinox));
	UASSERT(dayShare(-degrees(50), summer) < HALF);
	UASSERTEQ(fix, dayShare(degrees(80), winter), 0);
	UASSERTEQ(fix, dayShare(degrees(80), summer), ONE);
}

void TestTerragen::testCalendar()
{
	Calendar c = calendar(28, 30, ratio(1, 4));
	UASSERTEQ(uint32_t, c.year, 1u);
	UASSERTEQ(uint32_t, c.day, 2u);
	UASSERTEQ(fix, c.date, (fix)((2 * (int64_t)ONE + ratio(1, 4)) / 28));
	UASSERT(calendar(28, 27, ONE - 1).date < ONE);                       // never reaches the next year

	UASSERT(season(0, degrees(45)) == Season::Spring);
	UASSERT(season(ratio(3, 10), degrees(45)) == Season::Summer);
	UASSERT(season(ratio(3, 10), -degrees(45)) == Season::Winter);
	UASSERT(season(ratio(9, 10), -degrees(45)) == Season::Summer);
	UASSERT(season(ratio(6, 10), 0) == Season::Autumn);
}

void TestTerragen::testTemperature()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t quarter = w.code().size / 4;
	int32_t north = quarter + quarter / 2;   // ≈ 67°N
	fix midsummer = ratio(1, 4), midwinter = ratio(3, 4);
	fix afternoon = ratio(5, 8), night = ratio(1, 8);
	for (int32_t x = -quarter; x <= quarter; x += quarter / 2) {
		UASSERT(w.temperature(flat(w, x, quarter), midsummer, afternoon) > w.temperature(flat(w, x, quarter), midwinter, afternoon));
		UASSERT(w.temperature(flat(w, x, -quarter), midwinter, afternoon) > w.temperature(flat(w, x, -quarter), midsummer, afternoon));
		UASSERT(w.temperature(flat(w, x, 0), ratio(1, 8), afternoon) > w.temperature(flat(w, x, 0), ratio(1, 8), night));
		UASSERT(w.temperature(flat(w, x, 0), 0, HALF) > w.temperature(flat(w, x, north), 0, HALF));
	}
	fix t = w.temperature(flat(w, 0, 0), 0, HALF);
	UASSERT(t > toFix(-60) && t < toFix(60));
}

void TestTerragen::testLogExp()
{
	UASSERTEQ(fix, log2(ONE), 0);
	UASSERTEQ(fix, log2(toFix(8)), toFix(3));
	UASSERTEQ(fix, exp2(toFix(3)), toFix(8));
	UASSERT(near(log2(ratio(3, 2)), 38336, 4));          // 0.58496
	UASSERT(near(exp2(HALF), 92682, 4));                  // √2
	UASSERT(near(exp(ONE), 178145, 16));                  // e
	UASSERT(near(pow(toFix(9), HALF), toFix(3), 8));
	for (fix v = ratio(1, 64); v < toFix(1000); v += v / 3 + 1) // round trips across the range
		UASSERT(near(exp2(log2(v)), v, std::max<fix>(4, v / 2000)));
	UASSERTEQ(fix, exp2(toFix(-40)), 0);
	UASSERTEQ(fix, exp2(toFix(40)), INT32_MAX);
}

static fix real(double v)
{
	return (fix)(v * ONE);
}

// Our solar system from causes alone: every body must come out as itself.
void TestTerragen::testBodies()
{
	StarCauses sun{ 2, real(4.5), real(0.5) };
	std::vector<BodyCauses> c = {
		{ -1, real(1), real(0.017), real(1), real(0.35), real(0.6), real(24), true },          // Earth
		{ -1, real(0.72), real(0.007), real(0.82), real(0.3), real(0.6), real(5832), false },   // Venus
		{ -1, real(1.52), real(0.09), real(0.107), real(0.45), real(0.5), real(24.6), false },  // Mars
		{ -1, real(0.39), real(0.2), real(0.055), real(0.1), real(0.05), real(1408), false },   // Mercury
		{ 0, real(60), real(0.055), real(0.0123), real(0.35), real(0.05), real(655), false },   // the Moon
		{ -1, real(5.2), real(0.05), real(318), real(0.8), real(0.6), real(10), false },        // Jupiter
		{ 5, real(5.9), real(0.0041), real(0.015), real(0.55), real(0.05), real(42), false },   // Io
		{ 5, real(9.4), real(0.009), real(0.008), real(0.8), real(0.6), real(85), false },      // Europa
		{ -1, real(9.5), real(0.05), real(95), real(0.85), real(0.6), real(10.7), false },      // Saturn
		{ 8, real(20), real(0.029), real(0.0225), real(0.85), real(0.7), real(383), false },    // Titan
	};
	std::vector<BodyCard> b = bodies(sun, c);
	const Kind kinds[] = { Kind::Temperate, Kind::Hothouse, Kind::ColdDesert, Kind::Airless, Kind::Airless,
		Kind::Giant, Kind::Volcanic, Kind::IceShell, Kind::Giant, Kind::Hydrocarbon };
	for (size_t i = 0; i < b.size(); i++)
		UASSERT(b[i].kind == kinds[i]);

	const BodyCard &earth = b[0];
	UASSERT(earth.pressure > real(0.7) && earth.pressure < real(1.6));
	UASSERT(earth.temperature > toFix(278) && earth.temperature < toFix(300));
	UASSERT(earth.gases[(int)Gas::N2] > real(0.7) && earth.gases[(int)Gas::O2] > real(0.1));
	UASSERT(earth.field > HALF && earth.ocean && earth.plates);
	UASSERT(b[1].pressure > toFix(5));                      // Venus: thick CO₂
	UASSERT(b[1].gases[(int)Gas::CO2] > real(0.9));
	UASSERT(b[4].locked && b[6].tidal > toFix(10));        // the Moon faces us; Io is tidally heated

	// insides: Earth's molten core is about half its radius; Europa hides an
	// ocean under its ice; Jupiter is mostly envelope
	auto layer = [](const BodyCard &card, const char *name) -> const Layer * {
		for (const Layer &l : card.layers)
			if (std::string(l.name) == name)
				return &l;
		return nullptr;
	};
	UASSERT(layer(earth, "core") && layer(earth, "core")->molten);
	UASSERT(layer(earth, "core")->outer > real(0.45) && layer(earth, "core")->outer < real(0.65));
	UASSERT(layer(earth, "ocean") && !layer(b[2], "ocean"));
	UASSERT(layer(b[7], "ocean") && layer(b[7], "ice") && layer(b[7], "ocean")->outer < layer(b[7], "ice")->outer);
	UASSERT(layer(b[5], "envelope") && !layer(b[5], "crust"));

	auto told = [](const BodyCard &card, Event e) {
		for (const Moment &m : card.history)
			if (m.event == e)
				return true;
		return false;
	};
	UASSERT(told(b[2], Event::FieldFades) && told(b[2], Event::OceansFreeze)); // Mars's story
	UASSERT(told(b[1], Event::Runaway) && !told(b[3], Event::Runaway));
	UASSERT(told(earth, Event::LifeAppears) && told(earth, Event::OxygenRises));

	// the same causes, the same cards
	std::vector<BodyCard> again = bodies(sun, c);
	for (size_t i = 0; i < b.size(); i++)
		UASSERT(again[i].temperature == b[i].temperature && again[i].pressure == b[i].pressure);

	// impossible systems are refused
	std::vector<BodyCauses> bad = { c[4] }; // a moon with no planet before it
	EXCEPTION_CHECK(std::invalid_argument, bodies(sun, bad));
}

// The ground walked block by block over the north pole, and east across
// the date line (longitude ±180°), steps no more than the same walk on a
// meridian through mid-latitudes: no wall, no seam, nothing of a different
// kind there. (On the flat map both were edges.)
void TestTerragen::testNoPoleOrSeam()
{
	World w(WorldCode::decode("TF7.dGVycmE"));
	const double R = w.radius() / (double)ONE;
	// the steepest step of a walk of 20° along a great circle in plane (a, b), from angle `from`
	const auto walk = [&](double from, std::array<double, 3> a, std::array<double, 3> b) {
		int32_t was = INT32_MIN, steepest = 0;
		for (int32_t k = 0; k <= (int32_t)(R * M_PI / 9); k++) {
			const double t = from + k / R;
			const Dir d = normalise(std::llround((a[0] * std::cos(t) + b[0] * std::sin(t)) * (1 << 20)),
					std::llround((a[1] * std::cos(t) + b[1] * std::sin(t)) * (1 << 20)),
					std::llround((a[2] * std::cos(t) + b[2] * std::sin(t)) * (1 << 20)));
			const int32_t h = w.ground(d, w.sunlight(d, std::nullopt)).terrain;
			if (was != INT32_MIN)
				steepest = std::max(steepest, std::abs(h - was));
			was = h;
		}
		return steepest;
	};
	const double ten = M_PI / 18;
	const int32_t pole = walk(M_PI / 2 - ten, { 1, 0, 0 }, { 0, 1, 0 });      // up longitude 0, over, down 180°
	const int32_t seam = walk(M_PI - ten, { 1, 0, 0 }, { 0, 0, 1 });          // the equator across ±180°
	const int32_t middle = walk(M_PI / 6 - ten, { 0, 0, 1 }, { 0, 1, 0 });    // longitude 90°, 20°–40° north
	std::printf("TestTerragen: steepest step over the pole %d, across the date line %d, mid-latitudes %d\n",
			pole, seam, middle);
	UASSERT(pole <= std::max(2 * middle, 8));
	UASSERT(seam <= std::max(2 * middle, 8));
}

// A flat picture's (u, v) is a place and back (the preview's map), to a
// few units; v 0 is the north pole whatever u.
void TestTerragen::testCoordinates()
{
	for (int32_t i = 0; i < 16; i++)
		for (int32_t j = 1; j < 16; j++) {
			const fix u = ratio(i, 16), v = ratio(j, 16);
			const Dir d = World::place(u, v).dir();
			const fix du = World::u(d) - u;
			UASSERT(std::min(std::abs(du), ONE - std::abs(du)) < 16); // wraps
			UASSERT(std::abs(World::v(d) - v) < 16);
		}
	UASSERT(World::place(0, 0).dir().y > ONE - 4 && World::place(HALF, 0).dir().y > ONE - 4); // north
	UASSERT(World::place(HALF, ONE).dir().y < -ONE + 4);                                 // south
}

void TestTerragen::testLandShareExact()
{
	const Sphere &sphere = gridSphere();
	for (const char *text : WATER_CODES) {
		WorldCode code = WorldCode::decode(text);
		World w(code);
		const Grid &g = w.grid();
		int64_t land = 0, all = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			all += sphere.area(i);
			land += g.base[i] >= 0 ? sphere.area(i) : 0;
		}
		// within a fraction of a percent of the Land setting, by area
		fix share = (fix)((land << FRAC) / all);
		UASSERT(near(share, code.land, ONE / 200));
	}
}

void TestTerragen::testWaterDrainsToSea()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		for (int32_t i = 0; i < g.size(); i++) {
			if (g.base[i] < 0)
				continue;
			int32_t c = i, steps = 0;
			while (g.receiver[c] >= 0 && g.base[c] >= 0 && steps < g.size()) {
				c = g.receiver[c];
				steps++;
			}
			UASSERT(g.base[c] < 0); // every land cell's water reaches the sea
		}
	}
}

void TestTerragen::testWaterFlowsDownhill()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		for (int32_t i = 0; i < g.size(); i++) {
			int32_t r = g.receiver[i];
			if (r >= 0)
				UASSERT(g.filled[r] < g.filled[i]);
		}
	}
}

// What rivers wear away is laid down or carried out to the deep sea:
// nothing is made or lost, and some of it builds land.
void TestTerragen::testSediment()
{
	const Sphere &sphere = gridSphere();
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		int64_t lost = 0, laid = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			UASSERT(g.sediment[i] >= 0);
			lost += (int64_t)(g.base[i] - g.eroded[i]) * sphere.area(i);
			if (g.eroded[i] >= 0)
				laid += g.sediment[i];
		}
		UASSERTEQ(int64_t, lost + g.hollows, g.to_deep);
		UASSERT(laid > 0);
	}
}

// A lake has one surface: touching lake cells share a level, so real
// (flowing) water has nothing to spill down across it.
void TestTerragen::testLakesAreFlat()
{
	const Sphere &sphere = gridSphere();
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		int lakes = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			if (!g.lake[i])
				continue;
			lakes++;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
				if (g.lake[*p])
					UASSERTEQ(fix, g.lake_level[i], g.lake_level[*p]);
		}
		UASSERT(lakes > 0);
	}
}

// Plates cover the world; boundaries of every kind exist; mountains stand
// along the collisions.
void TestTerragen::testPlates()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		const Tectonics &t = g.tectonics;
		UASSERT(t.plates.size() >= 7);
		int kinds[4] = {};
		double near = 0, far = 0;
		int nn = 0, nf = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			UASSERT(t.plate[i] < t.plates.size());
			if (t.distance[i] == 0)
				kinds[(int)t.boundary[i]]++;
			if (g.eroded[i] < 0)
				continue;
			bool collision = t.boundary[i] == Boundary::Convergent && t.distance[i] < 5;
			(collision ? near : far) += g.eroded[i];
			(collision ? nn : nf)++;
		}
		UASSERT(kinds[(int)Boundary::Convergent] > 0 && kinds[(int)Boundary::Divergent] > 0 && kinds[(int)Boundary::Transform] > 0);
		if (nn > 50)
			UASSERT(near / nn > far / nf); // ranges along the collisions
	}
}

// Volcanoes stand where plates allow them, and some are active; a column
// at an active vent holds lava.
void TestTerragen::testVolcanoes()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	const Grid &g = w.grid();
	UASSERT(!g.volcanoes.empty());
	int active = 0;
	for (const Volcano &v : g.volcanoes) {
		active += v.active;
		UASSERT(v.radius > 0 && v.height > 0 && v.crater < v.radius);
	}
	UASSERT(active > 0);
	bool lava = false;
	for (const Volcano &v : g.volcanoes) {
		if (!v.active)
			continue;
		// beside it: past its slopes, to the north
		const Dir north = dirAt(latitudeOf(v.at) + w.angle(v.radius + 20), longitudeOf(v.at));
		Ground at = w.ground(v.at, w.sunlight(v.at, std::nullopt));
		Ground beside = w.ground(north, w.sunlight(north, std::nullopt));
		lava |= at.kind == Water::Lava;
		UASSERT(at.terrain > beside.terrain || at.kind == Water::Lava); // the cone stands up
	}
	UASSERT(lava);
}

// Rock follows the province: sediment over basement in basins, basalt under
// the sea floor, granite or gneiss in the shields.
void TestTerragen::testStrata()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	int seen[(int)Province::Count] = {};
	for (int32_t k = 0; k < 4000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Strata s = w.strata(at, g);
		seen[(int)s.province]++;
		Rock deep = w.rock(s, 400, g.terrain - 400);
		switch (s.province) {
		case Province::OceanFloor: UASSERT(deep == Rock::Basalt); break;
		case Province::Shield: UASSERT(deep == Rock::Granite || deep == Rock::Gneiss); break;
		case Province::Basin: {
			UASSERT(s.sediment >= 20);
			Rock top = w.rock(s, 0, g.terrain);
			UASSERT(top == Rock::Sandstone || top == Rock::Shale || top == Rock::Limestone || top == Rock::Obsidian);
			break;
		}
		default: break;
		}
	}
	for (int p = 0; p < (int)Province::Count; p++)
		UASSERT(seen[p] > 0); // every kind of ground somewhere
}

// Ores where their processes put them, and caves most of all in limestone.
void TestTerragen::testOresAndCaves()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	int ores[(int)Ore::Count] = {};
	int carved[(int)Rock::Count] = {}, blocks[(int)Rock::Count] = {};
	for (int32_t k = 0; k < 3000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Strata s = w.strata(at, g);
		for (int32_t depth = 0; depth < 140; depth++) {
			int32_t y = g.terrain - depth;
			Rock r = w.rock(s, depth, y);
			ores[(int)w.ore(s, r, depth, y)]++;
			// tunnels, above where deep caverns open (those are in the basement, whatever the rock)
			const bool cave = w.cave(s, r, depth, y, g.kind != Water::None) != Cave::None;
			if (depth <= 60) {
				blocks[(int)r]++;
				carved[(int)r] += cave;
			}
		}
	}
	for (Ore o : { Ore::Copper, Ore::Lead, Ore::Iron, Ore::Magnetite, Ore::Coal })
		UASSERT(ores[(int)o] > 0);
	UASSERT(ores[(int)Ore::None] > 50 * (ores[(int)Ore::Copper] + ores[(int)Ore::Iron])); // ore is the exception
	auto share = [&](Rock r) { return blocks[(int)r] ? (double)carved[(int)r] / blocks[(int)r] : 0.0; };
	UASSERT(blocks[(int)Rock::Limestone] > 0 && share(Rock::Limestone) > 2 * share(Rock::Granite));
}

// What can grow follows warmth and water: warm wet ground grows more than
// cold or dry; beyond the treeline nothing; trees stand only where it can grow.
void TestTerragen::testCover()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	double warm_wet = 0, cold = 0, dry = 0;
	int n_ww = 0, n_cold = 0, n_dry = 0, trees = 0, columns = 0;
	for (int32_t k = 0; k < 6000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Cover c = w.cover(at, g);
		UASSERT(c.density >= 0 && c.density <= ONE);
		if (g.kind != Water::None)
			continue;
		columns++;
		trees += c.tree;
		if (c.tree)
			UASSERT(c.density > 0);
		if (g.column.heat < -ratio(45, 100))
			UASSERT(c.density == 0); // beyond the treeline
		double d = c.density / 65536.0;
		if (g.column.heat > ratio(10, 100) && g.column.wet > ratio(20, 100)) { warm_wet += d; n_ww++; }
		else if (g.column.heat < -ratio(30, 100)) { cold += d; n_cold++; }
		else if (g.column.wet < -ratio(30, 100) && !c.bank) { dry += d; n_dry++; }
	}
	UASSERT(trees > 0 && trees < columns / 10);
	if (n_ww && n_cold) UASSERT(warm_wet / n_ww > cold / n_cold);
	if (n_ww && n_dry) UASSERT(warm_wet / n_ww > dry / n_dry);
}

// The climate of a real world: the air's bands show through, every biome
// the climate allows turns up somewhere, and fertility follows the rock.
void TestTerragen::testClimate()
{
	World w(WorldCode::decode("TF7.dGVycmE"));
	const Grid &g = w.grid();
	auto band = [&](double lo, double hi) {
		int64_t sum = 0, n = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			double lat = std::fabs(latitudeOf(gridSphere().centre(i)) / 65536.0 * 180 / M_PI);
			if (lat < lo || lat >= hi)
				continue;
			sum += g.rain(i);
			n++;
		}
		return (double)sum / n / ONE;
	};
	infostream << "climate: rain 0-10° " << band(0, 10) << ", 25-38° " << band(25, 38) << ", 50-65° " << band(50, 65) << std::endl;
	UASSERT(band(0, 10) > 3 * band(25, 38)); // the ITCZ, and the dry belts under the sinking air
	UASSERT(band(50, 65) > 2 * band(25, 38)); // the storm tracks
	// the same code, the same climate
	World again(WorldCode::decode("TF7.dGVycmE"));
	UASSERT(again.grid().rain_season[0] == g.rain_season[0] && again.grid().rain_season[1] == g.rain_season[1]);

	int counts[(int)Biome::Count] = {};
	double fertile[(int)Province::Count] = {};
	int cells[(int)Province::Count] = {};
	for (int32_t i = 0; i < g.size(); i++) {
		const Dir &d = gridSphere().centre(i);
		counts[(int)w.column(d, w.sunlight(d, std::nullopt)).biome]++;
		if (g.eroded[i] >= 0) {
			fertile[(int)g.province[i]] += g.fertility[i] / 65536.0;
			cells[(int)g.province[i]]++;
		}
	}
	for (Biome b : { Biome::Desert, Biome::Savanna, Biome::Forest, Biome::Jungle, Biome::Monsoon, Biome::Mediterranean })
		UASSERT(counts[(int)b] > 0);
	int volcanic = (int)Province::Volcanic, shield = (int)Province::Shield;
	if (cells[volcanic] && cells[shield])
		UASSERT(fertile[volcanic] / cells[volcanic] > fertile[shield] / cells[shield]);
}

// The climate over land we draw: a range in the westerlies has a wet side
// and a rain shadow; a continent in the tropics gets its rain in summer.
void TestTerragen::testClimateFromLand()
{
	WorldCode code = WorldCode::decode("TF7.dGVycmE");
	std::vector<fix> e((size_t)CLIMATE * CLIMATE, -ratio(3, 10));
	auto row = [](double lat) { return (int32_t)((90 - lat) / 180 * CLIMATE); };
	// a north–south range at 38–58° N, with lowland either side
	for (int32_t y = row(58); y <= row(38); y++)
		for (int32_t x = 50; x <= 74; x++)
			e[y * CLIMATE + x] = x >= 60 && x <= 63 ? ratio(6, 10) : ratio(5, 100);
	// a continent across 8–30° N
	for (int32_t y = row(30); y <= row(8); y++)
		for (int32_t x = 10; x <= 40; x++)
			e[y * CLIMATE + x] = ratio(8, 100);
	Climate c = climate(code, e);
	double windward = 0, leeward = 0, summer = 0, winter = 0;
	for (int32_t y = row(55); y <= row(42); y++) {
		for (int32_t x = 55; x <= 59; x++)
			windward += c.rain[0][y * CLIMATE + x] + c.rain[1][y * CLIMATE + x];
		for (int32_t x = 64; x <= 68; x++)
			leeward += c.rain[0][y * CLIMATE + x] + c.rain[1][y * CLIMATE + x];
	}
	for (int32_t y = row(24); y <= row(12); y++)
		for (int32_t x = 12; x <= 38; x++) {
			summer += c.rain[0][y * CLIMATE + x];
			winter += c.rain[1][y * CLIMATE + x];
		}
	infostream << "climate: windward " << windward / ONE << " leeward " << leeward / ONE
			<< "; tropical land summer " << summer / ONE << " winter " << winter / ONE << std::endl;
	UASSERT(windward > 1.5 * leeward);
	UASSERT(summer > 2 * winter);
	EXCEPTION_CHECK(std::invalid_argument, climate(code, std::vector<fix>(10, 0)));
}

// An ocean between two continents: a warm current up its western side (off
// the first continent's east coast), a cold one down its eastern side (off
// the second's west coast), which leaves that coast drier.
void TestTerragen::testOceanCurrents()
{
	WorldCode code = WorldCode::decode("TF7.dGVycmE");
	std::vector<fix> e((size_t)CLIMATE * CLIMATE, -ratio(3, 10));
	auto row = [](double lat) { return (int32_t)((90 - lat) / 180 * CLIMATE); };
	for (int32_t y = row(55); y <= row(5); y++)
		for (int32_t x = 0; x < CLIMATE; x++)
			if ((x >= 20 && x <= 40) || (x >= 70 && x <= 90))
				e[y * CLIMATE + x] = ratio(6, 100);
	Climate c = climate(code, e);
	double warm = 0, cold = 0, east_coast = 0, west_coast = 0;
	for (int32_t y = row(35); y <= row(20); y++) {
		warm += c.sea[y * CLIMATE + 42];
		cold += c.sea[y * CLIMATE + 68];
		for (int32_t x : { 38, 39, 40 })
			east_coast += c.rain[0][y * CLIMATE + x] + c.rain[1][y * CLIMATE + x];
		for (int32_t x : { 70, 71, 72 })
			west_coast += c.rain[0][y * CLIMATE + x] + c.rain[1][y * CLIMATE + x];
	}
	infostream << "currents: off the east coast " << warm / ONE << ", off the west coast " << cold / ONE
			<< "; rain on the east coast " << east_coast / ONE << ", west coast " << west_coast / ONE << std::endl;
	UASSERT(warm > 0 && cold < 0);
	UASSERT(east_coast > west_coast);
}

// Soil as a composition: every point round-trips through its index, and the
// soils follow why they differ: weathering makes clay where it's warm and
// wet, dry ground keeps sand, cold wet ground keeps peat.
void TestTerragen::testSoil()
{
	for (int32_t i = 0; i < SOIL_POINTS; i++) {
		Soil s = soilPoint((uint8_t)i);
		UASSERT(soilIndex(s) == i);
		UASSERT(std::abs(s.sand + s.silt + s.clay - ONE) < 8);
	}
	EXCEPTION_CHECK(std::invalid_argument, soilPoint((uint8_t)SOIL_POINTS));

	World w(WorldCode::decode("TF7.dGVycmE"));
	int32_t size = w.code().size;
	double wet_clay = 0, dry_clay = 0, cold_organic = 0, warm_organic = 0, steep_sand = 0, flat_sand = 0;
	int n_wet = 0, n_dry = 0, n_cold = 0, n_warm = 0, n_steep = 0, n_flat = 0;
	for (int32_t k = 0; k < 4000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		if (g.kind != Water::None)
			continue;
		Soil s = w.soil(at, g, w.strata(at, g));
		Cover c = w.cover(at, g);
		double clay = s.clay / 65536.0, sand = s.sand / 65536.0, organic = s.organic / 65536.0;
		if (g.column.heat > ratio(30, 100) && g.column.wet > ratio(30, 100)) { wet_clay += clay; n_wet++; }
		if (g.column.wet < -ratio(30, 100)) { dry_clay += clay; n_dry++; }
		if (g.column.heat < -ratio(20, 100) && g.column.wet > ratio(10, 100)) { cold_organic += organic; n_cold++; }
		if (g.column.heat > ratio(30, 100)) { warm_organic += organic; n_warm++; }
		if (c.slope > ratio(8, 10)) { steep_sand += sand; n_steep++; }
		if (c.slope < ratio(1, 10)) { flat_sand += sand; n_flat++; }
	}
	infostream << "soil: clay where warm and wet " << wet_clay / std::max(n_wet, 1) << " (" << n_wet << "), dry " << dry_clay / std::max(n_dry, 1)
			<< " (" << n_dry << "); organic cold-wet " << cold_organic / std::max(n_cold, 1) << ", warm " << warm_organic / std::max(n_warm, 1)
			<< "; sand steep " << steep_sand / std::max(n_steep, 1) << ", flat " << flat_sand / std::max(n_flat, 1) << std::endl;
	if (n_wet && n_dry) UASSERT(wet_clay / n_wet > dry_clay / n_dry);
	if (n_cold && n_warm) UASSERT(cold_organic / n_cold > warm_organic / n_warm);
	if (n_steep && n_flat) UASSERT(steep_sand / n_steep > flat_sand / n_flat);
}

void TestTerragen::testRiversOnLand()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		int rivers = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			if (!g.river(i))
				continue;
			rivers++;
			UASSERT(g.base[i] >= 0);
		}
		UASSERT(rivers > 0);
	}
}

static void describe(const char *code)
{
	World w(WorldCode::decode(code));
	Preview p = preview(w, std::nullopt, 64);
	int counts[(int)Biome::Count] = {};
	for (uint8_t b : p.biome)
		counts[b]++;
	infostream << "terragen " << code << ":";
	for (int b = 0; b < (int)Biome::Count; b++)
		infostream << " " << BIOME_NAMES[b] << "=" << counts[b];
	infostream << std::endl;
}

// Any change to generation changes this. That is the point: a changed world
// for an existing code needs a new code version, not a silent update.
void TestTerragen::testDeterminismGolden()
{
	describe("TF7.YWJj");
	describe("TF7.dGVycmE");
	World w(WorldCode::decode("TF7.YWJj.AQC2ZoCAgICAgICA41U"));
	Preview p = preview(w, std::nullopt, 48);
	uint32_t h = 2166136261u;
	auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
	for (size_t i = 0; i < p.biome.size(); i++) {
		mix((uint16_t)p.elevation[i]);
		mix((uint8_t)p.heat[i]);
		mix((uint8_t)p.wet[i]);
		mix((uint8_t)p.season[i]);
		mix(p.fertility[i]);
		mix(p.biome[i]);
	}
	// and what's underground: province, rock and ore down a few columns
	for (int32_t k = 0; k < 24; k++) {
		int32_t x = (k * 2311) % w.code().size - w.code().size / 2, z = (k * 977) % (w.code().size / 2) - w.code().size / 4;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Strata s = w.strata(at, g);
		mix((uint32_t)s.province);
		for (int32_t depth = 0; depth < 120; depth += 7) {
			Rock r = w.rock(s, depth, g.terrain - depth);
			mix((uint32_t)r);
			mix((uint32_t)w.ore(s, r, depth, g.terrain - depth));
			mix((uint32_t)w.cave(s, r, depth, g.terrain - depth, g.kind != Water::None));
		}
	}
	// what the world is made of, for a human reading --info output when tuning
	int counts[(int)Biome::Count] = {};
	int16_t lo = 32767, hi = -32767;
	for (size_t i = 0; i < p.biome.size(); i++) {
		counts[p.biome[i]]++;
		lo = std::min(lo, p.elevation[i]);
		hi = std::max(hi, p.elevation[i]);
	}
	infostream << "terragen golden: " << h << ", elevation " << lo / 10000.f << ".." << hi / 10000.f << ", biomes:";
	for (int b = 0; b < (int)Biome::Count; b++)
		infostream << " " << BIOME_NAMES[b] << "=" << counts[b];
	infostream << std::endl;
	// TF6: the world on the sphere (sprint 86; TF5, on the flat map, was
	// 2496371879). The code's size 1 is 62832 round since sprint 39.
	// (re-pinned: this code is 62 832 round, past where the radius overflowed a fix)
	// TF7, not yet released, re-pinned for sprint 88's sediment (was 1343096095)
	UASSERTEQ(uint32_t, h, 3935738997u);
}
