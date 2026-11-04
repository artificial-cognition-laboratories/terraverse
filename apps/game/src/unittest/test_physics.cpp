// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "physics/walk.h"
#include "physics/world.h"
#include "physics/aero.h"
#include "workshop/motion.h"
#include <algorithm>
#include <cmath>

using namespace physics;

class TestPhysics : public TestBase {
public:
	TestPhysics() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPhysics"; }

	void runTests(IGameDef *gamedef);

	void testBlockComesToRest();
	void testFreezesAndWakes();
	void testCartRollsDownhill();
	void testWheelsRollFurtherThanASledgeSlides();
	void testHingeKeepsItsAxle();
	void testHingeStops();
	void testStableAndTopHeavy();
	void testHeldGoesWhereItsTold();
	void testPlayerPushes();
	void testSameEveryTime();
	void testPushedWhenFrozen();
	void testWalkHasInertia();
	void testAirGivesATopSpeed();
	void testJumpIsAHeight();
	void testStepsAndRides();
	void testCarriedFollowsOrDrags();
	void testRollsDownASlicedSlope();
	void testCornerPushTurns();
	void testTippedWheelsHoldUp();
	void testPutDownInTheGround();
	void testWalkingIntoACartDoesntThrowIt();
	void testGrabbedLiftsTipsOrHolds();
	void testCorrectedTowardAnother();
	void testCartOnACart();
	void testDriven();
	void testAeroReadsShapes();
	void testGlides();
	void testTreadGrips();
	void testQuick();
	void testUpright();
	void testSprings();
	void testHubsRide();
	void testClientsGrabbedCartStaysUp();
	void testCartDraggedKeepsItsWheelsUp();
	void testWheelTurnedByItsRim();
	void testGrabbedKeepsItsGround();
};

static TestPhysics g_test_instance;

void TestPhysics::runTests(IGameDef *gamedef)
{
	TEST(testBlockComesToRest);
	TEST(testFreezesAndWakes);
	TEST(testCartRollsDownhill);
	TEST(testWheelsRollFurtherThanASledgeSlides);
	TEST(testHingeKeepsItsAxle);
	TEST(testHingeStops);
	TEST(testStableAndTopHeavy);
	TEST(testHeldGoesWhereItsTold);
	TEST(testPlayerPushes);
	TEST(testSameEveryTime);
	TEST(testPushedWhenFrozen);
	TEST(testWalkHasInertia);
	TEST(testAirGivesATopSpeed);
	TEST(testJumpIsAHeight);
	TEST(testStepsAndRides);
	TEST(testCarriedFollowsOrDrags);
	TEST(testRollsDownASlicedSlope);
	TEST(testCornerPushTurns);
	TEST(testTippedWheelsHoldUp);
	TEST(testPutDownInTheGround);
	TEST(testWalkingIntoACartDoesntThrowIt);
	TEST(testGrabbedLiftsTipsOrHolds);
	TEST(testCorrectedTowardAnother);
	TEST(testCartOnACart);
	TEST(testDriven);
	TEST(testAeroReadsShapes);
	TEST(testGlides);
	TEST(testTreadGrips);
	TEST(testQuick);
	TEST(testUpright);
	TEST(testSprings);
	TEST(testHubsRide);
	TEST(testGrabbedKeepsItsGround);
	TEST(testWheelTurnedByItsRim);
	TEST(testCartDraggedKeepsItsWheelsUp);
	TEST(testClientsGrabbedCartStaysUp);
}

namespace {

constexpr float G = 9.81f;
constexpr float STONE = 2700, WOOD = 600;

// A flat floor everywhere: the nodes at y = -1, so its top is y = -0.5;
// each node in sixteenth slices along z, as shaped ground's boxes are.
bool flatGround(v3s16 block, std::vector<aabb3f> &boxes)
{
	if (block.Y != -1)
		return true;
	for (s16 x = 0; x < BLOCK; x++)
		for (s16 z = 0; z < BLOCK; z++)
			for (int s = 0; s < 16; s++)
				boxes.push_back(aabb3f(x - 0.5f, BLOCK - 1.5f, z - 0.5f + s / 16.f, x + 0.5f, BLOCK - 0.5f,
						z - 0.5f + (s + 1) / 16.f));
	return true;
}

World::Opts flat(v3f gravity = v3f(0, -G, 0))
{
	return World::Opts{ physics::uniform(gravity), flatGround };
}

Body block(v3f centre, v3f size, float density, float friction = 0.6f)
{
	return Body{ { Box{ aabb3f(centre - size * 0.5f, centre + size * 0.5f), density, friction } }, std::nullopt };
}

Quat about(v3f axis, float angle)
{
	const float s = std::sin(angle / 2);
	return { axis.X * s, axis.Y * s, axis.Z * s, std::cos(angle / 2) };
}

v3f turned(const Quat &q, v3f v)
{
	const v3f u(q.x, q.y, q.z);
	return u * (2 * u.dotProduct(v)) + v * (q.w * q.w - u.dotProduct(u)) + u.crossProduct(v) * (2 * q.w);
}

// A cart: a wooden plate 2 long (x) and 1 wide, four wheels on axles along
// z (or, `hinged`, round parts on hinges). Each axle's friction is rolling
// resistance: 0.02 of its share of the cart's weight (~470 kg) at the
// wheel's radius.
Assembly cart(bool hinged = false, bool mixed = false)
{
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(2, 0.25f, 1), WOOD));
	for (float x : { -0.7f, 0.7f })
		for (float z : { -0.65f, 0.65f }) {
			const v3f c(x, -0.1f, z);
			Body w = block(c, v3f(0.6f, 0.6f, 0.2f), WOOD);
			w.round = Round{ c, v3f(0, 0, 1), 0.4f, 0.1f };
			const float friction = 0.02f * 470 / 4 * G * 0.4f;
			if (hinged || (mixed && z > 0))
				a.hinges.push_back(Hinge{ 0, a.bodies.size(), c, v3f(0, 0, 1), friction });
			else
				a.wheels.push_back(Wheel{ a.bodies.size(), c, v3f(0, 0, 1), 0.4f, 0.1f, friction });
			a.bodies.push_back(w);
		}
	return a;
}

// The same plate on runners: no wheels, it slides.
Assembly sledge()
{
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(2, 0.25f, 1), WOOD));
	a.bodies[0].boxes.push_back(Box{ aabb3f(-1, -0.5f, -0.6f, 1, -0.125f, -0.4f), WOOD, 0.6f });
	a.bodies[0].boxes.push_back(Box{ aabb3f(-1, -0.5f, 0.4f, 1, -0.125f, 0.6f), WOOD, 0.6f });
	return a;
}

void run(World &w, double seconds)
{
	for (double t = 0; t < seconds; t += STEP)
		w.step(STEP);
}

} // namespace

void TestPhysics::testBlockComesToRest()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	World::Id id = w.add(a, Pose{ v3d(3, 3, 3), {} });
	UASSERTEQ(float, w.mass(id), STONE);
	run(w, 4);
	const Pose p = w.pose(id);
	UASSERT(std::fabs(p.position.Y - 0) < 0.03); // resting on the floor's top
	UASSERT(std::fabs(p.position.X - 3) < 0.01 && std::fabs(p.position.Z - 3) < 0.01);
	UASSERT(w.asleep(id));
}

