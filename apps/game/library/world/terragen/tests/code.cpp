// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// Codes produced by the page's encodeWorldCode (ui/src/worldgen/config.ts).
TEST_CASE("DecodeDefaults")
{
	WorldCode w = WorldCode::decode("TF24.YWJj");
	REQUIRE_EQ(std::string(w.seed), "abc");
	REQUIRE_EQ(int(w.size), 16384); // as ui/src/worldgen/config.ts DEFAULTS
	REQUIRE_EQ(std::string(w.star->id), "G");
	REQUIRE_EQ(int(w.continents), 7);
	REQUIRE_EQ(int(roundInt(w.tilt)), 23);
	REQUIRE_EQ(int(w.year), 48);
	REQUIRE(near(w.land, ratio(25, 100), 256));
	REQUIRE(near(w.temperature, 0, 600));
}

TEST_CASE("DecodeParams")
{
	// size 1 (62832 round: radius 10k), red dwarf, continents 6, tilt 40,
	// everything else default
	WorldCode w = WorldCode::decode("TF24.YWJj.AQCAqoCAAFVmgICAgICAgIDjVQ");
	REQUIRE_EQ(int(w.size), 62832);
	REQUIRE_EQ(std::string(w.star->id), "M");
	REQUIRE_EQ(int(w.continents), 6);
	REQUIRE_EQ(int(roundInt(w.tilt)), 40);
	REQUIRE_EQ(int(w.year), 48);
}

TEST_CASE("DecodeRejects")
{
	REQUIRE_THROWS_AS(WorldCode::decode("XX.YWJj"), CodeError);
	REQUIRE_THROWS_AS(WorldCode::decode("TF1.YWJj"), CodeError); // an older generator: refused, not reinterpreted
	REQUIRE_THROWS_AS(WorldCode::decode("TF2.YWJj"), CodeError); // before plates and geology: refused too
	REQUIRE_THROWS_AS(WorldCode::decode("TF9."), CodeError);
	REQUIRE_THROWS_AS(WorldCode::decode("TF24.YWJj.AACAqoCAAA"), CodeError);
	REQUIRE_THROWS_AS(WorldCode::decode("TF24.YW*j"), CodeError);
}

TEST_CASE("Trig")
{
	const fix tol = ONE / 500;
	REQUIRE(near(terragen::sin(HALF_PI), ONE, tol));
	REQUIRE(near(terragen::cos(0), ONE, tol));
	REQUIRE(near(terragen::sin(PI), 0, tol));
	REQUIRE(near(terragen::cos(PI), -ONE, tol));
	REQUIRE(near(terragen::sin(-HALF_PI), -ONE, tol));
	REQUIRE(near(terragen::sin(TWO_PI + HALF_PI), ONE, tol)); // reduction
	REQUIRE(near(terragen::acos(0), HALF_PI, tol));
	REQUIRE(near(terragen::acos(ONE), 0, tol));
	REQUIRE(near(terragen::acos(-ONE), PI, tol));
	REQUIRE(near(terragen::atan2(ONE, -ONE), PI - PI / 4, tol));
	REQUIRE(near(terragen::atan2(-ONE, -ONE), -(PI - PI / 4), tol));
	REQUIRE(near(terragen::sqrt(toFix(4)), toFix(2), 2));
}

TEST_CASE("NoiseOnTheSphere")
{
	// across the east–west seam it carries on, and at a pole every
	// longitude is one place
	for (fix v = ONE / 16; v < ONE; v += ONE / 7)
		REQUIRE(near(fbm(World::place(0, v), 1234, 5, 6, HALF), fbm(World::place(ONE - 1, v), 1234, 5, 6, HALF), ONE / 64));
	for (fix u = 0; u < ONE; u += ONE / 9) {
		// (cos 90° in fixed point is a hair over 0: the same place within a unit)
		REQUIRE(near(noise(World::place(u, 0), 99, 3), noise(World::place(0, 0), 99, 3), ONE / 1024));
		REQUIRE(near(noise(World::place(u, ONE), 99, 3), noise(World::place(0, ONE), 99, 3), ONE / 1024));
	}
}

TEST_CASE("Sun")
{
	const fix tol = ONE / 50;
	REQUIRE(near(insolation(0, 0), ONE, tol));                       // equator at equinox
	fix lat60 = mul(PI, ratio(1, 3));
	REQUIRE(annualInsolation(0, toFix(23)) > annualInsolation(lat60, toFix(23)));
	fix lat80 = mul(PI, ratio(80, 180));
	fix winter = declination(toFix(23), ratio(3, 4));               // northern winter
	REQUIRE(near(insolation(lat80, winter), 0, tol));                // polar night
	fix summer = declination(toFix(23), ratio(1, 4));
	REQUIRE(insolation(lat80, summer) > ONE);                        // midnight sun
}

