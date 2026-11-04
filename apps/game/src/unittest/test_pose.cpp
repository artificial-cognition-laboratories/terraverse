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
static bool flightTiersHold();

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
	void testArmReachesThePoint();
	void testHandOverHand();
	void testHangingHandsReach();
	void testHigherBodyGoesOver();
	void testMovesGoByVelocity();
	void testHangingLegsSwing();
	void testHaul();
	void testWallPush();
	void testBalance();
	void testFallen();
	void testLimbsByAngleAndCorners();
	void testHandsStayOut();
	void testMassLeansAndJumpsAlternate();
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
	TEST(testArmReachesThePoint);
	TEST(testHandOverHand);
	TEST(testHangingHandsReach);
	TEST(testHigherBodyGoesOver);
	TEST(testMovesGoByVelocity);
	TEST(testHangingLegsSwing);
	TEST(testHaul);
	TEST(testWallPush);
	TEST(testBalance);
	TEST(testFallen);
	TEST(testLimbsByAngleAndCorners);
	TEST(testHandsStayOut);
	TEST(testMassLeansAndJumpsAlternate);
	UASSERT(flightTiersHold());
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
	UASSERT(j.foot[0].Y < 0.2f); // coming down, reaching for the ground
	UASSERT(j.shoulder_out[0] > 0.4f);
	UASSERT(j.shoulder_out[1] < -0.4f);
	// over the top, tucked
	in.moving = v3f(0, 0, 3);
	for (int i = 0; i < 120; i++)
		j = poser.step(1 / 60.f, in);
	UASSERT(j.foot[0].Y > 0.2f);
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
		const v3f up(foot.X, foot.Y + (high == 2 ? 0.5f : 1), foot.Z);
		const std::optional<Ledge> l = findLedge(solid, high == 1 ? foot : up, 0);
		UASSERT(l.has_value());
		UASSERT(std::abs(l->lip.Y - (high - 0.5f)) < 1e-3f);
		UASSERT(std::abs(l->normal.Z + 1) < 1e-3f);
		UASSERT(std::abs(l->lip.Z - 1.5f) < 1e-3f);
		UASSERT(moveFor(*l, foot.Y, high == 1 ? foot.Y : up.Y, v3f(0, 0, 3))->climb == want);
	}
	// nothing to hold looking away from it, or from too far, or too tall
	UASSERT(!findLedge(wallOf(1, 3), foot, core::PI));
	UASSERT(!findLedge(wallOf(1, 3), v3f(0, -0.5f, -2), 0));
	UASSERT(!findLedge(wallOf(5, 3), foot, 0));
	// a thin wall has a far side, and a vault lands past it
	const Move OVER{ "over", Climb::VAULT, true, false };
	const std::optional<Ledge> thin = findLedge(wallOf(1, 1), foot, 0);
	UASSERT(thin && thin->thin);
	const ClimbPath over = climbPath(OVER, *thin, foot, 0.3f);
	UASSERT(over.to.Z > 2.5f && over.to.Z < 3); // just past the far side
	UASSERT(std::abs(over.to.Y + 0.48f) < 0.01f); // and down on the ground there
	// at the speed it came in at, whatever that is
	const ClimbPath fast = climbPath(OVER, *thin, foot, 0.3f, 6);
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
	// looking back along or away, a kick goes that way
	UASSERT(wallKick(v3f(0), *wall, 5, v3f(0, 0, 1)).Z > 3);
	UASSERT(wallKick(v3f(0), *wall, 5, v3f(-1, 0, 0)).X < wallKick(v3f(0), *wall, 5).X);
	// a roof over the top: nowhere to mantle to, only hang
	const Solid roofed = [](v3s16 p) { return p.Y < 0 || (p.Z >= 2 && p.Y < 2) || (p.Z >= 2 && p.Y == 3); };
	const v3f up(foot.X, foot.Y + 1, foot.Z);
	const std::optional<Ledge> low = findLedge(roofed, up, 0);
	UASSERT(low && !low->room);
	UASSERT(moveFor(*low, foot.Y, up.Y, v3f(0))->climb == Climb::HANG);
	// an edge goes on till the wall ends
	const Solid stub = [](v3s16 p) { return p.Y < 0 || (p.Z == 2 && p.Y < 3 && std::abs(p.X) <= 1); };
	UASSERT(ledgeGoesOn(stub, v3f(1, 2.5f, 1.5f), v3f(0, 0, -1)));
	UASSERT(!ledgeGoesOn(stub, v3f(2, 2.5f, 1.5f), v3f(0, 0, -1)));
}

