// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "terragen/system.h"
#include "shell/system.h"
#include "terragen/body.h"
#include <cmath>
#include <stdexcept>

using terragen::System;
using terragen::SystemSpec;

/*
	The system (terragen/system.h): the home system is one that can exist;
	its orbits are Kepler's, with the periods ours have (an Earth's year, a
	Moon's month); a moon goes round its planet, not the star.
*/
class TestSystem : public TestBase {
public:
	TestSystem() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSystem"; }

	void runTests(IGameDef *gamedef);

	void testHomeIsOurs();
	void testOrbitsCloseAfterAPeriod();
	void testMoonsGoRoundTheirPlanet();
	void testOnePlayableBody();
	void testThePagesSystemIsTheEngines();
};

static TestSystem g_test_instance;

void TestSystem::runTests(IGameDef *gamedef)
{
	TEST(testHomeIsOurs);
	TEST(testOrbitsCloseAfterAPeriod);
	TEST(testMoonsGoRoundTheirPlanet);
	TEST(testOnePlayableBody);
	TEST(testThePagesSystemIsTheEngines);
}

namespace {

double length(const System::Vec &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
System::Vec minus(const System::Vec &a, const System::Vec &b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }

size_t find(const System &s, const char *id)
{
	for (size_t i = 0; i < s.spec().bodies.size(); i++)
		if (s.spec().bodies[i].id == id)
			return i;
	throw std::logic_error(std::string("no body ") + id);
}

} // namespace

void TestSystem::testHomeIsOurs()
{
	const System s(SystemSpec::home());
	const size_t earth = find(s, "earth"), moon = find(s, "moon");
	UTEST(s.playable() == earth, "the playable body is %zu, not Earth", s.playable());
	UTEST(std::abs(s.period(earth) - 365.25) < 1, "Earth's year is %.2f days", s.period(earth));
	UTEST(std::abs(s.period(moon) - 27.3) < 1.5, "the Moon's month is %.2f days", s.period(moon));
	UTEST(std::abs(s.radius(earth) - 6371) < 200, "Earth's radius is %.0f km", s.radius(earth));
	const double au = length(s.position(earth, 0)) / 149597870.7;
	UTEST(au > 0.98 && au < 1.02, "Earth is %.3f AU out", au);
}

void TestSystem::testOrbitsCloseAfterAPeriod()
{
	const System s(SystemSpec::home());
	for (size_t i = 0; i < s.spec().bodies.size(); i++) {
		if (s.spec().bodies[i].causes.parent >= 0)
			continue;
		// (its balance point with its moons: it swings round that as they go)
		const System::Vec a = s.balance(i, 10), b = s.balance(i, 10 + s.period(i));
		const double off = length(minus(a, b)) / length(a);
		UTEST(off < 1e-6, "%s is %.2g of its distance off after a year", s.spec().bodies[i].id.c_str(), off);
	}
}

void TestSystem::testMoonsGoRoundTheirPlanet()
{
	const System s(SystemSpec::home());
	for (size_t i = 0; i < s.spec().bodies.size(); i++) {
		const int32_t parent = s.spec().bodies[i].causes.parent;
		if (parent < 0)
			continue;
		for (double day : { 0.0, 3.0, 40.0 }) {
			const double d = length(minus(s.position(i, day), s.position((size_t)parent, day))) / s.radius((size_t)parent);
			const double a = s.spec().bodies[i].causes.orbit / 65536.0, e = s.spec().bodies[i].causes.eccentricity / 65536.0;
			UTEST(d >= a * (1 - e) - 1e-6 && d <= a * (1 + e) + 1e-6, "%s is %.2f radii from its planet (orbit %.2f)",
					s.spec().bodies[i].id.c_str(), d, a);
		}
	}
}

void TestSystem::testOnePlayableBody()
{
	SystemSpec none = SystemSpec::home();
	for (auto &b : none.bodies)
		b.playable = false;
	bool refused = false;
	try {
		System s(none);
	} catch (const std::invalid_argument &) {
		refused = true;
	}
	UTEST(refused, "a system with no playable body was taken (%d)", 0);
}

// The page holds no system of its own: its draft is the engine's
// (shell::systemJson), and sent back as causes (`system.preview`, as
// ui/src/system/model.ts causes() does) it's the same bodies, the same cards.
void TestSystem::testThePagesSystemIsTheEngines()
{
	const SystemSpec home = SystemSpec::home();
	const Json::Value page = shell::systemJson(home);
	UASSERT(page["bodies"].size() == home.bodies.size());
	Json::Value request;
	request["star"] = page["star"];
	request["starMass"] = page["starMass"];
	request["metallicity"] = page["metallicity"];
	request["age"] = page["age"];
	request["activity"] = page["activity"];
	request["companion"] = page["companion"];
	request["bodies"] = Json::Value(Json::arrayValue);
	for (const Json::Value &b : page["bodies"]) {
		Json::Value c;
		for (const char *field : { "id", "parent", "orbit", "eccentricity", "formed", "volatiles", "day", "life", "spin" })
			c[field] = b[field];
		c["mass"] = std::pow(10.0, b["mass"].asDouble());
		request["bodies"].append(c);
	}
	const Json::Value cards = shell::systemPreview(request);
	const System system(home);
	UASSERT(cards.size() == system.cards().size());
	for (Json::ArrayIndex i = 0; i < cards.size(); i++) {
		UASSERT(cards[i]["id"].asString() == home.bodies[i].id);
		UASSERT(cards[i]["kind"].asString() == terragen::KIND_NAMES[(int)system.cards()[i].kind]);
		UASSERT(std::fabs(cards[i]["radius"].asDouble() - (double)system.cards()[i].radius / terragen::ONE) < 1e-3);
	}
}