void TestPhysics::testFreezesAndWakes()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	World::Id id = w.add(a, Pose{ v3d(0, 2, 0), {} });
	run(w, 6);
	// at rest and no one near: frozen, and the ground round it let go
	UASSERT(w.tier(id) == Tier::Frozen);
	UASSERTEQ(size_t, w.groundBlocks(), 0);
	const Pose rest = w.pose(id);
	// a player comes near: it's Live again, where it was
	w.players({ Capsule{ v3d(2, -0.5, 0), 0.3f, 1.8f } });
	run(w, 0.1);
	UASSERT(w.tier(id) == Tier::Live);
	UASSERT(w.pose(id).position.getDistanceFrom(rest.position) < 0.01);
	UASSERT(w.groundBlocks() > 0);
	// they go: it freezes again
	w.players({});
	run(w, 2);
	UASSERT(w.tier(id) == Tier::Frozen);
}

void TestPhysics::testCartRollsDownhill()
{
	// a 10° slope: gravity turned 10° about z, downhill is +x
	const float a = 10 * core::DEGTORAD;
	World w(flat(v3f(G * std::sin(a), -G * std::cos(a), 0)));
	World::Id id = w.add(cart(), Pose{ v3d(0, 0.5, 0), {} });
	run(w, 1); // landed
	const double from = w.pose(id).position.X;
	// each wheel's turn, added up a step at a time (one reading wraps)
	auto angle = [](const Relative &r) { return 2 * std::atan2(r.rotation.z, r.rotation.w); };
	std::vector<float> last, turn(4, 0);
	for (const Relative &r : w.relatives(id))
		last.push_back(angle(r));
	for (double t = 0; t < 3; t += STEP) {
		w.step(STEP);
		const std::vector<Relative> rel = w.relatives(id);
		for (size_t i = 0; i < 4; i++) {
			float d = angle(rel[i]) - last[i];
			d -= std::round(d / (2 * core::PI)) * 2 * core::PI;
			turn[i] += d;
			last[i] = angle(rel[i]);
		}
	}
	UASSERT(w.velocity(id).X > 1.5f);
	const double went = w.pose(id).position.X - from;
	UASSERT(went > 2);
	// rolled, not slid: turned (clockwise from +z) as far as it went, at its radius
	for (float a : turn)
		UASSERT(std::fabs(-a * 0.4f - went) < went * 0.1);
	// each wheel's middle is on its axle, give or take its suspension's
	// travel, turned only about the axle
	const Assembly c = cart();
	const std::vector<Relative> rel = w.relatives(id);
	for (size_t i = 0; i < rel.size(); i++) {
		const v3f centre = c.bodies[i + 1].round->centre;
		UASSERT((rel[i].offset + turned(rel[i].rotation, centre) - centre).getLength() < 0.1f);
		UASSERT(std::fabs(rel[i].rotation.x) < 0.01f && std::fabs(rel[i].rotation.y) < 0.01f);
	}
}

void TestPhysics::testWheelsRollFurtherThanASledgeSlides()
{
	auto travel = [](const Assembly &a) {
		World w(flat());
		World::Id id = w.add(a, Pose{ v3d(0, 0.5, 0), {} }, v3f(3, 0, 0));
		run(w, 25);
		UASSERT(w.velocity(id).getLength() < 0.05f); // stopped
		return w.pose(id).position.X;
	};
	const double rolled = travel(cart()), slid = travel(sledge());
	UASSERT(rolled > 3);
	UASSERT(rolled > slid * 2);
}

void TestPhysics::testHingeKeepsItsAxle()
{
	const float a = 10 * core::DEGTORAD;
	World w(flat(v3f(G * std::sin(a), -G * std::cos(a), 0)));
	World::Id id = w.add(cart(true), Pose{ v3d(0, 0.5, 0), {} });
	run(w, 3);
	const Assembly c = cart(true);
	const std::vector<Relative> rel = w.relatives(id);
	UASSERTEQ(size_t, rel.size(), 4);
	for (size_t i = 0; i < rel.size(); i++) {
		// where it's turned to, the wheel's middle is still its axle's
		const v3f centre = c.bodies[i + 1].round->centre;
		UASSERT((rel[i].offset + turned(rel[i].rotation, centre) - centre).getLength() < 0.02f);
		// and it's turned only about the axle
		UASSERT(std::fabs(rel[i].rotation.x) < 0.01f && std::fabs(rel[i].rotation.y) < 0.01f);
	}
}

// Sprint 63: a hinge turns between its stops. The cart's round parts on hinges
// that stop at 20° either way, down the same slope: they turn to a stop
// (and slide) where the free ones roll on.
void TestPhysics::testHingeStops()
{
	const float a = 10 * core::DEGTORAD, limit = 20 * core::DEGTORAD;
	World w(flat(v3f(G * std::sin(a), -G * std::cos(a), 0)));
	Assembly stopped = cart(true);
	for (Hinge &h : stopped.hinges)
		h.limit = limit;
	World::Id id = w.add(stopped, Pose{ v3d(0, 0.5, 0), {} });
	run(w, 3);
	float most = 0;
	for (const Relative &r : w.relatives(id))
		most = std::max(most, 2 * std::acos(std::min(1.f, std::fabs(r.rotation.w))));
	UTEST(most > limit * 0.5f && most < limit + 2 * core::DEGTORAD, "turned %f°", most * core::RADTODEG);
}

void TestPhysics::testStableAndTopHeavy()
{
	auto upright = [](float base_density, float top_density) {
		World w(flat());
		Assembly a;
		// a narrow base and a wide top on a thin mast, tipped 12°
		a.bodies.push_back(block(v3f(0), v3f(0.6f, 0.5f, 0.6f), base_density));
		a.bodies[0].boxes.push_back(Box{ aabb3f(-0.1f, 0.25f, -0.1f, 0.1f, 2.25f, 0.1f), WOOD, 0.6f });
		a.bodies[0].boxes.push_back(Box{ aabb3f(-0.75f, 2.25f, -0.75f, 0.75f, 2.75f, 0.75f), top_density, 0.6f });
		World::Id id = w.add(a, Pose{ v3d(0, 0.1, 0), about(v3f(0, 0, 1), 12 * core::DEGTORAD) });
		run(w, 6);
		return turned(w.pose(id).rotation, v3f(0, 1, 0)).Y;
	};
	UASSERT(upright(STONE, WOOD / 10) > 0.98f); // stone at the bottom: it rights itself
	UASSERT(upright(WOOD / 10, STONE) < 0.5f);  // stone at the top: over it goes
}

void TestPhysics::testHeldGoesWhereItsTold()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	World::Id id = w.add(a, Pose{ v3d(0, 3, 0), {} });
	w.hold(id, true);
	w.carry(id, std::nullopt); // the workshop's grip: held, and not carried
	UASSERT(w.held(id));
	w.moveTo(id, Pose{ v3d(2, 4, 1), about(v3f(0, 1, 0), 0.5f) });
	run(w, 0.5);
	UASSERT(w.pose(id).position.getDistanceFrom(v3d(2, 4, 1)) < 0.001);
	UASSERT(std::fabs(w.pose(id).rotation.y - about(v3f(0, 1, 0), 0.5f).y) < 0.001f);
	// let go, it falls from there
	w.hold(id, false);
	run(w, 3);
	UASSERT(std::fabs(w.pose(id).position.Y) < 0.05);
	UASSERT(std::fabs(w.pose(id).position.X - 2) < 0.05);
}

