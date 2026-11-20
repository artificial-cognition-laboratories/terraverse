// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/server/world/run/pile/pile.h"
#include "src/server/world/run/pile/piles.h"
#include "src/shared/map/dummymap.h"
#include "src/shared/core/gamedef.h"
#include "src/shared/content/nodedef.h"
#include <cstdlib>

using namespace pile;

class TestPile : public TestBase {
public:
	TestPile() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPile"; }

	void runTests(IGameDef *gamedef);

	void testHeapRestsAtItsAngle();
	void testSameGroundEveryTime();
	void testMudHoldsTillPushed();
	void testRutStopsDeepening();
	void testWheelCutsTheWorld(IGameDef *gamedef);
	void testHoleSlumps(IGameDef *gamedef);
};

static TestPile g_test_instance;

void TestPile::runTests(IGameDef *gamedef)
{
	TEST(testHeapRestsAtItsAngle);
	TEST(testSameGroundEveryTime);
	TEST(testMudHoldsTillPushed);
	TEST(testRutStopsDeepening);
	TEST(testWheelCutsTheWorld, gamedef);
	TEST(testHoleSlumps, gamedef);
}

namespace {

const Material SAND{ 1, 0, 15, 4 };
const Material MUD{ 1, 3, 4, 10 };

// The steepest step between neighbours, units.
int steepest(const Field &f)
{
	int most = 0;
	for (int z = 0; z < f.depth(); z++)
		for (int x = 0; x < f.width(); x++) {
			if (x + 1 < f.width())
				most = std::max(most, std::abs(f.top(x, z) - f.top(x + 1, z)));
			if (z + 1 < f.depth())
				most = std::max(most, std::abs(f.top(x, z) - f.top(x, z + 1)));
		}
	return most;
}

// A node of sand poured as one column in the middle of a flat field.
Field heap(Material m)
{
	Field f(48, 48, m);
	f.set(24, 24, 0, 4 * UNIT);
	f.stir(24, 24);
	return f;
}

} // namespace

// A pour slides out to a cone and stops; not a grain is lost.
void TestPile::testHeapRestsAtItsAngle()
{
	Field f = heap(SAND);
	const long long before = f.total();
	const int steps = f.settle(10000);
	UASSERT(steps < 10000); // it sleeps
	UASSERTEQ(long long, f.total(), before);
	// at rest: no step steeper than it holds, give or take the quarter rounding
	UASSERT(steepest(f) <= SAND.repose);
	UASSERT(f.top(24, 24) < 4 * UNIT); // it spread
	UASSERT(f.top(24, 24) > f.top(30, 24)); // and still stands as a heap
}

// Same start, same end: the server and a predicting client agree.
void TestPile::testSameGroundEveryTime()
{
	Field one = heap(SAND), two = heap(SAND);
	one.press(20.5f, 22.5f, 2.5f, 60);
	two.press(20.5f, 22.5f, 2.5f, 60);
	one.settle(10000);
	two.settle(10000);
	for (int z = 0; z < one.depth(); z++)
		for (int x = 0; x < one.width(); x++)
			UASSERTEQ(int, one.top(x, z), two.top(x, z));
}

// A mud face steeper than sand rests at holds; sand the same falls.
void TestPile::testMudHoldsTillPushed()
{
	auto face = [](Material m) {
		Field f(16, 4, m);
		for (int z = 0; z < 4; z++)
			for (int x = 0; x < 16; x++) {
				f.set(x, z, 0, x < 8 ? UNIT / 2 : UNIT / 2 - 3);
				f.stir(x, z);
			}
		return f;
	};
	Field mud = face(MUD), sand = face(SAND);
	UASSERTEQ(int, mud.settle(1000), 0);
	// and untouched, sand that steep stands too (a generated slope)
	Field still(16, 4, SAND);
	for (int z = 0; z < 4; z++)
		for (int x = 0; x < 16; x++)
			still.set(x, z, 0, x < 8 ? UNIT : 0);
	UASSERTEQ(int, still.settle(1000), 0);
	UASSERT(sand.settle(1000) > 0);
}

// The same wheel ten times: a rut that deepens, then stops, with a ridge
// beside it; nothing lost.
void TestPile::testRutStopsDeepening()
{
	Field f(32, 32, SAND);
	for (int z = 0; z < 32; z++)
		for (int x = 0; x < 32; x++)
			f.set(x, z, 0, 2 * UNIT);
	const long long before = f.total();
	int last = f.top(16, 16), first = 0;
	for (int pass = 0; pass < 40; pass++) {
		f.press(16.5f, 16.5f, 3, 30);
		const int now = f.top(16, 16);
		UASSERT(now <= last);
		if (pass == 0)
			first = last - now;
		last = now;
	}
	UASSERT(first > 0);
	UASSERT(2 * UNIT - last == SAND.give); // packed: no deeper
	UASSERT(first < SAND.give);          // but not all at once
	UASSERT(f.top(16, 20) > 2 * UNIT); // the ridge, just outside the patch
	UASSERTEQ(long long, f.total(), before);
}

