// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/shared/game/dig/earthwork.h"
#include <cmath>

/*
	The shovel (sprint 124): digging is placing run backwards. Where a form
	goes (earthwork::place), what of the box it takes, and a swing's bite of
	it, the highest first. The rules the server digs by and the client
	outlines by.
*/
class TestScoop : public TestBase {
public:
	TestScoop() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestScoop"; }

	void runTests(IGameDef *gamedef);

	void testBoresNearestFirst();
	void testDigsTheCellAimedInto();
	void testSnapsToTheGrid();
	void testTurnsTheWayTheyFace();
	void testLevelsAtFeetOrAim();
	void testPitIsRound();
	void testBitesTheHighestFirst();
	void testFindsTheGround();
};

static TestScoop g_test_instance;

void TestScoop::runTests(IGameDef *gamedef)
{
	TEST(testBoresNearestFirst);
	TEST(testDigsTheCellAimedInto);
	TEST(testSnapsToTheGrid);
	TEST(testTurnsTheWayTheyFace);
	TEST(testLevelsAtFeetOrAim);
	TEST(testPitIsRound);
	TEST(testBitesTheHighestFirst);
	TEST(testFindsTheGround);
}

using namespace earthwork;

// Looking along +x from the origin: the cells round the line, the nearest first, none behind
void TestScoop::testBoresNearestFirst()
{
	const std::vector<v3s32> cells = bore(v3f(0, 2, 2), v3f(1, 0, 0), 8, 64, 4);
	UASSERT(!cells.empty());
	s32 last = -1000;
	bool hasAxis = false, far = false;
	for (const v3s32 &c : cells) {
		UASSERT(c.X >= -4 && c.X <= 64 && (c.X & 3) == 0);
		UASSERT(c.X >= last - 4); // nearest first, give or take a cell across
		last = std::max(last, c.X);
		hasAxis |= c.Y == 0 && c.Z == 0;
		far |= std::abs(c.Y + 2 - 2) > 10;
	}
	UASSERT(hasAxis && !far);
	UASSERT(cells.front().X == 0 && cells.back().X == 60);
}

// The tool's cell the aimed sixteenth is in, on its grid every way, below zero too, whatever the mode
void TestScoop::testDigsTheCellAimedInto()
{
	const Target t = place(v3s32(13, -3, 5), 99, 0, 0.7f, Size{ 8, 8, 8 }, Kind::Cell, Mode::Free);
	UASSERT(t.min == v3s32(8, -8, 0) && t.max == v3s32(15, -1, 7));
	UASSERT(fits(t, Size{ 8, 8, 8 }));
	const Target q = place(v3s32(13, 5, 5), 0, 0, 0, Size{ 4, 4, 4 }, Kind::Cell, Mode::Snap);
	UASSERT(q.min == v3s32(12, 4, 4));
}

// A 1 × 1 hole aimed off the grid lands on it, its top the ground there; Ctrl leaves it where aimed
void TestScoop::testSnapsToTheGrid()
{
	const Target t = place(v3s32(13, 0, 5), 40, 0, 0, Size{ 16, 16, 8 }, Kind::Box, Mode::Snap);
	UASSERT(t.min == v3s32(8, 32, 0) && t.max == v3s32(23, 39, 15));
	UASSERT(fits(t, Size{ 16, 16, 8 }) && !fits(t, Size{ 16, 16, 16 }));
	const Target f = place(v3s32(13, 0, 5), 40, 0, 0, Size{ 16, 16, 8 }, Kind::Box, Mode::Free);
	UASSERT(f.min == v3s32(5, 32, -3));
}

// A trench runs the way they face: along z facing z, along x facing x
void TestScoop::testTurnsTheWayTheyFace()
{
	const Target z = place(v3s32(0, 0, 0), 0, 0, 0, Size{ 32, 8, 8 }, Kind::Box, Mode::Snap);
	UASSERT(z.max.Z - z.min.Z + 1 == 32 && z.max.X - z.min.X + 1 == 8);
	const Target x = place(v3s32(0, 0, 0), 0, 0, 1.5f, Size{ 32, 8, 8 }, Kind::Box, Mode::Snap);
	UASSERT(x.max.X - x.min.X + 1 == 32 && x.max.Z - x.min.Z + 1 == 8);
	UASSERT(fits(x, Size{ 32, 8, 8 }));
}

void TestScoop::testLevelsAtFeetOrAim()
{
	const Target feet = place(v3s32(0, 0, 0), 50, 30, 0, Size{ 16, 16, 32 }, Kind::Level, Mode::Snap);
	UASSERT(feet.min.Y == 30 && feet.max.Y == 61);
	const Target aimed = place(v3s32(0, 0, 0), 50, 30, 0, Size{ 16, 16, 32 }, Kind::Level, Mode::Aimed);
	UASSERT(aimed.min.Y == 50);
}

void TestScoop::testPitIsRound()
{
	const Target t{ v3s32(0, 0, 0), v3s32(15, 7, 15), Kind::Round };
	UASSERT(holds(t, 8, 8) && holds(t, 0, 8) && !holds(t, 0, 0) && !holds(t, 15, 15) && !holds(t, 16, 8));
}

// Flat ground at 16 over a 2 × 2 box 8 deep: a swing of 8 cells a column takes whole layers from the top
void TestScoop::testBitesTheHighestFirst()
{
	const Target t{ v3s32(0, 8, 0), v3s32(1, 15, 1), Kind::Box };
	const auto flat = [](s32, s32) { return 16; };
	std::vector<Column> a = bite(t, 12, flat); // three layers cost 12
	UASSERT(a.size() == 4 && a[0].bed == 13 && a[0].top == 16);
	std::vector<Column> b = bite(t, 1, flat); // the top layer, whatever it costs
	UASSERT(b.size() == 4 && b[0].bed == 15);
	std::vector<Column> c = bite(t, 1000, flat); // never under the box
	UASSERT(c[0].bed == 8);
	UASSERT(bite(t, 100, [](s32, s32) { return 8; }).empty()); // dug out
	// a hump is taken first: the one high column alone
	std::vector<Column> d = bite(t, 2, [](s32 x, s32 z) { return x == 0 && z == 0 ? 16 : 10; });
	UASSERT(d.size() == 1 && d[0].bed == 14);
}

void TestScoop::testFindsTheGround()
{
	// ground filled up to 37 (its top sixteenth): the surface is 38, found from above or below
	const auto solid = [](v3s32 c) { return c.Y <= 37; };
	UASSERTEQ(s32, *surface(3, 3, 20, solid), 38);
	UASSERTEQ(s32, *surface(3, 3, 60, solid), 38);
	UASSERT(!surface(3, 3, 200, solid));
}