void TestPose::testArmReachesThePoint()
{
	for (const v3f to : { v3f(0.1f, 0.4f, 0.1f), v3f(0, -0.3f, 0.3f), v3f(-0.2f, 0.1f, 0.35f) }) {
		const ArmTo a = armTo(to, v3f(0.7f, -0.6f, -0.7f), UPPER_ARM, FOREARM);
		// the upper arm along -y, the forearm turned about x from there (-: forward)
		const v3f elbow = a.y * -UPPER_ARM;
		const v3f fore = (a.y * -std::cos(a.elbow) + a.z * -std::sin(a.elbow)) * FOREARM;
		UASSERT((elbow + fore - to).getLength() < 1e-2f);
		UASSERT(std::fabs(a.x.dotProduct(a.y)) < 1e-3f);
		UASSERT(a.x.crossProduct(a.y).dotProduct(a.z) > 0.99f); // a turn, not a mirror
	}
}

void TestPose::testHandOverHand()
{
	HandOverHand hands;
	const v3f apart(0.22f, 0, 0);
	v3f body(0, 0, 0);
	hands.place(body + apart, body - apart);
	int steps[2]{}, last = -1;
	for (int i = 0; i < 240; i++) {
		body.X += SHIMMY / 120;
		hands.step(1.f / 120, body + apart, body - apart);
		for (int side : { 0, 1 }) // never left behind by more than an arm can make up
			UASSERT((hands.at(side) - (body + (side ? apart * -1.f : apart))).getLength() < 0.55f);
		if (hands.moving() >= 0 && hands.moving() != last) {
			steps[hands.moving()]++;
			last = hands.moving();
		}
	}
	UASSERT(steps[0] >= 2 && steps[1] >= 2); // it took turns
	// stopped, they stop
	for (int i = 0; i < 60; i++)
		hands.step(1.f / 120, body + apart, body - apart);
	for (int i = 0; i < 240; i++) {
		hands.step(1.f / 120, body + apart, body - apart);
		UASSERT(hands.moving() < 0);
	}
}

void TestPose::testHangingHandsReach()
{
	// the figure's leaned in till its chest is on the wall; its shoulder to the lip, in its own lengths
	constexpr float S = 0.885f;
	const float up = (0.06f + HANG_DROP) / S - (HIP_HEIGHT + 0.25f + 0.375f - 0.02f); // (hips sit that little low)
	const float out = 0.35f / S - 0.12f;
	UASSERT(std::hypot(up, out) < UPPER_ARM + FOREARM);
}

void TestPose::testHigherBodyGoesOver()
{
	Ledge l{ v3f(0, 0, -1), v3f(0, 2, 0), false, true };
	const v3f at(0, 0, 3);
	// two high, jumped at: low in the jump it climbs, waist high it gets on top and keeps running
	UASSERT(moveFor(l, 0, 0.2f, at)->climb == Climb::MANTLE);
	UASSERT(moveFor(l, 0, 1.f, at)->climb == Climb::VAULT);
	// three high still hangs however it got there
	l.lip.Y = 3;
	UASSERT(moveFor(l, 0, 1.f, at)->climb == Climb::HANG);
}

void TestPose::testMovesGoByVelocity()
{
	const Ledge l{ v3f(0, 0, -1), v3f(0, 1, 0), true, true };
	const auto going = [](float pace, float degrees) {
		const float a = degrees * core::DEGTORAD;
		return v3f(pace * std::sin(a), 0, pace * std::cos(a));
	};
	const auto name = [&](v3f v) { return std::string(moveFor(l, 0, 0, v)->name); };
	UASSERT(name(going(6, 0)) == "kong");
	UASSERT(name(going(6, 20)) == "speed vault"); // too far off square for a Kong: it can't be done
	UASSERT(name(going(3, 0)) == "speed vault");
	UASSERT(moveFor(l, 0, 0, going(6, 40))->side);
	UASSERT(!moveFor(l, 0, 0, going(6, 70))->over); // nearly along it: onto it, not across
	UASSERT(!moveFor(l, 0, 0, going(1.5f, 0))->over); // slow: onto it
	const Ledge roofed{ v3f(0, 0, -1), v3f(0, 1, 0), true, false };
	UASSERT(moveFor(roofed, 0, 0, going(1.5f, 0))->over);
}

