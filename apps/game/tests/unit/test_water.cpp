// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "library/matter/fill/fill.h"
#include "library/world/planet/atlas.h"
#include "library/world/terragen/src/world.h"
#include "src/server/world/generate/land/terra_column.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include <cmath>
#include <optional>

/*
	The water a world is generated with (planning/sprint-122-water.md): a
	river's top node holds the sixteenths its surface reaches, so it slopes;
	and a cave by a river is full under the river's water when it's made, so
	nothing fills in front of the player as they come near.
*/
class TestWater : public TestBase {
public:
	TestWater() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestWater"; }

	void runTests(IGameDef *gamedef);

	void testRiverTopHoldsItsPart();
	void testCaveByRiverIsWet();
	void testHeldCornersMeet();
	void testFallStaysAStep();
	void testGridRampsBySixteenths();
	void testWaterMovesWithItsLevel();
	void testRiverAtRestKeepsToItsBanks();
	void testRiseBlendsBetweenBlocks();
};

static TestWater g_test_instance;

void TestWater::runTests(IGameDef *gamedef)
{
	TEST(testRiverTopHoldsItsPart);
	TEST(testCaveByRiverIsWet);
	TEST(testHeldCornersMeet);
	TEST(testFallStaysAStep);
	TEST(testGridRampsBySixteenths);
	TEST(testWaterMovesWithItsLevel);
	TEST(testRiverAtRestKeepsToItsBanks);
	TEST(testRiseBlendsBetweenBlocks);
}

namespace {

const terragen::Atlas::Params ATLAS = terragen::Atlas::standard(8192 / (2 * M_PI));
const s16 WATER_LEVEL = 1;
const content_t STONE = 10, WATER = 11, AIR_ = CONTENT_AIR;

// Made-up block ids: only water and air are looked at
terra::Blocks blocks()
{
	terra::Blocks b;
	content_t next = 100;
	for (terra::BiomeBlocks &bb : b.biomes)
		bb = { next++, next++, next++, CONTENT_IGNORE, 1, 3, 1, 0 };
	b.stone = STONE;
	b.water = WATER;
	b.lava = 13;
	b.placer = 14;
	b.soil = 15;
	for (auto &kinds : b.soils)
		for (content_t &c : kinds)
			c = next++;
	for (content_t &c : b.rock)
		c = next++;
	b.ore[0] = CONTENT_IGNORE;
	for (size_t i = 1; i < std::size(b.ore); i++)
		b.ore[i] = next++;
	return b;
}

struct Land
{
	terragen::World world{ terragen::WorldCode::decode("TF24.dGVycmE") };
	planet::Atlas atlas = planet::Atlas::resident(ATLAS);
	terra::Blocks b = blocks();
	terra::Columns columns{ world, b, WATER_LEVEL, atlas, planet::Atlas::HOME };

	// x east from longitude 0, z north from the equator, in blocks
	terra::Column at(s32 x, s32 z) const
	{
		const s32 size = world.code().size, half = size / 2;
		return columns.atPlace(terragen::World::place(terragen::ratio((((x + half) % size) + size) % size, size),
				terragen::ratio(half - z, size)));
	}
	// a grid river's column along a few parallels, wide enough to have banks
	std::optional<std::pair<s32, s32>> river() const
	{
		const s32 size = world.code().size;
		for (s32 z : { size / 16, -size / 16, size / 8, -size / 8, 0 })
			for (s32 x = 0; x < size; x += 3) {
				const terra::Column c = at(x, z);
				const terragen::Ground &g = c.ground;
				if (g.kind == terragen::Water::River && g.river_cell >= 0 && g.river_width >= terragen::toFix(3) &&
						g.water > 2)
					return std::pair{ x, z };
			}
		return std::nullopt;
	}
};

} // namespace

// Each river column's top node is held water filled to its column's part,
// with air over it; every node under it is held water, full
void TestWater::testRiverTopHoldsItsPart()
{
	const Land land;
	const auto found = land.river();
	UASSERT(found);
	int columns = 0, parted = 0;
	for (s32 dz = -24; dz <= 24; dz++)
		for (s32 dx = -24; dx <= 24; dx++) {
			const terra::Column c = land.at(found->first + dx, found->second + dz);
			if (c.ground.kind != terragen::Water::River || c.water <= c.height)
				continue;
			columns++;
			const MapNode top = land.columns.node(c, c.water);
			UASSERTEQ(content_t, top.getContent(), WATER);
			UASSERT(top.getParam2() & fill::HELD);
			UASSERTEQ(int, fill::partOf(top.getParam2()), c.ground.water_part);
			parted += c.ground.water_part < 16;
			UASSERTEQ(content_t, land.columns.node(c, c.water + 1).getContent(), AIR_);
			for (s32 y = c.height + 1; y < c.water; y++)
				UASSERTEQ(int, fill::partOf(land.columns.node(c, y).getParam2()), 16);
		}
	infostream << "water: " << columns << " river columns, " << parted << " with a top node part full" << std::endl;
	UASSERT(columns > 20);
	UASSERT(parted > 0);
}

