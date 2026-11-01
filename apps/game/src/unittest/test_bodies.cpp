// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "database/database-planet.h"
#include "database/database-sqlite3.h"
#include "filesys.h"
#include "planet/atlas.h"
#include "shell/preview.h"
#include "terragen/system.h"
#include "terragen/worlds.h"
#include "terragen/world.h"
#include <cmath>
#include <cstdio>
#include <set>

/*
	A world's bodies (planning/sprint-40-bodies.md): which can be stood on
	at each size, and their tiles sharing one window: each its own body's,
	never another's, its blocks kept in its body's database.
*/
class TestBodies : public TestBase {
public:
	TestBodies() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestBodies"; }

	void runTests(IGameDef *gamedef);

	void testWhichBodies();
	void testTilesOfTwoBodies();
	void testBlocksKeptByBody();
	void testAPreviewOfABody();
	void testABodysLandIsItsKind();
};

static TestBodies g_test_instance;

void TestBodies::runTests(IGameDef *gamedef)
{
	TEST(testWhichBodies);
	TEST(testTilesOfTwoBodies);
	TEST(testBlocksKeptByBody);
	TEST(testAPreviewOfABody);
	TEST(testABodysLandIsItsKind);
}

namespace {

using planet::Atlas;

const terragen::WorldCode HOME_CODE = terragen::WorldCode::decode("TF7.dGVycmE");

std::vector<terragen::BodyWorld> worldsAt(double radius)
{
	return terragen::bodyWorlds(HOME_CODE, terragen::Atlas::standard(radius));
}

} // namespace

// The home first; then every body with a surface big enough, at the home's
// scale, each with its own code text and seed.
void TestBodies::testWhichBodies()
{
	UASSERTEQ(size_t, worldsAt(2607.59).size(), 1); // the smallest world: its moon is too small
	const std::vector<terragen::BodyWorld> earth = worldsAt(6371000);
	const terragen::System system(terragen::SystemSpec::forWorld(HOME_CODE));
	UASSERT(earth[0].system == system.playable());
	std::set<std::string> ids, texts, seeds;
	for (const terragen::BodyWorld &b : earth) {
		std::printf("BODY %s radius=%.0f size=%d\n", b.id.c_str(), b.atlas.radius, b.code.size);
		UASSERT(!system.cards()[b.system].giant);
		UASSERT(b.atlas.radius >= terragen::SMALLEST_BODY);
		ids.insert(b.id);
		texts.insert(b.code.text);
		seeds.insert(std::to_string(b.code.seed_hash));
		// at the home's scale
		UASSERT(std::abs(b.atlas.radius / 6371000 - system.radius(b.system) / system.radius(system.playable())) < 1e-9);
	}
	UASSERT(ids.size() == earth.size() && texts.size() == earth.size() && seeds.size() == earth.size());
	UASSERT(ids.count("moon"));
	UASSERT(!ids.count("jovian"));
	// one atlas holds them all
	std::vector<terragen::Atlas::Params> params;
	for (const terragen::BodyWorld &b : earth)
		params.push_back(b.atlas);
	UASSERTEQ(int, Atlas(params).bodies(), (int)earth.size());
}

// The same square of two bodies' grids is two tiles: each stores its own
// body's cells, its window code says whose, and neither's frame sees the other.
void TestBodies::testTilesOfTwoBodies()
{
	const std::vector<terragen::BodyWorld> w = worldsAt(250000);
	Atlas a({ w[0].atlas, w[1].atlas });
	const Atlas::TileId home(0, 0, 0, 0), other(1, 0, 0, 0);
	a.put(10, home);
	a.put(11, other);
	UASSERT(*a.slotOf(home) == 10 && *a.slotOf(other) == 11);
	EXCEPTION_CHECK(std::invalid_argument, a.put(12, other)); // a tile is in one slot
	const auto cornerOf = [](int slot) {
		return Atlas::Node{ Atlas::ORIGIN + (slot % Atlas::SLOTS) * Atlas::SLOT + 100, 0,
			Atlas::ORIGIN + (slot / Atlas::SLOTS) * Atlas::SLOT + 100 };
	};
	const std::optional<Atlas::Cell> h = a.cell(cornerOf(10)), o = a.cell(cornerOf(11));
	UASSERT(h && o && h->body == 0 && o->body == 1);
	UASSERT(h->i == o->i && h->j == o->j && h->k == o->k && !(*h == *o));
	UASSERT(*a.bodyAt(cornerOf(11)) == 1);
	UASSERT(a.spot(cornerOf(11))->body == 1);
	// each cell's node is in its own body's slot
	UASSERT(*Atlas::slotAt(a.node(*o)) == 11);
	// the window says whose each tile is
	Atlas b({ w[0].atlas, w[1].atlas });
	b.setWindow(a.windowCode());
	UASSERT(b.windowCode() == a.windowCode());
	UASSERT(*b.slotOf(other) == 11);
	// a block of one body seen from the other: nothing to see
	const std::optional<Atlas::Frame> fh = a.frame(cornerOf(10), { 0, 0, 0 }), fo = a.frame(cornerOf(11), { 0, 0, 0 });
	UASSERT(fh && fo && fh->body == 0 && fo->body == 1);
	EXCEPTION_CHECK(std::invalid_argument, Atlas::view(*fh, *fo));
	EXCEPTION_CHECK(std::invalid_argument, a.body(2));
}

