// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include <map>
#include <tuple>
#include "terragen/channels.h"
#include "terragen/body.h"
#include "terragen/calendar.h"
#include "terragen/code.h"
#include "terragen/fixed.h"
#include "terragen/geology.h"
#include "terragen/noise.h"
#include "terragen/preview.h"
#include "terragen/sun.h"
#include "terragen/system.h"
#include "terragen/world.h"
#include "terragen/worlds.h"
#include "terragen/system.h"
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
	void testChannels();
	void testRanges();
	void testCoarse();

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
	void testHistory();
	void testVolcanoes();
	void testStrata();
	void testOresAndCaves();
	void testCover();
	void testClimate();
	void testClimateFromLand();
	void testOceanCurrents();
	void testStars();
	void testHomeFollowsCard();
	void testSpin();
	void testDouble();
	void testBinary();
	void testEveryBodysLand();
	void testEnergyBalance();
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
	TEST(testChannels);
	TEST(testRanges);
	TEST(testCoarse);
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
	TEST(testHistory);
	TEST(testVolcanoes);
	TEST(testStrata);
	TEST(testOresAndCaves);
	TEST(testCover);
	TEST(testClimate);
	TEST(testClimateFromLand);
	TEST(testOceanCurrents);
	TEST(testStars);
	TEST(testHomeFollowsCard);
	TEST(testSpin);
	TEST(testDouble);
	TEST(testBinary);
	TEST(testEveryBodysLand);
	TEST(testEnergyBalance);
	TEST(testSoil);
	TEST(testDeterminismGolden);
}

// Codes for the water tests: default, a supercontinent, an archipelago (as the page encodes them).
static const char *const WATER_CODES[] = {
	"TF18.dGVycmE",
	"TF18.dGVycmE.AgKAqoCAAACmWZmAgICAgICCVQ",
	"TF18.dGVycmE.AgKAqoCAAP9A2U2ApoCAgICCVQ",
};

static bool near(fix a, fix b, fix tolerance)
{
	return fabsx(a - b) <= tolerance;
}

// Codes produced by the page's encodeWorldCode (ui/src/worldgen/config.ts).
void TestTerragen::testDecodeDefaults()
{
	WorldCode w = WorldCode::decode("TF18.YWJj");
	UASSERTEQ(std::string, w.seed, "abc");
	UASSERTEQ(int, w.size, 16384); // as ui/src/worldgen/config.ts DEFAULTS
	UASSERTEQ(std::string, w.star->id, "G");
	UASSERTEQ(int, w.continents, 7);
	UASSERTEQ(int, roundInt(w.tilt), 23);
	UASSERTEQ(int, w.year, 48);
	UASSERT(near(w.land, ratio(25, 100), 256));
	UASSERT(near(w.temperature, 0, 600));
}

void TestTerragen::testDecodeParams()
{
	// size 1 (62832 round: radius 10k), red dwarf, continents 6, tilt 40,
	// everything else default
	WorldCode w = WorldCode::decode("TF18.YWJj.AQCAqoCAALZmgICAgICAgIDjVQ");
	UASSERTEQ(int, w.size, 62832);
	UASSERTEQ(std::string, w.star->id, "M");
	UASSERTEQ(int, w.continents, 6);
	UASSERTEQ(int, roundInt(w.tilt), 40);
	UASSERTEQ(int, w.year, 48);
}

void TestTerragen::testDecodeRejects()
{
	EXCEPTION_CHECK(CodeError, WorldCode::decode("XX.YWJj"));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF1.YWJj")); // an older generator: refused, not reinterpreted
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF2.YWJj")); // before plates and geology: refused too
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF9."));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF18.YWJj.AACAqoCAAA"));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF18.YW*j"));
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

// As the page's encodeWorldCode writes them: an Earth at 2 AU, a moon called Mún.
static const char *const PAGE_BODIES = "TF18.YWJj.AAKAqoCAAG1mgICAgICAgICCVQ.AgVlYXJ0aAVFYXJ0aFCkxf8DaAkAACIrA1mZgAMuAW0ETcO6bqq7zAAAkTeAwdFWDU0aGvsO";

