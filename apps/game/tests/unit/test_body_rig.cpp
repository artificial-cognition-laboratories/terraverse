// Terraverse: a body's skeleton turns as the figure the game draws (client/app/headless/rig.h)
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/client/app/headless/rig.h"
#include <matrix4.h>
#include <cmath>
#include <random>
#include <set>
#include <string>

class TestBodyRig : public TestBase
{
public:
	TestBodyRig() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestBodyRig"; }

	void runTests(IGameDef *gamedef);

	void testJointsTurnAsTheFigure();
	void testEveryBoneHasItsJoints();
};

static TestBodyRig g_test_instance;

void TestBodyRig::runTests(IGameDef *gamedef)
{
	TEST(testJointsTurnAsTheFigure);
	TEST(testEveryBoneHasItsJoints);
}

// A right-handed turn of `v` by `angle` about the unit `axis` (Rodrigues): how the contract turns.
static v3f turned(v3f v, v3f axis, float angle)
{
	return v * std::cos(angle) + axis.crossProduct(v) * std::sin(angle) + axis * axis.dotProduct(v) * (1 - std::cos(angle));
}

// The engine's axes from the contract's: flu's inverse.
static v3f engine(v3f flu) { return v3f(-flu.Y, flu.Z, flu.X); }

void TestBodyRig::testJointsTurnAsTheFigure()
{
	std::mt19937 random(126);
	std::uniform_real_distribution<float> angle(-180, 180);
	for (int i = 0; i < 200; i++) {
		const v3f turn(angle(random), angle(random), angle(random));
		core::matrix4 m;
		m.setRotationDegrees(turn);
		const v3f a = headless::rig::jointsOf(turn);
		for (const v3f basis : { v3f(1, 0, 0), v3f(0, 1, 0), v3f(0, 0, 1) }) {
			// the game's: the bone's turn on a vector, seen in flu
			v3f game = engine(basis);
			m.transformVect(game); // a turn alone: no translation set
			game = headless::rig::flu(game);
			// the contract's chain, outermost first: `.z` about −x, `.y` about −z, the bone's own about y
			const v3f chain = turned(turned(turned(basis, v3f(0, 1, 0), a.Z), v3f(0, 0, -1), a.Y), v3f(-1, 0, 0), a.X);
			UASSERT((game - chain).getLength() < 1e-4f);
		}
	}
}

void TestBodyRig::testEveryBoneHasItsJoints()
{
	const Json::Value frames = headless::rig::frames(physics::Skeleton{}.place(0, physics::Joints{}, 0, nullptr));
	const Json::Value sense = headless::rig::sense();
	// every component of /joints names a frame with a joint, and the angles come in that order
	UASSERTEQ(size_t, sense["components"].size(), headless::rig::angles(physics::Bones{}).size());
	for (const Json::Value &c : sense["components"]) {
		bool found = false;
		for (const Json::Value &f : frames)
			found = found || (f["name"] == c["name"] && f.isMember("joint"));
		UASSERT(found);
	}
	// one root, and every other frame hangs from one said before it
	std::set<std::string> said;
	for (const Json::Value &f : frames) {
		if (f["parent"].isNull()) {
			UASSERT(said.empty());
		} else {
			UASSERT(said.count(f["parent"].asString()) == 1);
		}
		said.insert(f["name"].asString());
	}
}