// A person (75 kg, pushing with 600 N) walks into a cart: it rolls, and
// slows them; into a 5 t block: it doesn't budge.
void TestPhysics::testPlayerPushes()
{
	const CharacterSpec person{ 0.3f, 1.8f, 75, 600, 50 };
	{
		World w(flat());
		World::Id id = w.add(cart(), Pose{ v3d(0, 0.5, 0), {} });
		run(w, 1);
		const double was = w.pose(id).position.X;
		World::CharId me = w.addCharacter(person, v3d(-2, -0.5, 0));
		for (double t = 0; t < 3; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, v3f(3, c.velocity.Y - G * (float)STEP, 0));
			w.step(STEP);
		}
		UASSERT(w.pose(id).position.X > was + 1);
		UASSERT(w.character(me).foot.X < -2 + 3 * 3 - 1); // held up by it
	}
	{
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(2, 1, 1), STONE));
		World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
		run(w, 1);
		World::CharId me = w.addCharacter(person, v3d(-2, -0.5, 0));
		for (double t = 0; t < 3; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, v3f(3, c.velocity.Y - G * (float)STEP, 0));
			w.step(STEP);
		}
		UASSERT(std::fabs(w.pose(id).position.X) < 0.05);
		UASSERT(w.character(me).foot.X < -1.25); // against its side
	}
	{
		// held (a client's mirror of the server's): it doesn't give, and
		// what's pressed on it is kept, as hard as a person pushes, along +x
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(1), STONE));
		World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
		w.hold(id, true);
		World::CharId me = w.addCharacter(person, v3d(-2, -0.5, 0));
		v3f pushed(0);
		double pressing = 0;
		for (double t = 0; t < 2; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, v3f(3, c.velocity.Y - G * (float)STEP, 0));
			w.step(STEP);
			for (const auto &[on, impulse] : w.pushes(me)) {
				UASSERT(on == id);
				pushed += impulse;
				pressing += STEP;
			}
		}
		UASSERT(std::fabs(w.pose(id).position.X) < 0.001);
		UASSERT(pressing > 1);
		UTEST(pushed.X > 0.8f * 600 * pressing && pushed.X < 1.01f * 600 * pressing, "pushed %f over %f s", pushed.X, pressing);
		UASSERT(std::fabs(pushed.Z) < 0.1f * pushed.X);
		UASSERT(w.pushes(me).empty()); // taken
	}
	{
		// stood on (sprint 67): its weight's the server's to give, not a push
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(2, 1, 2), STONE));
		World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
		w.hold(id, true);
		World::CharId me = w.addCharacter(person, v3d(0, 0.6, 0));
		v3f pushed(0);
		for (double t = 0; t < 1; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, v3f(0, c.velocity.Y - G * (float)STEP, 0));
			w.step(STEP);
			for (const auto &[on, impulse] : w.pushes(me))
				pushed += impulse;
		}
		UASSERT(w.character(me).foot.Y > 0.4); // on it
		UTEST(pushed.getLength() < 1, "pushed %f down", pushed.Y);
	}
}

void TestPhysics::testSameEveryTime()
{
	auto once = [] {
		const float a = 10 * core::DEGTORAD;
		World w(flat(v3f(G * std::sin(a), -G * std::cos(a), 0)));
		World::Id id = w.add(cart(), Pose{ v3d(0, 0.5, 0), about(v3f(0, 1, 0), 0.3f) });
		World::Id other = w.add(sledge(), Pose{ v3d(4, 1, 0.5), {} });
		run(w, 4);
		return std::make_pair(w.pose(id), w.pose(other));
	};
	UASSERT(once() == once()); // bit for bit
}

// At rest and frozen, a push wakes it and it slides.
void TestPhysics::testPushedWhenFrozen()
{
	World w(flat());
	// two blocks side by side, as a machine of two is
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	a.bodies[0].boxes.push_back(Box{ aabb3f(0.5f, -0.5f, -0.5f, 1.5f, 0.5f, 0.5f), STONE, 0.6f });
	World::Id id = w.add(a, Pose{ v3d(8, 6, 8), {} });
	run(w, 6);
	UASSERT(w.tier(id) == Tier::Frozen);
	w.impulse(id, v3f(STONE * 2 * 2, 0, 0)); // 2 nodes/s
	run(w, 2);
	// sliding against friction 0.6 from 2 nodes/s: 2² / (2 · 0.6 · g) ≈ 0.34
	UASSERT(w.pose(id).position.X > 8.25);
}

namespace {

// Walking for `seconds` asked so, on the ground (or not), from `v`.
v3f walked(const Gait &g, const Intent &i, v3f v, double seconds, bool ground = true, v3f under = v3f(0))
{
	for (double t = 0; t < seconds - 1e-9; t += STEP)
		v = walk(g, i, Footing{ v, ground, under }, (float)STEP);
	return v;
}

} // namespace

void TestPhysics::testWalkHasInertia()
{
	const Gait g;
	const Intent ahead{ 1, 0, 0, false }; // yaw 0: forward is +z
	// up to walking speed in about a quarter of a second, not at once
	UASSERT(walked(g, ahead, v3f(0), STEP).Z < 1);
	UASSERT(walked(g, ahead, v3f(0), 0.15).Z < g.walk * 0.8f);
	UASSERT(std::fabs(walked(g, ahead, v3f(0), 0.3).Z - g.walk) < 1e-3f);
	// running: past walking speed slowly
	Gait run = g;
	run.run = 1.6f;
	const v3f walking = walked(g, ahead, v3f(0), 0.3);
	UASSERT(walked(run, ahead, walking, 0.2).Z < g.walk * 1.3f);
	UASSERT(std::fabs(walked(run, ahead, walking, 0.6).Z - g.walk * 1.6f) < 1e-3f);
	// let go: stops sooner than it started
	UASSERT(walked(g, Intent{}, v3f(0, 0, g.walk), 0.15).getLength() < 1e-3f);
	// turned right at a run: a curve, still going forward a while
	const v3f turning = walked(run, Intent{ 0, 1, 0, false }, v3f(0, 0, g.walk * 1.6f), 0.1);
	UASSERT(turning.Z > 2 && turning.X > 0.5f);
	// yaw: 90° faces −x
	UASSERT(walked(g, Intent{ 1, 0, core::PI / 2, false }, v3f(0), 0.5).X < -g.walk + 1e-3f);
	// in the air, it barely steers; it falls
	const v3f air = walked(g, ahead, v3f(0), 0.5, false);
	UASSERT(air.Z < 2 && air.Y < -4);
	// on a moving cart, standing still is moving with it
	const v3f under(2, 0, 0);
	UASSERT(walked(g, Intent{}, under, 0.5, true, under).getDistanceFrom(under) < 1e-3f);
}

void TestPhysics::testJumpIsAHeight()
{
	for (float gravity : { 9.81f, 1.62f }) { // here and on a moon: the same height
		Gait g;
		g.gravity = v3f(0, -gravity, 0);
		v3f v = walk(g, Intent{ 0, 0, 0, true }, Footing{ v3f(0), true }, (float)STEP);
		double y = 0, top = 0;
		for (int i = 0; i < 2000 && (i == 0 || y > 0); i++) {
			y += v.Y * STEP;
			top = std::max(top, y);
			v = walk(g, Intent{}, Footing{ v, false }, (float)STEP);
		}
		UASSERT(std::fabs(top - g.jump) < 0.05);
	}
}

