// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/server/world/run/sculpt/sculpt.h"
#include "src/client/world/player/hold.h"
#include "src/server/world/run/machines/piece/frame.h"
#include "src/client/world/player/hand.h"
#include "src/shared/core/dummygamedef.h"
#include "src/shared/map/dummymap.h"
#include "src/shared/content/nodedef.h"
#include <algorithm>
#include <cmath>
#include <set>

using namespace sculpt;
using shape::STEPS;

class TestSculpt : public TestBase {
public:
	TestSculpt() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSculpt"; }

	void runTests(IGameDef *gamedef);

	void testSculptVolumes();
	void testCutAtThePlane();
	void testBore();
	void testOutline();
	void testApplied();
	void testSlopeDugFromItsShape();
	void testMirrored();
	void testUndo();
	void testHoldPointsWhereItLooks();
	void testDrawnByFootprint();
	void testStructure();
	void testRounded();
	void testTurnedByHandle();
};

static TestSculpt g_test_instance;

void TestSculpt::runTests(IGameDef *gamedef)
{
	TEST(testSculptVolumes);
	TEST(testCutAtThePlane);
	TEST(testBore);
	TEST(testOutline);
	TEST(testApplied);
	TEST(testSlopeDugFromItsShape);
	TEST(testMirrored);
	TEST(testUndo);
	TEST(testHoldPointsWhereItLooks);
	TEST(testDrawnByFootprint);
	TEST(testTurnedByHandle);
	TEST(testStructure);
	TEST(testRounded);
}

namespace {

const v3s16 UP(0, 1, 0);

// pointed at sixteenth (5, 15, 9) of node (2, 3, 4), on its top
Stroke at(Tool tool, int size, bool place)
{
	return Stroke{ tool, size, { v3s16(2, 3, 4), v3s16(5, 15, 9) }, UP, place ? Act::Add : Act::Cut };
}

std::set<std::array<s32, 3>> set(const std::vector<v3s32> &cells)
{
	std::set<std::array<s32, 3>> s;
	for (const v3s32 &c : cells)
		s.insert({ c.X, c.Y, c.Z });
	return s;
}

} // namespace

// A size-1 cube is the sixteenth pointed at (dig) or the one on its face
// (place); a block's size snaps to the node, or the one on its face; a ball
// is inside its cube and round.
void TestSculpt::testSculptVolumes()
{
	const v3s32 g = world(at(Tool::Box, 1, false).at);
	UASSERT(at(Tool::Box, 1, false).cells() == std::vector<v3s32>{ g });
	UASSERT(at(Tool::Box, 1, true).cells() == std::vector<v3s32>{ g + v3s32(0, 1, 0) });

	const std::vector<v3s32> block = at(Tool::Box, STEPS, false).cells();
	UASSERTEQ(size_t, block.size(), size_t(STEPS * STEPS * STEPS));
	for (const v3s32 &c : block)
		UASSERT(local(c).node == v3s16(2, 3, 4));
	for (const v3s32 &c : at(Tool::Box, STEPS, true).cells())
		UASSERT(local(c).node == v3s16(2, 4, 4));

	const std::vector<v3s32> cube = at(Tool::Box, 9, false).cells();
	const std::vector<v3s32> ball = at(Tool::Sphere, 9, false).cells();
	const auto in_cube = set(cube);
	for (const v3s32 &c : ball)
		UASSERT(in_cube.count({ c.X, c.Y, c.Z }));
	const float ratio = ball.size() / float(cube.size());
	UTEST(std::abs(ratio - 3.14159f / 6) < 0.06f, "ball/cube %.3f", ratio);
	UASSERT(set(ball).count({ g.X, g.Y, g.Z })); // the pointed sixteenth
}

