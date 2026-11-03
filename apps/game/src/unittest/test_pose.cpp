// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "physics/pose.h"
#include "physics/reach.h"
#include <cmath>

using namespace physics;

/*
	The figure's joints (physics/pose): limbs reach what they're asked to, knees
	bend forward and elbows back, a foot on the ground doesn't skate, and feet
	meet the ground they're given.
*/
class TestPose : public TestBase {
public:
	TestPose() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPose"; }

	void runTests(IGameDef *gamedef);

	void testTwoBonesReach();
	void testBendWays();
	void testOutOfReach();
	void testStandingStill();
	void testPlantedFootDoesntSkate();
	void testFeetMeetTheGround();
	void testLegsReachTheFeet();
	void testInTheAir();
	void testLongFrameStaysSane();
	void testHipsFaceTheWayItGoes();
	void testStandingStillHoldsTheFeet();
	void testStancesLower();
	void testRollGoesRound();
	void testKeepsItsMomentum();
	void testSlidesAndRolls();
	void testReach();
};

static TestPose g_test_instance;

void TestPose::runTests(IGameDef *gamedef)
{
	TEST(testTwoBonesReach);
	TEST(testBendWays);
	TEST(testOutOfReach);
	TEST(testStandingStill);
	TEST(testPlantedFootDoesntSkate);
	TEST(testFeetMeetTheGround);
	TEST(testLegsReachTheFeet);
	TEST(testInTheAir);
	TEST(testLongFrameStaysSane);
	TEST(testHipsFaceTheWayItGoes);
	TEST(testStandingStillHoldsTheFeet);
	TEST(testStancesLower);
	TEST(testRollGoesRound);
	TEST(testKeepsItsMomentum);
	TEST(testSlidesAndRolls);
	TEST(testReach);
}

void TestPose::testTwoBonesReach()
{
	for (const Bends way : { Bends::FORWARD, Bends::BACK })
		for (float z = -0.5f; z <= 0.5f; z += 0.1f)
			for (float y = -0.6f; y <= -0.2f; y += 0.1f) {
				float end_z, end_y;
				endOf(twoBones(z, y, THIGH, SHIN, way), THIGH, SHIN, end_z, end_y);
				if (std::hypot(z, y) >= THIGH + SHIN)
					continue;
				UASSERT(std::abs(end_z - z) < 1e-3f);
				UASSERT(std::abs(end_y - y) < 1e-3f);
			}
}

void TestPose::testBendWays()
{
	const Bend knee = twoBones(0, -0.5f, THIGH, SHIN, Bends::FORWARD);
	UASSERT(knee.root < 0);  // the thigh forward
	UASSERT(knee.joint > 0); // the shin back from it
	const Bend elbow = twoBones(0, -0.4f, UPPER_ARM, FOREARM, Bends::BACK);
	UASSERT(elbow.root > 0);
	UASSERT(elbow.joint < 0);
}

void TestPose::testOutOfReach()
{
	float z, y;
	endOf(twoBones(0, -5, THIGH, SHIN, Bends::FORWARD), THIGH, SHIN, z, y);
	UASSERT(std::abs(z) < 0.01f);
	UASSERT(std::abs(y + (THIGH + SHIN)) < 0.01f);
}

void TestPose::testStandingStill()
{
	Poser poser;
	Joints j;
	for (int i = 0; i < 120; i++)
		j = poser.step(1 / 60.f, PoseIn{});
	for (int side : { 0, 1 }) {
		UASSERT(std::abs(j.foot[side].Y) < 1e-3f);
		UASSERT(std::abs(j.foot[side].Z) < 0.02f);
		UASSERT(j.knee[side] >= 0);
	}
	UASSERT(std::abs(j.waist) < 0.01f);
}

