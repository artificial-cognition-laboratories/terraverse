// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "filesys.h"
#include "porting.h"
#include "matter/matter.h"
#include "sandbox/stage.h"
#include "shell/course.h"
#include "shell/finepowder.h"
#include "client/liquidlook.h"
#include "shell/finewater.h"
#include "shell/relief.h"
#include <map>
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
	void testBonesStayOut();
	void testEveryVehicle();
	void testEndless();
	void testDrivenDrawnSmoothly();
	void testIceMeltsInwards();
	void testFineWater();
	void testPourColour();
	void testPlume();
	void testFinePowder();
	void testSteps();
	void testWallsStayDry();
	void testBoiling();
	void testLook();
	void testBurner();
	void testRiverPatch();
};

static TestMoves g_test_instance;

void TestMoves::runTests(IGameDef *gamedef)
{
	TEST(testEveryMove);
	TEST(testBonesStayOut);
	TEST(testEveryVehicle);
	TEST(testEndless);
	TEST(testDrivenDrawnSmoothly);
	TEST(testIceMeltsInwards);
	TEST(testFineWater);
	TEST(testPourColour);
	TEST(testPlume);
	TEST(testFinePowder);
	TEST(testSteps);
	TEST(testWallsStayDry);
	TEST(testBoiling);
	TEST(testLook);
	TEST(testBurner);
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

// What a body's drawn doing against what's there: how deep any bone goes into anything solid, and when.
void TestMoves::testBonesStayOut()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	for (const Json::Value &move : shell::Course::read(game + "moves.json")) {
		shell::Course course(move, blocks, palette["machines"]);
		if (course.machine())
			continue;
		std::string rush_doing;
		float deepest = 0, when = 0, in_itself = 0, rush = 0, rush_at = 0, pop = 0, pop_at = 0, apart = 0, tallest_after_down = 0;
		bool was_down = false;
		int deep_frames = 0, frames = 0;
		// (an endless one, a good few goes of its move)
		const double until = course.endless() ? 30 : course.end();
		while (course.actor().time() + 1 / 60.0 < until) {
			course.update(1 / 60.f);
			const float sunk = course.actor().sunk();
			const v3f head = course.actor().head();
			apart = std::max(apart, head.getLength());
			in_itself = std::max(in_itself, course.actor().inItself());
			physics::Actor &actor = const_cast<physics::Actor &>(course.actor());
			if (const float r = actor.takeRush(); r > rush) {
				rush = r;
				rush_at = actor.time();
				rush_doing = actor.doing() + "/" + physics::stanceName(actor.stance());
			}
			if (const float p = actor.takePop(); p > pop) {
				pop = p;
				pop_at = actor.time();
			}
			was_down = was_down || course.actor().stance() == physics::Stance::DOWN;
			if (was_down && course.actor().stance() == physics::Stance::STAND)
				tallest_after_down = std::max(tallest_after_down, head.Y);
			frames++;
			deep_frames += sunk > 0.05f;
			if (sunk > deepest) {
				deepest = sunk;
				when = course.actor().time();
			}
		}
		std::cout << "sunk " << move["id"].asString() << ": " << deepest << " at " << when << " s, "
				<< deep_frames << "/" << frames << " frames past 0.05, in itself " << in_itself << ", head within " << apart << ", contacts missed " << course.actor().gaveUp() << ", body at most " << rush << " at " << rush_at << " s (" << rush_doing << "), a part " << pop << " at " << pop_at << " s";
		if (was_down)
			std::cout << ", up again head at " << tallest_after_down;
		std::cout << std::endl;
		UASSERT(deepest < 0.2f);  // (its parts are slimmer than drawn: a little always shows)
		UASSERT(apart < 2.5f);    // it's stayed in one piece
		UASSERT(in_itself < 0.02f); // and out of itself
		UASSERT(rush < 16 && pop < 18); // and nothing of it jumped: no step goes faster than a body can
		UASSERT(!was_down || tallest_after_down > 1.2f); // and got up
	}
}

