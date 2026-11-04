// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "dummygamedef.h"
#include "nodedef.h"
#include "physics/structure.h"
#include "physics/world.h"
#include "piece/machine.h"
#include "workshop/fracture.h"
#include "workshop/motion.h"
#include "workshop/rubble.h"
#include <chrono>
#include <iostream>

using namespace workshop;

class TestStructure : public TestBase {
public:
	TestStructure() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestStructure"; }

	void runTests(IGameDef *gamedef);

	void testCantileverAsTheBookSays();
	void testLoadSharesRoundALoop();
	void testFramesByMaterial();
	void testBeamsByThickness();
	void testArchCarriesBetterThanABeam();
	void testPiecesFallAndSleep();
	void testBlowsByHeight();
	void testToughnessAndBlasts();
	void testRunningOverRubble();
	void testLaidInTheOpenSpace();
	void testBiggestStillQuick();
};

static TestStructure g_test_instance;

void TestStructure::runTests(IGameDef *gamedef)
{
	TEST(testCantileverAsTheBookSays);
	TEST(testLoadSharesRoundALoop);
	TEST(testFramesByMaterial);
	TEST(testBeamsByThickness);
	TEST(testArchCarriesBetterThanABeam);
	TEST(testPiecesFallAndSleep);
	TEST(testBlowsByHeight);
	TEST(testToughnessAndBlasts);
	TEST(testRunningOverRubble);
	TEST(testLaidInTheOpenSpace);
	TEST(testBiggestStillQuick);
}

namespace {

constexpr float G = 9.81f;

// Dirt as tf_matter declares it, timber, steel.
struct Game
{
	DummyGameDef gamedef;
	Materials materials;
	content_t dirt, timber, steel;

	content_t add(const char *name, const Material &m)
	{
		ContentFeatures f;
		f.name = name;
		f.walkable = true;
		f.drawtype = NDT_NORMAL;
		const content_t c = gamedef.getWritableNodeDefManager()->set(f.name, std::move(f));
		materials.set(c, m);
		return c;
	}

	Game()
	{
		dirt = add("test:dirt", { 1500, 0.6f, Role::None, 0, 0, 3e4f, 3e5f, 100 });
		timber = add("test:timber", { 240, 0.6f, Role::None, 0, 0, 4e7f, 3e7f, 1e4f });
		steel = add("test:steel", { 7800, 0.6f, Role::None, 0, 0, 4e8f, 4e8f, 2e5f });
	}
};

// A shape: nodes from a test of (x, y, z), each cut to its middle `thick` sixteenths across, or whole.
template <class F>
piece::Machine build(Game &g, content_t material, int thick, const F &in)
{
	shape::Shape cross;
	const int lo = (16 - thick) / 2, hi = lo + thick;
	for (int y = 0; y < 16; y++)
		for (int z = 0; z < 16; z++)
			for (int x = 0; x < 16; x++) {
				// a node's a junction of bars: filled along each axis through its middle
				const bool xm = x >= lo && x < hi, ym = y >= lo && y < hi, zm = z >= lo && z < hi;
				cross.set(v3s16(x, y, z), (xm && ym) || (ym && zm) || (xm && zm));
			}
	piece::Object o(&g.gamedef);
	for (s16 x = 0; x < piece::SIDE; x++)
		for (s16 y = 0; y < piece::SIDE; y++)
			for (s16 z = 0; z < piece::SIDE; z++)
				if (in(x, y, z)) {
					o.design().map().setNode({ x, y, z }, MapNode(material));
					if (thick < 16)
						o.design().map().setShape({ x, y, z }, cross);
				}
	o.build();
	piece::Machine m(&g.gamedef);
	m.add({ std::move(o), piece::Pose{ v3s32(0), 0 } });
	return m;
}

// Everything's lowest layer of cells is held, as on flat ground.
physics::StructureResult onTheGround(const StructureOf &s)
{
	float lowest = 1e9;
	for (const aabb3f &b : s.boxes)
		lowest = std::min(lowest, b.MinEdge.Y);
	std::vector<bool> resting;
	for (const aabb3f &b : s.boxes)
		resting.push_back(b.MinEdge.Y < lowest + 0.01f);
	return physics::solveStructure(s.structure, resting, v3f(0, -G, 0));
}

float massOf(const physics::Structure &s)
{
	float sum = 0;
	for (const physics::StructureCell &c : s.cells)
		sum += c.mass;
	return sum;
}

// A bar of cells along x: the first held.
physics::Structure bar(int cells, float side, float strength)
{
	physics::Structure s;
	for (int i = 0; i < cells; i++)
		s.cells.push_back({ v3f(i * 0.25f, 0, 0), 1500 * side * side * 0.25f, strength, strength });
	const float bend = side * side * side * side / 12;
	for (int i = 0; i + 1 < cells; i++)
		s.bonds.push_back({ (unsigned)i, (unsigned)i + 1, 0, 0.25f, side * side, { bend, bend }, { side / 2, side / 2 } });
	return s;
}

} // namespace