// The heart of it: while a foot carries the body it moves back as fast as the body goes on.
void TestPose::testPlantedFootDoesntSkate()
{
	Poser poser;
	PoseIn in;
	in.moving = v3f(0, 0, 4);
	const float dt = 1 / 120.f;
	for (int i = 0; i < 240; i++)
		poser.step(dt, in);
	float was = 0;
	bool had = false;
	int checked = 0;
	for (int i = 0; i < 240; i++) {
		const Joints j = poser.step(dt, in);
		if (j.planted[0] && had) {
			const float slid = (j.foot[0].Z - was) + 4 * dt;
			// (the frame the stride wraps isn't a slide)
			if (std::abs(slid) < 0.1f) {
				UASSERT(std::abs(slid) < 2e-3f);
				checked++;
			}
		}
		had = j.planted[0];
		was = j.foot[0].Z;
	}
	UASSERT(checked > 50);
}

void TestPose::testFeetMeetTheGround()
{
	Poser poser;
	PoseIn in;
	in.ground[0] = 0.5f;
	in.ground[1] = -0.3f;
	Joints j;
	for (int i = 0; i < 120; i++)
		j = poser.step(1 / 60.f, in);
	UASSERT(std::abs(j.foot[0].Y - 0.5f) < 1e-3f);
	UASSERT(std::abs(j.foot[1].Y + 0.3f) < 1e-3f);
	UASSERT(j.drop > 0.25f);       // hips down so the low leg reaches
	UASSERT(j.knee[0] > j.knee[1]); // the high leg bent up
}

// Whatever the pace, the leg's end is where the foot was asked to be (unless out of reach).
void TestPose::testLegsReachTheFeet()
{
	for (const float pace : { 1.f, 4.f, 6.4f }) {
		Poser poser;
		PoseIn in;
		in.moving = v3f(0, 0, pace);
		for (int i = 0; i < 200; i++) {
			const Joints j = poser.step(1 / 60.f, in);
			if (i < 60)
				continue;
			for (int side : { 0, 1 }) {
				const float below = HIP_HEIGHT - j.drop - j.foot[side].Y;
				float z, y;
				endOf({ j.hip[side], j.knee[side] }, THIGH, SHIN, z, y);
				if (std::hypot(j.foot[side].Z, below) < THIGH + SHIN) {
					UASSERT(std::abs(z - j.foot[side].Z) < 0.01f);
				}
				UASSERT(std::abs(z - j.foot[side].Z) < 0.1f); // a run's reach is barely short
			}
		}
	}
}

void TestPose::testInTheAir()
{
	Poser poser;
	PoseIn in;
	in.airborne = true;
	in.moving = v3f(0, -5, 3);
	Joints j;
	for (int i = 0; i < 120; i++)
		j = poser.step(1 / 60.f, in);
	UASSERT(!j.planted[0] && !j.planted[1]);
	UASSERT(j.foot[0].Y > 0.2f);
	UASSERT(j.shoulder_out[0] > 0.4f);
	UASSERT(j.shoulder_out[1] < -0.4f);
}

void TestPose::testLongFrameStaysSane()
{
	Poser poser;
	PoseIn in;
	in.moving = v3f(4000, 300, 9000); // flying at orbital speed, on a stall
	const Joints j = poser.step(2.f, in);
	for (int side : { 0, 1 }) {
		UASSERT(std::isfinite(j.hip[side]));
		UASSERT(std::isfinite(j.knee[side]));
		UASSERT(std::isfinite(j.shoulder[side]));
		UASSERT(std::abs(j.foot[side].Z) < 2);
	}
	UASSERT(std::isfinite(j.waist));
	UASSERT(std::isfinite(j.drop));
}