// Stars by class: the Sun's exact, heavy ones short-lived and fierce, poor ones poor in metal.
// A system as the page writes it (config.ts bodyBytes), on a code of default settings
static std::string codeWith(const SystemSpec &s)
{
	std::vector<uint8_t> b{ (uint8_t)s.bodies.size() };
	const auto text = [&](const std::string &t) { b.push_back((uint8_t)t.size()); b.insert(b.end(), t.begin(), t.end()); };
	const auto logged = [&](fix v) {
		const int16_t w = (int16_t)std::lround(std::log10(v / 65536.0) * 8000);
		b.push_back((uint8_t)(w & 255));
		b.push_back((uint8_t)((uint16_t)w >> 8));
	};
	for (const SystemBody &o : s.bodies) {
		text(o.id);
		text(o.name);
		b.insert(b.end(), { 128, 128, 128 });
		b.push_back(o.causes.parent < 0 ? 255 : (uint8_t)o.causes.parent);
		b.push_back((o.causes.life ? 1 : 0) | (o.playable ? 2 : 0) | (o.land ? 4 : 0) | (uint8_t)o.causes.spin << 3);
		logged(o.causes.orbit);
		logged(o.causes.mass);
		logged(o.causes.day);
		for (const fix v : { o.causes.eccentricity, o.causes.formed, o.causes.volatiles })
			b.push_back((uint8_t)std::lround(v * 255.0 / ONE));
		b.push_back((uint8_t)std::lround(o.phase * 255));
		b.push_back((uint8_t)(int8_t)o.inclination);
		b.push_back((uint8_t)std::lround(o.tilt * 2));
		if (o.land)
			b.insert(b.end(), o.land->begin(), o.land->end());
	}
	static const char ABC[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string out;
	uint32_t acc = 0;
	int bits = 0;
	for (const uint8_t v : b) {
		acc = acc << 8 | v;
		for (bits += 8; bits >= 6; bits -= 6)
			out.push_back(ABC[(acc >> (bits - 6)) & 63]);
	}
	if (bits > 0)
		out.push_back(ABC[(acc << (6 - bits)) & 63]);
	return "TF18.dGVycmE.AAKAqoCAAG1mgICAgICAgICCVQ." + out;
}

// the mean of a world's year at a place, °C
static fix yearly(const World &w, const Place &at)
{
	fix sum = 0;
	for (int k = 0; k < 4; k++)
		sum += w.temperature(at, ratio(k, 4), ratio(3, 8)) / 4;
	return sum;
}

// Free by default however close; a lock sets the day; a world facing its star
// has a hot side, a frozen side and a ring between to live on.
void TestTerragen::testBinary()
{
	const auto with = [](Pairing kind, fix separation, int orbit_percent) {
		SystemSpec s = SystemSpec::home();
		s.star.companion = { kind, 1, ratio(75, 100), separation };
		s.bodies[1].causes.orbit = ratio(orbit_percent, 100);
		// (only Earth is the test's)
		s.bodies.resize(2);
		s.bodies.erase(s.bodies.begin());
		s.bodies[0].playable = true;
		return s;
	};
	const System one{ SystemSpec::home() };
	UASSERT(!one.hasCompanion());

	// a close pair: their light adds, their masses both pull, they circle a point between
	const System close{ with(Pairing::Close, ratio(20, 100), 100) };
	const fix alone = luminosity(ONE), both = alone + luminosity(ratio(75, 100));
	UASSERT(near(close.cards()[0].sunlight, both, 64));
	UASSERT(close.cards()[0].temperature > one.cards()[1].temperature);
	UASSERT(std::abs(close.period(0) - 365.25 / std::sqrt(1.75)) < 1);
	const System::Vec a = close.starAt(false, 3), b = close.starAt(true, 3);
	UASSERT(std::abs(a.x * 1 + b.x * 0.75) < 1 && std::abs(a.z * 1 + b.z * 0.75) < 1);
	UASSERT(std::abs(std::hypot(a.x - b.x, a.z - b.z) - 0.2 * 149597870.7) < 5000); // (the separation is fixed point)

	// a wide one: the first holds the planet, the other is a faint extra
	const System wide{ with(Pairing::Wide, toFix(300), 100) };
	UASSERT(near(wide.cards()[0].sunlight, alone, 16));
	UASSERT(std::abs(wide.period(0) - one.period(1)) < 1e-6);
	UASSERT(wide.starAt(false, 3).x == 0);
	UASSERT(wide.secondShare(0, 0) < 1e-3);

	// orbits that wouldn't last, stars that would touch
	EXCEPTION_CHECK(std::invalid_argument, System{ with(Pairing::Close, ratio(50, 100), 100) });
	EXCEPTION_CHECK(std::invalid_argument, System{ with(Pairing::Wide, toFix(30), 2000) });
	SystemSpec touching = with(Pairing::Close, ratio(2, 100), 100);
	touching.star.type = 6;
	touching.star.mass = toFix(30);
	touching.star.age = ratio(3, 1000);
	touching.star.companion.type = 6;
	touching.star.companion.mass = toFix(30);
	EXCEPTION_CHECK(std::invalid_argument, System{ touching });

	// the seed's: a real companion, the same each time, whatever it lays out holding together
	int close_ones = 0, wide_ones = 0;
	for (const char *seed : { "dGVycmE", "YWJj", "c2VlZDE", "c2VlZDI", "c2VlZDM", "c2VlZDQ", "c2VlZDU", "c2VlZDY", "c2VlZDc" })
		for (int star = 0; star < STAR_COUNT; star++) {
			WorldCode c = WorldCode::decode(std::string("TF18.") + seed);
			c.style = SystemStyle::Binary;
			c.star = &STARS[star];
			c.star_mass = STARS[star].mass;
			c.age = std::min(c.age, lifetime(c.star_mass));
			const System s{ SystemSpec::forWorld(c) };
			UASSERT(s.hasCompanion());
			(s.spec().star.companion.kind == Pairing::Close ? close_ones : wide_ones)++;
			UASSERT(SystemSpec::companionOf(c).mass == s.spec().star.companion.mass);
		}
	UASSERT(close_ones > 0 && wide_ones > 0);
}

void TestTerragen::testDouble()
{
	// our Moon's balance point is inside Earth: no double
	const System usual{ SystemSpec::home() };
	UASSERT(!usual.partner(1) && !usual.partner(4));

	// a second Earth ten radii off
	const auto doubled = [](Spin both) {
		SystemSpec s = SystemSpec::home();
		SystemBody other = s.bodies[1];
		other.id = "other";
		other.playable = false;
		other.causes.parent = 1;
		other.causes.orbit = toFix(10);
		other.causes.eccentricity = 0;
		other.causes.spin = both;
		other.phase = 0.3;
		s.bodies[1].causes.spin = both;
		s.bodies[1].causes.eccentricity = 0;
		s.bodies.push_back(other);
		return s;
	};
	const System free{ doubled(Spin::Free) };
	const size_t other = free.spec().bodies.size() - 1;
	UASSERT(free.partner(1) == other && free.partner(other) == (size_t)1);
	// they're opposite each other round a point between them, which keeps the star's orbit
	const System::Vec a = free.position(1, 40), b = free.position(other, 40), mid = free.balance(1, 40);
	const double moon = free.spec().bodies[4].causes.mass / 65536.0; // (the Moon swings Earth a little too)
	const double apart = std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
	UASSERT(std::abs(apart - 10 * free.radius(1)) < free.radius(1) / 100);
	const System::Vec r = free.fromHost(other, 40), m = free.fromHost(4, 40);
	UASSERT(std::abs((a.x + r.x / 2 + m.x * moon / (1 + moon)) - mid.x) < 1);
	UASSERT(std::abs((b.x - r.x / 2 + m.x * moon / (1 + moon)) - mid.x) < 1);
	// both masses pull: √2 quicker than the same orbit round one alone
	const double a_km = 10 * free.radius(1);
	UASSERT(std::abs(free.period(other) - 2 * M_PI * std::sqrt(a_km * a_km * a_km / (398600.4418 * 2)) / 86400) < 0.01);
	// each flexes the other
	UASSERT(free.cards()[1].tidal > 0 && free.cards()[1].tidal == free.cards()[other].tidal);

	// held to each other: each stays fixed in the other's sky, through the compressed calendar and between days
	const System held{ doubled(Spin::Partner) };
	const double month = held.period(other) * 24;
	UASSERT(std::abs(held.cards()[1].day / 65536.0 - month) < month / 100);
	UASSERT(std::abs(held.cards()[other].day / 65536.0 - month) < month / 100);
	UASSERT(held.spec().bodies[1].tilt == 0);
	const auto seen = [&](const System &s, size_t from, size_t to, double worldDays) {
		const double day = s.dayFor(worldDays, 48), time = worldDays - std::floor(worldDays);
		const System::Vec p = s.position(from, day), q = s.position(to, day);
		const double turned = std::atan2(-p.z, -p.x) + 2 * M_PI * (s.clock(from, worldDays, 48).hour(time) - 0.5);
		return std::atan2(q.z - p.z, q.x - p.x) - turned;
	};
	for (double d : { 3.7, 10.1, 31.5 }) {
		UASSERT(std::abs(std::remainder(seen(held, other, 1, d) - seen(held, other, 1, 3.2), 2 * M_PI)) < 0.02);
		UASSERT(std::abs(std::remainder(seen(held, 1, other, d) - seen(held, 1, other, 3.2), 2 * M_PI)) < 0.02);
	}
	// spinning free, it crosses the sky
	UASSERT(std::abs(std::remainder(seen(free, 1, other, 3.45) - seen(free, 1, other, 3.2), 2 * M_PI)) > 0.5);
	// the lock reaches the code, and starts on either half
	const WorldCode code = WorldCode::decode(codeWith(doubled(Spin::Partner)));
	UASSERT(code.processes.day > toFix(24) && code.tilt == 0);

	// what can't hold
	SystemSpec close = doubled(Spin::Free);
	close.bodies.back().causes.orbit = ratio(19, 10);
	EXCEPTION_CHECK(std::invalid_argument, System{ close });
	// (a body many times lighter but puffed up with ice, torn apart outside where it would touch)
	SystemSpec torn = SystemSpec::home();
	torn.bodies[4].causes.orbit = ratio(150, 100);
	torn.bodies[4].causes.mass = ratio(1, 100);
	torn.bodies[4].causes.formed = ratio(90, 100);
	EXCEPTION_CHECK(std::invalid_argument, System{ torn });

	// everything the seed lays out holds together, doubles too
	for (const char *seed : { "dGVycmE", "YWJj", "c2VlZDE", "c2VlZDI", "c2VlZDM", "c2VlZDQ", "c2VlZDU" }) {
		WorldCode c = WorldCode::decode(std::string("TF18.") + seed);
		for (int style = (int)SystemStyle::SunLike; style < (int)SystemStyle::Count; style++) {
			c.style = (SystemStyle)style;
			const System s{ SystemSpec::generate(c) };
			if (c.style == SystemStyle::Double)
				UASSERT(s.partner(s.playable()).has_value());
		}
	}
}

void TestTerragen::testSpin()
{
	const System usual{ SystemSpec::home() };
	for (const BodyCard &c : usual.cards())
		UASSERT(!c.locked);
	UASSERT(usual.clock(1, 12.25, 48).turns == 1 && usual.clock(1, 12.25, 48).at == 0); // Earth's clock is the engine's

	// the Moon held to Earth: its day is its month, the same face to Earth for good
	SystemSpec held = SystemSpec::home();
	held.bodies[4].causes.spin = Spin::Partner;
	held.bodies[4].tilt = 7;
	const System moon{ held };
	UASSERT(moon.cards()[4].locked && moon.spec().bodies[4].tilt == 0);
	const double month = moon.period(4) * 24;
	UASSERT(std::abs(moon.cards()[4].day / 65536.0 - month) < month / 100);
	UASSERT(std::abs(std::remainder(moon.turning(4, 3.7) - 2 * M_PI * (moon.spec().bodies[4].phase + 3.7 / moon.period(4)), 2 * M_PI)) < 1e-9);

	// three turns in two orbits: Mercury's, whose day is two of its years
	SystemSpec mercury = SystemSpec::home();
	mercury.bodies[0].causes.spin = Spin::Res3_2;
	const System m{ mercury };
	UASSERT(std::abs(m.cards()[0].day / 65536.0 - m.period(0) * 24 / 1.5) < m.period(0) * 24 / 100);
	// (the playable body's year is Earth's: Cinder's orbits as the world's calendar compresses them)
	const double perWorldDay = m.period(1) / 48;
	UASSERT(std::abs(m.clock(0, 0, 48).turns - 0.5 / m.period(0) * perWorldDay) < 1e-9);
	UASSERT(std::abs(std::fmod(m.clock(0, 5, 48).hour(0.5) - m.clock(0, 4, 48).hour(0.5), 1.0) - std::fmod(m.clock(0, 0, 48).turns, 1.0)) < 1e-6);

	// what can't hold
	SystemSpec bad = SystemSpec::home();
	bad.bodies[4].causes.spin = Spin::Star;
	EXCEPTION_CHECK(std::invalid_argument, System{ bad });
	bad = SystemSpec::home();
	bad.bodies[1].causes.spin = Spin::Partner;
	EXCEPTION_CHECK(std::invalid_argument, System{ bad });

	// Earth facing the Sun
	SystemSpec facing = SystemSpec::home();
	facing.bodies[1].causes.spin = Spin::Star;
	const WorldCode code = WorldCode::decode(codeWith(facing));
	UASSERT(code.processes.facing_star && code.tilt == 0);
	UASSERT(code.processes.day > toFix(8000));
	const World w(code);
	const auto at = [&](int degrees) { // on the equator, from under the star
		const double a = degrees * M_PI / 180;
		return Place(normalise((int64_t)(std::cos(a) * ONE), 0, (int64_t)(std::sin(a) * ONE)));
	};
	const fix hot = yearly(w, at(0)), ring = yearly(w, at(90)), night = yearly(w, at(180));
	UASSERT(hot > toFix(45));
	UASSERT(night < -toFix(25));
	UASSERT(ring > -toFix(15) && ring < toFix(30));
	UASSERT(yearly(w, at(80)) > 0 && yearly(w, at(80)) < toFix(30)); // (where a new player starts)
	// no seasons, no day's swing: the sun never moves
	UASSERT(w.temperature(at(90), ratio(1, 4), ratio(1, 8)) == w.temperature(at(90), ratio(3, 4), ratio(5, 8)));
	// the zones are round the star, not the pole: both poles lie on the ring
	UASSERT(std::abs((double)(yearly(w, Place(Dir{ 0, ONE, 0 })) - ring)) < 8 * ONE);
}

void TestTerragen::testHomeFollowsCard()
{
	// the default world is Earth's, with nothing pushing its climate
	const WorldCode earth = WorldCode::decode("TF18.dGVycmE");
	UASSERT(earth.processes.warming == 0);
	UASSERT(earth.processes.water && earth.processes.life && earth.processes.regime == Regime::Plates);

	const Place tropic = dirAt(ratio(20, 100), toFix(1));
	const fix warm = yearly(World(earth), tropic);
	UASSERT(warm > toFix(10) && warm < toFix(40));

	// moved out to 5 AU: its card freezes, so does its land
	SystemSpec far = SystemSpec::home();
	far.bodies[1].causes.orbit = toFix(5);
	const WorldCode frozen = WorldCode::decode(codeWith(far));
	UASSERT(!frozen.processes.water && !frozen.processes.life);
	UASSERT(frozen.processes.warming < -toFix(30)); // (carbon dioxide builds up as the seas freeze: the card keeps it at 251 K)
	const World cold(frozen);
	UASSERT(yearly(cold, tropic) < 0);
	UASSERT(cold.ground(tropic, cold.sunlight(tropic, std::nullopt)).kind == Water::None);

	// the body's tilt is the world's, and moving a moon keeps the land
	SystemSpec tipped = SystemSpec::home();
	tipped.bodies[1].tilt = 40;
	UASSERT(roundInt(WorldCode::decode(codeWith(tipped)).tilt) == 40);
	SystemSpec moved = SystemSpec::home(), moved2 = moved;
	moved.bodies[4].phase = 0.2;
	moved2.bodies[4].phase = 0.7;
	UASSERTEQ(std::string, WorldCode::decode(codeWith(moved)).grid_key, WorldCode::decode(codeWith(moved2)).grid_key);

	// starting on the Moon: a dry, airless lid; Earth still there to visit; its year is Earth's
	SystemSpec moon = SystemSpec::home();
	moon.bodies[1].playable = false;
	moon.bodies[4].playable = true;
	const WorldCode there = WorldCode::decode(codeWith(moon));
	UASSERT(!there.processes.water && !there.processes.life);
	UASSERT(there.processes.regime == Regime::Lid && there.processes.bare == Surface::Regolith);
	const std::vector<BodyWorld> worlds = bodyWorlds(there, Atlas::standard(there.size / (2 * M_PI)));
	UASSERTEQ(std::string, worlds[0].id, "moon");
	UASSERT(std::any_of(worlds.begin(), worlds.end(), [](const BodyWorld &b) { return b.id == "earth"; }));
	UASSERT(worlds[0].surroundings.pressure == 0);
	const System sys(SystemSpec::forWorld(there));
	UASSERT(std::abs(sys.year() - 365.25) < 1);

	// a giant can't be stood on: refused, not built
	SystemSpec giant = SystemSpec::home();
	giant.bodies[1].playable = false;
	giant.bodies[3].playable = true;
	EXCEPTION_CHECK(CodeError, WorldCode::decode(codeWith(giant)));

	// a sun-like seed round each ordinary star is somewhere to live
	for (int kind : { 1, 2, 3 }) {
		int living = 0;
		for (int seed = 0; seed < 50; seed++) {
			WorldCode c = WorldCode::decode("TF18.dGVycmE");
			c.seed_hash = seed * 2654435761u;
			c.style = SystemStyle::SunLike;
			c.star = &STARS[kind];
			c.star_mass = STARS[kind].mass;
			c.age = std::min(c.age, lifetime(c.star_mass));
			const System s{ SystemSpec::forWorld(c) };
			const BodyCard &home = s.cards()[s.playable()];
			if (home.ocean && home.kind == Kind::Temperate)
				living++;
		}
		infostream << "sun-like homes round " << STARS[kind].id << ": " << living << "/50 live" << std::endl;
		UASSERT(living >= 45);
	}
}

void TestTerragen::testEveryBodysLand()
{
	const auto moonOf = [](const SystemSpec &s) {
		const WorldCode home = WorldCode::decode(codeWith(s));
		for (const BodyWorld &b : bodyWorlds(home, Atlas::standard(250000)))
			if (b.id == "moon")
				return b.code;
		throw std::logic_error("no moon to stand on");
	};
	const WorldCode usual = moonOf(SystemSpec::home());
	UASSERT(usual.mountains == WorldCode::decode("TF18.dGVycmE").mountains);

	// a slider reaches the moon's own land, and nothing else's
	LandBytes high = defaultLand();
	high[3] = 255; // mountains
	high[4] = 255; // roughness
	high[6] = 0;   // humidity
	SystemSpec set = SystemSpec::home();
	set.bodies[4].land = high;
	const WorldCode rough = moonOf(set);
	UASSERT(rough.mountains == ONE);
	UASSERT(rough.grid_key != usual.grid_key);
	UASSERTEQ(std::string, WorldCode::decode(codeWith(set)).grid_key, WorldCode::decode(codeWith(SystemSpec::home())).grid_key);
	const World flat(usual), tall(rough);
	int differ = 0;
	for (int i = 0; i < 64; i++) {
		const Place at = dirAt(ratio(i - 32, 40), ratio(i * 5, 8));
		if (flat.ground(at, flat.sunlight(at, std::nullopt)).terrain != tall.ground(at, tall.sunlight(at, std::nullopt)).terrain)
			differ++;
	}
	UASSERT(differ > 32);

	// the code's land settings are the start body's: they don't reach the moon
	WorldCode home = WorldCode::decode(codeWith(SystemSpec::home()));
	home.mountains = 0;
	UASSERT(bodyWorlds(home, Atlas::standard(250000))[1].code.mountains != 0);

	// another body's edit moves no moon's land; its grid is kept by what decides it
	SystemSpec nudged = SystemSpec::home();
	nudged.bodies[0].phase = 0.9;
	nudged.bodies[3].inclination = 3;
	UASSERTEQ(std::string, moonOf(nudged).grid_key, usual.grid_key);
}

void TestTerragen::testStars()
{
	UASSERT(luminosity(ONE) == ONE);
	UASSERT(luminosity(toFix(30)) > toFix(10000));
	UASSERT(luminosity(ratio(30, 100)) < ratio(2, 100));
	UASSERT(lifetime(ONE) == toFix(10) || near(lifetime(ONE), toFix(10), 64));
	UASSERT(lifetime(toFix(30)) < ratio(1, 100));

	const std::vector<BodyCauses> earth = { { -1, real(1), real(0.017), real(1), real(0.35), real(0.6), real(24), true } };
	const BodyCard ours = bodies({ 2, ONE, 0, real(4.5), real(0.5) }, earth)[0];
	UASSERTEQ(std::string, KIND_NAMES[(int)ours.kind], KIND_NAMES[(int)Kind::Temperate]);

	// an O star's world: a few million years old and baking, however close, at any distance in range
	const BodyCard baked = bodies({ 6, toFix(30), 0, ratio(3, 1000), real(0.5) }, earth)[0];
	UASSERT(baked.temperature > toFix(1000));
	UASSERT(!baked.ocean);
	const BodyCauses close = { -1, real(0.05), 0, real(1), real(0.35), real(0.6), real(24), false };
	const BodyCard burnt = bodies({ 6, toFix(60), 0, ratio(3, 1000), real(0.5) }, { close })[0];
	UASSERT(burnt.temperature > toFix(1000));

	const BodyCard poor = bodies({ 2, ONE, -ONE, real(4.5), real(0.5) }, earth)[0];
	UASSERT(poor.metal < ours.metal);

	// as the page encodes them (ui/src/worldgen/config.ts): the star's whole description
	const WorldCode oh = WorldCode::decode("TF18.YWJj.AQa1VQVAAG1mgICAgICAgICCVQ");
	UASSERTEQ(std::string, oh.star->id, "O");
	UASSERT(near(oh.star_mass, toFix(40), ONE / 8));
	UASSERT(oh.metallicity == -HALF);
	UASSERT(oh.age >= ratio(2, 1000) && oh.age <= lifetime(oh.star_mass)); // however it rounds
	UASSERT(oh.activity == ONE / 4);
	System{ SystemSpec::forWorld(oh) };

	// a system from the seed: its own, the same every time, in the model's range
	WorldCode a = WorldCode::decode("TF18.YWJj.AAKAqoCAAW1mgICAgICAgICCVQ");
	WorldCode b = a;
	b.seed_hash ^= 77;
	const SystemSpec one = SystemSpec::generate(a), again = SystemSpec::generate(a), other = SystemSpec::generate(b);
	UASSERT(one.bodies.size() == again.bodies.size());
	for (size_t i = 0; i < one.bodies.size(); i++) {
		UASSERTEQ(std::string, one.bodies[i].name, again.bodies[i].name);
		UASSERT(one.bodies[i].causes.orbit == again.bodies[i].causes.orbit);
	}
	bool differs = one.bodies.size() != other.bodies.size();
	for (size_t i = 0; !differs && i < one.bodies.size(); i++)
		differs = one.bodies[i].causes.orbit != other.bodies[i].causes.orbit;
	UASSERT(differs);
	// any star, any style, any seed: it can exist, and the home has ground under it
	for (uint32_t seed = 0; seed < 200; seed++)
		for (int style = (int)SystemStyle::SunLike; style < (int)SystemStyle::Count; style++)
			for (int kind = 0; kind < STAR_COUNT; kind++) {
				WorldCode c = a;
				c.seed_hash = seed * 2654435761u;
				c.style = (SystemStyle)style;
				c.star = &STARS[kind];
				c.star_mass = STARS[kind].mass;
				c.age = std::min(c.age, lifetime(c.star_mass));
				const System made{ SystemSpec::forWorld(c) };
				UASSERT(!made.cards()[made.playable()].giant);
			}

	// the page's bodies (bodyBytes): one planet, one moon
	const SystemSpec read = SystemSpec::forWorld(WorldCode::decode(PAGE_BODIES));
	UASSERT(read.bodies.size() == 2);
	UASSERTEQ(std::string, read.bodies[1].name, "Mún");
	UASSERT(read.bodies[1].causes.parent == 0);
	UASSERT(near(read.bodies[0].causes.orbit, toFix(2), 64));
	UASSERT(near(read.bodies[1].causes.mass, ratio(1, 100), 8));
	UASSERT(read.bodies[0].playable);
	System{ read };
	EXCEPTION_CHECK(CodeError, SystemSpec::read({ 1, 9 }));

	// a star can't have outlived itself
	EXCEPTION_CHECK(std::invalid_argument, bodies({ 6, toFix(30), 0, toFix(4), real(0.5) }, earth));
}

// Our solar system from causes alone: every body must come out as itself.
void TestTerragen::testBodies()
{
	StarCauses sun{ 2, ONE, 0, real(4.5), real(0.5) };
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
	UASSERT(b[6].tidal > toFix(10));        // the Moon faces us; Io is tidally heated

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
	World w(WorldCode::decode("TF18.dGVycmE"));
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
			// every land cell's water reaches the sea, or a closed basin's lake or
			// dry floor (where it evaporates: keepRealLakes)
			UASSERT(g.base[c] < 0 || g.lake[c] || g.pan[c]);
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

// What the plates push up and rivers wear away is laid down or carried out
// to the deep sea (creep only moves it): nothing is made or lost, and some
// of it builds land. Gold and tin settle out below the mountains.
void TestTerragen::testSediment()
{
	const Sphere &sphere = gridSphere();
	int32_t any_rich = 0;
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
		UASSERTEQ(int64_t, lost + g.uplifted + g.hollows, g.to_deep);
		UASSERT(laid > 0);
		// placers: only in what was laid, below the mountain belts' cores, and rare
		int32_t rich = 0, land = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			UASSERT(g.placer[i] >= 0 && g.placer[i] <= ONE);
			if (g.placer[i] > 0)
				UASSERT(g.sediment[i] > 0 && g.tectonics.belt[i] <= VEIN_CORE);
			land += g.eroded[i] >= 0;
			rich += g.placer[i] >= ratio(1, 4);
		}
		UASSERT(rich * 10 < land);
		any_rich += rich;
	}
	UASSERT(any_rich > 0); // (a world without high mountains has none)
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

// The plates' history: continental crust is kept (what collisions fold
// away and arcs build is counted; the rest moves cell to cell, which the
// nearest-cell handover only keeps roughly), ocean floor forms where plates
// part, and things happen: collisions, openings, arcs.
void TestTerragen::testHistory()
{
	const Sphere &sphere = gridSphere();
	for (const char *text : WATER_CODES) {
		const History h = history(WorldCode::decode(text));
		UASSERT(h.cells.size() == (size_t)sphere.size());
		int64_t continental = 0, young = 0;
		for (size_t i = 0; i < h.cells.size(); i++) {
			const History::Cell &c = h.cells[i];
			UASSERT(c.plate < h.plates.size());
			UASSERT(c.continental == (c.formed != Formed::Ocean));
			if (c.continental)
				continental += sphere.area((int32_t)i);
			else if (c.age < HISTORY_STEPS)
				young++;
		}
		UASSERT(continental == h.continental_end);
		// within a fifth of what the rules alone would leave
		const int64_t expected = h.continental_start + h.made - h.lost;
		UASSERT(std::abs(h.continental_end - expected) * 5 < expected);
		UASSERT(young > 0);
		bool kinds[3] = {};
		for (const History::Event &e : h.events) {
			UASSERT(e.step >= 0 && e.step < HISTORY_STEPS);
			kinds[(int)e.kind] = true;
		}
		UASSERT(kinds[(int)History::Kind::Collision] || kinds[(int)History::Kind::Arc]);
		UASSERT(kinds[(int)History::Kind::Opening]);
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
	// every kind of ground somewhere, but a lid's (on a plate world: none)
	for (int p = 0; p < (int)Province::Count; p++)
		UASSERT((seen[p] > 0) == (p != (int)Province::Highland && p != (int)Province::Mare));
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
	World w(WorldCode::decode("TF18.dGVycmE"));
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
	World again(WorldCode::decode("TF18.dGVycmE"));
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

// Land drawn on the grid's cells by latitude (degrees) and column x (0..128
// eastward round the world), and a field's mean over a patch the same way.
template <typename Height>
static std::vector<fix> drawn(Height height)
{
	const Sphere &s = gridSphere();
	std::vector<fix> e(s.size());
	for (int32_t i = 0; i < s.size(); i++) {
		const Dir &d = s.centre(i);
		e[i] = height(latitudeOf(d) * 180.0 / PI, World::u(d) * 128.0 / ONE);
	}
	return e;
}
template <typename Where>
static double meanOver(const std::vector<fix> &f, Where where)
{
	const Sphere &s = gridSphere();
	double sum = 0;
	int n = 0;
	for (int32_t i = 0; i < s.size(); i++) {
		const Dir &d = s.centre(i);
		if (where(latitudeOf(d) * 180.0 / PI, World::u(d) * 128.0 / ONE)) {
			sum += f[i] / 65536.0;
			n++;
		}
	}
	return n ? sum / n : 0;
}

// The climate over land we draw: a range in the westerlies has a wet side
// and a rain shadow; a continent in the tropics gets its rain in summer.
void TestTerragen::testClimateFromLand()
{
	WorldCode code = WorldCode::decode("TF18.dGVycmE");
	const std::vector<fix> e = drawn([](double lat, double x) {
		if (lat >= 38 && lat <= 58 && x >= 50 && x <= 74) // a north–south range, lowland either side
			return x >= 60 && x <= 63 ? ratio(6, 10) : ratio(5, 100);
		if (lat >= 8 && lat <= 30 && x >= 10 && x <= 40) // a continent in the tropics
			return ratio(8, 100);
		return -ratio(3, 10);
	});
	Climate c = climate(code, e);
	std::vector<fix> year(e.size());
	for (size_t i = 0; i < e.size(); i++)
		year[i] = c.rain[0][i] + c.rain[1][i];
	const double windward = meanOver(year, [](double lat, double x) { return lat >= 42 && lat <= 55 && x >= 55 && x <= 59; });
	const double leeward = meanOver(year, [](double lat, double x) { return lat >= 42 && lat <= 55 && x >= 64 && x <= 68; });
	const auto tropical = [](double lat, double x) { return lat >= 12 && lat <= 24 && x >= 12 && x <= 38; };
	const double summer = meanOver(c.rain[0], tropical), winter = meanOver(c.rain[1], tropical);
	infostream << "climate: windward " << windward << " leeward " << leeward
			<< "; tropical land summer " << summer << " winter " << winter << std::endl;
	UASSERT(windward > 1.5 * leeward);
	UASSERT(summer > 2 * winter);
	EXCEPTION_CHECK(std::invalid_argument, climate(code, std::vector<fix>(10, 0)));
}

// An ocean between two continents: a warm current up its western side (off
// the first continent's east coast), a cold one down its eastern side (off
// the second's west coast), which leaves that coast drier.
void TestTerragen::testOceanCurrents()
{
	WorldCode code = WorldCode::decode("TF18.dGVycmE");
	const std::vector<fix> e = drawn([](double lat, double x) {
		return lat >= 5 && lat <= 55 && ((x >= 20 && x <= 40) || (x >= 70 && x <= 90)) ? ratio(6, 100) : -ratio(3, 10);
	});
	Climate c = climate(code, e);
	std::vector<fix> year(e.size());
	for (size_t i = 0; i < e.size(); i++)
		year[i] = c.rain[0][i] + c.rain[1][i];
	const auto band = [](double lat) { return lat >= 20 && lat <= 35; };
	const double warm = meanOver(c.sea, [&](double lat, double x) { return band(lat) && x >= 41 && x <= 43; });
	const double cold = meanOver(c.sea, [&](double lat, double x) { return band(lat) && x >= 67 && x <= 69; });
	const double east_coast = meanOver(year, [&](double lat, double x) { return band(lat) && x >= 38 && x <= 40; });
	const double west_coast = meanOver(year, [&](double lat, double x) { return band(lat) && x >= 70 && x <= 72; });
	infostream << "currents: off the east coast " << warm << ", off the west coast " << cold
			<< "; rain on the east coast " << east_coast << ", west coast " << west_coast << std::endl;
	UASSERT(warm > 0 && cold < 0);
	UASSERT(east_coast > west_coast);
}

// The energy balance: an Earth-like world is about Earth's temperature; a
// colder one has more ice than its forcing alone would give (ice brightens
// the world and cools it further); a continent's interior has fiercer
// seasons than its coast.
void TestTerragen::testEnergyBalance()
{
	const std::vector<fix> e = drawn([](double lat, double x) {
		return lat >= 30 && lat <= 65 && x >= 20 && x <= 70 ? ratio(5, 100) : -ratio(3, 10);
	});
	const Sphere &s = gridSphere();
	auto mean = [&](const Climate &c) {
		int64_t sum = 0, area = 0;
		for (int32_t i = 0; i < s.size(); i++) {
			sum += (int64_t)celsiusOf(c.heat[i]) * s.area(i) >> FRAC;
			area += s.area(i);
		}
		return (double)sum / area;
	};
	const Climate earth = climate(WorldCode::decode("TF18.dGVycmE"), e);
	// the Temperature setting at −0.5 and −1
	const Climate cool = climate(WorldCode::decode("TF18.dGVycmE.AAKAqoCAAG1mgICAQICAgICCVQ"), e);
	const Climate cold = climate(WorldCode::decode("TF18.dGVycmE.AAKAqoCAAG1mgICAAICAgICCVQ"), e);
	const double t_earth = mean(earth), t_cool = mean(cool), t_cold = mean(cold);
	infostream << "energy balance: " << t_earth << " °C, ice " << earth.ice / 655.36 << "%; cooler " << t_cool << " °C, ice "
			<< cool.ice / 655.36 << "%; colder " << t_cold << " °C, ice " << cold.ice / 655.36 << "%" << std::endl;
	UASSERT(t_earth > 10 && t_earth < 25); // (mostly sea: darker, so warmer than Earth)
	UASSERT(cool.ice > earth.ice && cold.ice > cool.ice);
	// the second half of the cooling cools more than the first: the ice
	UASSERT(t_cool - t_cold > t_earth - t_cool);
	const double coast = meanOver(earth.gain, [](double lat, double x) { return lat >= 45 && lat <= 50 && x >= 20 && x <= 22; });
	const double interior = meanOver(earth.gain, [](double lat, double x) { return lat >= 45 && lat <= 50 && x >= 44 && x <= 46; });
	infostream << "seasons at 47° N: coast " << coast << ", interior " << interior << std::endl;
	UASSERT(interior > 1.2 * coast);
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

	World w(WorldCode::decode("TF18.dGVycmE"));
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
		// rivers are where rain gathers: on land, and wet land carries more of
		// them than dry (rivers per land cell, wettest third against driest)
		std::vector<std::pair<fix, bool>> land;
		for (int32_t i = 0; i < g.size(); i++) {
			if (g.river(i))
				UASSERT(g.base[i] >= 0);
			if (g.base[i] >= 0)
				land.emplace_back(g.rain(i), g.river(i));
		}
		std::sort(land.begin(), land.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
		const size_t third = land.size() / 3;
		int dry = 0, wet = 0;
		for (size_t k = 0; k < third; k++) {
			dry += land[k].second;
			wet += land[land.size() - 1 - k].second;
		}
		UASSERT(wet > 0);
		UASSERT(wet > dry);
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
	describe("TF18.YWJj");
	describe("TF18.dGVycmE");
	World w(WorldCode::decode("TF18.YWJj.AQCAqoCAALZmgICAgICAgIDjVQ"));
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
	// TF9, not yet released, re-pinned for sprint 88 (was 1343096095)
	// re-pinned for sprint 97: no beaches on hard coasts (was 3559267434)
	// re-pinned for sprint 100: relief from the drainage below the grid (was 1626736796)
	// re-pinned for sprint 101: ranges by what built them (was 1400668538)
	// TF17, sprint 106: the land's shape (plates that wander, deltas, alpine crests…; was 1902362085)
	// TF18, sprint 107: old land worn flat (shields; was 2420144613)
	UASSERTEQ(uint32_t, h, 3548419286u);
}

// Sprint 100: the drainage below the grid. Every channel runs down the land to
// what it joins, and the network branches as real ones do (Horton: each Strahler order
// has 3–5 times the streams of the next).
// Ranges by what built them (planning/sprint-101-ranges.md)
// A far tile's columns leave out what's narrower than its cells: nothing for a
// block-wide one, and what's left stands about where the whole land does.
void TestTerragen::testCoarse()
{
	const WorldCode code = WorldCode::decode("TF18.a3U3eWd1Y2c.BQKAqoCAAG1mgICAgICAgICCVQ") /* 250k radius */;
	const World world(code);
	int64_t moved = 0;
	int worst = 0, seas = 0, n = 0;
	for (int k = 0; k < 400; k++) {
		const Place at = placeOf(k * 7919 % 2001 - 1000, k * 6271 % 2003 - 1001, k * 4057 % 1999 - 999 + 1);
		const fix sun = world.sunlight(at, std::nullopt);
		const Ground whole = world.ground(at, sun);
		{
			const World::Coarse same(world, 1);
			UASSERTEQ(int, world.ground(at, sun).terrain, whole.terrain);
		}
		const World::Coarse coarse(world, 4096);
		const Ground far = world.ground(at, sun);
		const int d = std::abs(far.terrain - whole.terrain);
		moved += d;
		worst = std::max(worst, d);
		seas += (far.kind == Water::Sea) != (whole.kind == Water::Sea);
		n++;
	}
	infostream << "coarse columns: moved " << moved / n << " blocks on average, at most " << worst << ", " << seas << " of " << n << " change sea" << std::endl;
	UASSERT(moved / n < 150);
	UASSERT(seas == 0);
}

void TestTerragen::testRanges()
{
	terragen::WorldCode code = terragen::WorldCode::decode("TF18.dGVycmE");
	code.size = 628320;
	code.grid_key += "@wide"; // (what the grid's kept by)
	terragen::World world(code);
	const std::shared_ptr<const terragen::Grid> grid = terragen::gridFor(code);
	const terragen::Tectonics &t = grid->tectonics;
	const terragen::Sphere &sphere = terragen::gridSphere();
	using terragen::ratio;

	// all parts present, and a plateau stands high and level: far less rough
	// across its neighbours than a range still rising without one
	int folded = 0, plains = 0;
	int64_t plain_rough = 0, range_rough = 0;
	int ranges = 0;
	for (int32_t i = 0; i < sphere.size(); i++) {
		folded += grid->eroded[i] > 0 && t.fold[i] > ratio(1, 2);
		const bool plain = grid->eroded[i] > 0 && t.plateau[i] > ratio(95, 100); // (its interior: the margins are cut by rivers)
		const bool range = grid->eroded[i] > 0 && t.plateau[i] == 0 && t.fold[i] == 0 && t.belt[i] > ratio(8, 10);
		if (!plain && !range)
			continue;
		int64_t rough = 0;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			rough += std::abs(grid->eroded[*p] - grid->eroded[i]);
		if (plain) {
			plains++;
			plain_rough += rough;
			UASSERT(grid->eroded[i] > ratio(6, 10));
		} else {
			ranges++;
			range_rough += rough;
		}
	}
	UASSERT(folded > 500);
	UASSERT(plains > 50);
	UASSERT(ranges > 50);
	UASSERT(plain_rough / plains * 2 < range_rough / ranges);

	// fold ridges run along the belt: crossing its contours crosses many, and
	// a young belt's stand taller than an old one's
	int across = 0, along = 0, tested = 0;
	for (int32_t i = 0; i < sphere.size() && tested < 40; i++) {
		if (grid->eroded[i] <= 0 || t.fold[i] < ratio(9, 10))
			continue;
		// the neighbours most up and most level along the contours
		int32_t up = i, level = i;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++) {
			if (t.front[*p] > t.front[up])
				up = *p;
			if (level == i || std::abs(t.front[*p] - t.front[i]) < std::abs(t.front[level] - t.front[i]))
				level = *p;
		}
		if (up == i || t.fold[up] < ratio(1, 2))
			continue;
		const terragen::Place a(sphere.centre(i));
		const auto changes = [&](const terragen::Place &b) {
			int count = 0;
			terragen::fix was = terragen::ONE / 2;
			for (int k = 0; k <= 64; k++) {
				const terragen::Place p = terragen::placeOf((int64_t)a.x + ((int64_t)b.x - a.x) * k / 64,
						(int64_t)a.y + ((int64_t)b.y - a.y) * k / 64, (int64_t)a.z + ((int64_t)b.z - a.z) * k / 64);
				const terragen::fix crest = world.ridgesAt(p);
				if ((crest > terragen::ONE / 2) != (was > terragen::ONE / 2))
					count++;
				was = crest;
			}
			return count;
		};
		across += changes(terragen::Place(sphere.centre(up)));
		along += changes(terragen::Place(sphere.centre(level)));
		tested++;
	}
	UASSERT(tested >= 20);
	// (by a good deal, not twice: they bend, split and close, sprint 106)
	UASSERT(across > along * 5 / 4);
}

void TestTerragen::testChannels()
{
	terragen::WorldCode code = terragen::WorldCode::decode("TF18.dGVycmE");
	code.size = 628320;
	code.grid_key += "@wide"; // (what the grid's kept by)
	terragen::World world(code);
	const std::shared_ptr<const terragen::Grid> grid = terragen::gridFor(code);
	terragen::Channels channels(code, *grid, world, terragen::World::radiusOf(code), terragen::World::cellBlocksOf(code));
	UASSERT(channels.levels() >= 4);
	// every drawn run's water falls downstream, and meets the next where it ends (sprint 107)
	for (int32_t c = 0; c < grid->size(); c++) {
		const int32_t *p = world.profile(c);
		if (!p)
			continue;
		for (int j = 0; j < terragen::World::PROFILE; j++)
			UASSERT(p[j + 1] <= p[j]);
		const int32_t *next = world.profile(grid->receiver[c]);
		if (next && !grid->lake[grid->receiver[c]])
			UASSERT(next[0] <= p[terragen::World::PROFILE]);
	}
	int32_t centre = -1;
	for (int32_t i = 0; i < grid->size() && centre < 0; i++)
		if (grid->eroded[i] > terragen::ratio(10, 100))
			centre = i;
	UASSERT(centre >= 0);
	const terragen::Place at(terragen::gridSphere().centre(centre));
	struct Key { int level; int32_t i, j, l; bool operator<(const Key &o) const { return std::tie(level, i, j, l) < std::tie(o.level, o.i, o.j, o.l); } };
	std::map<Key, int> order;
	std::map<Key, std::vector<int>> children;
	const int K = std::min(channels.levels(), 4);
	const int64_t patch = channels.spacing(1) * 3;
	std::array<int, 16> count{};
	for (int k = K; k >= 1; k--) {
		const int64_t L = channels.spacing(k);
		const auto range = [&](int32_t c, int32_t &a, int32_t &b) { a = channels.latticeOf(k, c - (int32_t)patch); b = channels.latticeOf(k, c + (int32_t)patch); };
		int32_t i0, i1, j0, j1, l0, l1;
		range(at.x, i0, i1);
		range(at.y, j0, j1);
		range(at.z, l0, l1);
		(void)L;
		for (int32_t i = i0; i <= i1; i++)
			for (int32_t j = j0; j <= j1; j++)
				for (int32_t l = l0; l <= l1; l++) {
					const terragen::Channels::Node n = channels.node(k, i, j, l);
					if (!n.exists)
						continue;
					// (never up the land to what it joins; a first level's foot is
					// on the drawn water, which curves off the run a little)
					if (k > 1 || !n.wet)
						UASSERT(world.between(grid->eroded, n.join) <= world.between(grid->eroded, n.at));
					// water runs into water, down to a drawn run of the grid's (sprint 107)
					if (n.wet && k > 1)
						UASSERT(channels.node(k - 1, n.parent[0], n.parent[1], n.parent[2]).wet);
					if (n.wet && k == 1)
						UASSERT(world.drawn(n.parent[0]));
					const std::vector<int> &from = children[{ k, i, j, l }];
					int top = 1, tops = 0;
					for (int o : from)
						top = std::max(top, o);
					for (int o : from)
						tops += o == top;
					const int mine = tops >= 2 ? top + 1 : top;
					count[mine]++;
					if (k > 1)
						children[{ k - 1, n.parent[0], n.parent[1], n.parent[2] }].push_back(mine);
				}
	}
	std::printf("HORTON");
	for (int o = 1; o < 8; o++)
		std::printf(" %d", count[o]);
	std::printf("\n");
	// the ratio across the orders, as Horton's is measured: order 1's count over
	// order 3's, per order
	UASSERT(count[3] > 0);
	const double bifurcation = std::sqrt((double)count[1] / count[3]);
	UASSERT(bifurcation > 3 && bifurcation < 5);
}