// The first joint of a cantilever bends as weight × lever says: it's the book's answer it's matching.
void TestStructure::testCantileverAsTheBookSays()
{
	constexpr int CELLS = 16;
	constexpr float SIDE = 0.25f;
	std::vector<bool> resting(CELLS, false);
	resting[0] = true;
	// the book: the root carries the rest's weight at its middle
	const float weight = (CELLS - 1) * 1500 * SIDE * SIDE * 0.25f * G;
	const float lever = (CELLS - 1) * 0.25f / 2;
	const float book = weight * lever * (SIDE / 2) / (SIDE * SIDE * SIDE * SIDE / 12);
	const physics::StructureResult r = physics::solveStructure(bar(CELLS, SIDE, book), resting, v3f(0, -G, 0));
	UASSERT(r.settled);
	std::cout << "cantilever: worst " << r.worst << " of strength by the book" << std::endl;
	// (a little off: the book adds the axial and shear terms in differently)
	UASSERT(r.worst > 0.85f && r.worst < 1.2f);
}

// A bar held at both ends carries a quarter of what the same bar does hung from one, at the root: the load shares.
void TestStructure::testLoadSharesRoundALoop()
{
	constexpr int CELLS = 17;
	constexpr float SIDE = 0.25f, STRONG = 1e6f;
	std::vector<bool> one(CELLS, false), two(CELLS, false);
	one[0] = two[0] = two[CELLS - 1] = true;
	const float hung = physics::solveStructure(bar(CELLS, SIDE, STRONG), one, v3f(0, -G, 0)).worst;
	const float both = physics::solveStructure(bar(CELLS, SIDE, STRONG), two, v3f(0, -G, 0)).worst;
	std::cout << "hung from one end " << hung << ", held at both " << both << std::endl;
	UASSERT(both < hung / 3);
}

// The same frame: dirt falls apart, timber and steel stand.
void TestStructure::testFramesByMaterial()
{
	Game g;
	const auto frame = [](s16 x, s16 y, s16 z) {
		const bool cx = x == 0 || x == 5, cz = z == 0 || z == 3, cy = y == 0 || y == 3;
		return x <= 5 && y <= 3 && z <= 3 && ((cx && cz) || (cx && cy) || (cy && cz));
	};
	for (const auto &[name, material, holds] : { std::tuple{ "dirt", g.dirt, false }, std::tuple{ "timber", g.timber, true }, std::tuple{ "steel", g.steel, true } }) {
		const piece::Machine m = build(g, material, 3, frame);
		const std::optional<StructureOf> s = structureOf(m, g.materials, &g.gamedef);
		UASSERT(s.has_value());
		const physics::StructureResult r = onTheGround(*s);
		std::cout << name << " frame: " << s->cells.size() << " cells, worst " << r.worst << ", " << r.groups
				<< " pieces, " << r.broken << " joints broke, " << r.rounds << " solves" << std::endl;
		UASSERT(r.settled);
		UASSERT((r.groups == 1) == holds);
		if (!holds) {
			// its weight's all still there, in its pieces
			const std::vector<Cut> pieces = cut(m, *s, r, &g.gamedef);
			UASSERTEQ(size_t, pieces.size(), r.groups);
			float mass = 0;
			for (const Cut &p : pieces) {
				UASSERT(p.machine.parts().front().object.built().node(v3s16(0, 0, 0)).getContent() != CONTENT_IGNORE);
				const physics::Assembly a = compile(p.machine, g.materials, &g.gamedef);
				for (const physics::Body &b : a.bodies)
					for (const physics::Box &box : b.boxes)
						mass += box.box.getVolume() * box.density;
			}
			const float was = massOf(s->structure);
			UASSERT(std::fabs(mass - was) < was * 0.001f);
		}
	}
}