// The cut works at the plane of the face pointed at: dig everything past it,
// place everything short of it, over the size's square.
void TestSculpt::testCutAtThePlane()
{
	const v3s32 g = world(at(Tool::Cut, 6, false).at);
	const std::vector<v3s32> shave = at(Tool::Cut, 6, false).cells();
	const std::vector<v3s32> fill = at(Tool::Cut, 6, true).cells();
	UASSERTEQ(size_t, shave.size(), size_t(6 * 6 * 6));
	UASSERTEQ(size_t, fill.size(), size_t(6 * 6 * 6));
	for (const v3s32 &c : shave)
		UASSERT(c.Y > g.Y);
	for (const v3s32 &c : fill)
		UASSERT(c.Y <= g.Y);
	UASSERT(set(fill).count({ g.X, g.Y, g.Z }));
}

// The bore goes into the face twice its size, round across its axis, and
// doesn't place.
void TestSculpt::testBore()
{
	const v3s32 g = world(at(Tool::Bore, 8, false).at);
	const std::vector<v3s32> hole = at(Tool::Bore, 8, false).cells();
	std::set<s32> depths;
	std::set<std::pair<s32, s32>> across;
	for (const v3s32 &c : hole) {
		UASSERT(c.Y <= g.Y);
		depths.insert(c.Y);
		across.insert({ c.X, c.Z });
	}
	UASSERTEQ(size_t, depths.size(), size_t(16));
	UASSERTEQ(size_t, hole.size(), across.size() * 16); // the same disc all the way
	UASSERT(across.size() < 64 && across.size() > 40);   // round, not the square
	UASSERT(!at(Tool::Bore, 8, true).valid());
}

// A volume's outline is its bends: a box has 12 edges however many cells
// it's made of; an L has 18.
void TestSculpt::testOutline()
{
	UASSERTEQ(size_t, outline({ v3s32(0, 0, 0) }).size(), size_t(12));
	UASSERTEQ(size_t, outline(at(Tool::Box, 5, false).cells()).size(), size_t(12));
	UASSERTEQ(size_t, outline({ v3s32(0, 0, 0), v3s32(1, 0, 0), v3s32(0, 1, 0) }).size(), size_t(18));
}

namespace {

struct World
{
	DummyGameDef gamedef;
	content_t stone, dirt;
	std::unique_ptr<DummyMap> map;

	World()
	{
		NodeDefManager *ndef = gamedef.getWritableNodeDefManager();
		ContentFeatures s;
		s.name = "test:stone";
		s.walkable = true;
		s.drawtype = NDT_NORMAL;
		stone = ndef->set(s.name, std::move(s));
		ContentFeatures d;
		d.name = "test:dirt";
		d.walkable = true;
		d.drawtype = NDT_NORMAL;
		d.groups["loose"] = 1;
		dirt = ndef->set(d.name, std::move(d));
		map = std::make_unique<DummyMap>(&gamedef, v3s16(-2, -2, -2), v3s16(2, 2, 2));
	}
};

} // namespace

// Applied to a map: a block dug whole is air; part of one is cut; placing
// into the open makes the material there, and never into another material.
void TestSculpt::testApplied()
{
	World w;
	Map &map = *w.map;
	for (s16 x = -8; x < 8; x++)
		for (s16 z = -8; z < 8; z++)
			for (s16 y = -8; y < 8; y++)
				map.setNode({ x, y, z }, MapNode(y < 0 ? w.stone : CONTENT_AIR));

	Sculptor sculptor(map);
	const shape::Sixteenth top{ v3s16(0, -1, 0), v3s16(8, 15, 8) };
	sculptor.apply(Stroke{ Tool::Box, STEPS, top, UP, Act::Cut }, CONTENT_IGNORE);
	UASSERT(map.getNode({ 0, -1, 0 }).getContent() == CONTENT_AIR);

	const shape::Sixteenth beside{ v3s16(2, -1, 0), v3s16(8, 15, 8) };
	sculptor.apply(Stroke{ Tool::Sphere, 6, beside, UP, Act::Cut }, CONTENT_IGNORE);
	const shape::Shape *cut = map.getShape({ 2, -1, 0 });
	UASSERT(cut && !cut->has({ 8, 15, 8 }) && cut->has({ 0, 0, 0 }));

	// placed on the cut: into the open above (dirt), and nothing in the stone
	sculptor.apply(Stroke{ Tool::Box, 4, { v3s16(2, -1, 0), v3s16(8, 13, 8) }, UP, Act::Add }, w.dirt);
	UASSERT(map.getNode({ 2, 0, 0 }).getContent() == w.dirt);
	UASSERT(map.getShape({ 2, 0, 0 }));
	UASSERT(map.getShape({ 2, -1, 0 }) && !map.getShape({ 2, -1, 0 })->has({ 8, 14, 8 }));

	// stone filled back into its own cut, whole again
	sculptor.apply(Stroke{ Tool::Box, STEPS, { v3s16(2, -2, 0), v3s16(8, 15, 8) }, UP, Act::Add }, w.stone);
	UASSERT(!map.getShape({ 2, -1, 0 }));
	UASSERT(map.getNode({ 2, -1, 0 }).getContent() == w.stone);
}

