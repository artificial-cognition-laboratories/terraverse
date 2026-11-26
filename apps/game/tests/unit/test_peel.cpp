// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/shared/game/cut/peel.h"
#include "src/shared/game/cut/snap.h"
#include "src/shared/game/cut/stack.h"
#include "src/shared/base/constants.h"
#include <map>

/*
	Peeling (sprint 123): what comes off a log where it's aimed. The rule both
	the server cuts by and the client shows by, so it's pinned here.
*/
class TestPeel : public TestBase {
public:
	TestPeel() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPeel"; }

	void runTests(IGameDef *gamedef);

	void testLogIsTheRun();
	void testCutWhereAimed();
	void testPlanksFillABlock();
	void testLongerThanABlock();
	void testNotFromTheEnd();
	void testLaidOnTheFace();
	void testSnapsToWhatsNear();
	void testStacksInRows();
};

static TestPeel g_test_instance;

void TestPeel::runTests(IGameDef *gamedef)
{
	TEST(testLogIsTheRun);
	TEST(testCutWhereAimed);
	TEST(testPlanksFillABlock);
	TEST(testLongerThanABlock);
	TEST(testNotFromTheEnd);
	TEST(testLaidOnTheFace);
	TEST(testSnapsToWhatsNear);
	TEST(testStacksInRows);
}

namespace {
// logs lying along x (facedir top +x), content 7, from x = 0 to n−1 at y = z = 0
std::function<std::optional<peel::Block>(v3s16)> lying(int n, u16 content = 7)
{
	return [n, content](v3s16 q) -> std::optional<peel::Block> {
		if (q.Y != 0 || q.Z != 0 || q.X < 0 || q.X >= n)
			return peel::Block{ 0, 0, nullptr };
		return peel::Block{ content, 3 << 2, nullptr };
	};
}
size_t cells(const peel::Box &b)
{
	return size_t(b.max.X - b.min.X + 1) * (b.max.Y - b.min.Y + 1) * (b.max.Z - b.min.Z + 1);
}
} // namespace

void TestPeel::testLogIsTheRun()
{
	// six lying in a row: the log through the third is four of them, it near the middle
	const peel::Log log = peel::read(v3s16(2, 0, 0), lying(6));
	UASSERTEQ(int, log.grain, 0);
	UASSERTEQ(size_t, log.shapes.size(), size_t(peel::LONGEST));
	UASSERT(log.start == v3s16(0, 0, 0) || log.start == v3s16(1, 0, 0));
	// one alone is a log of one
	UASSERTEQ(size_t, peel::read(v3s16(0, 0, 0), lying(1)).shapes.size(), size_t(1));
}

void TestPeel::testCutWhereAimed()
{
	const peel::Log log = peel::read(v3s16(0, 0, 0), lying(1));
	// from the top (+y), aimed near the +z side: a post 4 across sits against that side, at the top
	const std::optional<peel::Box> b = peel::at(log, v3s16(0, 1, 0), { 16, 4, 4 }, v3s32(8, 15, 14));
	UASSERT(b.has_value());
	UASSERTEQ(s32, b->min.Y, 12);
	UASSERTEQ(s32, b->max.Y, 15);
	UASSERTEQ(s32, b->max.Z, 15);
	UASSERTEQ(s32, b->min.Z, 12);
	UASSERTEQ(size_t, cells(*b), size_t(16 * 4 * 4));
}

void TestPeel::testPlanksFillABlock()
{
	// a whole block is exactly 64 planks of 16 × 4 × 1 off its top: nothing left between them
	peel::Log log = peel::read(v3s16(0, 0, 0), lying(1));
	int planks = 0;
	for (s16 y = 15; y >= 0; y--)
		for (s16 z = 1; z < 16; z += 4) {
			const std::optional<peel::Box> b = peel::at(log, v3s16(0, 1, 0), { 16, 4, 1 }, v3s32(8, y, z));
			UASSERT(b.has_value());
			for (const auto &[i, c] : peel::split(log, *b))
				log.shapes[i] = peel::without(log.shapes[i], c.first, c.second);
			planks++;
		}
	UASSERTEQ(int, planks, 64);
	UASSERT(log.shapes[0].empty());
	UASSERT(!peel::any(log, v3s16(0, 1, 0), { 16, 4, 1 }));
}

void TestPeel::testLongerThanABlock()
{
	// a 4 m beam off four lying in a row takes a part of each
	const peel::Log log = peel::read(v3s16(1, 0, 0), lying(4));
	const std::optional<peel::Box> b = peel::at(log, v3s16(0, 1, 0), { 64, 8, 4 }, peel::cell(log, v3s16(1, 0, 0), v3s16(8, 15, 8)));
	UASSERT(b.has_value());
	UASSERTEQ(size_t, peel::split(log, *b).size(), size_t(4));
	// three are too short for it
	const peel::Log short_ = peel::read(v3s16(0, 0, 0), lying(3));
	UASSERT(!peel::at(short_, v3s16(0, 1, 0), { 64, 8, 4 }, v3s32(8, 15, 8)).has_value());
}