// Across two legs: a thin dirt beam breaks, a thick short one holds.
void TestStructure::testBeamsByThickness()
{
	Game g;
	const piece::Machine thin = build(g, g.dirt, 16, [](s16 x, s16 y, s16 z) {
		return z == 0 && x <= 7 && (y == 1 || (y == 0 && (x == 0 || x == 7)));
	});
	const piece::Machine thick = build(g, g.dirt, 16, [](s16 x, s16 y, s16 z) {
		return z <= 1 && x <= 2 && y <= 2 && (y >= 1 || x == 0 || x == 2);
	});
	const physics::StructureResult a = onTheGround(*structureOf(thin, g.materials, &g.gamedef));
	const physics::StructureResult b = onTheGround(*structureOf(thick, g.materials, &g.gamedef));
	std::cout << "thin beam: worst " << a.worst << ", " << a.groups << " pieces; thick: worst " << b.worst << ", " << b.groups << " pieces" << std::endl;
	UASSERT(a.groups > 1);
	UASSERT(b.groups == 1);
}

// Dirt takes crushing better than pulling: over the same gap an arch's load stays closer to its limit's good side.
void TestStructure::testArchCarriesBetterThanABeam()
{
	Game g;
	const piece::Machine flat = build(g, g.dirt, 16, [](s16 x, s16 y, s16 z) {
		return z == 0 && x <= 6 && ((y == 0 && (x == 0 || x == 6)) || y == 1);
	});
	const piece::Machine arch = build(g, g.dirt, 16, [](s16 x, s16 y, s16 z) {
		static const int top[7] = { 0, 1, 2, 2, 2, 1, 0 };
		return z == 0 && x <= 6 && y <= 2 && (y == top[x] || (y < top[x] && (x == 0 || x == 6 || y >= top[x] - 1)));
	});
	const float beam = onTheGround(*structureOf(flat, g.materials, &g.gamedef)).worst;
	const float curved = onTheGround(*structureOf(arch, g.materials, &g.gamedef)).worst;
	std::cout << "over a gap of 5: flat beam worst " << beam << ", arch worst " << curved << std::endl;
	UASSERT(curved < beam);
}

