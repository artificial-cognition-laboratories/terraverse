// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "filesys.h"
#include "porting.h"
#include "matter/matter.h"
#include "shell/course.h"
#include "shell/finepowder.h"
#include "shell/finewater.h"
#include "shell/relief.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>

/*
	Every Movement scene in the wiki, played through the game's own moves: what
	moves.json says should happen does, with the keys it lists. A scene whose
	keys stop producing its move fails here, not in front of someone reading it.
*/
class TestMoves : public TestBase {
public:
	TestMoves() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMoves"; }
	void runTests(IGameDef *gamedef);
	void testEveryMove();
	void testEveryVehicle();
	void testFineWater();
	void testPourColour();
	void testPlume();
	void testFinePowder();
	void testSteps();
	void testWallsStayDry();
	void testRiverPatch();
};

static TestMoves g_test_instance;

void TestMoves::runTests(IGameDef *gamedef)
{
	TEST(testEveryMove);
	TEST(testEveryVehicle);
	TEST(testFineWater);
	TEST(testPourColour);
	TEST(testPlume);
	TEST(testFinePowder);
	TEST(testSteps);
	TEST(testWallsStayDry);
	TEST(testRiverPatch);
}

void TestMoves::testEveryMove()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	int bad = 0;
	for (const Json::Value &move : shell::Course::read(game + "moves.json")) {
		shell::Course course(move, blocks, palette["machines"]);
		v3f most(0);
		std::vector<std::string> did;
		while (true) {
			course.actor(); // (restart replaces it: the end's read just before)
			if (course.actor().time() + 1 / 60.0 >= course.end()) {
				did = course.actor().did();
				break;
			}
			course.update(1 / 60.f);
			most.Y = std::max(most.Y, course.actor().at().Y);
		}
		std::string missing;
		for (const Json::Value &want : move["expect"])
			if (std::find(did.begin(), did.end(), want.asString()) == did.end())
				missing += " " + want.asString();
		std::string all;
		for (const std::string &d : did)
			all += d + ", ";
		std::cout << (missing.empty() ? "ok   " : "FAIL ") << move["id"].asString() << ": ends at z " << course.actor().at().Z
				<< " pace " << course.actor().pace() << " | did: " << all << (missing.empty() ? "" : " | missing:" + missing) << std::endl;
		bad += !missing.empty();
	}
	UASSERTEQ(int, bad, 0);
}

// Each Vehicles scene: the car got as fast, as far and as high as it says, and its bodies are all drawn.
void TestMoves::testEveryVehicle()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	int bad = 0;
	std::vector<Json::Value> all;
	for (const char *file : { "vehicles.json", "ground.json" })
		for (const Json::Value &m : shell::Course::read(game + file))
			all.push_back(m);
	for (const Json::Value &move : all) {
		if (move["later"].asBool())
			continue;
		shell::Course course(move, blocks, palette["machines"]);
		if (!move.isMember("machine")) { // a body on loose ground, or the ground alone
			const std::array<int, 2> began = course.groundBegan();
			std::array<int, 2> at_end = began;
			while (course.actor().time() + 1 / 60.0 < course.end()) {
				course.update(1 / 60.f);
				at_end = course.groundRange();
			}
			const Json::Value &g = move["expect"];
			// the biggest step between neighbouring columns: how steep it's left standing
			int steepest = 0;
			if (const std::optional<shell::Scene::Ground> ground = course.ground())
				for (int z = 0; z < ground->depth; z++)
					for (int x = 0; x + 1 < ground->width; x++)
						steepest = std::max(steepest, std::abs(ground->top[(size_t)z * ground->width + x] - ground->top[(size_t)z * ground->width + x + 1]));
			const bool ok = at_end[0] <= g.get("lowest_at_most", 1000000).asInt() && at_end[1] >= g.get("highest_at_least", -1000000).asInt()
					&& steepest <= g.get("steepest_at_most", 1000000).asInt() && steepest >= g.get("steepest_at_least", 0).asInt();
			std::cout << (ok ? "ok   " : "FAIL ") << move["id"].asString() << ": ground began " << began[0] << ".." << began[1] << ", ends " << at_end[0] << ".." << at_end[1] << ", steepest " << steepest << std::endl;
			bad += !ok;
			continue;
		}
		float fastest = 0, last_pace = 0;
		v3d last(0, 0, 0);
		double highest = -1e9;
		float lowest_up = 1;
		while (course.driven()->time() + 1 / 60.0 < course.end()) {
			course.update(1 / 60.f);
			const physics::Pose p = course.driven()->pose();
			last = p.position;
			last_pace = course.driven()->pace();
			fastest = std::max(fastest, last_pace);
			highest = std::max(highest, p.position.Y);
			lowest_up = std::min(lowest_up, 1 - 2 * (p.rotation.x * p.rotation.x + p.rotation.z * p.rotation.z));
		}
		const Json::Value &e = move["expect"];
		const bool ok = fastest >= e.get("fastest", 0).asFloat() && last.Z >= e.get("past_z", -1e9).asDouble() && last.Z <= e.get("before_z", 1e9).asDouble()
				&& std::fabs(last.X - move["start"][0].asDouble()) >= e.get("aside", 0).asDouble() && highest >= e.get("up_to", -1e9).asDouble()
				&& last_pace <= e.get("ends_under", 1e9).asFloat() && lowest_up >= e.get("stays_up", -2).asFloat() && lowest_up <= e.get("tips", 2).asFloat()
				&& course.groundRange()[0] <= e.get("rut_to", 1000000).asInt();
		std::cout << (ok ? "ok   " : "FAIL ") << move["id"].asString() << ": " << course.driven()->mass() << " kg, fastest " << fastest << ", ends "
				<< last.X << "," << last.Y << "," << last.Z << " pace " << last_pace << ", highest " << highest << ", least upright " << lowest_up << ", ground " << course.groundRange()[0] << ".." << course.groundRange()[1] << std::endl;
		bad += !ok;
	}
	UASSERTEQ(int, bad, 0);
}