void TestPose::testHipsFaceTheWayItGoes()
{
	const auto settled = [](v3f moving) {
		Poser poser;
		PoseIn in;
		in.moving = moving;
		Joints j;
		for (int i = 0; i < 120; i++)
			j = poser.step(1 / 60.f, in);
		return j.twist;
	};
	UASSERT(std::abs(settled(v3f(0, 0, 4))) < 1e-3f);
	UASSERT(std::abs(settled(v3f(3, 0, 3)) - 0.785f) < 0.01f);  // W + D: diagonal
	UASSERT(settled(v3f(4, 0, 0)) < 1.2f);                      // strafing stops short of square on
	UASSERT(settled(v3f(4, 0, 0)) > 1.f);
	UASSERT(std::abs(settled(v3f(0, 0, -4))) < 1e-3f);          // backing up doesn't turn round
	UASSERT(std::abs(settled(v3f(-3, 0, -3)) - 0.785f) < 0.01f); // nor backing away on a diagonal
}

void TestPose::testStandingStillHoldsTheFeet()
{
	Poser poser;
	PoseIn in;
	in.turned = 0.3f;
	Joints j = poser.step(1 / 60.f, in);
	UASSERT(std::abs(j.twist + 0.3f) < 1e-3f); // the hips stayed; the view went
	for (int i = 0; i < 20; i++)
		j = poser.step(1 / 60.f, in);
	UASSERT(std::abs(j.twist + 1.13f) < 1e-3f); // at the limit they come along
	in.turned = 0;
	const float held = poser.step(1 / 60.f, in).twist;
	UASSERT(std::abs(held - j.twist) < 1e-5f);
	// the ground is looked for where the foot really is
	const v3f foot = poser.footAt(0);
	UASSERT(std::abs(foot.getLength() - HIP_WIDE) < 0.03f);
	UASSERT(std::abs(foot.Z) > 0.08f);
}

void TestPose::testStancesLower()
{
	const auto settled = [](Stance s) {
		Poser poser;
		PoseIn in;
		in.stance = s;
		Joints j;
		for (int i = 0; i < 180; i++)
			j = poser.step(1 / 60.f, in);
		return j;
	};
	const Joints stand = settled(Stance::STAND), crouch = settled(Stance::CROUCH), prone = settled(Stance::PRONE);
	UASSERT(crouch.drop > stand.drop + 0.2f);
	UASSERT(prone.drop > crouch.drop);
	UASSERT(crouch.knee[0] > stand.knee[0] + 0.5f);
	UASSERT(prone.pitch > 1.2f);
	UASSERT(prone.foot[0].Z < -0.5f); // legs out behind
	UASSERT(prone.back > 0.3f);
}

void TestPose::testRollGoesRound()
{
	Poser poser;
	PoseIn in;
	in.stance = Stance::ROLL;
	in.moving = v3f(0, 0, 5);
	float last = -1;
	const int steps = (int)(ROLL_TIME * 60);
	for (int i = 0; i < steps; i++) {
		const Joints j = poser.step(1 / 60.f, in);
		UASSERT(j.spin > last);
		last = j.spin;
	}
	UASSERT(last > 5.5f);
	in.stance = Stance::STAND;
	UASSERTEQ(float, poser.step(1 / 60.f, in).spin, 0);
}