void TestPose::testHangingLegsSwing()
{
	Poser p;
	PoseIn in;
	in.faces = true; // square to the wall, as a catch turns it
	in.moving = v3f(3, 0, 0); // going along the wall
	for (int i = 0; i < 30; i++)
		p.step(1.f / 60, in);
	in.stance = Stance::HANG;
	in.moving = v3f(0); // caught: the hands stop, the rest goes on
	float most = 0;
	Joints j;
	for (int i = 0; i < 60; i++) {
		j = p.step(1.f / 60, in);
		most = std::max(most, j.swing_side);
		UASSERT(j.swing_back >= 0); // never into the wall
	}
	UASSERT(most > 0.05f); // (a held body: it gives, it doesn't fling)
	for (int i = 0; i < 900; i++)
		j = p.step(1.f / 60, in);
	UASSERT(std::fabs(j.swing_side) < 0.02f); // and it settles
}

void TestPose::testMassLeansAndJumpsAlternate()
{
	Poser p;
	PoseIn in;
	in.moving = v3f(0, 0, 6);
	Joints j;
	for (int i = 0; i < 12; i++)
		j = p.step(1.f / 60, in);
	UASSERT(j.lean_ahead > 0.05f); // setting off, it leans in
	for (int i = 0; i < 120; i++)
		j = p.step(1.f / 60, in);
	const float running = j.lean_ahead;
	in.moving = v3f(0);
	for (int i = 0; i < 12; i++)
		j = p.step(1.f / 60, in);
	UASSERT(j.lean_ahead < running); // stopping, it comes back
	// off the ground twice: the other leg leads
	const auto jump = [&] {
		PoseIn up;
		up.airborne = true;
		up.moving = v3f(0, 6, 0);
		Joints a;
		for (int i = 0; i < 40; i++)
			a = p.step(1.f / 60, up);
		PoseIn on;
		for (int i = 0; i < 40; i++)
			p.step(1.f / 60, on);
		return a.foot[0].Z;
	};
	const float first = jump(), second = jump();
	UASSERT((first > 0) != (second > 0));
}

void TestPose::testHaul()
{
	const Ledge l{ v3f(0, 0, -1), v3f(0, 2.5f, 0), false, true };
	const auto run = [&](v3f foot, v3f v, bool pull, float &lowest, float &hardest) {
		Haul h(l, foot, v, 0.3f, pull, -10);
		v3f was = h.velocity();
		lowest = foot.Y;
		hardest = 0;
		int steps = 0;
		bool left_hands = false;
		while (!h.done() && steps < 600) {
			h.step(1.f / 60);
			lowest = std::min(lowest, h.at().Y);
			hardest = std::max(hardest, (h.velocity() - was).getLength() * 60);
			was = h.velocity();
			if (!h.holding())
				left_hands = true;
			else
				UASSERT(!left_hands); // hands come off once, on the way up
			steps++;
		}
		UASSERT(h.done());
		return steps / 60.f;
	};
	const v3f foot(0, 1, -0.5f);
	float low, hard;
	const float falling = run(foot, v3f(0, -5, 3), true, low, hard);
	UASSERT(low < foot.Y - 0.1f); // it sags under its hands
	UASSERT(hard < 80); // and nothing's instant: no reversal in one step
	const float rising = run(foot, v3f(0, 3, 3), true, low, hard);
	UASSERT(rising < falling); // arriving well costs less
	UASSERT(low >= foot.Y - 0.01f);
	// its feet above where it'd hang, coming down or still: it always goes on (this once held it there for good)
	for (const v3f v : { v3f(0, -2, 3), v3f(0, -0.1f, 0), v3f(0) })
		UASSERT(run(v3f(0, 1.6f, -0.5f), v, true, low, hard) < 1.5f);
	// not asked to pull: it comes to rest hanging
	Haul h(l, foot, v3f(0, -3, 0), 0.3f, false, -10);
	for (int i = 0; i < 180 && !h.done(); i++)
		h.step(1.f / 60);
	UASSERT(h.done() && std::abs(h.at().Y - (l.lip.Y - HANG_DROP)) < 0.01f);
	// a run's kept only as far as it wasn't spent stopping a fall
	Haul clean(l, foot, v3f(0, 2, 5), 0.3f, true, -10), late(l, foot, v3f(0, -4, 5), 0.3f, true, -10);
	UASSERT(clean.exit().getLength() > 3 && late.exit().getLength() < 0.01f);
	// falling with the edge at its chest is a catch, not a vault; rising it goes over; coming down hard it hangs
	const Ledge low_edge{ v3f(0, 0, -1), v3f(0, 2, 0), false, true };
	UASSERT(moveFor(low_edge, 0, 1, v3f(0, 2, 4))->climb == Climb::VAULT);
	UASSERT(moveFor(low_edge, 0, 0.9f, v3f(0, -3, 4))->climb == Climb::MANTLE);
	UASSERT(moveFor(low_edge, 0, 0.9f, v3f(0, -8, 4))->climb == Climb::HANG);
}

