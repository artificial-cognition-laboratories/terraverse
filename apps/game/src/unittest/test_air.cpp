// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "air/optics.h"
#include "air/tables.h"
#include "terragen/system.h"
#include <cmath>
#include <string>

using terragen::System;
using terragen::SystemSpec;

/*
	The air (air/*): Earth's gases come out at the measured numbers, a body with
	no air has none, the sky over Earth is blue and its low sun red, and resizing
	an air keeps how much of it is overhead.
*/
class TestAir : public TestBase {
public:
	TestAir() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestAir"; }

	void runTests(IGameDef *gamedef);

	void testEarthsGases();
	void testNoAir();
	void testBlueSkyRedSun();
	void testHazeIsOrange();
	void testThickAirStillLights();
	void testSkyImage();
	void testHorizonsLineUp();
	void testRimImage();
	void testSunTint();
	void testResizingKeepsTheColumn();
	void testStarColours();
};

static TestAir g_test_instance;

void TestAir::runTests(IGameDef *gamedef)
{
	TEST(testEarthsGases);
	TEST(testNoAir);
	TEST(testBlueSkyRedSun);
	TEST(testHazeIsOrange);
	TEST(testThickAirStillLights);
	TEST(testSkyImage);
	TEST(testHorizonsLineUp);
	TEST(testRimImage);
	TEST(testSunTint);
	TEST(testResizingKeepsTheColumn);
	TEST(testStarColours);
}

static air::Medium homeMedium(const std::string &id)
{
	const System system(SystemSpec::home());
	for (size_t b = 0; b < system.spec().bodies.size(); b++)
		if (system.spec().bodies[b].id == id)
			return air::mediumOf(system.cards()[b], system.radius(b), system.spec().star.type);
	throw std::invalid_argument("no body " + id);
}

void TestAir::testEarthsGases()
{
	const air::Medium m = homeMedium("earth");
	// Penndorf's sea-level figure at 550 nm is 1.16e-5 per m, at 1013 mbar and 288 K. λ⁻⁴ sets the others.
	const System system(SystemSpec::home());
	for (size_t i = 0; i < system.spec().bodies.size(); i++)
		if (system.spec().bodies[i].id == "earth") {
			const double bar = (double)system.cards()[i].pressure / terragen::ONE;
			const double kelvin = (double)system.cards()[i].temperature / terragen::ONE;
			const double standard = m.rayleigh[1] * (1.013 / bar) * (kelvin / 288);
			UASSERT(std::abs(standard - 1.16e-5) < 1e-6);
		}
	UASSERT(m.rayleigh[2] / m.rayleigh[0] > 5 && m.rayleigh[2] / m.rayleigh[0] < 6);
	UASSERT(m.rayleigh_height > 8000 && m.rayleigh_height < 9000);
	UASSERT(m.ozone[1] > m.ozone[2]);
}

void TestAir::testNoAir()
{
	UASSERT(homeMedium("moon").none());
	EXCEPTION_CHECK(std::invalid_argument, air::Tables{ homeMedium("moon") });
}

void TestAir::testBlueSkyRedSun()
{
	const air::Tables t(homeMedium("earth"));
	const air::Rgb zenith = t.sky(0, { 0, 1, 0 }, { 0.5, 0.86, 0 });
	UASSERT(zenith[2] > zenith[1] && zenith[1] > zenith[0]);
	const air::Rgb high = t.transmittance(0, 1), low = t.transmittance(0, 0.03);
	UASSERT(high[0] > 0.8);
	UASSERT(low[0] > low[1] && low[1] > low[2]);
	UASSERT(low[2] < 0.1);
	const air::Rgb night = t.transmittance(0, -0.2);
	UASSERTEQ(double, night[0], 0);
	// from above the air there's nothing left in the way, looking up
	const air::Rgb above = t.through(t.medium().top - t.medium().radius, { 0, 1, 0 });
	UASSERT(above[0] > 0.999);
}

// (methane air's haze is what colours it; a cold world's air must not go green)
void TestAir::testHazeIsOrange()
{
	const air::Tables t(homeMedium("nacre"));
	const air::Rgb up = t.sky(0, { 0, 1, 0 }, { 0.5, 0.86, 0 });
	UASSERT(up[0] > up[1] && up[1] > up[2]);
}