namespace {

// All the solid sixteenths in a box of the map.
long long solid(Map &map, v3s16 lo, v3s16 hi)
{
	long long n = 0;
	for (s16 z = lo.Z; z <= hi.Z; z++)
		for (s16 y = lo.Y; y <= hi.Y; y++)
			for (s16 x = lo.X; x <= hi.X; x++) {
				const v3s16 p(x, y, z);
				if (const auto s = map.solidShape(p)) {
					for (s16 a = 0; a < 16; a++)
						for (s16 b = 0; b < 16; b++)
							for (s16 c = 0; c < 16; c++)
								n += s->has(v3s16(a, b, c));
				} else if (map.getNode(p).getContent() == t_CONTENT_SAND) {
					n += 16 * 16 * 16;
				}
			}
	return n;
}

} // namespace

// A wheel rolled across a sand floor: the world's sand is cut into a rut
// (as shapes, which the clients are sent), none lost, and it all sleeps.
void TestPile::testWheelCutsTheWorld(IGameDef *gamedef)
{
	DummyMap map(gamedef, { 0, 0, 0 }, { 1, 0, 1 });
	map.fill({ 0, 0, 0 }, { 1, 0, 1 }, MapNode(CONTENT_AIR));
	for (s16 z = 0; z < 32; z++)
		for (s16 x = 0; x < 32; x++)
			for (s16 y = 0; y <= 7; y++)
				map.setNode(v3s16(x, y, z), MapNode(t_CONTENT_SAND));
	const v3s16 lo(4, 2, 4), hi(27, 12, 27);
	const long long before = solid(map, lo, hi);
	Piles piles(map, *gamedef->ndef());
	for (int i = 0; i <= 60; i++) {
		// a car's wheel: 3 kN on a tyre 0.25 wide, a tenth of a node a step
		piles.press(1, v3f(13 + i * 0.1f, 7.5f, 16), 0.125f, 60);
		piles.step(0.05f);
	}
	UASSERT(piles.awake() > 0);
	for (int i = 0; i < 100; i++)
		piles.step(0.05f);
	UASSERTEQ(size_t, piles.awake(), 0);
	// a rut under where it rolled: the node's cut, lower in the middle
	const shape::Shape *cut = map.getShape(v3s16(16, 7, 16));
	UASSERT(cut != nullptr);
	UASSERT(!cut->has(v3s16(8, 15, 8)));
	UASSERTEQ(long long, solid(map, lo, hi), before);
}

// A node dug out of a sand floor: the sand round it doesn't ramp into the
// hole as smoothing would, it slides in (some of the hole fills), and not a
// grain's lost.
void TestPile::testHoleSlumps(IGameDef *gamedef)
{
	DummyMap map(gamedef, { 0, 0, 0 }, { 1, 0, 1 });
	map.fill({ 0, 0, 0 }, { 1, 0, 1 }, MapNode(CONTENT_AIR));
	for (s16 z = 0; z < 32; z++)
		for (s16 x = 0; x < 32; x++)
			for (s16 y = 0; y <= 7; y++)
				map.setNode(v3s16(x, y, z), MapNode(t_CONTENT_SAND));
	const v3s16 lo(4, 2, 4), hi(27, 12, 27), hole(16, 7, 16);
	const MapNode was = map.getNode(hole);
	map.setNode(hole, MapNode(CONTENT_AIR));
	const long long before = solid(map, lo, hi);
	Piles piles(map, *gamedef->ndef());
	piles.changed(hole, was);
	// frozen: the neighbours are cut as they stood (whole: a flat floor)
	const shape::Shape *side = map.getShape(hole + v3s16(1, 0, 0));
	UASSERT(side != nullptr && side->isWhole());
	for (int i = 0; i < 200 && (i == 0 || piles.awake() > 0); i++)
		piles.step(0.05f);
	UASSERTEQ(size_t, piles.awake(), 0);
	// sand slid in: the hole's sand again (some of it: it's cut, or smoothed)
	UASSERTEQ(content_t, map.getNode(hole).getContent(), t_CONTENT_SAND);
	// the piles keep every grain; the smoothed sand just past what they wrote
	// takes its shape from its new neighbours (computed, never stored), so the
	// world's count moves by a sliver of a node
	UASSERT(std::llabs(solid(map, lo, hi) - before) < 16 * 16 * 16 / 8);
}