void TestPose::testWallPush()
{
	const v3f normal(0, 0, -1);
	const auto run = [&](float pace, float &hardest, float &in_by) {
		WallPush p(normal, v3f(0, -1, pace), 5, v3f(0, 0, 0.3f));
		v3f at, was = v3f(0, -1, pace), v;
		hardest = 0;
		in_by = 0;
		int steps = 0;
		while (!p.done() && steps++ < 120) {
			v = p.step(1.f / 60);
			hardest = std::max(hardest, (v - was).getLength() * 60);
			was = v;
			at += v / 60.f;
			in_by = std::max(in_by, at.Z);
		}
		UASSERT(p.done());
		return v;
	};
	float hard, slow_in, fast_in;
	run(2, hard, slow_in);
	const v3f v = run(6, hard, fast_in);
	UASSERT(fast_in > slow_in); // faster, it gets closer before it goes
	UASSERT(v.Z < -3 && v.Y > 4); // and it's off, up and out
	UASSERT(hard < 150); // built, not appeared
}

void TestPose::testHandsStayOut()
{
	// back and forth, stopping and starting: no hand comes in past the head
	HandOverHand hands;
	const v3f apart(0.34f, 0, 0);
	float x = 0;
	hands.place(apart, apart * -1.f);
	for (int i = 0; i < 1200; i++) {
		const float way = (i / 90) % 3 == 0 ? 1.f : (i / 90) % 3 == 1 ? -1.f : 0.f;
		x += way * SHIMMY / 120;
		const v3f body(x, 0, 0);
		hands.step(1.f / 120, body + apart, body - apart);
		// (a hand in the air is on its way out; once put down it's outside the head)
		if (hands.moving() != 0)
			UASSERT(hands.at(0).X - x > 0.1f);
		if (hands.moving() != 1)
			UASSERT(hands.at(1).X - x < -0.1f);
	}
	// nothing to hold further on: a hand's kept on
	HandOverHand kept;
	kept.place(apart, apart * -1.f);
	x = 0;
	for (int i = 0; i < 240; i++) {
		x += SHIMMY / 120;
		kept.step(1.f / 120, v3f(x, 0, 0) + apart, v3f(x, 0, 0) - apart, [](v3f to) { to.X = std::min(to.X, 0.5f + 1.f); return to; });
		UASSERT(kept.moving() == 0 || kept.at(0).X <= 1.5f + 1e-3f);
	}
}