void TestPose::testKeepsItsMomentum()
{
	Gait run;
	run.run = 1.6f;
	run.flow = 1.25f;
	const float dt = 1 / 60.f;
	const float top = run.walk * run.run;
	// in the air, turning round and pushing back takes nothing off
	const Footing flying{ v3f(0, 1, top), false };
	const v3f turned = walk(run, Intent{ -1, 0, 0, false }, flying, dt);
	UASSERT(std::abs(turned.Z - top) < 1e-3f);
	// from a standstill it can still hop onto something, a little
	v3f hop(0);
	for (int i = 0; i < 60; i++)
		hop = walk(run, Intent{ 1, 0, 0, false }, Footing{ hop, false }, dt);
	UASSERT(hop.Z > 1 && hop.Z < 1.6f);
	// a clean line builds past a run; a sharp turn costs it
	v3f v(0, 0, top);
	for (int i = 0; i < 240; i++)
		v = walk(run, Intent{ 1, 0, 0, false }, Footing{ v, true }, dt);
	UASSERT(v.Z > top * 1.2f);
	const float built = v.getLength();
	for (int i = 0; i < 30; i++)
		v = walk(run, Intent{ 0, 1, 0, false }, Footing{ v, true }, dt);
	UASSERT(v.X > v.Z);                   // it's come round
	UASSERT(v.getLength() < built * 0.75f); // and paid for it
	// a jump goes where it's looked, as far as a leg throws it: a little look, that little
	const Footing running{ v3f(0, 0, top), true };
	const float little = 0.1f;
	const v3f slight = walk(run, Intent{ 1, 0, -little, true }, running, dt);
	UASSERT(std::abs(std::atan2(slight.X, slight.Z) - little) < 0.01f);
	UASSERT(std::hypot(slight.X, slight.Z) > top * 0.95f);
	// asked square to the side: 30° at most; asked back: never backward
	const v3f side = walk(run, Intent{ 0, 1, 0, true }, running, dt);
	UASSERT(std::abs(std::atan2(side.X, side.Z) - 0.52f) < 0.01f);
	const v3f rear = walk(run, Intent{ -1, 0, 0, true }, running, dt);
	UASSERT(rear.Z > top * 0.5f);
}

void TestPose::testSlidesAndRolls()
{
	// a crouch at a run is a slide; at a walk it's a crouch
	UASSERT(nextStance(Stance::STAND, Stance::CROUCH, 6, true, 0) == Stance::SLIDE);
	UASSERT(nextStance(Stance::STAND, Stance::CROUCH, 3, true, 0) == Stance::CROUCH);
	UASSERT(nextStance(Stance::SLIDE, Stance::CROUCH, 4, true, 0) == Stance::SLIDE);
	UASSERT(nextStance(Stance::SLIDE, Stance::CROUCH, 2, true, 0) == Stance::CROUCH);
	UASSERT(nextStance(Stance::SLIDE, Stance::STAND, 4, true, 0) == Stance::STAND);
	// not in the air, till it lands
	UASSERT(nextStance(Stance::STAND, Stance::CROUCH, 6, false, 0) == Stance::STAND);
	// a roll runs its time
	UASSERT(nextStance(Stance::ROLL, Stance::STAND, 6, true, 0.1f) == Stance::ROLL);
	UASSERT(nextStance(Stance::ROLL, Stance::STAND, 6, true, 0) == Stance::STAND);
	UASSERT(rollsOut(HARD_LANDING, Stance::CROUCH));
	UASSERT(!rollsOut(HARD_LANDING, Stance::STAND));
	UASSERT(!rollsOut(3, Stance::CROUCH));

	Gait g;
	const float dt = 1 / 60.f;
	// a slide slows on the flat, and speeds up downhill
	v3f flat = slide(g, Intent{}, Footing{ v3f(0, 0, 6), true }, 0.5f);
	UASSERT(flat.Z < 6 && flat.Z > 2);
	v3f normal(0.5f, 0.866f, 0); // falls away toward +x
	v3f down = slide(g, Intent{}, Footing{ v3f(6, 0, 0), true, v3f(0), normal }, dt);
	UASSERT(down.X > 6);
	// a roll keeps speed, and from standing still goes the way it looks
	UASSERT(std::abs(roll(g, Intent{}, Footing{ v3f(0, 0, 6.4f), true }, dt).Z - 6.4f) < 1e-3f);
	UASSERT(std::abs(roll(g, Intent{}, Footing{ v3f(0), true }, dt).Z - g.walk) < 1e-3f);
}

// A floor at y = -1 (its top at -0.5, where the feet are) and a wall `high` tall and `deep` thick at z = 2..
static Solid wallOf(int high, int deep)
{
	return [=](v3s16 p) {
		return p.Y < 0 || (p.Z >= 2 && p.Z < 2 + deep && p.Y < high);
	};
}