// Smoothed ground dug the first time is dug from the shape it had, not
// its cube: what's left is its slope less the stroke.
void TestSculpt::testSlopeDugFromItsShape()
{
	World w;
	Map &map = *w.map;
	auto height = [](int x) { return 2 + int(std::floor(x / 3.f)); };
	for (s16 x = -16; x < 16; x++)
		for (s16 z = -16; z < 16; z++)
			for (s16 y = -16; y < 16; y++)
				map.setNode({ x, y, z }, MapNode(y < height(x) ? w.dirt : CONTENT_AIR));

	const v3s16 p(6, height(6) - 1, 0); // the foot of a step: lowered
	const std::optional<ground::Top> was = map.groundTop(p);
	UASSERT(was && !was->shape(0).isWhole());
	const shape::Sixteenth s{ p, v3s16(8, 0, 8) };
	Sculptor(map).apply(Stroke{ Tool::Box, 1, s, v3s16(0, -1, 0), Act::Cut }, CONTENT_IGNORE);

	shape::Shape expect = was->shape(0);
	expect.set({ 8, 0, 8 }, false);
	const shape::Shape *now = map.getShape(p);
	UASSERT(now && *now == expect);
}

// A mirrored stroke is its cells and their reflection across the plane,
// once each; a stroke across the plane is its own mirror.
void TestSculpt::testMirrored()
{
	Stroke s = at(Tool::Sphere, 5, false);
	const v3s32 g = world(s.at);
	const s32 twice = (g.X + 20) * 2 + 1; // between cells, 20 or so to the east
	s.mirror = Stroke::Mirror{ 0, twice };
	const auto both = set(s.cells());
	s.mirror.reset();
	const std::vector<v3s32> one = s.cells();
	UASSERTEQ(size_t, both.size(), one.size() * 2);
	for (const v3s32 &c : one) {
		UASSERT(both.count({ c.X, c.Y, c.Z }));
		UASSERT(both.count({ twice - 1 - c.X, c.Y, c.Z }));
	}
	s.mirror = Stroke::Mirror{ 0, g.X * 2 + 1 }; // through the pointed sixteenth's edge
	const auto self = set(s.cells());
	UASSERT(self.size() < one.size() * 2);
}