// Each endless move goes on: a minute in, it's still making way and has done its thing again and again,
// and never been found stuck. Where it's drawn stays in what's drawn, however far it's gone.
void TestMoves::testEndless()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	int bad = 0;
	for (const Json::Value &move : shell::Course::read(game + "moves.json")) {
		if (!move.isMember("endless"))
			continue;
		shell::Course course(move, blocks, palette["machines"]);
		v3f hopped(0), was = course.centre();
		float furthest = 0, jolt = 0;
		const physics::Actor *actor = &course.actor();
		bool restarted = false;
		for (int f = 0; f < 60 * 60; f++) {
			course.update(1 / 60.f);
			restarted = restarted || &course.actor() != actor;
			const v3f by = course.takeHop();
			const v3f now = course.centre();
			jolt = std::max(jolt, (now - by - was).getLength());
			was = now;
			furthest = std::max(furthest, std::fabs(now.Z));
			hopped += by;
		}
		std::string all, missing;
		for (const std::string &d : course.actor().did())
			all += d + "×" + std::to_string(course.actor().times(d)) + " ";
		for (const std::string &want : move["repeats"].getMemberNames())
			if (course.actor().times(want) < move["repeats"][want].asInt())
				missing += " " + want;
		const bool ok = missing.empty() && !restarted && furthest < 40;
		std::cout << (ok ? "ok   " : "FAIL ") << move["id"].asString() << ": went " << course.travelled().Z << ", across " << course.travelled().X << ", up " << course.travelled().Y
				<< ", drawn within " << furthest << ", jolt " << jolt << (restarted ? " RESTARTED" : "") << " | " << all << (missing.empty() ? "" : " | missing:" + missing) << std::endl;
		bad += !ok;
	}
	UASSERTEQ(int, bad, 0);
}

// Each Vehicles scene: the car got as fast, as far and as high as it says, and its bodies are all drawn.
// Frames don't land on the 60 Hz steps. What's drawn keeps pace between them: a frame with
// no step in it can't hold the car still while the camera goes on, then jump it two.
void TestMoves::testDrivenDrawnSmoothly()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	for (const Json::Value &move : shell::Course::read(game + "vehicles.json")) {
		if (move["id"].asString() != "pull")
			continue;
		shell::Course course(move, blocks, palette["machines"]);
		const float dts[] = { 0.016f, 0.017f, 0.016f, 0.009f, 0.024f };
		double drawn = course.driven()->drawnPose().position.Z, stepped = course.driven()->pose().position.Z;
		float worst_drawn = 1, worst_stepped = 1, back = 0;
		for (int i = 0; i < 300; i++) {
			const float dt = dts[i % 5];
			const float pace = course.driven()->pace();
			course.update(dt);
			const double d = course.driven()->drawnPose().position.Z, s = course.driven()->pose().position.Z;
			if (pace > 3) { // well under way: it should go about as far as its pace says, each frame
				const float want = pace * dt;
				worst_drawn = std::min(worst_drawn, (float)(d - drawn) / want);
				worst_stepped = std::min(worst_stepped, (float)(s - stepped) / want);
			}
			back = std::min(back, (float)(s - stepped));
			drawn = d;
			stepped = s;
		}
		std::cout << "pull: slowest frame drawn " << worst_drawn << " of its pace (raw steps " << worst_stepped << "), most the physics went back " << back << std::endl;
		UASSERT(worst_drawn > 0.4f);
		UASSERT(back > -0.001f);
	}
}