void TestPose::testReach()
{
	const v3f foot(0, -0.5f, 1.1f);
	for (const auto &[high, want] : std::initializer_list<std::pair<int, Climb>>{ { 1, Climb::VAULT }, { 2, Climb::MANTLE }, { 3, Climb::HANG } }) {
		const Solid solid = wallOf(high, 3);
		// jumped: up where its feet are at the top of it
		const v3f up(foot.X, foot.Y + 1, foot.Z);
		const std::optional<Ledge> l = findLedge(solid, high == 1 ? foot : up, 0);
		UASSERT(l.has_value());
		UASSERT(std::abs(l->lip.Y - (high - 0.5f)) < 1e-3f);
		UASSERT(std::abs(l->normal.Z + 1) < 1e-3f);
		UASSERT(std::abs(l->lip.Z - 1.5f) < 1e-3f);
		UASSERT(climbFor(*l, foot.Y, high == 1 ? foot.Y : up.Y) == want);
	}
	// nothing to hold looking away from it, or from too far, or too tall
	UASSERT(!findLedge(wallOf(1, 3), foot, core::PI));
	UASSERT(!findLedge(wallOf(1, 3), v3f(0, -0.5f, -2), 0));
	UASSERT(!findLedge(wallOf(5, 3), foot, 0));
	// a thin wall has a far side, and a vault lands past it
	const std::optional<Ledge> thin = findLedge(wallOf(1, 1), foot, 0);
	UASSERT(thin && thin->thin);
	const ClimbPath over = climbPath(Climb::VAULT, *thin, foot, 0.3f);
	UASSERT(over.to.Z > 2.5f && over.to.Z < 3); // just past the far side
	UASSERT(std::abs(over.to.Y + 0.48f) < 0.01f); // and down on the ground there
	// at the speed it came in at, whatever that is
	const ClimbPath fast = climbPath(Climb::VAULT, *thin, foot, 0.3f, 6);
	const float dist = v3f(fast.to.X - foot.X, 0, fast.to.Z - foot.Z).getLength();
	UASSERT(std::abs(dist / fast.time - 6) < 0.01f);
	// a wall alongside: run on it at speed, kick off it otherwise
	const Solid side = [](v3s16 p) { return p.X == 1; };
	const std::optional<v3f> wall = findWall(side, v3f(0.2f, 0, 0), 0.3f);
	UASSERT(wall && std::abs(wall->X + 1) < 1e-3f);
	UASSERT(!findWall(side, v3f(-1.5f, 0, 0), 0.3f));
	UASSERT(runsAlong(v3f(1, 0, 5), *wall));
	UASSERT(!runsAlong(v3f(5, 0, 1), *wall));
	const v3f kicked = wallKick(v3f(1, 0, 5), *wall, 5);
	UASSERT(kicked.X < -4 && kicked.Y == 5 && kicked.Z > 4);
	UASSERT(over.at(over.time * 0.6f).Y >= thin->lip.Y);
	UASSERT((over.at(over.time) - over.to).getLength() < 1e-3f);
	// a roof over the top: nowhere to mantle to, only hang
	const Solid roofed = [](v3s16 p) { return p.Y < 0 || (p.Z >= 2 && p.Y < 2) || (p.Z >= 2 && p.Y == 3); };
	const v3f up(foot.X, foot.Y + 1, foot.Z);
	const std::optional<Ledge> low = findLedge(roofed, up, 0);
	UASSERT(low && !low->room);
	UASSERT(climbFor(*low, foot.Y, up.Y) == Climb::HANG);
	// an edge goes on till the wall ends
	const Solid stub = [](v3s16 p) { return p.Y < 0 || (p.Z == 2 && p.Y < 3 && std::abs(p.X) <= 1); };
	UASSERT(ledgeGoesOn(stub, v3f(1, 2.5f, 1.5f), v3f(0, 0, -1)));
	UASSERT(!ledgeGoesOn(stub, v3f(2, 2.5f, 1.5f), v3f(0, 0, -1)));
}
