// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "client/sky.h"
#include "client/solar.h"
#include "daynightratio.h"

class TestSolar : public TestBase {
public:
	TestSolar() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSolar"; }

	void runTests(IGameDef *gamedef);

	void testElevation();
	void testWickedInverse();
	void testLight();
	void testDate();
};

static TestSolar g_test_instance;

void TestSolar::runTests(IGameDef *gamedef)
{
	TEST(testElevation);
	TEST(testWickedInverse);
	TEST(testLight);
	TEST(testDate);
}

static bool near(float a, float b, float tolerance)
{
	return std::fabs(a - b) <= tolerance;
}

void TestSolar::testElevation()
{
	UASSERT(near(solar::elevation({ true, 0, 0 }, 0.5f), 90, 0.1f));
	UASSERT(near(solar::elevation({ true, 60, 23 }, 0.5f), 53, 0.1f));
	UASSERT(near(solar::elevation({ true, 0, 0 }, 0.25f), 0, 0.1f));
	UASSERT(solar::elevation({ true, 80, -23 }, 0.5f) < 0);  // polar night
	UASSERT(solar::elevation({ true, 80, 23 }, 0.0f) > 0);   // midnight sun
}

void TestSolar::testWickedInverse()
{
	for (float t = 0.0f; t < 1.0f; t += 0.01f)
		UASSERT(near(getTimeOfDayFromWicked(getWickedTimeOfDay(t)), t, 1e-4f));
}

static u32 light(const SkySolar &s, float time_of_day)
{
	return time_to_daynight_ratio(solar::apparentTime(s, 0, time_of_day) * 24000, true);
}

void TestSolar::testLight()
{
	SkySolar equator{ true, 0, 0 }, polar_winter{ true, 80, -23 }, polar_summer{ true, 80, 23 };
	UASSERTEQ(u32, light(equator, 0.5f), 1000u);       // full day at noon
	UASSERT(light(equator, 0.0f) < 300);                // night at midnight
	UASSERT(light(polar_winter, 0.5f) < 1000);          // the sun never rises
	SkySolar arctic_winter{ true, 70, -23 };            // noon sun 3° below the horizon
	UASSERT(light(arctic_winter, 0.5f) > light(arctic_winter, 0.0f)); // a twilight noon
	UASSERT(light(polar_summer, 0.0f) > 900);           // midnight sun, 3° up: dawn-bright
	// the same sun height gives the same light on either side of noon
	UASSERTEQ(u32, light(equator, 0.3f), light(equator, 0.7f));
}

// A 30-hour day: the clock runs on through the engine's midnight, and one date
// and time always read the same, however often they're asked or set.
void TestSolar::testDate()
{
	SkySolar s{ true, 40, 10 };
	s.turns = 0.8f;
	s.at = 0.1f;
	UASSERT(near(solar::localTime(s, 7, 0.9999f), solar::localTime(s, 8, 0), 0.001f));
	const float light = solar::apparentTime(s, 12, 0.5f);
	for (int i = 0; i < 5; i++)
		UASSERT(solar::apparentTime(s, 12, 0.5f) == light);
	UASSERT(!near(solar::localTime(s, 12, 0.5f), solar::localTime(s, 13, 0.5f), 0.1f)); // (the day isn't 24 hours)
}
