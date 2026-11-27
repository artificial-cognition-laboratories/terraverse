// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/client/render/map/detail/rings.h"
#include <cmath>

/*
	Terraverse: how finely the world's drawn (detail/sight.h): a voxel's never
	smaller than its pixels, and it's the finest that isn't; the near meshes'
	ladder (detail::Rings) doubles out from sixteenths to whole nodes, and holds
	a mesh's detail till the camera's well past the ring it crossed.
*/
class TestDetailRings : public TestBase
{
public:
	TestDetailRings() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestDetailRings"; }

	void runTests(IGameDef *gamedef);

	void testVoxelsAreTheirPixels();
	void testLadderDoubles();
	void testRingsHold();
};

static TestDetailRings g_test_instance;

void TestDetailRings::runTests(IGameDef *gamedef)
{
	TEST(testVoxelsAreTheirPixels);
	TEST(testLadderDoubles);
	TEST(testRingsHold);
}

namespace {
// a window 695 px high at 72°: a block a block off is about 478 px
const detail::Sight SIGHT = detail::Sight::of(695, 72 * M_PI / 180, 4);
} // namespace

void TestDetailRings::testVoxelsAreTheirPixels()
{
	UASSERT(std::fabs(SIGHT.scale - 478) < 478 * 0.03f); // its rounding to a twelfth of an octave
	int was = -100;
	for (float d = 1; d < 100000; d *= 1.07f) {
		const int e = SIGHT.exponent(d);
		const float px = std::exp2((float)e) * SIGHT.scale / d;
		UTEST(px >= SIGHT.pixels - 1e-3f, "at %.0f blocks a voxel of 2^%d is %.2f px", d, e, px);
		UTEST(px / 2 < SIGHT.pixels, "at %.0f blocks 2^%d isn't the finest that's %g px", d, e, SIGHT.pixels);
		UASSERT(e >= was); // coarser only further off
		was = e;
	}
	// in this window: sixteenths within ~7 blocks, whole blocks from ~60 to ~120
	UASSERTEQ(int, SIGHT.exponent(7), -4);
	UASSERTEQ(int, SIGHT.exponent(100), 0);
	UASSERTEQ(int, SIGHT.exponent(200), 1);
}

void TestDetailRings::testLadderDoubles()
{
	detail::Rings rings(SIGHT);
	u8 was = 1;
	for (float d = 0; d < 400; d += 0.5f) {
		const u8 now = rings.wanted({ 0, 0, 0 }, d);
		UASSERT(now == 1 || now == 2 || now == 4 || now == 8 || now == 16);
		UTEST(now == was || now == was * 2, "at %.1f blocks the ladder went from %d to %d", d, was, now);
		was = now;
	}
	UASSERTEQ(int, was, 16); // whole nodes at the far end: the near meshes go no coarser
}

void TestDetailRings::testRingsHold()
{
	detail::Rings rings(SIGHT);
	// the ring where sixteenths become eighths
	float ring = 1;
	while (rings.wanted({ 0, 0, 0 }, ring) == 1)
		ring += 0.01f;
	const v3s16 p(1, 2, 3);
	UASSERTEQ(int, rings.make(p, ring * 0.98f), 1);
	// just past it: kept; well past it: eighths
	UASSERTEQ(int, rings.wanted(p, ring * 1.05f), 1);
	UASSERTEQ(int, rings.wanted(p, ring * 1.3f), 2);
	// made coarse, it's held coming back in too
	UASSERTEQ(int, rings.make(p, ring * 1.3f), 2);
	UASSERTEQ(int, rings.wanted(p, ring * 0.95f), 2);
	UASSERTEQ(int, rings.wanted(p, ring * 0.7f), 1);
	// forgotten, it's the ring's own again
	rings.forget(p);
	UASSERTEQ(int, rings.wanted(p, ring * 0.95f), 1);
}