// Under the ground by a river, under its water's level, no cave is air:
// what's carved there is held water, at the river's own level at most
void TestWater::testCaveByRiverIsWet()
{
	const Land land;
	const auto found = land.river();
	UASSERT(found);
	int tabled = 0, wet = 0;
	for (s32 dz = -40; dz <= 40; dz += 2)
		for (s32 dx = -40; dx <= 40; dx += 2) {
			const terra::Column c = land.at(found->first + dx, found->second + dz);
			if (c.table == INT32_MIN)
				continue;
			tabled++;
			UASSERT(c.table < WATER_LEVEL + c.ground.river_level); // under the river's top node
			for (s32 y = std::min(c.table, c.height); y > c.table - 80; y--) {
				const MapNode n = land.columns.node(c, y);
				UASSERT(n.getContent() != AIR_);
				wet += y < c.height && n.getContent() == WATER;
			}
		}
	infostream << "water: " << tabled << " columns by a river, " << wet << " cave nodes under it wet" << std::endl;
	UASSERT(tabled > 100);
	UASSERT(wet > 0); // (caves there to be wet: else the test shows nothing)
}

namespace {
// Levels round node (x, z) of a field given as a function
template <class F>
liquids::held::Around aroundOf(int x, int z, F level)
{
	liquids::held::Around a;
	for (int dz = -1; dz <= 1; dz++)
		for (int dx = -1; dx <= 1; dx++)
			a[dz + 1][dx + 1] = level(x + dx, z + dz);
	return a;
}
} // namespace

// A river falling 5 sixteenths a node along x, bending along z: wherever two
// nodes touch, their corners there are the same heights, so the water's one
// sheet, never a ledge between nodes
void TestWater::testHeldCornersMeet()
{
	const auto level = [](int x, int z) { return s32(1000 - 5 * x - 3 * z + ((x * z) & 1)); };
	for (int z = 0; z < 6; z++)
		for (int x = 0; x < 6; x++) {
			const auto here = liquids::held::corners(aroundOf(x, z, level));
			const auto east = liquids::held::corners(aroundOf(x + 1, z, level));
			const auto north = liquids::held::corners(aroundOf(x, z + 1, level));
			UASSERT(here[1] == east[0] && here[3] == east[2]);   // the +x edge
			UASSERT(here[2] == north[0] && here[3] == north[1]); // the +z edge
			// and the node's own field stands about its level
			UASSERT(std::fabs(liquids::held::at(here, 0.5f, 0.5f) - level(x, z)) < 5);
		}
}

// A fall: past x = 3 the river's 2 blocks lower. The nodes either side keep
// their own corners at the lip (a step, drawn as one), not a ramp between
void TestWater::testFallStaysAStep()
{
	const auto level = [](int x, int) { return s32(x <= 3 ? 500 : 500 - 32); };
	const auto above = liquids::held::corners(aroundOf(3, 0, level));
	const auto below = liquids::held::corners(aroundOf(4, 0, level));
	UASSERT(above[1] == 500 && above[3] == 500);
	UASSERT(below[0] == 468 && below[2] == 468);
	// and with no held water beside it (a bank), a node stands at its own level
	const auto alone = liquids::held::corners(aroundOf(0, 0, [](int x, int z) {
		return x == 0 && z == 0 ? s32(300) : liquids::NONE;
	}));
	for (float c : alone)
		UASSERT(c == 300);
}

