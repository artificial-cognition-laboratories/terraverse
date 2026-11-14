// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include <chrono>

#include "database/database-planet.h"
#include "database/database-sqlite3.h"
#include "filesys.h"
#include "planet/atlas.h"
#include "shell/preview.h"
#include "terragen/grid.h"
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
	void testAFullSystem();
	void testLidsByTheirCards();

	void testWhichBodies();
	void testTilesOfTwoBodies();
	void testBlocksKeptByBody();
	void testAPreviewOfABody();
	void testABodysLandIsItsKind();
	void testSurroundings();
};

static TestBodies g_test_instance;

void TestBodies::runTests(IGameDef *gamedef)
{
	TEST(testAFullSystem);
	TEST(testWhichBodies);
	TEST(testTilesOfTwoBodies);
	TEST(testBlocksKeptByBody);
	TEST(testAPreviewOfABody);
	TEST(testABodysLandIsItsKind);
	TEST(testSurroundings);
	TEST(testLidsByTheirCards);
}

namespace {

using planet::Atlas;

const terragen::WorldCode HOME_CODE = terragen::WorldCode::decode("TF23.dGVycmE");

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
	UASSERT(w[0].code.processes.life && w[0].code.processes.water && w[0].code.processes.regime == terragen::Regime::Plates);
	UASSERT(water > 0 && grows > 0 && biomes.size() > 1);
	UASSERT(w[3].id == "moon" && w[3].code.processes.bare == terragen::Surface::Regolith && !w[3].code.processes.water && w[3].code.processes.regime == terragen::Regime::Lid);
	biomes.clear();
	survey(w[3].code, water, grows, biomes);
	UASSERT(water == 0 && grows == 0);
	// regolith: light on the highlands, dark on the maria (sprint 95)
	for (int b : biomes)
		UASSERT(b == (int)terragen::Biome::Regolith || b == (int)terragen::Biome::Mare);
	{
		// a stagnant lid and no water: one plate, nothing worn, laid or pooled
		const auto moon = terragen::gridFor(w[3].code);
		for (int32_t i = 0; i < moon->size(); i++) {
			UASSERT(moon->tectonics.plate[i] == moon->tectonics.plate[0]);
			UASSERT(moon->worn[i] == 0 && moon->sediment[i] == 0 && !moon->lake[i] && !moon->stream(i));
		}
		UASSERT(moon->history.empty());
		// impacts (sprint 94): kept on a dry, still body, wide ones in the grid
		UASSERT(moon->craters.size() > 100 && moon->crater_from > 0);
		const terragen::Crater &c = moon->craters.back();
		const terragen::Place mid = c.at;
		UASSERT(terragen::Craters::shape(c, mid) < 0); // a pit in the middle
		UASSERT(c.rim > 0);
		// Earth keeps few (water and plates)
		const auto home = terragen::gridFor(w[0].code);
		UASSERT(home->craters.size() < moon->craters.size() / 20);

		// maria (sprint 95): the share the card floods, level, on the thin
		// side, with few craters after them
		const terragen::Processes &p = w[3].code.processes;
		UASSERT(p.flooding > 0 && p.young > 0 && p.young < terragen::ratio(1, 5));
		const terragen::Sphere &sphere = terragen::gridSphere();
		int64_t all = 0, flooded = 0;
		double thin[2] = {}, cells[2] = {};
		for (int32_t i = 0; i < moon->size(); i++) {
			const bool mare = moon->mare[i] > 0;
			all += sphere.area(i);
			flooded += mare ? sphere.area(i) : 0;
			thin[mare] += moon->thin[i];
			cells[mare]++;
			if (mare) // its pool's level, the young craters on it
				UASSERT(moon->base[i] == moon->mare_level[i] + moon->crater_young[i]);
		}
		const double share = (double)flooded / all, asked = p.flooding / 65536.0;
		UASSERT(share > asked - 0.01 && share < asked + 0.01);
		UASSERT(thin[1] / cells[1] > thin[0] / cells[0] + 0.15 * 65536);
		// craters made since: on the maria, a fraction of the highlands' density
		int on[2] = {};
		for (const terragen::Crater &c : moon->craters) {
			const int32_t i = sphere.cell(c.at.dir());
			if (moon->mare[i] > 0 ? c.young : true)
				on[moon->mare[i] > 0]++;
		}
		UASSERT(on[1] / cells[1] < on[0] / cells[0] / 4);

		// their rock: basalt as deep as the lava over the shattered crust;
		// the highlands' breccia over anorthosite
		const terragen::World moonWorld(w[3].code);
		int seen[2] = {};
		for (int k = 0; k < 400; k++) {
			const terragen::Place at = terragen::World::place(terragen::ratio(k % 20, 20), terragen::ratio(k / 20 * 2 + 1, 42));
			const terragen::Ground g = moonWorld.ground(at, moonWorld.sunlight(at, std::nullopt));
			const terragen::Strata s = moonWorld.strata(at, g);
			const terragen::Rock deep = moonWorld.rock(s, s.mare + s.breccia + 5, g.terrain - 400);
			UASSERT(deep == terragen::Rock::Anorthosite);
			// (lava laps onto the highlands' edge: a column's depth of it is
			// between its cells')
			UASSERT(moonWorld.rock(s, 0, g.terrain) == (s.mare > 0 ? terragen::Rock::Basalt : terragen::Rock::Breccia));
			UASSERT(s.province == terragen::Province::Mare || s.province == terragen::Province::Highland);
			seen[s.province == terragen::Province::Mare]++;
		}
		UASSERT(seen[0] > 0 && seen[1] > 0);
	}
	terragen::WorldCode hundred = terragen::WorldCode::decode("TF23.dGVycmE");
	hundred.size = 628320;
	// a body never warm enough for volcanism floods none
	for (const terragen::BodyWorld &b : w)
		if (b.id == "aster") {
			UASSERT(b.code.processes.flooding == 0);
			const auto g = terragen::gridFor(b.code);
			for (terragen::fix m : g->mare)
				UASSERT(m == 0);
		}
	for (const terragen::BodyWorld &b : w)
		if (b.id == "nacre")
			UASSERT(b.code.processes.bare == terragen::Surface::Ice && !b.code.processes.life);
	// every body's land stays land-sized, the low-gravity ones too (a crater's
	// size overflowed under a tenth of a g: holes and spikes tens of km high);
	// an ice crust is ice, cracked as hard as its tides flex it
	for (const terragen::BodyWorld &b : terragen::bodyWorlds(hundred, terragen::Atlas::standard(terragen::World::radiusOf(hundred) / 65536.0))) {
		const terragen::World world(b.code);
		const bool icy = b.code.processes.stiff < terragen::ONE;
		for (int i = 0; i < 400; i++) {
			const terragen::Place at = terragen::World::place((terragen::fix)(i * 163 % 65536), (terragen::fix)(i * 7919 % 65536));
			const terragen::Ground g = world.ground(at, 0);
			UASSERT(std::abs(g.terrain) < 4000);
			if (icy)
				UASSERT(world.rock(world.strata(at, g), 0, g.terrain) == terragen::Rock::Ice);
		}
	}
}