// Undone, a stroke leaves the map as it found it: blocks, cut nodes, and
// smoothed ground smoothed again.
void TestSculpt::testUndo()
{
	World w;
	Map &map = *w.map;
	auto height = [](int x) { return 2 + int(std::floor(x / 3.f)); };
	for (s16 x = -16; x < 16; x++)
		for (s16 z = -16; z < 16; z++)
			for (s16 y = -16; y < 16; y++)
				map.setNode({ x, y, z }, MapNode(y < height(x) ? w.dirt : CONTENT_AIR));
	const v3s16 p(5, height(5) - 1, 0); // under a step: it rises into the cell above
	const std::optional<ground::Top> smooth = map.groundTop(p);
	UASSERT(smooth && !smooth->shape(1).empty());

	History history;
	Sculptor sculptor(map);
	history.record("cody", { 0, 0, sculptor.apply(Stroke{ Tool::Sphere, 24, { p, v3s16(8, 8, 8) }, UP, Act::Cut }, CONTENT_IGNORE) });
	history.record("cody", { 0, 0, sculptor.apply(Stroke{ Tool::Box, 4, { p, v3s16(8, 2, 8) }, UP, Act::Add }, w.stone) });
	UASSERT(!map.groundTop(p));

	for (int i = 0; i < 2; i++) {
		const std::optional<History::Entry> e = history.pop("cody");
		UASSERT(e && e->machine == 0);
		e->edit.restore(map);
	}
	UASSERT(!history.pop("cody"));
	UASSERT(!history.pop("someone else"));
	for (s16 x = -4; x < 16; x++)
		for (s16 z = -6; z < 6; z++)
			for (s16 y = -2; y < 10; y++) {
				UASSERT(map.getNode({ x, y, z }).getContent() == (y < height(x) ? w.dirt : CONTENT_AIR));
				UASSERT(!map.getShape({ x, y, z }));
			}
	const std::optional<ground::Top> again = map.groundTop(p);
	UASSERT(again && again->cells == smooth->cells);
}

// An object's frame is as the server turns it: yaw 90° faces its +z west.
// On the workbench its middle stays where it's anchored however it's
// turned; mirroring is across its own grid's axis that runs left to right
// for the player. A tool the page names that doesn't exist is the page's bug.
void TestSculpt::testHoldPointsWhereItLooks()
{
	v3f forward(0, 0, 1);
	piece::frame(v3f(0), v3f(0, 90, 0)).transformVect(forward);
	UASSERT(forward.getDistanceFrom(v3f(-1, 0, 0)) < 0.001f);

	const v3f anchor(10, 9, -7), middle(1, 0.5f, 2);
	Hold hold(7, middle, 0, core::matrix4(), anchor);
	hold.watch(anchor - v3f(0, 0, 3), v3f(0, -0.3f, 1)); // the player south of it, looking north
	UASSERT(hold.held() == anchor);
	UASSERT(hold.across() == 0); // left and right run along x
	const v3f c = anchor * BS;
	for (int i = 0; i < 2; i++) {
		const Hold::Pose pose = hold.pose();
		v3f m = middle * BS;
		piece::frame(pose.position, pose.rotation).transformVect(m);
		UASSERT(m.getDistanceFrom(c) < 0.01f * BS);
		hold.turn(Hold::Spin::Yaw, core::PI / 2);
		hold.turn(Hold::Spin::Pitch, 0.4f);
	}
	Hold quarter(7, middle, 0, core::matrix4(), anchor);
	quarter.watch(anchor - v3f(0, 0, 3), v3f(0, 0, 1));
	quarter.turn(Hold::Spin::Yaw, core::PI / 2);
	UASSERT(quarter.across() == 2); // a quarter turn: its z runs across
	// the player walked round to its east, looking west: its z runs across
	Hold walked(7, middle, 0, core::matrix4(), anchor);
	walked.watch(anchor + v3f(3, 0, 0), v3f(-1, 0, 0));
	UASSERT(walked.across() == 2);

	Hand hand;
	hand.pick("bore");
	UASSERT(hand.tool() == Tool::Bore);
	EXCEPTION_CHECK(BaseException, hand.pick("hammer"));
}