void TestPhysics::testStepsAndRides()
{
	const CharacterSpec person{ 0.3f, 1.8f, 75, 600, 50 };
	const Gait g;
	auto go = [&](World &w, World::CharId me, const Intent &i, double seconds) {
		for (double t = 0; t < seconds; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, walk(g, i, Footing{ c.velocity, c.on_ground, c.ground_velocity }, (float)STEP));
			w.step(STEP);
		}
	};
	// a step half a node high it walks up; a whole node it doesn't
	for (float rise : { 0.5f, 1.0f }) {
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(12, rise, 4), STONE)); // from x 1 on, longer than the walk
		World::Id id = w.add(a, Pose{ v3d(7, -0.5 + rise / 2, 0), {} });
		w.hold(id, true); // a fixed step
		World::CharId me = w.addCharacter(person, v3d(0, -0.5, 0));
		go(w, me, Intent{ 0, 1, 0, false }, 2); // right is +x at yaw 0
		const World::CharacterState c = w.character(me);
		if (rise < 0.6f) {
			UASSERT(c.foot.Y > -0.5 + rise - 0.05 && c.foot.X > 1.5);
		} else {
			UASSERT(c.foot.Y < -0.4 && c.foot.X < 1.0);
		}
	}
	// standing on a platform moving at 2 nodes/s, it's carried along
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(4, 0.5f, 4), STONE));
	World::Id id = w.add(a, Pose{ v3d(0, 0.25, 0), {} });
	w.hold(id, true);
	World::CharId me = w.addCharacter(person, v3d(0, 0.5, 0));
	go(w, me, Intent{}, 0.5); // settles on it
	const double from = w.character(me).foot.X;
	for (double t = 0; t < 1; t += STEP) {
		w.moveTo(id, Pose{ v3d(2 * (t + STEP), 0.25, 0), {} });
		go(w, me, Intent{}, STEP);
	}
	UASSERT(std::fabs(w.character(me).foot.X - from - 2) < 0.15);
	UASSERT(w.character(me).on_ground);
}

// Carried by someone with 2000 N: a wooden block follows the hands; a
// stone one more than they can hold up sags to the ground and drags.
void TestPhysics::testCarriedFollowsOrDrags()
{
	for (float density : { WOOD, STONE }) {
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(0.5f), density));
		World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
		run(w, 0.5);
		w.carry(id, 2000.f);
		for (double t = 0; t < 2; t += STEP) {
			w.pullTo(id, Pose{ v3d(t, 1.5, 0), {} }); // carried along at a node a second, up high
			w.step(STEP);
		}
		const v3d p = w.pose(id).position;
		if (density == WOOD) { // 75 kg: in the hands
			UASSERT(p.getDistanceFrom(v3d(2, 1.5, 0)) < 0.15);
		} else { // 340 kg: on the ground
			UASSERT(p.Y < 0);
		}
		w.carry(id, std::nullopt);
		run(w, 3);
		UASSERT(std::fabs(w.pose(id).position.Y - (-0.25)) < 0.05); // let go, it falls
	}
	{
		// the ground above y 0 never loaded (a client's sky, a planet's
		// seam) and the block held first, as a client's mirror is: carried,
		// it still follows the hands
		World w(World::Opts{ physics::uniform(v3f(0, -G, 0)), [](v3s16 block, std::vector<aabb3f> &boxes) {
			return block.Y < 0 && flatGround(block, boxes);
		} });
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(0.5f), WOOD));
		World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
		w.hold(id, true);
		run(w, 0.5);
		w.hold(id, false);
		w.carry(id, 2000.f);
		for (double t = 0; t < 2; t += STEP) {
			w.pullTo(id, Pose{ v3d(t, 1.5, 0), {} });
			w.step(STEP);
		}
		UASSERT(w.pose(id).position.getDistanceFrom(v3d(2, 1.5, 0)) < 0.15);
	}
}

// Sprint 59: gravity and air are a field. A block dropped through air
// settles at √(2mg / ρ·Cd·A); in none it keeps speeding up.
void TestPhysics::testAirGivesATopSpeed()
{
	const float AIR = 1.225f;
	auto fall = [](float air) {
		World w(World::Opts{ [air](const v3d &, const v3f &) { return physics::Here{ v3f(0, -G, 0), air }; }, flatGround });
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(1), 100)); // 100 kg, a square metre to the air
		const World::Id id = w.add(a, { v3d(0, 3000, 0), {} });
		for (int i = 0; i < 20 * 60; i++)
			w.step(STEP);
		return -w.velocity(id).Y;
	};
	const float top = std::sqrt(2 * 100 * G / AIR); // ~40 m/s (Cd 1)
	UASSERT(std::fabs(fall(AIR) - top) < 0.03f * top);
	UASSERT(std::fabs(fall(0) - G * 20) < 1);
}

namespace {

// A hill falling a quarter node per node along +x (14°), as shaped
// ground comes: each node the surface crosses a staircase of sixteenth
// slices along x, the rest whole blocks. The surface is y = -x / 4.
bool slicedSlope(v3s16 block, std::vector<aabb3f> &boxes)
{
	auto surface = [](float gx) { return -gx / 4; };
	for (s16 x = 0; x < BLOCK; x++)
		for (s16 z = 0; z < BLOCK; z++)
			for (s16 y = 0; y < BLOCK; y++) {
				const int gx = block.X * BLOCK + x, gy = block.Y * BLOCK + y;
				const v3f at((float)x, (float)y, (float)z);
				if (gy + 0.5f <= surface(gx + 0.5f)) {
					boxes.push_back(aabb3f(at - v3f(0.5f), at + v3f(0.5f)));
					continue;
				}
				for (int s = 0; s < 16; s++) {
					const float top = std::min(gy + 0.5f, surface(gx - 0.5f + s / 16.f));
					if (top > gy - 0.5f)
						boxes.push_back(aabb3f(x - 0.5f + s / 16.f, y - 0.5f, z - 0.5f,
								x - 0.5f + (s + 1) / 16.f, top - gy + y, z + 0.5f));
				}
			}
	return true;
}

} // namespace

void TestPhysics::testRollsDownASlicedSlope()
{
	// down a staircase of sixteenths a wheel hops; down its hull it rolls
	World w(World::Opts{ physics::uniform(v3f(0, -G, 0)), slicedSlope });
	World::Id id = w.add(cart(), Pose{ v3d(-6, 2.5, 0.5), {} });
	run(w, 1.5); // landed
	float jolt = 0;
	v3f was = w.velocity(id);
	for (double t = 0; t < 2; t += STEP) {
		w.step(STEP);
		const v3f v = w.velocity(id);
		// rolling down, it's never thrown up off the slope
		jolt = std::max(jolt, (v.Y - was.Y) / (float)STEP);
		was = v;
	}
	UASSERT(w.velocity(id).X > 1);
	UTEST(jolt < 3, "thrown off the slope at %f m/s²", jolt);
}

void TestPhysics::testCornerPushTurns()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(4, 1, 1), WOOD));
	World::Id id = w.add(a, Pose{ v3d(8, 0.1, 8), {} });
	run(w, 2);
	// pushed along +z from beside its +x end: it turns as well as goes
	w.impulse(id, v3f(0, 0, WOOD * 4 * 2), v3d(9.9, 0, 7));
	run(w, 0.5);
	const Quat q = w.pose(id).rotation;
	UTEST(std::fabs(q.y) > 0.02f, "turned %f", q.y);
	UASSERT(w.pose(id).position.Z > 8.05);
}

