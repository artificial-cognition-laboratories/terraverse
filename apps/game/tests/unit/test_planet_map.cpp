// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/shared/map/dummymap.h"
#include "library/world/planet/atlas.h"
#include <cmath>
#include <limits>

/*
	The engine's Map on a planet: what it answers across an atlas's seams is
	the atlas's (planet::Atlas), and how far apart two places are is through
	the planet, not across the map's storage.
*/
class TestPlanetMap : public TestBase {
public:
	TestPlanetMap() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanetMap"; }

	void runTests(IGameDef *gamedef);

	void testNextFlat(IGameDef *gamedef);
	void testNextIsTheAtlasStep(IGameDef *gamedef);
	void testSeeIsTheAtlas(IGameDef *gamedef);
	void testDistanceThroughThePlanet(IGameDef *gamedef);
};

static TestPlanetMap g_test_instance;

void TestPlanetMap::runTests(IGameDef *gamedef)
{
	TEST(testNextFlat, gamedef);
	TEST(testNextIsTheAtlasStep, gamedef);
	TEST(testSeeIsTheAtlas, gamedef);
	TEST(testDistanceThroughThePlanet, gamedef);
}

namespace {

// A small planet: few, wide segments and a thin sky.
const planet::Atlas SMALL = planet::Atlas::resident({ 300, 12, 80, 60, 0.95, 32 });

// The last surface node of each region's middle row, east: one step on is a seam.
std::vector<v3s16> eastEdges()
{
	std::vector<v3s16> out;
	for (const planet::Atlas::Tile &r : SMALL.tiles())
		for (int32_t x = r.rect.x + r.rect.width - 1; x > r.rect.x; x--) {
			const v3s16 n(x, 0, r.rect.z + r.rect.depth / 2);
			if (SMALL.cell({ n.X, n.Y, n.Z })) {
				out.push_back(n);
				break;
			}
		}
	return out;
}

// A map holding nothing but the planet: Map::next, see and distance read only it.
std::unique_ptr<DummyMap> planetMap(IGameDef *gamedef)
{
	auto map = std::make_unique<DummyMap>(gamedef, v3s16(0, 0, 0), v3s16(0, 0, 0));
	map->setAtlas(&SMALL);
	return map;
}

} // namespace

void TestPlanetMap::testNextFlat(IGameDef *gamedef)
{
	DummyMap map(gamedef, v3s16(0, 0, 0), v3s16(0, 0, 0));
	const Map::Next n = map.next(v3s16(3, 4, 5), v3s16(0, 0, -1));
	UASSERT(n.pos == v3s16(3, 4, 4) && n.turn == 0);
	EXCEPTION_CHECK(BaseException, map.next(v3s16(), v3s16(1, 1, 0)));
}

// Over a seam the map's next node is the atlas's step; inside a grid, p + dir.
void TestPlanetMap::testNextIsTheAtlasStep(IGameDef *gamedef)
{
	const auto map = planetMap(gamedef);
	size_t crossed = 0;
	for (const v3s16 n : eastEdges()) {
		const Map::Next next = map->next(n, v3s16(1, 0, 0));
		const std::optional<planet::Atlas::Step> s = SMALL.step({ n.X, n.Y, n.Z }, 0, 1);
		if (s) {
			crossed++;
			UASSERT(next.pos == v3s16(s->node.x, s->node.y, s->node.z) && next.turn == s->turn);
		} else {
			UASSERT(next.pos == n + v3s16(1, 0, 0) && next.turn == 0);
		}
		const Map::Next inside = map->next(n, v3s16(-1, 0, 0));
		UASSERT(inside.pos == n - v3s16(1, 0, 0) && inside.turn == 0);
	}
	UASSERT(crossed > 0);
}

// Looking past a seam from a grid's node, the map sees what the atlas does.
void TestPlanetMap::testSeeIsTheAtlas(IGameDef *gamedef)
{
	const auto map = planetMap(gamedef);
	size_t across = 0;
	for (const v3s16 n : eastEdges()) {
		const v3s16 p = n + v3s16(2, 0, 0);
		const Map::Next seen = map->see(n, p);
		const std::optional<planet::Atlas::Seen> s = SMALL.see({ n.X, n.Y, n.Z }, { p.X, p.Y, p.Z });
		if (!s) {
			UASSERT(seen.pos == p && seen.turn == 0);
			continue;
		}
		across++;
		UASSERT(seen.pos == v3s16(s->node.x, s->node.y, s->node.z) && seen.turn == s->turn);
	}
	UASSERT(across > 0);
}

// Neighbours across a seam are within a node or so of each other (cut cells'
// centres can be nearer), wherever their regions are stored; past the
// atlas's top a position is out of every reach.
void TestPlanetMap::testDistanceThroughThePlanet(IGameDef *gamedef)
{
	const auto map = planetMap(gamedef);
	size_t crossed = 0;
	for (const v3s16 n : eastEdges()) {
		const std::optional<planet::Atlas::Step> s = SMALL.step({ n.X, n.Y, n.Z }, 0, 1);
		if (!s)
			continue;
		crossed++;
		const v3f a = intToFloat(n, BS), b = intToFloat(v3s16(s->node.x, s->node.y, s->node.z), BS);
		UASSERT(a.getDistanceFrom(b) > 16 * BS); // stored apart
		const f32 d = map->distance(a, b);
		UTEST(d > 0 && d < 2.0f * BS, "neighbours across a seam %f apart", d / BS);
	}
	UASSERT(crossed > 0);
	const planet::Atlas::Node seam = SMALL.seam();
	UASSERT(map->distance(intToFloat(eastEdges()[0], BS), intToFloat(v3s16(seam.x, seam.y, seam.z), BS)) ==
			std::numeric_limits<f32>::infinity());
}