// A block of ice over a burner goes from the outside in: its water runs out as it's made, what's
// left stands on the floor and is the matter's solid to within a cut, and no water's lost.
// A heap round a hole in a ledge: what's at its edge runs in, and doesn't stand there as a wall.
static void testGapRunsIn(const std::vector<sandbox::StageBlock> &blocks)
{
	int stone = -1;
	for (size_t i = 0; i < blocks.size(); i++)
		if (blocks[i].spec.node == "tf_world:stone")
			stone = (int)i;
	UASSERT(stone >= 0);
	std::vector<sandbox::Stage::Cell> cells(27);
	const auto at = [](int x, int y, int z) { return ((size_t)y * 3 + z) * 3 + x; };
	for (int z = 0; z < 3; z++)
		for (int x = 0; x < 3; x++) {
			cells[at(x, 0, z)].block = stone;
			if (x != 1 || z != 1) {
				cells[at(x, 1, z)].block = stone;
				sandbox::Stage::Cell &c = cells[at(x, 2, z)];
				c.main = 0;
				c.state = matter::State::Solid;
				c.filled = 0.5;
			}
		}
	shell::FinePowder powder;
	powder.sync(3, 3, 3, cells, blocks);
	for (int i = 0; i < 400; i++) { // the matter going down the hole, as it does
		for (sandbox::Stage::Cell &c : cells)
			if (c.main == 0)
				c.filled = std::max(0.25, c.filled - 0.001);
		powder.sync(3, 3, 3, cells, blocks);
		powder.step(0.1f, 1);
	}
	int edge = 0;
	for (const shell::FinePowder::Column &col : powder.columns())
		if (col.base == 32 && (((col.x == 15 || col.x == 32) && col.z >= 16 && col.z < 32) || ((col.z == 15 || col.z == 32) && col.x >= 16 && col.x < 32)))
			edge = std::max(edge, (int)(col.top - col.base));
	std::cout << "gap: tallest next to the hole " << edge << " sixteenths" << std::endl;
	UASSERT(edge <= 4);
}