void TestPeel::testNotFromTheEnd()
{
	// a member's cut from a log's side, never its end grain
	const peel::Log log = peel::read(v3s16(0, 0, 0), lying(1));
	UASSERT(!peel::at(log, v3s16(1, 0, 0), { 16, 4, 4 }, v3s32(15, 8, 8)).has_value());
	UASSERT(!peel::any(log, v3s16(-1, 0, 0), { 16, 4, 4 }));
}

void TestPeel::testLaidOnTheFace()
{
	// a 1 m plank laid on a block's top, along x, aimed at its middle: just above it, flat, centred
	const peel::Box b = peel::lay(v3s16(0, 0, 0), v3s16(0, 1, 0), v3s16(8, 15, 8), peel::dims({ 16, 4, 1 }, { 0, 2, 1 }), false);
	UASSERTEQ(s32, b.min.Y, 16);
	UASSERTEQ(s32, b.max.Y, 16);
	UASSERTEQ(s32, b.min.X, 0);
	UASSERTEQ(s32, b.max.Z - b.min.Z + 1, 4);
	// a post stood up against a block's −x side: wholly outside it, upright, centred on the aim's height
	const peel::Box p = peel::lay(v3s16(0, 0, 0), v3s16(-1, 0, 0), v3s16(0, 8, 8), peel::dims({ 32, 4, 4 }, { 1, 0, 2 }), false);
	UASSERTEQ(s32, p.max.X, -1);
	UASSERTEQ(s32, p.max.Y - p.min.Y + 1, 32);
	UASSERTEQ(s32, p.min.Y, -8);
	UASSERTEQ(u8, peel::facedir(1), 0);
	// a 2 m beam laid along x aimed off a quarter: centred on the nearest quarter; across, on its own 8-wide grid
	const peel::Box r = peel::lay(v3s16(0, 0, 0), v3s16(0, 1, 0), v3s16(9, 15, 6), peel::dims({ 32, 8, 4 }, { 0, 2, 1 }), false);
	UASSERTEQ(s32, r.min.X, -8);
	UASSERTEQ(s32, r.max.X, 23);
	UASSERTEQ(s32, r.min.Z, 0);
}

void TestPeel::testSnapsToWhatsNear()
{
	// a 2 m post, 4 × 4, at x 6..9
	const record::Member post{ "core:post_2m_birch", { v3s32(6, 0, 6), v3s32(9, 31, 9) } };
	// a beam laid beside it, its end 2 short of the post's far side: flush with it
	peel::Box beam{ v3s32(-20, 16, 10), v3s32(11, 19, 13) };
	peel::Box b = snap::align(beam, { post }, 1);
	UASSERTEQ(s32, b.max.X, 9);
	UASSERTEQ(s32, b.min.Z, 10); // already butted against its side
	// far off it: left where it's aimed
	beam = peel::Box{ v3s32(-60, 16, 40), v3s32(-29, 19, 43) };
	b = snap::align(beam, { post }, 1);
	UASSERTEQ(s32, b.min.X, -60);
	// resting: one butted against the post's side touches it; one a sixteenth off doesn't
	const auto post_at = [&](v3s32 c) { return record::meets(post.box, peel::Box{ c, c }); };
	UASSERT(record::touches(peel::Box{ v3s32(10, 8, 6), v3s32(41, 11, 9) }, post_at));
	UASSERT(!record::touches(peel::Box{ v3s32(11, 8, 6), v3s32(42, 11, 9) }, post_at));
	// a board through it, looked at from −z: moved toward them till it's on its face
	const peel::Box board = snap::clear(peel::Box{ v3s32(-10, 8, 7), v3s32(21, 11, 7) }, { post }, v3s16(0, 0, -1));
	UASSERTEQ(s32, board.max.Z, 5);
}

void TestPeel::testStacksInRows()
{
	// 1 m planks lying along x: four a layer across z, then the next layer up
	const v3s32 base(16, 32, -16), dims(16, 1, 4);
	std::vector<peel::Box> taken;
	for (int i = 0; i < 5; i++)
		taken.push_back(stack::next(base, dims, taken));
	UASSERT(taken[0].min == base);
	UASSERT(taken[3].min == base + v3s32(0, 0, 12));
	UASSERT(taken[4].min == base + v3s32(0, 1, 0));
	// taken from the top: the one on the first is the fifth; its stack's base is the first's
	UASSERT(stack::above(taken[0], taken)->min == taken[4].min);
	UASSERT(!stack::above(taken[4], taken));
	UASSERT(stack::base(taken[4], taken) == base);
	// a gap left by one taken is filled first
	taken.erase(taken.begin() + 1);
	UASSERT(stack::next(base, dims, taken).min == base + v3s32(0, 0, 4));
}