// A shape dragged out on a face fills its footprint, corner to corner, as
// deep as its size: the box all of it; the cylinder the ellipse across it
// all the way down; the cone that ellipse narrowing away from the face;
// the sphere the ellipsoid. A footprint off the face's plane, or wider than
// the largest size, isn't a stroke. Its size reads as the player sees it.
void TestSculpt::testDrawnByFootprint()
{
	const v3s16 up(0, 1, 0);
	const shape::Sixteenth at{ v3s16(0, 0, 0), v3s16(2, 15, 3) };
	const shape::Sixteenth to{ v3s16(1, 0, 0), v3s16(5, 15, 10) }; // 20 across x, 8 along z
	auto stroke = [&](Tool tool, bool place) {
		return Stroke{ tool, 6, at, up, place ? Act::Add : Act::Cut, std::nullopt, to };
	};
	const Stroke box = stroke(Tool::Box, true);
	UASSERT(box.valid());
	UASSERTEQ(size_t, box.cells().size(), size_t(20 * 8 * 6));
	UASSERT(box.measure() == "20/16 × 8/16, 6/16 deep");
	// placed, it's on the face: from y 16 (the node above's first) up
	for (const v3s32 &c : box.cells())
		UASSERT(c.Y >= 16 && c.Y < 22 && c.X >= 2 && c.X < 22 && c.Z >= 3 && c.Z < 11);

	auto has = [](const std::vector<v3s32> &cells, v3s32 c) {
		return std::find(cells.begin(), cells.end(), c) != cells.end();
	};
	// a round one's drawn from its middle (sprint 65): a circle round where
	// it began, as far out as the drag went furthest (19 along x)
	const std::vector<v3s32> cylinder = stroke(Tool::Cylinder, true).cells();
	UASSERT(has(cylinder, { 2, 16, 3 }) && has(cylinder, { 2, 21, 3 })); // its middle, all the way up
	UASSERT(has(cylinder, { 21, 18, 3 }) && has(cylinder, { -17, 18, 3 }) && has(cylinder, { 2, 18, 22 })); // round
	UASSERT(!has(cylinder, { 21, 18, 22 }) && !has(cylinder, { -17, 18, -16 })); // not its box's corners
	UASSERT(cylinder.size() < 39 * 39 * 6 * 0.85 && cylinder.size() > 39 * 39 * 6 * 0.7);

	const std::vector<v3s32> cone = stroke(Tool::Cone, true).cells();
	auto layer = [&](s32 y) { return std::count_if(cone.begin(), cone.end(), [&](const v3s32 &c) { return c.Y == y; }); };
	UASSERT(layer(16) > layer(18) && layer(18) > layer(20) && layer(20) > 0 && layer(20) >= layer(21));
	UASSERT(!has(cone, { 19, 18, 3 }) && has(cone, { 19, 16, 3 })); // out by its edge only near the face

	const std::vector<v3s32> sphere = stroke(Tool::Sphere, true).cells();
	UASSERT(has(sphere, { 2, 18, 3 }) && !has(sphere, { 20, 16, 3 }) && sphere.size() < cylinder.size());

	// dug, it goes into the face: y 15 down
	for (const v3s32 &c : stroke(Tool::Box, false).cells())
		UASSERT(c.Y <= 15 && c.Y > 15 - 6);

	Stroke off = box;
	off.to->cell.Y = 14;
	UASSERT(!off.valid());
	Stroke wide = box;
	wide.to = shape::Sixteenth{ v3s16(5, 0, 0), v3s16(0, 15, 3) };
	UASSERT(!wide.valid());
	// pulled out: as deep as it's pulled, whatever its size; measured so
	Stroke pulled = box;
	pulled.deep = 40;
	UASSERT(pulled.valid());
	UASSERTEQ(size_t, pulled.cells().size(), size_t(20 * 8 * 40));
	UASSERT(pulled.measure() == "20/16 × 8/16, 40/16 deep");
	pulled.deep = 0;
	UASSERT(!pulled.valid());
	// the cursor's ray, past a line up out of the face: where it comes nearest
	// it, up it or below
	const v3f foot(1, 2, 3), upward(0, 1, 0);
	for (const f32 h : { 5.f, -2.f }) {
		const core::line3d<f32> ray(v3f(10, 2 + h, -4), v3f(-10, 2 + h, 10)); // level, passing over it
		const std::optional<f32> t = nearestAlong(foot, upward, ray);
		UASSERT(t && std::abs(*t - h) < 1e-3f);
	}
	UASSERT(!nearestAlong(foot, upward, { v3f(0, 0, 0), v3f(0, 9, 0) }));
	pulled.deep = MAX_DEEP + 1;
	UASSERT(!pulled.valid());
	// clicked: the size square, as before
	const Stroke click{ Tool::Box, 6, at, up, Act::Add, std::nullopt };
	UASSERTEQ(size_t, click.cells().size(), size_t(6 * 6 * 6));
	UASSERT(click.measure() == "6/16 × 6/16, 6/16 deep");
}