// Sprint 67: a tyre is cast for the ground and a wheel's body never meets
// it: on its side a cart lay on nothing and sank. Its wheels' backstops
// hold it up.
void TestPhysics::testTippedWheelsHoldUp()
{
	World w(flat());
	// on its side: its axles (z) upright
	World::Id id = w.add(cart(), Pose{ v3d(0, 1.5, 0), about(v3f(1, 0, 0), core::PI / 2) });
	run(w, 3);
	// on its wheels' outer faces (0.75 out): its middle 0.25 up; through
	// them, on the plate's edge (0.5 out), it'd be at 0
	UTEST(w.pose(id).position.Y > 0.2, "sank to %f", w.pose(id).position.Y);
}

// Built on the ground, a wheel's in it: it's lifted out, not dropped through.
void TestPhysics::testPutDownInTheGround()
{
	World w(flat());
	// the wheels' bottoms (0.5 under the middle) 0.2 in the floor
	World::Id id = w.add(cart(), Pose{ v3d(0, -0.2, 0), {} });
	run(w, 2);
	const double y = w.pose(id).position.Y;
	UTEST(y > -0.1 && y < 0.5, "at %f", y);
}

// A player placed once a server step (0.09 s, 5 of ours) walks into a
// light cart: it's pushed along, not thrown (all the way in one of five
// steps, a stroll was 30 m/s).
void TestPhysics::testWalkingIntoACartDoesntThrowIt()
{
	World w(flat());
	Assembly light = cart();
	for (Body &b : light.bodies)
		for (Box &x : b.boxes)
			x.density = 50;
	World::Id id = w.add(light, Pose{ v3d(2, 0.1, 0), {} });
	run(w, 1);
	float most = 0;
	for (int i = 0; i < 20; i++) {
		// 1.5 m/s toward it, along x, through where it is
		w.players({ Capsule{ v3d(-0.5 + i * 0.135, -0.5, 0), 0.35f, 1.8f } });
		w.step(0.09);
		most = std::max(most, w.velocity(id).getLength());
	}
	UTEST(most < 4, "thrown at %f m/s", most);
}

// Sprint 68: grabbed at a point, pulled toward a hand as hard as a person.
// A light block comes up into it; a long beam grabbed at its end tips up
// about its other end; a heavy one barely gives.
void TestPhysics::testGrabbedLiftsTipsOrHolds()
{
	constexpr float PERSON = 1500;
	{
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(0.5f), WOOD)); // 75 kg
		World::Id id = w.add(a, Pose{ v3d(0, -0.25, 0), {} });
		run(w, 0.5);
		w.grab(id, 1, v3f(0, 0.25f, 0), PERSON); // its top
		for (double t = 0; t < 1.5; t += STEP) {
			w.grabTo(id, 1, v3d(0, 1.5, 0));
			w.step(STEP);
		}
		const v3d p = w.pose(id).position;
		UTEST(p.getDistanceFrom(v3d(0, 1.25, 0)) < 0.15, "at %f %f %f", p.X, p.Y, p.Z);
		w.release(id, 1);
		run(w, 2);
		UTEST(w.pose(id).position.Y < 0, "let go, it's at %f", w.pose(id).position.Y);
	}
	{
		// a beam 4 long, 250 kg: its end lifted (half its weight, in a
		// person's reach), it turns up about the other, which stays down
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(4, 0.5f, 0.5f), 250));
		World::Id id = w.add(a, Pose{ v3d(0, -0.25, 0), {} });
		run(w, 0.5);
		w.grab(id, 1, v3f(2, 0.25f, 0), PERSON);
		for (double t = 0; t < 2; t += STEP) {
			w.grabTo(id, 1, v3d(2, 1.5, 0));
			w.step(STEP);
		}
		const Quat q = w.pose(id).rotation;
		UTEST(std::fabs(q.z) > 0.1f, "turned %f", q.z);
		UTEST(w.pose(id).position.Y < 0.8, "lifted whole to %f", w.pose(id).position.Y);
	}
	{
		// 4 t of stone: a person can't lift it
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(1.2f), STONE));
		World::Id id = w.add(a, Pose{ v3d(0, 0.1, 0), {} });
		run(w, 1);
		const double was = w.pose(id).position.Y;
		w.grab(id, 1, v3f(0, 0.6f, 0), PERSON);
		for (double t = 0; t < 1; t += STEP) {
			w.grabTo(id, 1, v3d(0, 2, 0));
			w.step(STEP);
		}
		UTEST(w.pose(id).position.Y - was < 0.02, "lifted %f", w.pose(id).position.Y - was);
	}
}

// A copy brought toward the server's word: a share of the way, or put
// there when it's far off.
void TestPhysics::testCorrectedTowardAnother()
{
	World w(flat(v3f(0)));
	World::Id id = w.add(cart(), Pose{ v3d(0, 2, 0), {} });
	w.impulse(id, v3f(0)); // live
	w.correct(id, Pose{ v3d(1, 2, 0), {} }, 0.5f, 3);
	UTEST(std::fabs(w.pose(id).position.X - 0.5) < 1e-3, "at %f", w.pose(id).position.X);
	// the wheels came along (each body's frame is the cart's: none off it)
	for (const Relative &r : w.relatives(id))
		UTEST(r.offset.getLength() < 1e-3, "a wheel left %f behind", r.offset.getLength());
	w.correct(id, Pose{ v3d(10, 2, 0), {} }, 0.1f, 3);
	UTEST(std::fabs(w.pose(id).position.X - 10) < 1e-3, "at %f", w.pose(id).position.X);
}

// One cart put on another and the lower pushed: they settle and go
// together, without jolting about.
void TestPhysics::testCartOnACart()
{
	World w(flat());
	World::Id low = w.add(cart(), Pose{ v3d(0, 0.1, 0), {} });
	World::Id high = w.add(cart(), Pose{ v3d(0, 0.9, 0), {} });
	run(w, 2);
	float most = 0;
	for (int i = 0; i < 60; i++) {
		w.impulse(low, v3f(470 * 0.05f, 0, 0)); // ~1 m/s² for a second
		w.step(STEP);
		most = std::max(most, std::fabs(w.velocity(high).Y));
	}
	run(w, 1);
	UTEST(most < 0.5f, "the top one jolted at %f m/s", most);
	UTEST(w.pose(high).position.Y > 0.5, "the top one's at %f", w.pose(high).position.Y);
}

// a lawnmower's engine and a strong car's (W)
constexpr float SMALL = 3000, BIG = 370000;

// Sprint 69: a driver's forward turns the engines' power into the wheels;
// a cart with none goes nowhere. Sprint 73: only the driven wheels are
// turned and only the steering ones steer, and ahead is the seat's facing.
// A car facing +x: its front wheels steer, its back ones are driven.
// Sprint 78: an engine's a power (W) to its most rpm, not a torque.
Assembly car(float power, v3f facing = v3f(1, 0, 0), bool steers = true, bool driven = true)
{
	Assembly a = cart();
	a.power = power;
	a.rpm = 6000;
	a.facing = facing;
	for (Wheel &w : a.wheels) {
		const bool front = w.centre.dotProduct(facing) > 0;
		w.steers = steers && front;
		w.driven = driven && !front;
	}
	return a;
}