// Dirt on real physics: at rest it's found to be standing on the ground, is cut, every piece falls and goes back to sleep.
void TestStructure::testPiecesFallAndSleep()
{
	Game g;
	const piece::Machine m = build(g, g.dirt, 3, [](s16 x, s16 y, s16 z) {
		const bool cx = x == 0 || x == 5, cz = z == 0 || z == 3, cy = y == 0 || y == 3;
		return x <= 5 && y <= 3 && z <= 3 && ((cx && cz) || (cx && cy) || (cy && cz));
	});
	physics::World world(physics::World::Opts{ physics::uniform(v3f(0, -G, 0)), [](v3s16 block, std::vector<aabb3f> &boxes) {
		if (block.Y == -1)
			boxes.push_back(aabb3f(-0.5f, 14.5f, -0.5f, 15.5f, 15.5f, 15.5f));
		return true;
	} });
	world.players({ physics::Capsule{ v3d(2, 0, 12), 0.3f, 1.8f } }); // (Live: someone's near)
	// it's laid just over the floor; each thing in it lands
	const physics::Assembly a = compile(m, g.materials, &g.gamedef);
	const physics::World::Id id = world.add(a, physics::Pose{ v3d(2, 0.1f, 2), {} });
	world.step(physics::STEP);
	int steps = 0;
	while (!world.asleep(id) && steps++ < 1200)
		world.step(physics::STEP);
	UASSERT(world.asleep(id));
	const std::optional<StructureQuestion> q = asked(m, g.materials, &g.gamedef, world, id);
	UASSERT(q.has_value());
	const auto t0 = std::chrono::steady_clock::now();
	const physics::StructureResult r = physics::solveStructure(q->of.structure, q->resting, q->load);
	const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
	const std::vector<Cut> broke = cut(m, q->of, r, &g.gamedef);
	std::cout << "came to rest in " << steps << " steps, broke into " << broke.size() << " in " << ms << " ms" << std::endl;
	UASSERT(broke.size() > 1);
	const physics::Pose was = world.pose(id);
	world.remove(id);
	std::vector<physics::World::Id> pieces;
	for (const Cut &p : broke)
		pieces.push_back(world.add(compile(p.machine, g.materials, &g.gamedef), physics::Pose{ was.position + v3d::from(p.by), was.rotation }));
	for (int i = 0; i < 5; i++)
		world.step(physics::STEP); // (they start out of Jolt: asleep says nothing till they're in it)
	steps = 0;
	const auto allAsleep = [&] {
		for (physics::World::Id p : pieces)
			if (!world.asleep(p))
				return false;
		return true;
	};
	while (!allAsleep() && steps++ < 1800)
		world.step(physics::STEP);
	double fell = 0;
	for (size_t i = 0; i < pieces.size(); i++)
		fell = std::max(fell, std::fabs(world.pose(pieces[i]).position.Y - (was.position.Y + broke[i].by.Y)));
	std::cout << pieces.size() << " pieces all asleep after " << steps << " steps; the furthest moved " << fell << " down" << std::endl;
	UASSERT(allAsleep());
	UASSERT(fell > 0.5); // the top's come down: they didn't just sit there
}

// Dropped on the floor: the fall decides, by what it's made of. A hand's pull is no blow.
void TestStructure::testBlowsByHeight()
{
	Game g;
	const auto drop = [&](content_t material, float height, bool grabbed = false) {
		const piece::Machine m = build(g, material, 16, [](s16 x, s16 y, s16 z) { return x <= 1 && y <= 1 && z <= 1; });
		physics::World world(physics::World::Opts{ physics::uniform(v3f(0, -G, 0)), [](v3s16 block, std::vector<aabb3f> &boxes) {
			if (block.Y == -1)
				boxes.push_back(aabb3f(-16.5f, 14.5f, -16.5f, 31.5f, 15.5f, 31.5f));
			return true;
		} });
		world.players({ physics::Capsule{ v3d(2, 0, 12), 0.3f, 1.8f } });
		const physics::World::Id id = world.add(compile(m, g.materials, &g.gamedef), physics::Pose{ v3d(2, 0.05f + height, 2), {} });
		world.step(physics::STEP);
		if (grabbed) {
			world.grab(id, 1, v3f(0.5f, 1, 0.5f), 1e6f);
			world.grabTo(id, 1, v3d(2.5f, height + 3, 2.5f));
		}
		std::optional<physics::World::Blow> hardest;
		for (int i = 0; i < 300; i++) {
			world.step(physics::STEP);
			if (const auto b = world.blow(id); b && (!hardest || b->velocity.getLength() > hardest->velocity.getLength()))
				hardest = b;
			if (hardest && hardest->velocity.getLength() >= LEAST_BLOW && !grabbed)
				break;
		}
		const float size = hardest ? hardest->velocity.getLength() : 0;
		if (size < LEAST_BLOW)
			return std::pair{ size, 1u };
		const std::optional<StructureQuestion> q = asked(m, g.materials, &g.gamedef, world, id, hardest);
		UASSERT(q.has_value());
		return std::pair{ size, physics::solveStructure(q->of.structure, q->resting, q->load).groups };
	};
	const auto low = drop(g.dirt, 0.5f), high = drop(g.dirt, 12), steel = drop(g.steel, 12), pulled = drop(g.dirt, 0.05f, true);
	std::cout << "dirt from 0.5: blow " << low.first << " m/s, " << low.second << " pieces; from 12: " << high.first << ", " << high.second
			<< "; steel from 12: " << steel.first << ", " << steel.second << "; yanked up by a hand: " << pulled.first << std::endl;
	UASSERTEQ(unsigned, low.second, 1u);
	UASSERT(high.second > 1);
	UASSERTEQ(unsigned, steel.second, 1u);
	UASSERT(pulled.first < LEAST_BLOW);
}