// A held object turns in whole steps about one axis; dragged round its
// disc it turns as the cursor goes round, not against it; the ring's rim
// rolls it, the disc yaws it, the middle is neither; levelled it stands
// upright, its heading kept; reset, square to the player.
void TestSculpt::testTurnedByHandle()
{
	const v3f eye(0, 10, 0);
	auto turned = [](const Hold &h, v3f v) {
		const Hold::Pose pose = h.pose();
		core::matrix4 m = piece::frame(v3f(0), pose.rotation);
		m.transformVect(v);
		return v;
	};
	Hold hold(1, v3f(0), 1, core::matrix4(), eye + v3f(0, 0, 3));
	hold.watch(eye, v3f(0, 0, 1));
	hold.begin(Hold::Spin::Yaw);
	hold.spin(20 * core::DEGTORAD, 15 * core::DEGTORAD);
	const v3f x = turned(hold, v3f(1, 0, 0));
	UASSERT(std::abs(std::acos(std::clamp(x.X, -1.f, 1.f)) - 15 * core::DEGTORAD) < 0.01f);
	hold.end();

	// down onto the disc at its angle 0, then 60° round: the object's x
	// follows where the cursor went
	Hold disc(1, v3f(0), 1, core::matrix4(), eye + v3f(0, 0, 3));
	disc.watch(eye, v3f(0, 0, 1));
	const Hold::Ring r = disc.disc();
	auto down = [&](f32 a) {
		const v3f at = r.centre + v3f(std::cos(a), 0, -std::sin(a)) * r.radius * 0.6f;
		return core::line3d<f32>((at + v3f(0, 5, 0)) * BS, (at - v3f(0, 5, 0)) * BS);
	};
	const std::optional<Hold::Grip> g = disc.grip(down(0));
	UASSERT(g && g->spin == Hold::Spin::Yaw);
	disc.begin(g->spin);
	disc.spin(*disc.angle(Hold::Spin::Yaw, down(core::PI / 3)) - g->angle, 0);
	const v3f went = turned(disc, v3f(1, 0, 0)), cursor(std::cos(core::PI / 3), 0, -std::sin(core::PI / 3));
	UASSERT(went.getDistanceFrom(cursor) < 0.01f);

	// the ring's rim, from the player's eye: a roll; through its middle: nothing
	const Hold::Ring ring = disc.ring();
	const v3f rim = ring.centre + v3f(0, ring.radius, 0);
	UASSERT(disc.grip({ eye * BS, (eye + (rim - eye) * 3) * BS })->spin == Hold::Spin::Roll);
	UASSERT(!disc.grip({ eye * BS, (eye + (ring.centre - eye) * 3) * BS }));

	// levelled, what's most up of it is up exactly, its heading kept, and
	// it's still a turn; flipped, it stays flipped; reset, square to you
	Hold lv(1, v3f(0), 1, core::matrix4(), eye + v3f(0, 0, 3));
	lv.watch(eye, v3f(0, 0, 1));
	lv.turn(Hold::Spin::Yaw, 0.5f);
	lv.turn(Hold::Spin::Pitch, 0.3f);
	lv.turn(Hold::Spin::Roll, 0.2f);
	lv.level();
	UASSERT(turned(lv, v3f(0, 1, 0)).getDistanceFrom(v3f(0, 1, 0)) < 0.001f);
	const v3f heading = turned(lv, v3f(0, 0, 1));
	UASSERT(std::abs(std::atan2(heading.X, heading.Z) - 0.5f) < 0.15f);
	const v3f lx = turned(lv, v3f(1, 0, 0)), ly = turned(lv, v3f(0, 1, 0)), lz = turned(lv, v3f(0, 0, 1));
	UASSERT(std::abs(lx.dotProduct(ly.crossProduct(lz)) - 1) < 0.001f);
	lv.turn(Hold::Spin::Pitch, core::PI * 0.9f);
	lv.level();
	UASSERT(turned(lv, v3f(0, -1, 0)).getDistanceFrom(v3f(0, 1, 0)) < 0.001f);
	lv.reset();
	UASSERT(turned(lv, v3f(0, 0, 1)).getDistanceFrom(v3f(0, 0, 1)) < 0.001f);
	UASSERT(turned(lv, v3f(1, 0, 0)).getDistanceFrom(v3f(1, 0, 0)) < 0.001f);
}