// The grid's columns across a sloped river: each stands within a sixteenth
// of the one beside it (a ramp, as the ground's sand), and none past its
// nodes' levels (it's the river's own surface, not a guess beyond it)
void TestWater::testGridRampsBySixteenths()
{
	liquids::Grid g({ /*res*/ 16, /*nodes*/ 8, /*follow*/ 4, /*scans*/ 4 });
	g.place(v3f(0, 0, 0));
	for (liquids::Tile &t : g.tile)
		t.known = true;
	// every node a river falling 5 sixteenths a node along x, its bed two blocks under
	for (int nz = 0; nz < g.nodes; nz++)
		for (int nx = 0; nx < g.nodes; nx++)
			g.levels[nz * g.nodes + nx] = 800 - 5 * nx;
	for (int c = 0; c < g.cols * g.cols; c++)
		g.floor[c] = 700;
	for (int t = 0; t < g.tiles * g.tiles; t++)
		g.relevel(t);
	int steepest = 0;
	for (int z = 0; z < g.cols; z++)
		for (int x = 0; x + 1 < g.cols; x++) {
			const int c = z * g.cols + x;
			UASSERT(g.held(c) && g.held(c + 1));
			steepest = std::max(steepest, std::abs(g.sea[c + 1] - g.sea[c]));
			UASSERT(g.sea[c] <= 800 && g.sea[c] >= 800 - 5 * g.nodes);
			// its water first found stands at that level
			UASSERT(std::fabs(g.surface(c) - g.sea[c] / 16.0f) < 1e-4f);
		}
	UASSERTEQ(int, steepest, 1);
}

namespace {
// A grid over a flat river: every node held at `level`, its bed at 700
liquids::Grid riverGrid(s32 level)
{
	liquids::Grid g({ /*res*/ 16, /*nodes*/ 8, /*follow*/ 4, /*scans*/ 4 });
	g.place(v3f(0, 0, 0));
	for (liquids::Tile &t : g.tile)
		t.known = true;
	for (s32 &l : g.levels)
		l = level;
	for (s32 &f : g.floor)
		f = 700;
	for (int t = 0; t < g.tiles * g.tiles; t++)
		g.relevel(t);
	return g;
}
} // namespace

// A level found again (its neighbours scanned, its river risen) moves the
// water with it, keeping what's disturbed of it: never left standing off its
// level to settle (seen as slabs that stood till something stirred them)
void TestWater::testWaterMovesWithItsLevel()
{
	liquids::Grid g = riverGrid(800);
	const int c = 5 * g.cols + 5;
	UASSERT(std::fabs(g.off(c)) < 1e-4f);
	g.depth[c] += 0.25f; // a wake a quarter of a node high
	for (s32 &l : g.levels)
		l = 790;
	for (int t = 0; t < g.tiles * g.tiles; t++)
		g.relevel(t);
	UASSERT(std::fabs(g.surface(c) - (790 / 16.0f + 0.25f)) < 1e-4f);
	UASSERT(std::fabs(g.off(c) - 0.25f) < 1e-4f);
	UASSERT(std::fabs(g.off(c + 1)) < 1e-4f);
}

// Held water beside a bank lower than it: at rest nothing drives it over (the
// river's level is its own, not a head); only what's over its level runs off
void TestWater::testRiverAtRestKeepsToItsBanks()
{
	liquids::Grid g = riverGrid(800);
	const int c = 5 * g.cols + 5, bank = c + 1;
	g.sea[bank] = liquids::NONE; // not the river's: ground a block under its surface, dry
	g.floor[bank] = 784;
	g.depth[bank] = 0;
	UASSERT(g.held(c) && !g.held(bank));
	UASSERT(g.higher(c, bank) <= 0);
	g.depth[c] += 0.125f;
	UASSERT(std::fabs(g.higher(c, bank) - 0.125f) < 1e-4f);
}

// A river's rise blended between its map blocks' middles: at a block's middle
// its own, halfway to the next the mean, and a block with no river not counted
void TestWater::testRiseBlendsBetweenBlocks()
{
	liquids::Rivers rivers;
	rivers[v3s16(0, 0, 0)].rise = 0;
	rivers[v3s16(1, 0, 0)].rise = 8;
	rivers[v3s16(1, 0, 1)].rise = 8;
	UASSERT(std::fabs(liquids::riseAt(rivers, v3s16(7, 3, 7))) < 1e-4f); // (by its middle: its own)
	const float edge = liquids::riseAt(rivers, v3s16(15, 3, 7)), across = liquids::riseAt(rivers, v3s16(16, 3, 7));
	UASSERT(edge > 0 && edge < 8 && across > edge && across < 8); // a slope across the edge, not a step
	UASSERT(std::fabs(liquids::riseAt(rivers, v3s16(24, 3, 24)) - 8) < 1e-4f);
	// a river crossing a block's floor: the block under counted
	UASSERT(std::fabs(liquids::riseAt(rivers, v3s16(24, 16, 8)) - 8) < 1e-4f);
	UASSERT(liquids::riseAt(liquids::Rivers{}, v3s16(3, 3, 3)) == 0);
}