// A solid object near the most we'll ask about: how long it holds the server up.
void TestStructure::testBiggestStillQuick()
{
	Game g;
	const piece::Machine big = build(g, g.dirt, 16, [](s16 x, s16 y, s16 z) {
		return x <= 15 && y <= 3 && z <= 3 && (y >= 1 || x == 0 || x == 15);
	});
	const std::optional<StructureOf> s = structureOf(big, g.materials, &g.gamedef);
	UASSERT(s.has_value());
	const auto t0 = std::chrono::steady_clock::now();
	const physics::StructureResult r = onTheGround(*s);
	const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
	std::cout << s->cells.size() << " cells: " << ms << " ms, " << r.rounds << " solves, settled " << r.settled << ", " << r.groups << " pieces" << std::endl;
}

void TestStructure::testToughnessAndBlasts()
{
	Game g;
	const auto frame = [](s16 x, s16 y, s16 z) {
		const bool cx = x == 0 || x == 5, cz = z == 0 || z == 3, cy = y == 0 || y == 3;
		return x <= 5 && y <= 3 && z <= 3 && ((cx && cz) || (cx && cy) || (cy && cz));
	};
	const content_t al = g.add("test:al", { 2700, 0.6f, Role::None, 0, 0, 2e8f, 2e8f, 1e5f });
	const content_t weak = g.add("test:weak", { 1500, 0.6f, Role::None, 0, 0, 3e4f, 3e5f, 100 });
	for (const auto &[name, material] : { std::pair{ "aluminium", al }, std::pair{ "dirt", weak } }) {
		const piece::Machine m = build(g, material, 1, frame);
		physics::World world(physics::World::Opts{ physics::uniform(v3f(0, -G, 0)), [](v3s16 block, std::vector<aabb3f> &boxes) {
			if (block.Y == -1)
				boxes.push_back(aabb3f(-16.5f, 14.5f, -16.5f, 31.5f, 15.5f, 31.5f));
			return true;
		} });
		world.players({ physics::Capsule{ v3d(2, 0, 14), 0.3f, 1.8f } });
		const physics::World::Id id = world.add(compile(m, g.materials, &g.gamedef), physics::Pose{ v3d(2, 2.0f, 2), { 0.1f, 0.05f, 0.08f, 0.99f } });
		unsigned most = 1;
		for (int i = 0; i < 60 && most == 1; i++) {
			world.step(physics::STEP);
			const auto b = world.blow(id);
			if (!b || (b->velocity.getLength() < LEAST_BLOW && b->spin.getLength() < LEAST_BLOW))
				continue;
			const auto q = asked(m, g.materials, &g.gamedef, world, id, b);
			const auto r = physics::solveStructure(q->of.structure, q->resting, q->load);
			most = r.groups;
			std::cout << name << " 1/16 frame dropped: " << r.groups << " pieces, energy " << q->load.energy << " J, worst " << r.worst << std::endl;
		}
		UASSERT(std::string(name) == "aluminium" ? most <= 2 : most > 2);
	}
	// A blast: near dirt goes, distant dirt's only pushed, steel stays whole.
	for (const auto &[name, material, power] : { std::tuple{ "dirt near", g.dirt, 3000.f }, std::tuple{ "steel near", g.steel, 3000.f } }) {
		const piece::Machine m = build(g, material, 16, [](s16 x, s16 y, s16 z) { return x <= 3 && y <= 3 && z == 0; });
		physics::World world(physics::World::Opts{ physics::uniform(v3f(0, -G, 0)), [](v3s16 block, std::vector<aabb3f> &boxes) {
			if (block.Y == -1)
				boxes.push_back(aabb3f(-16.5f, 14.5f, -16.5f, 31.5f, 15.5f, 31.5f));
			return true;
		} });
		world.players({ physics::Capsule{ v3d(2, 0, 14), 0.3f, 1.8f } });
		const physics::World::Id id = world.add(compile(m, g.materials, &g.gamedef), physics::Pose{ v3d(2, 0.05f, 2), {} });
		for (int i = 0; i < 60; i++)
			world.step(physics::STEP);
		const auto b = blasted(m, g.materials, &g.gamedef, world, id, v3f(3.5f, 1.5f, 0.5f), power);
		UASSERT(b.has_value());
		const auto r = physics::solveStructure(b->question.of.structure, b->question.resting, b->question.load);
		std::cout << name << " blast: flung by " << b->impulse.getLength() << " N s, " << r.groups << " pieces, worst " << r.worst << std::endl;
		UASSERT((r.groups > 1) == (material == g.dirt));
	}
}