static fix degrees(int32_t d)
{
	return (fix)(((int64_t)d * PI) / 180);
}

TEST_CASE("SunPath")
{
	const fix tol = degrees(1);
	fix equinox = 0, summer = declination(toFix(23), ratio(1, 4)), winter = -summer;
	REQUIRE(near(elevation(0, equinox, HALF), HALF_PI, tol));            // overhead at the equator
	REQUIRE(near(elevation(degrees(60), equinox, HALF), degrees(30), tol));
	REQUIRE(near(elevation(degrees(60), summer, HALF), degrees(53), tol)); // 90 − 60 + 23
	REQUIRE(near(elevation(0, equinox, ratio(1, 4)), 0, tol));           // sunrise at six
	REQUIRE(elevation(degrees(45), equinox, 0) < 0);                      // midnight
	REQUIRE(elevation(degrees(80), winter, HALF) < 0);                    // polar night: no sunrise
	REQUIRE(elevation(degrees(80), summer, 0) > 0);                       // midnight sun
	REQUIRE(near(elevation(-degrees(60), winter, HALF), degrees(53), tol)); // the south mirrors

	const fix share = ONE / 100;
	REQUIRE(near(dayShare(degrees(50), equinox), HALF, share));
	REQUIRE(dayShare(degrees(50), summer) > dayShare(degrees(50), equinox));
	REQUIRE(dayShare(-degrees(50), summer) < HALF);
	REQUIRE_EQ(fix(dayShare(degrees(80), winter)), 0);
	REQUIRE_EQ(fix(dayShare(degrees(80), summer)), fix(ONE));
}

TEST_CASE("Calendar")
{
	Calendar c = calendar(28, 30, ratio(1, 4));
	REQUIRE_EQ(uint32_t(c.year), 1u);
	REQUIRE_EQ(uint32_t(c.day), 2u);
	REQUIRE_EQ(fix(c.date), fix((fix)((2 * (int64_t)ONE + ratio(1, 4)) / 28)));
	REQUIRE(calendar(28, 27, ONE - 1).date < ONE);                       // never reaches the next year

	REQUIRE(season(0, degrees(45)) == Season::Spring);
	REQUIRE(season(ratio(3, 10), degrees(45)) == Season::Summer);
	REQUIRE(season(ratio(3, 10), -degrees(45)) == Season::Winter);
	REQUIRE(season(ratio(9, 10), -degrees(45)) == Season::Summer);
	REQUIRE(season(ratio(6, 10), 0) == Season::Autumn);
}

TEST_CASE("Temperature")
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t quarter = w.code().size / 4;
	int32_t north = quarter + quarter / 2;   // ≈ 67°N
	fix midsummer = ratio(1, 4), midwinter = ratio(3, 4);
	fix afternoon = ratio(5, 8), night = ratio(1, 8);
	for (int32_t x = -quarter; x <= quarter; x += quarter / 2) {
		REQUIRE(w.temperature(flat(w, x, quarter), midsummer, afternoon) > w.temperature(flat(w, x, quarter), midwinter, afternoon));
		REQUIRE(w.temperature(flat(w, x, -quarter), midwinter, afternoon) > w.temperature(flat(w, x, -quarter), midsummer, afternoon));
		REQUIRE(w.temperature(flat(w, x, 0), ratio(1, 8), afternoon) > w.temperature(flat(w, x, 0), ratio(1, 8), night));
		REQUIRE(w.temperature(flat(w, x, 0), 0, HALF) > w.temperature(flat(w, x, north), 0, HALF));
	}
	fix t = w.temperature(flat(w, 0, 0), 0, HALF);
	REQUIRE((t > toFix(-60) && t < toFix(60)));
}

TEST_CASE("LogExp")
{
	REQUIRE_EQ(fix(log2(ONE)), 0);
	REQUIRE_EQ(fix(log2(toFix(8))), fix(toFix(3)));
	REQUIRE_EQ(fix(exp2(toFix(3))), fix(toFix(8)));
	REQUIRE(near(log2(ratio(3, 2)), 38336, 4));          // 0.58496
	REQUIRE(near(exp2(HALF), 92682, 4));                  // √2
	REQUIRE(near(exp(ONE), 178145, 16));                  // e
	REQUIRE(near(pow(toFix(9), HALF), toFix(3), 8));
	for (fix v = ratio(1, 64); v < toFix(1000); v += v / 3 + 1) // round trips across the range
		REQUIRE(near(exp2(log2(v)), v, std::max<fix>(4, v / 2000)));
	REQUIRE_EQ(fix(exp2(toFix(-40))), 0);
	REQUIRE_EQ(fix(exp2(toFix(40))), fix(INT32_MAX));
}