void TestPhysics::testAeroReadsShapes()
{
	const v3f ahead(0, 0, 1);
	auto area = [](const physics::Aero &a) {
		float sum = 0;
		for (const physics::Surface &s : a.surfaces)
			sum += s.area;
		return sum;
	};
	// a plate: one wing, flat, all its area
	const physics::Aero plate = physics::aeroOf({ aabb3f(-3, 0, -.5f, 3, 1 / 16.f, .5f) }, ahead);
	UASSERT(!plate.surfaces.empty());
	UASSERT(std::fabs(area(plate) - 6) < 0.1f);
	for (const physics::Surface &s : plate.surfaces) {
		UASSERT(std::fabs(s.normal.Y) == 1);
		UASSERT(std::fabs(s.incidence) < 0.01f);
		UASSERT(std::fabs(s.aspect - 6) < 0.1f);
		UASSERT(s.at.Z > 0); // a quarter of its chord from the front
	}
	UASSERT(std::fabs(plate.shown.Y - 6) < 0.1f);
	UASSERT(plate.shown.Z < 0.1f * 6 / 16.f + 0.01f); // edge on, it parts the air
	// a fin: the same on its side
	const physics::Aero fin = physics::aeroOf({ aabb3f(0, 0, -.5f, 1 / 16.f, 2, .5f) }, ahead);
	UASSERT(!fin.surfaces.empty() && std::fabs(fin.surfaces[0].normal.X) == 1);
	// a block is no wing
	UASSERT(physics::aeroOf({ aabb3f(0, 0, 0, 1, 1, 1) }, ahead).surfaces.empty());
	// a sixteenth up each sixteenth across a block: its front up by that
	std::vector<aabb3f> ramp;
	for (int i = 0; i < 16; i++)
		ramp.push_back(aabb3f(0, i / 16.f * 0.25f, i / 16.f, 4, i / 16.f * 0.25f + 1 / 16.f, (i + 1) / 16.f));
	const physics::Aero up = physics::aeroOf(ramp, ahead);
	UASSERT(!up.surfaces.empty());
	UASSERT(std::fabs(up.surfaces[0].incidence - std::atan(0.25f)) < 0.05f);
	// a stalled wing lifts less than one just short of it
	const physics::Surface s = plate.surfaces[0];
	UASSERT(physics::coefficients(s, 0.2f).lift > physics::coefficients(s, 0.05f).lift);
	UASSERT(physics::coefficients(s, 0.2f).lift > physics::coefficients(s, 0.5f).lift);
	UASSERT(physics::coefficients(s, -0.2f).lift < 0);
	// lift acts behind the middle of mass of a wing and tail
	v3f centre;
	const physics::Aero both = physics::aeroOf({ aabb3f(-3, 0, -.5f, 3, 1 / 16.f, .5f), aabb3f(-1, 0, -5, 1, 1 / 16.f, -4) }, ahead);
	UASSERT(physics::centreOfLift({ both }, v3f(0, 1, 0), centre));
	UASSERT(centre.Z < 0);
	// the balance of a machine: a stone ahead of a wing, its mass ahead of the lift
	Assembly a;
	a.bodies.push_back(block(v3f(0, 0, 0), v3f(6, 1 / 16.f, 1), WOOD));
	a.bodies[0].boxes.push_back(Box{ aabb3f(-.5f, -.5f, 3, .5f, .5f, 4), STONE, 0.6f });
	const physics::Balance balance = physics::balanceOf(a);
	UASSERT(balance.lifts);
	UASSERT(balance.mass_at.Z > balance.lift_at.Z);
}

void TestPhysics::testGlides()
{
	const float AIR = 1.225f;
	World::Opts opts{ [AIR](const v3d &, const v3f &) { return physics::Here{ v3f(0, -G, 0), AIR }; }, flatGround };
	struct Flown
	{
		float fell, went;
	};
	auto fly = [&](const Assembly &a, int settle, int judged) {
		World w(opts);
		const World::Id id = w.add(a, { v3d(0, 30000, 0), {} }, v3f(0, 0, 44));
		// launched off its trim it swings about its glide a while: judged after
		for (int i = 0; i < settle * 60; i++)
			w.step(STEP);
		const Pose from = w.pose(id);
		// the way it went, not how far off: with no dihedral it wanders round
		float went = 0;
		for (int i = 0; i < judged * 60; i++) {
			w.step(STEP);
			const v3f u = w.velocity(id);
			went += std::hypot(u.X, u.Z) * (float)STEP;
		}
		const Pose p = w.pose(id);
		return Flown{ float(from.position.Y - p.position.Y), went };
	};
	// a wing a little nose up, a level tail well behind, ballast keeping the
	// centre of mass ahead of the wing's lift
	Assembly glider;
	Body whole;
	for (int i = 0; i < 16; i++) // an eighth up over a metre and a half's chord
		whole.boxes.push_back(Box{ aabb3f(-6, 1 / 8.f * i / 16, -1.5f + 1.5f * i / 16, 6, 1 / 8.f * i / 16 + 1 / 16.f, -1.5f + 1.5f * (i + 1) / 16), WOOD, 0.6f });
	whole.boxes.push_back(Box{ aabb3f(-1.5f, 0, -6, 1.5f, 1 / 16.f, -5), WOOD, 0.6f }); // tail
	whole.boxes.push_back(Box{ aabb3f(-1 / 32.f, 0, -6, 1 / 32.f, 1, -5), WOOD, 0.6f }); // fin
	whole.boxes.push_back(Box{ aabb3f(-1 / 16.f, -1 / 8.f, -6, 1 / 16.f, 0, 2), WOOD, 0.6f }); // boom
	whole.boxes.push_back(Box{ aabb3f(-.25f, -.25f, 2, .25f, .25f, 2.5f), STONE, 0.6f }); // ballast
	glider.bodies.push_back(whole);
	const Flown g = fly(glider, 240, 60);
	infostream << "glider fell " << g.fell << " went " << g.went << std::endl;
	// the same weight as a lump
	float kg = 0;
	for (const Box &x : whole.boxes)
		kg += x.box.getVolume() * x.density;
	Assembly lump;
	const float side = std::cbrt(kg / STONE);
	lump.bodies.push_back(block(v3f(0), v3f(side), STONE));
	const Flown l = fly(lump, 0, 20);
	infostream << "lump fell " << l.fell << " went " << l.went << std::endl;
	UASSERT(g.went > 5 * g.fell);  // it flies
	UASSERT(l.fell > l.went);      // a lump only falls
}