// Sprint 75: a flat frame dragged out is a ring of beams the size wide,
// open in the middle; a truss is the frame and its diagonals; a beam
// dragged corner to corner is a diagonal the size thick, not the box.
void TestSculpt::testStructure()
{
	const shape::Sixteenth from{ v3s16(0, 0, 0), v3s16(0, 15, 0) }, to{ v3s16(2, 0, 2), v3s16(15, 15, 15) };
	auto drawn = [&](Tool tool, int size) {
		return set(Stroke{ tool, size, from, UP, Act::Add, std::nullopt, to }.cells());
	};
	const auto box = drawn(Tool::Box, 4), frame = drawn(Tool::Frame, 4), truss = drawn(Tool::Truss, 4),
			beam = drawn(Tool::Beam, 4);
	// 48 × 48 across, 4 up
	UASSERTEQ(size_t, box.size(), size_t(48 * 48 * 4));
	UASSERTEQ(size_t, frame.size(), size_t((48 * 48 - 40 * 40) * 4));
	UASSERT(!frame.count({ 24, 16, 24 }) && frame.count({ 1, 16, 24 }) && frame.count({ 24, 17, 46 }));
	for (const auto &c : frame)
		UASSERT(truss.count(c) && box.count(c));
	UASSERT(truss.count({ 24, 16, 24 }) && truss.size() > frame.size());
	// the beam: along the diagonal, the corners off it empty
	UASSERT(beam.count({ 24, 16, 24 }) && beam.count({ 2, 16, 2 }) && beam.count({ 45, 16, 45 }));
	UASSERT(!beam.count({ 45, 16, 2 }) && !beam.count({ 2, 16, 45 }));
	UTEST(beam.size() < box.size() / 4, "a beam of %zu", beam.size());
	// across a face of the frame, and no deeper than the size
	for (const auto &c : beam)
		UASSERT(c[1] >= 16 && c[1] < 20);
}

// A block rounded keeps its middle and loses its corners and edges, not
// its faces' middles; rounded again it's as it was (an opening is idempotent).
void TestSculpt::testRounded()
{
	World w;
	Map &map = *w.map;
	for (s16 x = -2; x < 4; x++)
		for (s16 z = -2; z < 4; z++)
			for (s16 y = -2; y < 4; y++)
				map.setNode({ x, y, z }, MapNode(x >= 0 && y >= 0 && z >= 0 && x < 2 && y < 2 && z < 2 ? w.stone : CONTENT_AIR));
	Sculptor sculptor(map);
	const VoxelArea area(v3s16(-2), v3s16(3));
	const Edit edit = sculptor.round(area, 6);
	UASSERT(!edit.was.empty());
	const shape::Shape *corner = map.getShape({ 0, 0, 0 });
	UASSERT(corner && !corner->has({ 0, 0, 0 }) && corner->has({ 15, 15, 15 }));
	UASSERT(corner->has({ 15, 15, 0 })); // a face's middle (of the 2-block cube) stays
	UASSERT(!corner->has({ 0, 0, 15 })); // an edge goes
	UASSERT(sculptor.round(area, 6).was.empty());
	edit.restore(map);
	UASSERT(!map.getShape({ 0, 0, 0 }));
}