// Venus's kind of air: dim, not black, and with the blue taken out
void TestAir::testThickAirStillLights()
{
	const System system(SystemSpec::home());
	terragen::BodyCard card = system.cards()[system.playable()];
	card.ocean = false;
	card.pressure = terragen::toFix(92);
	card.temperature = terragen::toFix(735);
	for (terragen::fix &g : card.gases)
		g = 0;
	card.gases[(int)terragen::Gas::CO2] = terragen::ONE;
	const air::Tables thick(air::mediumOf(card, system.radius(system.playable()), system.spec().star.type));
	const air::Rgb up = thick.sky(0, { 0, 1, 0 }, { 0, 1, 0 });
	UASSERT(up[1] > 1e-3);
	UASSERT(up[0] > up[2]);
	const air::Tables earth(homeMedium("earth"));
	UASSERTEQ(double, earth.diffuse()[1], 0);
}

void TestAir::testSkyImage()
{
	const air::Tables t(homeMedium("earth"));
	const air::SkyImage noon = air::skyImage(t, 0, 1.2, 14, { 1, 1, 1 });
	const air::SkyImage night = air::skyImage(t, 0, -0.5, 14, { 1, 1, 1 });
	UASSERTEQ(size_t, noon.rgba.size(), (size_t)air::SkyImage::W * air::SkyImage::H * 4);
	// the top row, looking away from the star: blue, with stars drowned; at night dark, stars out
	const size_t top = (size_t)(air::SkyImage::W - 1) * 4;
	UASSERT(noon.rgba[top + 2] > noon.rgba[top] && noon.rgba[top + 2] > 100);
	UASSERT(noon.rgba[top + 3] < 20);
	UASSERT(night.rgba[top + 2] < 20);
	UASSERT(night.rgba[top + 3] > 150);
	UASSERT(noon.horizon[1] > 0.3);
	UASSERT(night.sun[0] < 1e-6);
}

void TestAir::testHorizonsLineUp()
{
	const double right = M_PI / 2, drawn = 0.68, real = 0.08;
	UASSERT(std::abs(air::realElevation(-drawn, drawn, real) + real) < 1e-12);
	UASSERT(std::abs(air::realElevation(right, drawn, real) - right) < 1e-12);
	UASSERT(std::abs(air::realElevation(-right, drawn, real) + right) < 1e-12);
	double last = -2;
	for (double e = -right; e <= right; e += 0.05) {
		const double r = air::realElevation(e, drawn, real);
		UASSERT(r > last);
		last = r;
	}
}

void TestAir::testRimImage()
{
	const air::Tables t(homeMedium("earth"));
	const air::RimImage rim = air::rimImage(t, 0.5, 14);
	const auto at = [&](int x, int y, int c) { return (int)rim.rgb[((size_t)y * air::RimImage::W + x) * 3 + c]; };
	const int day = air::RimImage::W - 1, row = 6;
	UASSERT(at(day, row, 2) > at(day, row, 0)); // blue by day
	UASSERT(at(0, row, 2) < 10);                // nothing on the night side
	UASSERT(at(day, row, 2) > at(day, air::RimImage::H - 1, 2)); // fading up and out
}

void TestAir::testSunTint()
{
	const air::Tables t(homeMedium("earth"));
	const air::Rgb noon = air::sunTint(t, 0, M_PI / 2), low = air::sunTint(t, 0, 0.03), dusk = air::sunTint(t, 0, -0.2);
	for (double v : noon)
		UASSERT(std::abs(v - 1) < 1e-6);
	UASSERT(low[0] > low[1] && low[1] > low[2]);
	UASSERT(dusk[2] > dusk[0]);
}

void TestAir::testResizingKeepsTheColumn()
{
	const air::Medium real = homeMedium("earth");
	const air::Medium small = real.resized(1000, 100);
	const air::Tables a(real), b(small);
	const air::Rgb x = a.through(0, { 0, 1, 0 }), y = b.through(0, { 0, 1, 0 });
	for (int c = 0; c < 3; c++)
		UASSERT(std::abs(x[c] - y[c]) < 0.03);
	UASSERT(std::abs(small.top - 1800) < 1e-6);
}

void TestAir::testStarColours()
{
	UASSERT(std::abs(air::starColour(2)[0] - 1) < 1e-9); // the Sun is white
	const air::Rgb cool = air::starColour(0), hot = air::starColour(6);
	UASSERT(cool[0] > cool[2]);
	UASSERT(hot[2] > hot[0]);
}