void TestPhysics::testDriven()
{
	for (float power : { 0.f, SMALL }) {
		World w(flat());
		World::Id id = w.add(car(power), Pose{ v3d(2, 0.1, 8), {} });
		run(w, 1);
		w.drive(id, physics::Drive{ 1, 0, 0 });
		run(w, 1.5);
		const float speed = w.velocity(id).getLength();
		if (power == 0) {
			UTEST(speed < 0.1f, "no engine, yet it goes %f", speed);
			continue;
		}
		UTEST(speed > 1.f && speed < 6, "driven to %f", speed);
		UTEST(w.velocity(id).X > 0, "driven back, at %f", w.velocity(id).X);
		w.drive(id, physics::Drive{ 0.3f, 1, 0 });
		run(w, 1);
		const Quat q = w.pose(id).rotation;
		UTEST(std::fabs(q.y) > 0.05f, "steered only %f", q.y);
		// right is the driver's right: facing +x, toward -z (sprint 73: it was left)
		UTEST(w.velocity(id).Z < -0.1f, "steered right, it went %f along z", w.velocity(id).Z);
		w.drive(id, physics::Drive{ 0, 0, 1 });
		run(w, 3);
		UTEST(w.velocity(id).getLength() < 0.2f, "braked, still at %f", w.velocity(id).getLength());
		UTEST(w.pose(id).position.Y > -0.5, "off the floor at %f", w.pose(id).position.Y);
	}
	{
		// the seat facing the other way, forward goes the other way
		float went[2];
		for (int ahead : { 1, -1 }) {
			World w(flat());
			World::Id id = w.add(car(SMALL, v3f(float(ahead), 0, 0)), Pose{ v3d(8, 0.1, 8), {} });
			run(w, 1);
			w.drive(id, physics::Drive{ 1, 0, 0 });
			run(w, 1);
			went[ahead > 0] = float(w.pose(id).position.X - 8);
		}
		UTEST(went[0] < -0.1f && went[1] > 0.1f, "went %f facing back, %f ahead", went[0], went[1]);
	}
	{
		// an engine driving no wheel goes nowhere
		World w(flat());
		World::Id id = w.add(car(SMALL, v3f(1, 0, 0), true, false), Pose{ v3d(2, 0.1, 8), {} });
		run(w, 1);
		w.drive(id, physics::Drive{ 1, 0, 0 });
		run(w, 1.5);
		UTEST(w.velocity(id).getLength() < 0.1f, "no wheel driven, yet it goes %f", w.velocity(id).getLength());
	}
	{
		// no wheel steering, it goes straight
		World w(flat());
		World::Id id = w.add(car(SMALL, v3f(1, 0, 0), false, true), Pose{ v3d(2, 0.1, 8), {} });
		run(w, 1);
		w.drive(id, physics::Drive{ 1, 1, 0 });
		run(w, 2);
		UTEST(std::fabs(w.pose(id).rotation.y) < 0.01f, "no wheel steers, yet it turned %f", w.pose(id).rotation.y);
		UTEST(w.velocity(id).X > 1, "it went %f", w.velocity(id).X);
	}
}

// Sprint 69: something in someone's hands (grabbed, carried, driven) wasn't
// asked what ground it needs, so two seconds on the ground under it was
// dropped and it fell through. A block dragged along for five seconds
// stays on the floor.
void TestPhysics::testGrabbedKeepsItsGround()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), 150)); // 150 kg, its pull past its friction
	World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
	run(w, 0.5);
	w.grab(id, 1, v3f(0.5f, 0, 0), 1500);
	for (double t = 0; t < 5; t += STEP) {
		w.grabTo(id, 1, v3d(1 + t, 0, 0));
		w.step(STEP);
	}
	UTEST(w.pose(id).position.Y > -0.1, "fell through to %f", w.pose(id).position.Y);
	UTEST(w.pose(id).position.X > 2, "dragged only to %f", w.pose(id).position.X);
}

// Sprint 71: a grip on a wheel was a grip on the car (wheels aren't bodies
// to Jolt), so its mark slid off the turning wheel and a rim pulled dragged
// the car. Now the grip's on the wheel and turns with it: the front of a
// rim lifted turns the wheel, and the car isn't hauled up with it.
void TestPhysics::testWheelTurnedByItsRim()
{
	World w(flat());
	World::Id id = w.add(cart(), Pose{ v3d(2, 0.1, 8), {} });
	run(w, 1);
	const v3d root = w.pose(id).position;
	const v3d hub = root + v3d(0.7, -0.1, 0.65);
	w.grab(id, 1, v3f(1.06f, -0.1f, 0.65f), 400); // the front of the front wheel's rim
	const v3d from = *w.gripAt(id, 1);
	for (double t = 0; t < 0.6; t += STEP) {
		w.grabTo(id, 1, from + v3d(0, std::min(t, 0.3), 0));
		w.step(STEP);
	}
	const v3d at = *w.gripAt(id, 1);
	const v3d moved = w.pose(id).position - root;
	const double off = (at - (hub + moved)).getLength();
	UTEST(at.Y - from.Y > 0.1, "the rim's point rose only %f", at.Y - from.Y);
	UTEST(std::fabs(off - 0.36) < 0.08, "the grip left the rim: %f from the hub", off);
	UTEST(std::fabs(moved.Y) < 0.1, "the car was hauled %f", moved.Y);
}

// Sprint 71: wheels looked to fall through the ground as a car was pulled
// about. A cart dragged by a corner, sideways and along, over many ground
// blocks: its wheels' middles stay on the floor here (what sank was the
// client's drawing of them: content_cao glides a group by its middle now).
void TestPhysics::testCartDraggedKeepsItsWheelsUp()
{
	for (const v3d way : { v3d(0, 0, 1), v3d(1, 0, 0), v3d(0.7, 0, 0.7) }) {
		World w(flat());
		World::Id id = w.add(cart(), Pose{ v3d(2, 0.1, 2), {} });
		run(w, 1);
		w.grab(id, 1, v3f(1, 0.1f, 0.5f), 1500);
		const v3d from = *w.gripAt(id, 1);
		double lowest = 1e9, lowest_wheel = 1e9;
		float tilt = 1;
		for (double t = 0; t < 6; t += STEP) {
			w.grabTo(id, 1, from + way * (2.5 * t) + v3d(0, 0.3, 0));
			w.step(STEP);
			const Pose p = w.pose(id);
			lowest = std::min(lowest, p.position.Y);
			tilt = std::min(tilt, workshop::turned(p.rotation, v3f(0, 1, 0)).Y);
			// each wheel's middle (a wheel body's origin is the cart's: it swings as it turns)
			const std::vector<Relative> rel = w.relatives(id);
			const Assembly shape = cart();
			for (const Wheel &wheel : shape.wheels) {
				const Relative &r = rel[wheel.body - 1];
				const v3f at = workshop::turned(p.rotation, r.offset + workshop::turned(r.rotation, wheel.centre));
				lowest_wheel = std::min(lowest_wheel, p.position.Y + at.Y);
			}
		}
		UTEST(lowest > -0.3, "the cart sank to %f", lowest);
		UTEST(lowest_wheel > -0.3, "a wheel sank to %f", lowest_wheel);
	}
}

// Sprint 72: a car sank on one side while grabbed and came back up when let
// go: the client's copy. As a client has it: held where the server says,
// then let go and grabbed, pulled, and brought toward the server's word
// (a step late) a share a frame. Its body and wheels stay on the floor.
void TestPhysics::testClientsGrabbedCartStaysUp()
{
	for (const int kind : { 0, 1, 2 }) {
	const bool hinged = kind == 1, mixed = kind == 2;
	World server(flat()), client(flat());
	const Pose start{ v3d(2, 0.1, 2), {} };
	World::Id s = server.add(cart(hinged, mixed), start), c = client.add(cart(hinged, mixed), start);
	client.hold(c, true);
	for (double t = 0; t < 1; t += STEP) {
		server.step(STEP);
		client.moveTo(c, server.pose(s));
		client.step(STEP);
	}
	client.hold(c, false);
	const v3f point(1, 0.1f, 0.5f);
	server.grab(s, 1, point, 1500);
	client.grab(c, 1, point, 1500);
	const v3d from = *client.gripAt(c, 1);
	double lowest = 1e9, lowest_wheel = 1e9;
	const Assembly shape = cart(hinged, mixed);
	for (double t = 0; t < 6; t += STEP) {
		const v3d hand = from + v3d(0, 0, 2.5 * t) + v3d(0, 0.3, 0);
		server.grabTo(s, 1, hand);
		client.grabTo(c, 1, hand);
		server.step(STEP);
		client.step(STEP);
		client.correct(c, { server.pose(s).position + v3d::from(server.velocity(s) * 0.1f), server.pose(s).rotation }, 0.05f, 1.5f);
		const Pose p = client.pose(c);
		lowest = std::min(lowest, p.position.Y);
		const std::vector<Relative> rel = client.relatives(c);
		for (const Wheel &wheel : shape.wheels) {
			const Relative &r = rel[wheel.body - 1];
			const v3f at = workshop::turned(p.rotation, r.offset + workshop::turned(r.rotation, wheel.centre));
			lowest_wheel = std::min(lowest_wheel, p.position.Y + at.Y);
		}
	}
	UTEST(lowest > -0.3, "the client's cart sank to %f", lowest);
	UTEST(lowest_wheel > -0.3, "a client's wheel sank to %f", lowest_wheel);
	}
}