// Each body's gravity and air come from its card (sprint 96): the home's air
// has oxygen and pulls about a g; the Moon pulls less and has none, so its
// flight has no drag; a thin air is thinner than the home's.
void TestBodies::testSurroundings()
{
	const std::vector<terragen::BodyWorld> earth = worldsAt(6371000);
	const terragen::BodyWorld *home = &earth[0], *moon = nullptr, *thin = nullptr;
	for (const terragen::BodyWorld &b : earth) {
		const terragen::Surroundings &s = b.surroundings;
		std::printf("AIR %s g=%.2f P=%.0f T=%.0f rho=%.4f\n", b.id.c_str(), s.gravity, s.pressure, s.temperature, terragen::planetOf(b).density);
		if (b.id == "moon")
			moon = &b;
		else if (s.pressure > 0 && s.pressure < home->surroundings.pressure / 4)
			thin = &b;
		double shares = 0;
		for (const auto &[id, share] : s.gases)
			shares += share;
		UASSERT(s.gases.empty() || std::abs(shares - 1) < 1e-9);
	}
	UASSERT(moon && thin);
	UASSERT(std::abs(home->surroundings.gravity - 9.81) < 1);
	bool oxygen = false;
	for (const auto &[id, share] : home->surroundings.gases)
		oxygen |= id == "oxygen" && share > 0.1;
	UASSERT(oxygen);
	UASSERT(moon->surroundings.gravity < home->surroundings.gravity / 4);
	UASSERT(moon->surroundings.pressure == 0 && moon->surroundings.gases.empty());
	UASSERT(terragen::planetOf(*moon).density == 0);
	UASSERT(terragen::planetOf(*moon).gravity == moon->surroundings.gravity);
	UASSERT(terragen::planetOf(*thin).density < terragen::planetOf(*home).density / 4);
}

