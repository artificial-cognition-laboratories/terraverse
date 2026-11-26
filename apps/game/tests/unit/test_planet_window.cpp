// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/server/world/load/database/database-planet.h"
#include "src/server/world/load/database/database-sqlite3.h"
#include "src/shared/platform/system/filesys.h"
#include "library/world/planet/pager.h"
#include <cmath>

/*
	A planet's window as the server keeps it (planning/sprint-39-tiles.md):
	the pager putting tiles in and taking them out, and the blocks of a tile
	kept where they are on the planet, whatever slot it's in.
*/
class TestPlanetWindow : public TestBase {
public:
	TestPlanetWindow() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanetWindow"; }

	void runTests(IGameDef *gamedef);

	void testPagerFillsThenTakesOut();
	void testPagerSkipsWhatsInUse();
	void testPagerFull();
	void testPagerKeepsRoundPlayers();
	void testBlocksFollowTheirTile();
};

static TestPlanetWindow g_test_instance;

void TestPlanetWindow::runTests(IGameDef *gamedef)
{
	TEST(testPagerFillsThenTakesOut);
	TEST(testPagerSkipsWhatsInUse);
	TEST(testPagerFull);
	TEST(testPagerKeepsRoundPlayers);
	TEST(testBlocksFollowTheirTile);
}

namespace {

using planet::Atlas;
const terragen::Atlas::Params EARTH = terragen::Atlas::standard(6371000);
constexpr int SLOTS = Atlas::SLOTS * Atlas::SLOTS;

// The n-th tile along segment 0's equator row, east.
Atlas::TileId nth(int n)
{
	return { Atlas::HOME, 0, n - 200, 0 };
}

struct Watched
{
	std::vector<int> emptied, moved;
	std::function<bool(int)> canEmpty = [](int) { return true; };
	planet::Pager::Hooks hooks()
	{
		return { [this](int s) { emptied.push_back(s); return canEmpty(s); }, [this](int s) { moved.push_back(s); } };
	}
};

} // namespace

void TestPlanetWindow::testPagerFillsThenTakesOut()
{
	Atlas a(EARTH);
	Watched w;
	planet::Pager pager(a, w.hooks());
	// every slot filled, none emptied
	for (int n = 0; n < SLOTS; n++)
		UASSERTEQ(int, pager.need(nth(n), n * 0.01), n);
	UASSERT(w.emptied.empty());
	UASSERTEQ(size_t, w.moved.size(), (size_t)SLOTS);
	// needing a tile again keeps it
	UASSERTEQ(int, pager.need(nth(0), 10), 0);
	// past HOLD: the one needed longest ago goes (tile 1, slot 1: 0 was needed again)
	const double later = 10 + planet::Pager::HOLD - 1;
	UASSERTEQ(int, pager.need(nth(SLOTS), later + 1), 1);
	UASSERT(w.emptied.back() == 1);
	UASSERT(!a.slotOf(nth(1)));
	UASSERT(*a.slotOf(nth(SLOTS)) == 1);
}

void TestPlanetWindow::testPagerSkipsWhatsInUse()
{
	Atlas a(EARTH);
	Watched w;
	w.canEmpty = [](int s) { return s != 0; };
	planet::Pager pager(a, w.hooks());
	for (int n = 0; n < SLOTS; n++)
		pager.need(nth(n), 0);
	// slot 0 is the oldest but its blocks are in use: slot 1 goes
	UASSERTEQ(int, pager.need(nth(SLOTS), 100), 1);
	UASSERT(*a.slotOf(nth(0)) == 0);
}

void TestPlanetWindow::testPagerFull()
{
	Atlas a(EARTH);
	Watched w;
	planet::Pager pager(a, w.hooks());
	for (int n = 0; n < SLOTS; n++)
		pager.need(nth(n), 0);
	// every tile needed a moment ago
	EXCEPTION_CHECK(std::runtime_error, pager.need(nth(SLOTS), 1));
	// and none taken out
	UASSERT(w.emptied.empty());
}

void TestPlanetWindow::testPagerKeepsRoundPlayers()
{
	Atlas a(EARTH);
	Watched w;
	planet::Pager pager(a, w.hooks());
	const double lon = 97.5 * M_PI / 180, r = EARTH.radius;
	const terragen::Atlas::Vec here{ r * std::cos(lon), 0, r * std::sin(lon) };
	pager.keep({ { Atlas::HOME, here } }, 0);
	const Atlas::Cell c(Atlas::HOME, a.body(planet::Atlas::HOME).locate(here));
	UASSERT(a.slotOf(a.tileOf(c)));
	// the player's ground has a node, and it's the cell
	UASSERT(*a.cell(a.node(c)) == c);
	UASSERT(a.tiles().size() >= 1 && a.tiles().size() <= 4);
}

// A block saved while its tile is in one slot loads from another; seam
// between slots is never kept.
void TestPlanetWindow::testBlocksFollowTheirTile()
{
	const std::string dir = getTestTempDirectory() + DIR_DELIM + "planet_window";
	fs::RecursiveDelete(dir);
	fs::CreateAllDirs(dir);
	Atlas a(EARTH);
	const Atlas::TileId t = nth(3);
	a.put(5, t);
	{
		MapDatabasePlanet db(std::make_unique<MapDatabaseSQLite3>(dir), a, [](int b) -> std::unique_ptr<MapDatabase> { throw std::logic_error("a one-body atlas opened body " + std::to_string(b)); });
		db.beginSave();
		// a block of slot 5: its corner, 3 blocks in
		const s16 bx = (Atlas::ORIGIN + 5 * Atlas::SLOT) / 16 + 3, bz = Atlas::ORIGIN / 16 + 7;
		UASSERT(db.saveBlock(v3s16(bx, -2, bz), "tile three"));
		// the seam block after slot 5: not kept
		UASSERT(db.saveBlock(v3s16((Atlas::ORIGIN + 5 * Atlas::SLOT + Atlas::TILE) / 16, -2, bz), "seam"));
		db.endSave();
	}
	// the tile moves to slot 220
	a.put(5, std::nullopt);
	a.put(220, t);
	MapDatabasePlanet db(std::make_unique<MapDatabaseSQLite3>(dir), a, [](int b) -> std::unique_ptr<MapDatabase> { throw std::logic_error("a one-body atlas opened body " + std::to_string(b)); });
	std::string got;
	const s16 bx = (Atlas::ORIGIN + (220 % Atlas::SLOTS) * Atlas::SLOT) / 16 + 3;
	const s16 bz = (Atlas::ORIGIN + (220 / Atlas::SLOTS) * Atlas::SLOT) / 16 + 7;
	db.loadBlock(v3s16(bx, -2, bz), &got);
	UASSERTEQ(std::string, got, "tile three");
	// slot 5 is empty now: nothing there
	db.loadBlock(v3s16((Atlas::ORIGIN + 5 * Atlas::SLOT) / 16 + 3, -2, Atlas::ORIGIN / 16 + 7), &got);
	UASSERT(got.empty());
	// the seam wasn't kept anywhere: the only row is the tile's
	db.loadBlock(v3s16((Atlas::ORIGIN + 5 * Atlas::SLOT + Atlas::TILE) / 16, -2, Atlas::ORIGIN / 16 + 7), &got);
	UASSERT(got.empty());
	fs::RecursiveDelete(dir);
}