// Sprint 78: a strong car's engine on a light cart is quick: it's past
// 25 m/s (90 km/h) in six seconds from a stop, and still upright.
void TestPhysics::testQuick()
{
	World w(flat());
	World::Id id = w.add(car(BIG), Pose{ v3d(2, 0.1, 8), {} });
	run(w, 1);
	w.drive(id, physics::Drive{ 1, 0, 0 });
	float at3 = 0;
	for (int i = 0; i < 6; i++) {
		run(w, 1);
		if (i == 2)
			at3 = w.velocity(id).X;
	}
	const float speed = w.velocity(id).X;
	infostream << "TestPhysics: a 370 kW cart at 3 s " << at3 << " m/s, at 6 s " << speed << " m/s" << std::endl;
	UTEST(speed > 25, "only %f m/s after six seconds", speed);
	UTEST(w.pose(id).position.Y > -0.5 && w.pose(id).position.Y < 1, "off the floor at %f", w.pose(id).position.Y);
}

// Sprint 79: a car on its roof, set upright, lands on its wheels facing
// as it did.
void TestPhysics::testUpright()
{
	World w(flat());
	const Quat roof = about(v3f(1, 0, 0), core::PI); // on its roof, rolled over its length (it's along x)
	World::Id id = w.add(car(SMALL), Pose{ v3d(2, 0.6, 8), roof });
	run(w, 2);
	UTEST(w.pose(id).rotation.x * w.pose(id).rotation.x > 0.5f, "it didn't stay on its roof (%f)", w.pose(id).rotation.x);
	w.upright(id, 1.5f);
	run(w, 3);
	const Quat q = w.pose(id).rotation;
	// level: no pitch or roll left, only a turn about the up
	UTEST(q.x * q.x + q.z * q.z < 0.01f, "still tipped (%f, %f)", q.x, q.z);
	// facing +x as it did (rolled, not spun round)
	const float ahead = 1 - 2 * (q.y * q.y + q.z * q.z);
	UTEST(ahead > 0.9f, "turned about (%f along x)", ahead);
	UTEST(w.pose(id).position.Y > -0.3 && w.pose(id).position.Y < 1, "at %f", w.pose(id).position.Y);
}

// A wheel's tread grips as its rim's material does (sprint 74): an
// engine spins wheels on ice and gets nowhere much; on rubber it goes.
void TestPhysics::testTreadGrips()
{
	float speed[2];
	for (int i = 0; i < 2; i++) {
		World w(flat());
		Assembly a = car(BIG);
		for (Wheel &wheel : a.wheels)
			wheel.grip = i == 0 ? 0.05f : 1.f;
		World::Id id = w.add(a, Pose{ v3d(2, 0.1, 8), {} });
		run(w, 1);
		w.drive(id, physics::Drive{ 1, 0, 0 });
		run(w, 1);
		speed[i] = w.velocity(id).getLength();
	}
	UTEST(speed[0] < speed[1] * 0.7f, "on ice %f, on rubber %f", speed[0], speed[1]);
}

// Sprint 75: dropped from a height, a car on long soft springs gives more
// as it lands than one on the short stiff ones, and both settle on their
// wheels where they were built.
void TestPhysics::testSprings()
{
	double lowest[2], rest[2];
	for (int i = 0; i < 2; i++) {
		World w(flat());
		Assembly a = car(0);
		if (i == 1)
			for (Wheel &wheel : a.wheels) {
				wheel.travel = 0.8f;
				wheel.sag = 0.17f;
			}
		{
			World settle(flat());
			const World::Id s = settle.add(a, Pose{ v3d(2, 0.1, 8), {} });
			run(settle, 2);
			rest[i] = settle.pose(s).position.Y;
		}
		World::Id id = w.add(a, Pose{ v3d(2, rest[i] + 1.5, 8), {} });
		lowest[i] = rest[i] + 10;
		for (int k = 0; k < 120; k++) {
			run(w, 1.0 / 60);
			lowest[i] = std::min(lowest[i], w.pose(id).position.Y);
		}
		run(w, 3);
		UTEST(std::fabs(w.pose(id).position.Y - rest[i]) < 0.05, "settled at %f, not %f", w.pose(id).position.Y, rest[i]);
	}
	UTEST(rest[0] - lowest[0] < rest[1] - lowest[1], "gave %f stiff, %f soft", rest[0] - lowest[0], rest[1] - lowest[1]);
	// both rest where they were built (sprint 76: the soft one rode high)
	UTEST(std::fabs(rest[0] - rest[1]) < 0.03, "at rest stiff %f, soft %f", rest[0], rest[1]);
}

// Sprint 76: a hub on each wheel rides its wheel's spring: dropped on soft
// springs, the hubs come up toward the root as the car lands, and are
// back where they were built once it's settled; a hub with no wheel on
// it stays put on the root.
void TestPhysics::testHubsRide()
{
	Assembly a = car(0);
	for (size_t i = 0; i < a.wheels.size(); i++) {
		Wheel &wheel = a.wheels[i];
		wheel.travel = 0.8f;
		wheel.sag = 0.17f;
		a.hubs.push_back(Hub{ a.bodies.size(), i });
		a.bodies.push_back(block(wheel.centre + v3f(0, 0, wheel.centre.Z > 0 ? -0.15f : 0.15f), v3f(0.1f), WOOD));
	}
	const size_t lone = a.bodies.size();
	a.hubs.push_back(Hub{ lone, std::nullopt });
	a.bodies.push_back(block(v3f(0, 0.3f, 0), v3f(0.1f), WOOD));

	World w(flat());
	const World::Id id = w.add(a, Pose{ v3d(2, 1.5, 8), {} });
	const size_t hub = a.hubs[0].body;
	float highest = -10;
	for (int k = 0; k < 180; k++) {
		run(w, 1.0 / 60);
		highest = std::max(highest, w.relatives(id)[hub].offset.Y);
	}
	run(w, 3);
	const std::vector<Relative> rel = w.relatives(id);
	UTEST(highest > 0.1f, "the hub rose only %f as it landed", highest);
	UTEST(std::fabs(rel[hub].offset.Y) < 0.05f, "settled, the hub's %f off where it was built", rel[hub].offset.Y);
	UTEST(rel[lone].offset.getLength() < 1e-3f, "the lone hub's %f off", rel[lone].offset.getLength());
}