// Run over rubble: the foot doesn't get thrown.
void TestStructure::testRunningOverRubble()
{
	Game g;
	physics::World world(physics::World::Opts{ physics::uniform(v3f(0, -G, 0)), [](v3s16 block, std::vector<aabb3f> &boxes) {
		if (block.Y == -1)
			boxes.push_back(aabb3f(-16.5f, 14.5f, -16.5f, 31.5f, 15.5f, 31.5f));
		return true;
	} });
	const piece::Machine bit = build(g, g.dirt, 3, [](s16 x, s16 y, s16 z) { return x == 0 && y == 0 && z == 0; });
	const physics::Assembly a = compile(bit, g.materials, &g.gamedef);
	std::vector<physics::World::Id> bits;
	for (int i = 0; i < 12; i++)
		bits.push_back(world.add(a, physics::Pose{ v3d(1 + i * 0.7, -0.3, 4 + (i % 3) * 0.3), {} }));
	const physics::CharacterSpec spec{ 0.3f, 1.8f, 80, 1500, 50 };
	const physics::World::CharId who = world.addCharacter(spec, v3d(-2, -0.5, 4.3));
	float highest = -0.5f;
	for (int i = 0; i < 300; i++) {
		world.want(who, v3f(6, -2, 0));
		world.step(physics::STEP);
		highest = std::max(highest, (float)world.character(who).foot.Y);
	}
	std::cout << "run over rubble: foot's highest " << highest << std::endl;
	UASSERT(highest < 1.0f);
}

// Rubble falls down its column, stacks, and every sixteenth is there at the end.
void TestStructure::testLaidInTheOpenSpace()
{
	Game g;
	piece::Piece ground(&g.gamedef);
	Map &map = ground.map();
	for (s16 x = 0; x < 4; x++)
		for (s16 z = 0; z < 4; z++)
			map.setNode({ x, 0, z }, MapNode(g.dirt));
	for (s16 x = 0; x < 4; x++)
		for (s16 y = 1; y < 6; y++)
			for (s16 z = 0; z < 4; z++)
				map.setNode({ x, y, z }, MapNode(CONTENT_AIR));
	workshop::Laying laying(map, g.gamedef.ndef(), [&](v3s16 p, MapNode n) { map.setNode(p, n); });
	int laid = 0;
	for (int i = 0; i < 40; i++) // more than a node's height in one column, from high up
		laid += laying.drop(v3s32(24, 16 * 5, 24), g.dirt);
	laying.finish();
	UASSERTEQ(int, laid, 40);
	int found = 0;
	for (s16 y = 1; y < 6; y++)
		if (const shape::Shape *s = map.getShape({ 1, y, 1 }))
			for (s16 c = 0; c < 16; c++)
				found += s->has(v3s16(8, c, 8));
		else if (map.getNode({ 1, y, 1 }).getContent() == g.dirt)
			found += 16;
	std::cout << "40 dropped, " << found << " stacked in their column" << std::endl;
	UASSERTEQ(int, found, 40);
}