// Sprint 99: what a lid's interior does comes from its card. Long plumes and old
// seas (Vesper): a low side, a rise with shields, canyons, valleys, rust.
// Eruptions (Pyre): paterae on plains, and no flooding. Neither on the rest.
// The most bodies a system can have: laid out as the server lays them, and how long it takes.
void TestBodies::testAFullSystem()
{
	std::vector<uint8_t> b{ (uint8_t)terragen::MOST_BODIES };
	const auto text = [&](const std::string &t) { b.push_back((uint8_t)t.size()); b.insert(b.end(), t.begin(), t.end()); };
	for (size_t i = 0; i < terragen::MOST_BODIES; i++) {
		text("b" + std::to_string(i));
		text("b" + std::to_string(i));
		b.insert(b.end(), { 100, 100, 100, 255, (uint8_t)(i == 0 ? 3 : 0) });
		const int orbit = 800 * (int)i, mass = i == 0 ? 0 : -2400; // 1 AU out; Moon-sized
		b.insert(b.end(), { (uint8_t)(orbit & 255), (uint8_t)(orbit >> 8), (uint8_t)(mass & 255), (uint8_t)((mass >> 8) & 255), 0x09, 0x2c });
		b.insert(b.end(), { 0, 90, 30, 0, 0, 0 });
	}
	terragen::WorldCode code = HOME_CODE;
	code.bodies = b;
	const auto start = std::chrono::steady_clock::now();
	const auto worlds = terragen::bodyWorlds(code, terragen::Atlas::standard(100000));
	const auto worldsMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	infostream << "bodyWorlds: " << worldsMs << " ms" << std::endl;
	std::vector<terragen::Atlas::Params> params;
	for (const terragen::BodyWorld &w : worlds)
		params.push_back(w.atlas);
	const planet::Atlas atlas(params);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	infostream << "a full system: " << worlds.size() << " worlds laid out in " << ms << " ms" << std::endl;
	UASSERT(worlds.size() == terragen::MOST_BODIES);
	UASSERT(ms < 2000); // a world opening, or a system changed in the menu: laid out by what shapes it, not every edit
}

void TestBodies::testLidsByTheirCards()
{
	for (const terragen::BodyWorld &b : worldsAt(100000)) {
		const terragen::Processes &p = b.code.processes;
		if (b.id == "vesper") {
			UASSERT(p.plumes > 0 && p.ancient_water > 0 && p.bare == terragen::Surface::Rust);
			const std::shared_ptr<const terragen::Grid> g = terragen::gridFor(b.code);
			UASSERT(g->canyons.size() == 3);
			int shields = 0;
			for (const terragen::Volcano &v : g->volcanoes)
				shields += v.kind == terragen::VolcanoKind::Giant && !v.active;
			UASSERT(shields == 5);
			int worn = 0;
			for (size_t i = 0; i < g->eroded.size(); i++)
				worn += g->eroded[i] < g->base[i];
			UASSERT(worn > (int)g->eroded.size() / 20);
		} else if (b.id == "pyre") {
			UASSERT(p.eruptions > 0 && p.flooding == 0 && p.bare == terragen::Surface::Sulfur);
			const std::shared_ptr<const terragen::Grid> g = terragen::gridFor(b.code);
			int lakes = 0;
			for (const terragen::Volcano &v : g->volcanoes) {
				UASSERT(v.kind == terragen::VolcanoKind::Patera);
				lakes += v.active;
			}
			UASSERT(lakes > 50);
			terragen::World w(b.code);
			UASSERT(w.strata(terragen::World::place(terragen::ratio(1, 3), terragen::ratio(1, 3)), w.ground(terragen::World::place(terragen::ratio(1, 3), terragen::ratio(1, 3)), terragen::ONE / 2)).province == terragen::Province::Volcanic);
		} else if (b.id != b.code.seed && b.id != "earth") {
			UASSERT(p.plumes == 0 && p.ancient_water == 0 && p.eruptions == 0);
		}
	}
}