// A column let go: its sixteenth-wide tops aren't whole blocks or even whole levels while it runs (real waves, on
// the sixteenth), and when it's settled the picture holds what the world's matter has.
void TestMoves::testFineWater()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	sandbox::Stage stage(sandbox::Stage::Opts{ table, blocks, 9, 5, 9, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	for (int x = 0; x < 9; x++)
		for (int z = 0; z < 9; z++) {
			stage.place({ x, 0, z }, "tf_world:stone");
			if (x == 0 || z == 0 || x == 8 || z == 8)
				stage.place({ x, 1, z }, "tf_world:stone_bricks");
		}
	for (int y = 1; y <= 3; y++)
		for (int x = 3; x <= 5; x++)
			for (int z = 3; z <= 5; z++)
				stage.place({ x, y, z }, "tf_world:water_source");
	shell::FineWater fine;
	int most_kinds = 0;
	for (int frame = 0; frame < 60 * 40; frame++) {
		if (frame % 6 == 0) {
			stage.step(0.1);
			fine.sync(stage.sizeX(), stage.sizeY(), stage.sizeZ(), stage.view(), blocks);
		}
		fine.step(1 / 60.f, 1);
		if (frame % 30 == 0) {
			std::set<int> tops;
			for (int c = 0; c < fine.width() * fine.depth(); c++)
				if (fine.shown(c) != shell::FineWater::NONE)
					tops.insert(fine.shown(c));
			most_kinds = std::max(most_kinds, (int)tops.size());
		}
	}
	std::cout << "fine water: most different tops " << most_kinds << ", holds " << fine.volume() << " of " << fine.target() << std::endl;
	UASSERT(most_kinds >= 8);
	UASSERT(std::fabs(fine.volume() - fine.target()) < 1.0f);
}

// Copper running ahead of the matter is copper, and nothing of it hangs solid in the air on its way down.
void TestMoves::testPourColour()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	sandbox::Stage stage(sandbox::Stage::Opts{ table, blocks, 5, 5, 5, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	for (int x = 0; x < 5; x++)
		for (int z = 0; z < 5; z++) {
			stage.place({ x, 0, z }, "tf_world:stone");
			if (x == 0 || z == 0 || x == 4 || z == 4)
				stage.place({ x, 1, z }, "tf_world:stone_bricks");
		}
	stage.pour({ 2, 4, 2 }, "copper", 8000, 1500, 15);
	shell::FineWater fine;
	int wet = 0, blue = 0, hanging = 0;
	for (int frame = 0; frame < 60 * 30; frame++) {
		if (frame % 6 == 0) {
			stage.step(0.1);
			const std::vector<sandbox::Stage::Cell> cells = stage.view();
			fine.sync(5, 5, 5, cells, blocks);
			for (int y = 2; y < 5; y++)
				for (int i = 0; i < 25; i++) {
					const sandbox::Stage::Cell &c = cells[y * 25 + i];
					hanging += c.main >= 0 && c.filled > 0.005 && c.state == matter::State::Solid;
				}
		}
		fine.step(1 / 60.f, 1);
		for (int c = 0; c < fine.width() * fine.depth(); c++)
			if (fine.shown(c) != shell::FineWater::NONE) {
				wet++;
				blue += fine.colourAt(c) == 0x3f76e4;
			}
	}
	std::cout << "copper pour: " << wet << " wet columns seen, blue " << blue << ", solid in the air " << hanging << std::endl;
	UASSERT(wet > 0);
	UASSERTEQ(int, blue, 0);
	UASSERTEQ(int, hanging, 0);
}

// Gas shows where the matter has it and only there, in its own colour; a flame only where something burns.
void TestMoves::testPlume()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	const std::vector<video::SColor> colours = { video::SColor(255, 58, 127, 196), video::SColor(255, 232, 238, 242) };
	const std::vector<bool> water = { true, false };
	std::vector<sandbox::Stage::Cell> cells(27);
	shell::Plume plume;
	plume.sync(3, 3, 3, cells, blocks, colours, water);
	plume.step(0.5f);
	UASSERT(plume.puffs().empty());
	cells[13].vapour = 0; // steam in the middle: white, not water's blue
	cells[13].haze = 1;
	cells[13].temperature = 500;
	cells[4].flame = 0x4a6cff; // and a blue flame under it
	cells[4].activity = 2000;
	cells[4].temperature = 900;
	plume.sync(3, 3, 3, cells, blocks, colours, water);
	int gas = 0, flames = 0;
	for (int i = 0; i < 60; i++)
		plume.step(1 / 60.f);
	for (const shell::Plume::Puff &p : plume.puffs()) {
		if (p.flame()) {
			flames++;
			UASSERT(p.colour().getBlue() > p.colour().getRed());
		} else {
			gas++;
			UASSERT(p.colour().getBlue() > 200 && p.colour().getRed() > 200);
		}
	}
	UASSERT(gas > 0 && flames > 0);
	cells[13].vapour = -1;
	cells[13].haze = 0;
	cells[4].flame = 0;
	cells[4].activity = 0;
	plume.sync(3, 3, 3, cells, blocks, colours, water);
	for (int i = 0; i < 60 * 8; i++)
		plume.step(1 / 60.f);
	UASSERT(plume.puffs().empty());
}

// A heap is a slope of many heights, on the ground, holding about what the matter has, and what's falling isn't in it.
void TestMoves::testFinePowder()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	sandbox::Stage stage(sandbox::Stage::Opts{ table, blocks, 11, 7, 11, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	for (int x = 0; x < 11; x++)
		for (int z = 0; z < 11; z++)
			stage.place({ x, 0, z }, "tf_world:stone");
	stage.pour({ 5, 6, 5 }, "salt", 12000, 293, 90);
	shell::FinePowder fine;
	size_t most_falling = 0;
	for (int s = 0; s < 40; s++) {
		stage.step(1);
		fine.sync(11, 7, 11, stage.view(), blocks);
		most_falling = std::max(most_falling, fine.falling().size());
		fine.step(1, 10);
	}
	std::set<int> tops;
	float held = 0, has = 0;
	for (const shell::FinePowder::Column &c : fine.columns()) {
		UASSERTEQ(int, c.base, 16);
		UASSERT(c.top > c.base);
		tops.insert(c.top);
		held += (c.top - c.base) / 4096.f;
	}
	for (const sandbox::Stage::Cell &c : stage.view())
		if (c.main >= 0 && c.state == matter::State::Solid && c.block >= 0 && shell::FinePowder::isPowder(blocks[c.block]))
			has += (float)c.level;
	std::cout << "salt heap: " << tops.size() << " heights, holds " << held << " of " << has << ", most falling blocks " << most_falling << std::endl;
	UASSERT(tops.size() >= 8);
	UASSERT(most_falling >= 1);
	UASSERT(std::fabs(held - has) < has * 0.35f);
}

// Water down a flight of steps at speed stays water: no column stands far over what's beside it.
void TestMoves::testSteps()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	sandbox::Stage stage(sandbox::Stage::Opts{ table, blocks, 5, 7, 14, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	for (int x = 0; x < 5; x++)
		for (int z = 0; z < 14; z++) {
			stage.place({ x, 0, z }, "tf_world:stone");
			const int high = z <= 5 ? 3 : z <= 8 ? 2 : z <= 11 ? 1 : 0;
			for (int y = 1; y <= high; y++)
				stage.place({ x, y, z }, "tf_world:stone");
			if (z == 0 || (z <= 5 && (x == 0 || x == 4)))
				stage.place({ x, 4, z }, "tf_world:stone_bricks");
		}
	stage.pour({ 2, 6, 2 }, "water", 55500 * 0.6, 293, 120);
	shell::FineWater fine;
	int roughest = 0, spills = 0, lip_wet = 0, lip_seen = 0;
	for (int frame = 0; frame < 60 * 60; frame++) {
		if (frame % 6 == 0) {
			stage.step(1);
			fine.sync(5, 7, 14, stage.view(), blocks);
		}
		fine.step(1 / 60.f, 10);
		spills += (int)fine.takeSpills().size();
		if (frame > 60 * 10 && frame < 60 * 12) { // spilling: the top step's last row, just before its edge
			lip_seen++;
			lip_wet += fine.shown(9 * 16 * fine.width() - 40) != shell::FineWater::NONE;
		}
		if (frame % 30 == 0)
			for (int c = 0; c + 1 < fine.width() * fine.depth(); c++) {
				const int a = fine.shown(c), b = fine.shown(c + 1);
				if (a != shell::FineWater::NONE && b != shell::FineWater::NONE && (c + 1) % fine.width() && fine.floorAt(c) == fine.floorAt(c + 1))
					roughest = std::max(roughest, std::abs(a - b));
			}
	}
	std::cout << "steps: roughest neighbours " << roughest << " sixteenths, holds " << fine.volume() << " of " << fine.target() << ", spills " << spills << ", lip wet " << lip_wet << " of " << lip_seen << std::endl;
	UASSERT(roughest <= 8);
	UASSERT(spills > 0);
	UASSERT(lip_wet > lip_seen / 2);
}

// A full basin let go may surge onto its walls, as the matter says, but once that's over nothing's left on them.
void TestMoves::testWallsStayDry()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	sandbox::Stage stage(sandbox::Stage::Opts{ table, blocks, 11, 5, 5, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	for (int x = 0; x < 11; x++)
		for (int z = 0; z < 5; z++) {
			stage.place({ x, 0, z }, "tf_world:stone");
			for (int y = 1; y <= 3; y++)
				if (x == 0 || z == 0 || x == 10 || z == 4 || (x == 5 && y >= 2))
					stage.place({ x, y, z }, "tf_world:stone_bricks");
				else if (x < 5)
					stage.place({ x, y, z }, "tf_world:water_source");
		}
	shell::FineWater fine;
	int on_walls = 0, spills = 0;
	for (int frame = 0; frame < 60 * 30; frame++) {
		if (frame % 6 == 0) {
			stage.step(1);
			fine.sync(11, 5, 5, stage.view(), blocks);
		}
		fine.step(1 / 60.f, 10);
		const std::vector<shell::FineWater::Spill> gone = fine.takeSpills();
		if (frame < 60 * 15)
			continue; // the surge, and its drying
		for (const shell::FineWater::Spill &s : gone)
			spills += fine.floorAt(s.column) >= 64;
		for (int c = 0; c < fine.width() * fine.depth(); c++)
			on_walls += fine.floorAt(c) >= 64 && fine.shown(c) != shell::FineWater::NONE;
	}
	std::cout << "walls: wet " << on_walls << ", spills from them " << spills << std::endl;
	UASSERTEQ(int, on_walls, 0);
	UASSERTEQ(int, spills, 0);
}

// The Worlds time-lapse says what its pages say: rounds of a real river, sediment gathering, heavy grains
// laid, and its bed worn below where it began.
void TestMoves::testRiverPatch()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const shell::Relief relief(palette["river"]);
	const int last = relief.rounds() - 1;
	std::cout << "river: " << relief.rounds() << " stages, sediment " << relief.totalSediment(0) << " to " << relief.totalSediment(last)
			<< ", lowest on it " << relief.lowestOnRiver(0) << " to " << relief.lowestOnRiver(last) << ", richest gravel " << relief.richest() << "%" << std::endl;
	UASSERTEQ(int, relief.rounds(), 13);
	UASSERT(relief.totalSediment(0) == 0 && relief.totalSediment(last) > 0);
	UASSERT(relief.richest() > 0);
}