void TestPose::testLimbsByAngleAndCorners()
{
	const v3f face(0, 0, 0.3f), wall(0, 0, -1); // a wall ahead, at +z
	// square on: all of it
	UASSERT(WallPush(wall, v3f(0, 0, 5), 5, face).nearSide() == 2);
	// running along it with the wall on its left (going +x, facing +x: its left is +z)
	WallPush along(wall, v3f(6, 0, 0.5f), 5, face);
	UASSERT(along.nearSide() == 1);
	UASSERT(along.facing().X > 0.8f);
	// and with the wall on its right
	UASSERT(WallPush(wall, v3f(-6, 0, 0.5f), 5, face).nearSide() == 0);
	// over a corner: a block alone, coming in on the slant near its end
	const Solid block = [](v3s16 p) { return p.Y < 0 || (p.X == 0 && p.Y == 0 && p.Z == 1); };
	Ledge l{ wall, v3f(0.4f, 0.5f, 0.5f), true, true };
	const v3f slant(-3, 0, 3);
	const Ledge c = overCorner(block, l, slant);
	UASSERT(c.in.getLengthSQ() > 0 && c.in.X < 0 && c.in.Z > 0);
	UASSERT(!c.thin); // onto it, not across
	// square, or heading away from the block, or mid-edge: as it was
	UASSERT(overCorner(block, l, v3f(0, 0, 3)).in.getLengthSQ() == 0);
	UASSERT(overCorner(block, l, v3f(3, 0, 3)).in.getLengthSQ() == 0);
	Ledge middle = l;
	middle.lip.X = 0;
	UASSERT(overCorner(block, middle, slant).in.getLengthSQ() == 0);
}

void TestPose::testBalance()
{
	const v3f ahead(0, 0, 1);
	// a jump's landing the way it's going costs nothing
	UASSERT(landingStagger(4, v3f(0, -4, 6), ahead) < 0.15f);
	// harder, more; and against the way it's going, far more
	UASSERT(landingStagger(8, v3f(0, -8, 6), ahead) > landingStagger(5, v3f(0, -5, 6), ahead));
	UASSERT(landingStagger(4, v3f(0, -4, 6), ahead * -1.f) > 0.6f);
	UASSERT(landingStagger(4, v3f(0, -4, 6), v3f(1, 0, 0)) > 0.3f);
	// standing, it doesn't matter which way it wants to go
	UASSERT(landingStagger(3, v3f(0, -3, 0), ahead * -1.f) < 0.15f);
	UASSERT(landingStagger(30, v3f(0, -30, 6), ahead * -1.f) <= 1);
	// a trip takes a real step and a run
	UASSERT(trips(0.4f, 6) && !trips(0.2f, 6) && !trips(0.4f, 3));
	// off balance it pitches over and comes down
	Poser p;
	PoseIn in;
	in.stagger = 1;
	Joints j;
	for (int i = 0; i < 40; i++)
		j = p.step(1.f / 60, in);
	UASSERT(j.lean_ahead > 0.7f && j.drop > 0.2f);
}

void TestPose::testFallen()
{
	const auto run = [](float severity, bool wants, float &most) {
		Fallen f(severity, true);
		float t = 0;
		most = 0;
		float was = 0;
		while (!f.done() && t < 10) {
			f.step(1.f / 60, wants);
			t += 1.f / 60;
			most = std::max(most, std::fabs(f.angle() - was) * 60);
			was = f.angle();
		}
		UASSERT(f.done());
		UASSERT(std::fabs(f.angle()) < 1e-3f); // on its feet
		return t;
	};
	float most;
	const float bad = run(1, false, most), mild = run(0, false, most), quick = run(1, true, most);
	UASSERT(bad > mild);
	UASSERT(quick < 1.2f && quick < bad); // asked, it's up soon whatever it was
	UASSERT(most < 15); // nothing's a snap
	UASSERT(Fallen(0.5f, false).angle() == 0);
	// a body on its back goes the other way
	Fallen back(0, false);
	for (int i = 0; i < 30; i++)
		back.step(1.f / 60, false);
	UASSERT(back.angle() < -1);
	// a bad landing takes it down; a clean one doesn't
	UASSERT(landingStagger(6, v3f(0, -6, 7), v3f(0, 0, -1)) >= FALLS_FROM);
	UASSERT(landingStagger(6, v3f(0, -6, 7), v3f(0, 0, 1)) < FALLS_FROM);
}

// (flight tiers) walking asks for all of the map; speed or height ask for less, and it doesn't flip on the line
static bool flightTiersHold()
{
	using namespace physics;
	return flightTier(5, 20, 0) == 0 && flightTier(100, 0, 0) == 1 && flightTier(500, 0, 0) == 2 &&
			flightTier(0, 600, 0) == 2 && flightTier(95, 0, 1) == 1 && flightTier(70, 0, 1) == 0;
}