// A block of another body is saved in that body's database, opened when
// it's first needed; the home's has none of it.
void TestBodies::testBlocksKeptByBody()
{
	const std::string dir = getTestTempDirectory() + DIR_DELIM + "bodies";
	fs::RecursiveDelete(dir);
	fs::CreateAllDirs(dir + DIR_DELIM + "moon");
	const std::vector<terragen::BodyWorld> w = worldsAt(250000);
	Atlas a({ w[0].atlas, w[1].atlas });
	a.put(10, Atlas::TileId(0, 0, 0, 0));
	a.put(11, Atlas::TileId(1, 0, 0, 0));
	std::vector<int> opened;
	const auto open = [&](int body) -> std::unique_ptr<MapDatabase> {
		opened.push_back(body);
		return std::make_unique<MapDatabaseSQLite3>(dir + DIR_DELIM + "moon");
	};
	const auto blockIn = [](int slot) {
		return v3s16((Atlas::ORIGIN + (slot % Atlas::SLOTS) * Atlas::SLOT) / 16 + 3, -2,
				(Atlas::ORIGIN + (slot / Atlas::SLOTS) * Atlas::SLOT) / 16 + 7);
	};
	{
		MapDatabasePlanet db(std::make_unique<MapDatabaseSQLite3>(dir), a, open);
		db.beginSave();
		UASSERT(db.saveBlock(blockIn(10), "home"));
		UASSERT(opened.empty());
		UASSERT(db.saveBlock(blockIn(11), "moon"));
		UASSERT(opened.size() == 1 && opened[0] == 1);
		db.endSave();
	}
	// the same square of each: each body's own block
	MapDatabasePlanet db(std::make_unique<MapDatabaseSQLite3>(dir), a, open);
	std::string got;
	db.loadBlock(blockIn(10), &got);
	UASSERTEQ(std::string, got, "home");
	db.loadBlock(blockIn(11), &got);
	UASSERTEQ(std::string, got, "moon");
	// both are at one spot (the same square of each grid): the home's
	// database holds the home's block there, not the moon's
	const Atlas::Spot spot = *a.blockSpot({ blockIn(11).X, blockIn(11).Y, blockIn(11).Z });
	UASSERT(spot.body == 1);
	MapDatabaseSQLite3 home(dir);
	home.loadBlockAt(v3s32(spot.x, spot.y, spot.z), &got);
	UASSERTEQ(std::string, got, "home");
	fs::RecursiveDelete(dir);
}

// The map of a body is its own world's: the moon's preview isn't the home's,
// and it's the same each time it's asked.
void TestBodies::testAPreviewOfABody()
{
	const std::vector<terragen::BodyWorld> w = worldsAt(250000);
	Json::Value intent;
	intent["code"] = HOME_CODE.text;
	intent["resolution"] = 64;
	const Json::Value home = shell::previewJson(intent);
	const Json::Value moon = shell::previewJson(intent, w[3].code);
	const Json::Value again = shell::previewJson(intent, w[3].code);
	UASSERT(w[3].id == "moon");
	UASSERT(moon["elevation"].asString() != home["elevation"].asString());
	UASSERT(moon["elevation"].asString() == again["elevation"].asString());
	// and the home's asked by its code is the home's world
	UASSERT(shell::previewJson(intent, w[0].code)["elevation"].asString() != moon["elevation"].asString());
}

// The home lives (seas, plants); the moon is airless: bare rock all over,
// its basins dry, nothing growing; the icy moons are ice.
void TestBodies::testABodysLandIsItsKind()
{
	const std::vector<terragen::BodyWorld> w = worldsAt(250000);
	const auto survey = [](const terragen::WorldCode &code, int &water, int &grows, std::set<int> &biomes) {
		const terragen::World world(code);
		water = grows = 0;
		for (int i = 0; i < 400; i++) {
			// within ±30°
			const terragen::Place at = terragen::World::place(terragen::ratio(i % 20, 20),
					terragen::HALF + terragen::ratio(1, 6) - terragen::ratio(i / 20, 60));
			const terragen::Ground g = world.ground(at, world.sunlight(at, std::nullopt));
			water += g.kind != terragen::Water::None && g.kind != terragen::Water::Lava;
			grows += world.cover(at, g).density > 0;
			biomes.insert((int)g.column.biome);
		}
	};
	int water, grows;
	std::set<int> biomes;
	survey(w[0].code, water, grows, biomes);
	UASSERT(w[0].code.surface == terragen::Surface::Living);
	UASSERT(water > 0 && grows > 0 && biomes.size() > 1);
	UASSERT(w[3].id == "moon" && w[3].code.surface == terragen::Surface::Rock);
	biomes.clear();
	survey(w[3].code, water, grows, biomes);
	UASSERT(water == 0 && grows == 0);
	UASSERT(biomes == std::set<int>{ (int)terragen::Biome::Rock });
	for (const terragen::BodyWorld &b : w)
		if (b.id == "nacre")
			UASSERT(b.code.surface == terragen::Surface::Ice);
}