void TestMoves::testIceMeltsInwards()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	static matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	sandbox::Stage stage({ table, blocks, 3, 2, 3, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	Json::Value spec;
	Json::Value &list = spec["blocks"];
	for (int x = 0; x < 3; x++)
		for (int z = 0; z < 3; z++) {
			Json::Value b;
			b["at"].append(x); b["at"].append(0); b["at"].append(z);
			b["node"] = "tf_world:stone";
			list.append(b);
		}
	Json::Value ice;
	ice["at"].append(1); ice["at"].append(1); ice["at"].append(1);
	ice["node"] = "tf_world:ice";
	ice["temperature"] = 258;
	ice["power"] = 5e6;
	list.append(ice);
	stage.setUp(spec, 293);
	const size_t middle = ((size_t)1 * 3 + 1) * 3 + 1;
	const double water = stage.moles("water");
	int steps = 0, was = 4096, sizes = 0, worst_jump = 0;
	shell::FineWater fine;
	float held = 0, worst_water = 0;
	bool seated = true, inward = true;
	for (; steps < 6000; steps++) {
		stage.step(0.1);
		const std::vector<sandbox::Stage::Cell> view = stage.view();
		const sandbox::Stage::Cell &c = view[middle];
		fine.sync(3, 2, 3, view, blocks);
		worst_water = std::max(worst_water, fine.target() - held);
		held = fine.target();
		const bool still_ice = c.block >= 0 && blocks[c.block].spec.matter != "air";
		if (!still_ice)
			break;
		if (!c.shape)
			continue;
		const int cells = (int)c.shape->count();
		if (cells != was) {
			sizes++;
			worst_jump = std::max(worst_jump, was - cells);
			was = cells;
		}
		if (cells > 200) {
			seated = seated && c.shape->has(v3s16(8, 0, 8));
			inward = inward && c.shape->has(v3s16(8, 2, 8)) && !c.shape->has(v3s16(0, 15, 0));
		}
	}
	std::cout << "ice: gone after " << steps * 0.1 << " s in " << sizes << " cuts, biggest " << worst_jump << "/4096, water " << water << " -> " << stage.moles("water") << ", most water shown at once " << worst_water << std::endl;
	UASSERT(steps < 6000);
	UASSERT(sizes >= 20);
	UASSERT(worst_jump < 500);
	UASSERT(worst_water < 0.15f);
	UASSERT(seated && inward);
	UASSERT(std::fabs(stage.moles("water") - water) < water * 0.02);
	// and never in pieces
	shape::Shape left = shape::melted(nullptr, 300, 0b111111, true);
	UASSERT(left.count() <= 300 && left.count() > 150);
	shape::Shape again = shape::melted(&left, 100, 0b111111, true);
	UASSERT(again.count() <= 100 && !again.empty());
	std::vector<v3s16> todo;
	shape::Shape seen = again;
	int reached = 0;
	for (s16 y = 0; y < 16 && todo.empty(); y++)
		for (s16 z = 0; z < 16 && todo.empty(); z++)
			for (s16 x = 0; x < 16 && todo.empty(); x++)
				if (seen.has(v3s16(x, y, z))) {
					seen.set(v3s16(x, y, z), false);
					todo.push_back(v3s16(x, y, z));
				}
	static const v3s16 NEAR[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	while (!todo.empty()) {
		const v3s16 c = todo.back();
		todo.pop_back();
		reached++;
		for (const v3s16 &d : NEAR)
			if (shape::Shape::inside(c + d) && seen.has(c + d)) {
				seen.set(c + d, false);
				todo.push_back(c + d);
			}
	}
	UASSERTEQ(int, reached, (int)again.count());
	// a crucible's charge is drawn in its walls: no heap of it reaches past them
	sandbox::Stage pot({ table, blocks, 3, 2, 3, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	Json::Value charge;
	Json::Value crucible;
	crucible["at"].append(1); crucible["at"].append(0); crucible["at"].append(1);
	crucible["node"] = "tf_matter:crucible";
	crucible["add"]["hematite"] = 500;
	charge["blocks"].append(crucible);
	pot.setUp(charge, 293);
	shell::FinePowder powder;
	for (int i = 0; i < 100; i++) {
		pot.step(0.1);
		powder.sync(3, 2, 3, pot.view(), blocks);
		powder.step(0.1, 1);
	}
	UASSERT(powder.columns().empty());
	testGapRunsIn(blocks);
}

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
		for (int f = 0; f < 60; f++)
			fine.step(1 / 60.f, 1);
	}
	std::set<int> tops;
	std::map<std::pair<int, int>, int> at;
	float held = 0, has = 0;
	for (const shell::FinePowder::Column &c : fine.columns()) {
		UASSERTEQ(int, c.base, 16);
		UASSERT(c.top > c.base);
		tops.insert(c.top);
		at[{ c.x, c.z }] = c.top;
		held += (c.top - c.base) / 4096.f;
	}
	for (const sandbox::Stage::Cell &c : stage.view())
		if (c.main >= 0 && c.state == matter::State::Solid && c.block >= 0 && shell::FinePowder::isPowder(blocks[c.block]))
			has += (float)c.level;
	std::cout << "salt heap: " << tops.size() << " heights, holds " << held << " of " << has << ", most falling blocks " << most_falling << std::endl;
	// no cliffs at a block's edge: stepping across one is as across anywhere, and it's about as wide on a diagonal
	int cliff = 0, along = 0, across = 0;
	for (const auto &[where, top] : at) {
		const auto next = at.find({ where.first + 1, where.second });
		cliff = std::max(cliff, std::abs(top - (next == at.end() ? 16 : next->second)));
		const int dx = where.first - 88, dz = where.second - 88;
		if (dz == 0)
			along = std::max(along, std::abs(dx));
		if (dx == dz)
			across = std::max(across, std::abs(dx));
	}
	std::cout << "salt heap: steepest step " << cliff << ", reaches " << along << " along, " << across << " on the diagonal (" << across * 1.41f << ")" << std::endl;
	UASSERT(tops.size() >= 8);
	UASSERT(most_falling >= 1);
	UASSERT(std::fabs(held - has) < has * 0.1f);
	UASSERT(cliff <= 3);
	UASSERT(across * 1.41f > along * 0.7f && across * 1.41f < along * 1.3f);
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

// Cold water has no bubbles; near boiling a few die on the way up; boiling, they break the surface and ripple it.
void TestMoves::testBoiling()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	int water = -1;
	for (size_t i = 0; i < blocks.size(); i++)
		if (blocks[i].spec.node == "tf_world:water_source")
			water = (int)i;
	std::vector<sandbox::Stage::Cell> cells(3 * 3 * 3);
	for (size_t i = 0; i < 9; i++)
		cells[i].block = water, cells[i].level = 1;
	const auto run = [&](double boil) {
		for (size_t i = 0; i < 9; i++)
			cells[i].boil = boil;
		shell::FineWater fine;
		int bursts = 0, most = 0, rippled = 0;
		for (int f = 0; f < 240; f++) {
			fine.sync(3, 3, 3, cells, blocks);
			fine.step(1 / 60.f, 1);
			fine.bubble(1 / 60.f, 1);
			bursts += (int)fine.takeBursts().size();
			most = std::max(most, (int)fine.bubbles().size());
		}
		for (int c = 0; c < fine.width() * fine.depth(); c++)
			rippled += fine.lookOf(c).light != 1;
		return std::array<int, 3>{ bursts, most, rippled };
	};
	const auto cold = run(0), near = run(0.5), boiling = run(1);
	std::cout << "boiling (bursts, bubbles, rippled): cold " << cold[0] << "," << cold[1] << "," << cold[2] << " near " << near[0] << "," << near[1] << "," << near[2]
			<< " boiling " << boiling[0] << "," << boiling[1] << "," << boiling[2] << std::endl;
	UASSERTEQ(int, cold[1], 0);
	UASSERTEQ(int, cold[2], 0);
	UASSERT(near[1] > 0 && near[0] < boiling[0] / 3);
	UASSERT(boiling[0] > 50 && boiling[2] > 100);
}

// A burner warms water over time, then it holds at boiling and boils away at the rate the heat allows.
void TestMoves::testBurner()
{
	const std::string game = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM;
	std::ifstream in(game + "palette.json");
	Json::Value palette;
	in >> palette;
	const std::vector<sandbox::StageBlock> blocks = sandbox::Stage::read(palette["scene"]);
	const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	sandbox::Stage stage(sandbox::Stage::Opts{ table, blocks, 3, 4, 3, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	for (int x = 0; x < 3; x++)
		for (int z = 0; z < 3; z++) {
			stage.place({ x, 0, z }, "tf_world:stone");
			stage.place({ x, 1, z }, x == 1 && z == 1 ? "tf_world:water_source" : "tf_world:stone_bricks");
		}
	stage.burner({ 1, 1, 1 }, 6e6);
	const auto hot = [&] { return stage.view()[(1 * 3 + 1) * 3 + 1].temperature; };
	const double start = stage.moles("water");
	stage.step(10);
	const double ten = hot();
	stage.step(90);
	const double hundred = hot(), left = stage.moles("water");
	std::cout << "burner: 10 s " << ten << " K, 100 s " << hundred << " K, water left " << left / start << std::endl;
	// 6 MW into 4.2 MJ/K: about 1.4 K a second, by the block's heat capacity
	UASSERT(ten > 295 && ten < 330);
	UASSERT(hundred > 365 && hundred < 380);
	UASSERT(left > start * 0.85);
	// nothing but air: it has nothing to heat
	sandbox::Stage empty(sandbox::Stage::Opts{ table, blocks, 1, 1, 1, 293, 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	empty.burner({ 0, 0, 0 }, 6e6);
	empty.step(30);
	UASSERT(empty.view()[0].temperature < 400);
}

// A black body: nothing below Draper's point, then red, orange, and towards white; ripples are on steps of light.
void TestMoves::testLook()
{
	using namespace liquids::look;
	UASSERT(glow(700).share == 0);
	const Glow red = glow(900), orange = glow(1500), white = glow(5000);
	std::cout << "glow 900 K " << red.r << "," << red.g << "," << red.b << " x" << red.share << "; 1500 K " << orange.r << "," << orange.g << "," << orange.b << " x" << orange.share << std::endl;
	UASSERT(red.r > 0.9f && red.g < 0.5f && red.share > 0 && red.share < 0.3f);
	UASSERT(orange.g > red.g && orange.b < orange.g && orange.share > 0.8f);
	UASSERT(white.b > 0.9f && white.g > 0.9f);
	const Surface flat = surface(1, { 1, 1, 1, 1 }, 1 / 16.f, {}, 1);
	UASSERT(flat.light == 1 && flat.foam == 0);
	const Surface sloped = surface(1, { 0.99f, 1.01f, 1, 1 }, 1 / 16.f, {}, 1);
	UASSERT(sloped.light != 1);
	const Surface ledge = surface(1, { 0, 1, 1, 1 }, 1 / 16.f, {}, 1);
	UASSERT(ledge.light == 1);
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
