// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"
#include <chrono>

#include "porting.h"
#include "sandbox/sandbox.h"
#include "sandbox/world.h"
#include <map>
#include <tuple>
#include <cmath>

using namespace sandbox;

class TestSandbox : public TestBase {
public:
	TestSandbox() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSandbox"; }

	void runTests(IGameDef *gamedef);

	void testRejects();
	void testConduction();
	void testMeltingAndPouring();
	void testFireNeedsAir();
	void testGasVents();
	void testDeterministic();
	void testWorldFire();
	void testWorldWalls();
	void testWorldMeltsAndSleeps();
	void testWorldSaves();
	void testWorldPours();
	void testWorldBudget();
	void testPuddleSpreads();
	void testDitchLevels();
	void testPowderHeaps();
	void testVapourLeaves();
	void testSpiritBurns();
	void testPoolSleeps();
	void testBudgetPicksBusiest();
	void testPitFills();
	void testPitOverflows();
};

static TestSandbox g_test_instance;

static const matter::Matter &table()
{
	static matter::Matter m = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	return m;
}

static const Parts AIR = { { "nitrogen", 0.79 }, { "oxygen", 0.21 } };

static Opts box(int32_t x, int32_t y, int32_t z, double block = 0.1, Parts air = AIR)
{
	return Opts{ table(), x, y, z, block, 293.15, 101325, air };
}

static double total(const Sandbox &s, const char *id)
{
	size_t k = table().index(id);
	double n = 0;
	for (int32_t y = 0; y < s.sizeY(); y++)
		for (int32_t z = 0; z < s.sizeZ(); z++)
			for (int32_t x = 0; x < s.sizeX(); x++)
				n += s.block({ x, y, z }).amounts[k];
	return n;
}

void TestSandbox::runTests(IGameDef *gamedef)
{
	TEST(testRejects);
	TEST(testConduction);
	TEST(testMeltingAndPouring);
	TEST(testFireNeedsAir);
	TEST(testGasVents);
	TEST(testDeterministic);
	TEST(testWorldFire);
	TEST(testWorldWalls);
	TEST(testWorldMeltsAndSleeps);
	TEST(testWorldSaves);
	TEST(testWorldPours);
	TEST(testWorldBudget);
	TEST(testPuddleSpreads);
	TEST(testDitchLevels);
	TEST(testPowderHeaps);
	TEST(testVapourLeaves);
	TEST(testSpiritBurns);
	TEST(testPoolSleeps);
	TEST(testBudgetPicksBusiest);
	TEST(testPitFills);
	TEST(testPitOverflows);
}

void TestSandbox::testRejects()
{
	EXCEPTION_CHECK(SandboxError, Sandbox(box(0, 1, 1)));
	EXCEPTION_CHECK(SandboxError, Sandbox(box(17, 1, 1)));
	Sandbox s(box(2, 2, 2));
	EXCEPTION_CHECK(SandboxError, s.fill({ 2, 0, 0 }, {}, 300));
	EXCEPTION_CHECK(SandboxError, s.fill({ 0, 0, 0 }, { { "iron", 0.7 }, { "copper", 0.7 } }, 300));
	EXCEPTION_CHECK(SandboxError, s.fill({ 0, 0, 0 }, { { "oxygen", 1 } }, 300)); // a gas, not a fill
	EXCEPTION_CHECK(matter::MatterError, s.fill({ 0, 0, 0 }, { { "unobtainium", 1 } }, 300));
	EXCEPTION_CHECK(SandboxError, s.step(0));
}

// A rod of copper and a rod of wood, each heated at one end: heat runs down
// the copper far faster.
void TestSandbox::testConduction()
{
	Sandbox s(box(2, 1, 6, 0.02));
	for (int32_t z = 0; z < 6; z++) {
		s.fill({ 0, 0, z }, { { "copper", 1 } }, 293.15);
		s.fill({ 1, 0, z }, { { "wood", 1 } }, 293.15);
	}
	s.heater({ 0, 0, 0 }, 20);
	s.heater({ 1, 0, 0 }, 20);
	s.step(60);
	double copper = s.block({ 0, 0, 5 }).temperature, wood = s.block({ 1, 0, 5 }).temperature;
	infostream << "sandbox: far end after 60 s, copper " << copper << " K, wood " << wood << " K" << std::endl;
	UASSERT(copper - 293.15 > 20 * (wood - 293.15)); // copper carries it; wood barely does
	UASSERT(s.block({ 1, 0, 0 }).temperature > s.block({ 0, 0, 0 }).temperature); // the wood keeps its heat where it's put
}

// Ice in a warm room melts, and the water runs down to the bottom.
void TestSandbox::testMeltingAndPouring()
{
	Sandbox s(box(1, 3, 1, 0.02));
	s.fill({ 0, 2, 0 }, { { "water", 0.5 } }, 263);
	size_t water = table().index("water");
	UASSERT(s.block({ 0, 2, 0 }).amounts[water] > 0);
	s.heater({ 0, 2, 0 }, 30);
	for (int k = 0; k < 120 && s.block({ 0, 0, 0 }).amounts[water] < 1e-3; k++)
		s.step(5);
	infostream << "sandbox: water at the bottom " << s.block({ 0, 0, 0 }).amounts[water] << " mol after " << s.time() << " s" << std::endl;
	UASSERT(s.block({ 0, 0, 0 }).amounts[water] > 1e-3);
	std::vector<BlockView> v = s.view();
	UASSERT(v[0].main == (int32_t)water && v[0].state == matter::State::Liquid);
}

// Wood lit in air burns: oxygen goes, carbon dioxide comes. Lit in pure
// nitrogen it only chars.
void TestSandbox::testFireNeedsAir()
{
	struct Fire { double co2, charcoal, temperature; };
	auto light = [](Parts air) {
		Sandbox s(box(3, 3, 3, 0.05, air));
		s.fill({ 1, 0, 1 }, { { "wood", 0.6 } }, 293.15);
		s.touch({ 1, 0, 1 }, 900);
		s.heater({ 1, 0, 1 }, 200);
		s.step(30);
		return Fire{ total(s, "carbon_dioxide"), total(s, "carbon"), s.block({ 1, 0, 1 }).temperature };
	};
	Fire air = light(AIR), nitrogen = light({ { "nitrogen", 1 } });
	infostream << "sandbox: in air CO₂ " << air.co2 << " mol at " << air.temperature << " K; in nitrogen CO₂ "
			<< nitrogen.co2 << " mol, charcoal " << nitrogen.charcoal << " mol at " << nitrogen.temperature << " K" << std::endl;
	UASSERT(air.co2 > 1e-4);                             // burning (most of it has already left by the top)
	UASSERT(air.temperature > nitrogen.temperature + 50); // and the fire's own heat
	UASSERT(nitrogen.co2 < 1e-9);                        // no air, no fire
	UASSERT(nitrogen.charcoal > 1e-3);                   // only charring
}

// Carbon dioxide released at the bottom of an open box leaves by the top.
void TestSandbox::testGasVents()
{
	Sandbox s(box(1, 4, 1));
	s.gas({ 0, 0, 0 }, { { "carbon_dioxide", 1 } }, 293.15);
	double start = total(s, "carbon_dioxide");
	s.step(600);
	double now = total(s, "carbon_dioxide");
	infostream << "sandbox: CO₂ " << start << " → " << now << " mol" << std::endl;
	UASSERT(now < start * 0.8);
	UASSERT(s.block({ 0, 3, 0 }).amounts[table().index("carbon_dioxide")] > 0); // it passed through the top
}

void TestSandbox::testDeterministic()
{
	auto run = [] {
		Sandbox s(box(3, 2, 3, 0.05));
		s.fill({ 1, 0, 1 }, { { "wood", 0.5 } }, 293.15);
		s.touch({ 1, 0, 1 }, 900);
		s.step(10);
		std::vector<double> out;
		for (const BlockView &v : s.view())
			out.push_back(v.temperature);
		return out;
	};
	UASSERT(run() == run());
}

// ---- the world's own blocks (WorldMatter), over a little plot of node names

namespace {

struct Plot
{
	std::map<std::tuple<int, int, int>, std::string> nodes; // anything missing is air
	std::map<std::tuple<int, int, int>, double> levels;    // a levelled block's (sprint 80)
	// sprint 81: a cut block's open sixteenths by layer, and the liquid kept in open space
	std::map<std::tuple<int, int, int>, sandbox::Room> rooms;
	std::map<std::tuple<int, int, int>, sandbox::Liquid> fills;
	// what's open: air, a liquid, a levelled heap; or as cut
	sandbox::Room roomAt(Pos p) const
	{
		auto cut = rooms.find({ p.x, p.y, p.z });
		if (cut != rooms.end())
			return cut->second;
		const std::string n = at(p.x, p.y, p.z);
		const bool open = n == "air" || n == "water" || n == "sea" || n == "puddle" || n == "ethanol" ||
				n == "ethanol_puddle" || n == "salt_heap" || n == "tin_liquid";
		return open ? sandbox::Room::open() : sandbox::Room::closed();
	}
	double ambient = 293.15;
	std::unique_ptr<WorldMatter> matter;
	std::map<std::string, int32_t> kinds;

	explicit Plot(std::vector<WorldMatter::Air> airs = { { 101325, 9.81, AIR } })
	{
		WorldMatter::Ground g{
			[this](Pos p) {
				auto it = nodes.find({ p.x, p.y, p.z });
				return kinds.at(it == nodes.end() ? "air" : it->second);
			},
			[this](Pos p, int32_t kind, double level) {
				nodes[{ p.x, p.y, p.z }] = matter->node(kind);
				levels[{ p.x, p.y, p.z }] = level;
			},
			[this](Pos) { return ambient; },
			[this](Pos p) {
				auto it = levels.find({ p.x, p.y, p.z });
				return it == levels.end() ? 1.0 : it->second;
			},
			[this](Pos p) { return roomAt(p); },
			[this](Pos p, int side) {
				// a cut block here is open through its sides where its layers are (a pit's walls aren't modelled)
				static const Pos D[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
				const sandbox::Room a = roomAt(p), b = roomAt({ p.x + D[side].x, p.y, p.z + D[side].z });
				uint16_t mask = 0;
				for (int y = 0; y < 16; y++)
					if (a.layers[y] && b.layers[y])
						mask |= 1 << y;
				return mask;
			},
			[this](Pos p) {
				auto it = fills.find({ p.x, p.y, p.z });
				return it == fills.end() ? sandbox::Liquid{} : it->second;
			},
			[this](Pos p, int32_t substance, double share, uint32_t) {
				if (substance < 0)
					fills.erase({ p.x, p.y, p.z });
				else
					fills[{ p.x, p.y, p.z }] = sandbox::Liquid{ substance, std::round(share * 255) / 255 };
			},
		};
		matter = std::make_unique<WorldMatter>(WorldMatter::Opts{ table(), airs, g });
		using matter::State;
		for (auto [node, id, share, state, form] : std::vector<std::tuple<const char *, const char *, double, State, bool>>{
				{ "air", "air", 1, State::Gas, true }, { "log", "wood", 0.4, State::Solid, true },
				{ "charcoal", "carbon", 0.15, State::Solid, true }, { "clay", "kaolinite", 0.6, State::Solid, true },
				{ "granite", "granite", 0.99, State::Solid, true }, { "water", "water", 1, State::Liquid, true },
				{ "ice", "water", 1, State::Solid, true }, { "tin", "tin", 1, State::Solid, true },
				{ "tin_liquid", "tin", 1, State::Liquid, true } })
			kinds[node] = matter->kind({ node, id, {}, share, state, form, std::string(node) == "water" }); // water flows by the engine, as in the game
		// sprint 80: liquids and powders the world's matter runs, full and levelled
		for (auto [node, id, state, leveled, loose] : std::vector<std::tuple<const char *, const char *, State, bool, bool>>{
				{ "sea", "water", State::Liquid, false, false }, { "puddle", "water", State::Liquid, true, false },
				{ "ethanol", "ethanol", State::Liquid, false, false }, { "ethanol_puddle", "ethanol", State::Liquid, true, false },
				{ "salt_heap", "salt", State::Solid, true, true } }) {
			WorldMatter::Spec spec{ node, id, {}, 1, state, true };
			spec.leveled = leveled;
			spec.loose = loose;
			kinds[node] = matter->kind(spec);
		}
	}
	// a floor of granite under y = 0, from −r to r
	void floor(int r)
	{
		for (int x = -r; x <= r; x++)
			for (int z = -r; z <= r; z++)
				nodes[{ x, -1, z }] = "granite";
	}
	// moles of a substance, condensed, in the blocks from −r to r (awake, held or as their kind)
	double held(const std::string &id, int r, int lo = -1, int hi = 4)
	{
		const size_t s = table().index(id);
		double n = 0;
		for (int x = -r; x <= r; x++)
			for (int y = lo; y <= hi; y++)
				for (int z = -r; z <= r; z++)
					if (auto mix = matter->mixture({ x, y, z }))
						n += mix->in(s, matter::State::Liquid) + mix->in(s, matter::State::Solid);
		return n;
	}
	double level(int x, int y, int z) const
	{
		auto it = levels.find({ x, y, z });
		return it == levels.end() ? 0 : it->second;
	}
	std::string at(int x, int y, int z) const
	{
		auto it = nodes.find({ x, y, z });
		return it == nodes.end() ? "air" : it->second;
	}
	// a hole: walls of `wall` around and under (0, 0, 0), open above
	void hole(const std::string &wall)
	{
		for (int x = -1; x <= 1; x++)
			for (int z = -1; z <= 1; z++) {
				nodes[{ x, -1, z }] = wall;
				if (x || z)
					nodes[{ x, 0, z }] = wall;
			}
	}
};

// The hottest any block gets over `seconds` (the flame, mostly), stepping a second at a time.
double peak(Plot &m, double seconds)
{
	double hottest = 0;
	for (double t = 0; t < seconds; t += 1) {
		m.matter->step(1);
		for (const WorldMatter::Awake &a : m.matter->awake({ -9, -9, -9 }, { 9, 9, 9 }))
			hottest = std::max(hottest, a.temperature);
	}
	return hottest;
}

} // namespace

// A log lit in a hole burns: hot, then charcoal, then gone to air; the
// ground around warms and wakes.
void TestSandbox::testWorldFire()
{
	Plot m;
	m.hole("granite");
	m.nodes[{ 0, 0, 0 }] = "log";
	m.matter->heat({ 0, 0, 0 }, 900);
	double hottest = peak(m, 900);
	infostream << "world: the fire peaked at " << hottest << " K; the log is now " << m.at(0, 0, 0)
			<< "; " << m.matter->awakeCount() << " blocks awake" << std::endl;
	UASSERT(hottest > 1000);                   // a flame over it
	UASSERT(m.matter->awakeCount() > 1);       // and warmed the ground around it
	// buried with no air, the same log only chars
	Plot buried;
	buried.hole("granite");
	buried.nodes[{ 0, 1, 0 }] = "granite";
	buried.nodes[{ 0, 0, 0 }] = "log";
	buried.matter->heat({ 0, 0, 0 }, 900);
	double smothered = peak(buried, 900);
	infostream << "world: buried, it peaked at " << smothered << " K" << std::endl;
	UASSERT(smothered < hottest - 100);
	// on a body with no air (sprint 96: the Moon), lit in the open, it only chars
	Plot airless({ { 0, 1.62, {} } });
	airless.hole("granite");
	airless.nodes[{ 0, 0, 0 }] = "log";
	airless.matter->heat({ 0, 0, 0 }, 900);
	double starved = peak(airless, 900);
	infostream << "world: with no air, it peaked at " << starved << " K" << std::endl;
	UASSERT(starved < hottest - 100);
}

// Clay insulates: a hot core walled in clay keeps its heat longer than one
// walled in granite, which conducts about three times as well.
void TestSandbox::testWorldWalls()
{
	Plot clay, stone;
	for (auto [m, wall] : { std::pair<Plot *, const char *>{ &clay, "clay" }, { &stone, "granite" } }) {
		for (int x = -1; x <= 1; x++)
			for (int y = -1; y <= 1; y++)
				for (int z = -1; z <= 1; z++)
					m->nodes[{ x, y, z }] = (x || y || z) ? wall : "granite";
		m->matter->heat({ 0, 0, 0 }, 1000);
		for (int k = 0; k < 1800; k++)
			m->matter->step(1);
	}
	double in_clay = clay.matter->temperature({ 0, 0, 0 }), in_stone = stone.matter->temperature({ 0, 0, 0 });
	infostream << "world: a 1000 K core after 30 min, walled in clay " << in_clay << " K, in granite " << in_stone << " K" << std::endl;
	UASSERT(in_clay > in_stone + 2); // a cubic metre of hot granite cools slowly either way: the walls change how slowly
}

// Ice warmed past 0 °C becomes water (the block changes to water's form);
// left alone, everything settles and sleeps.
void TestSandbox::testWorldMeltsAndSleeps()
{
	Plot m;
	m.hole("granite"); // (sprint 80: melted, it runs; on a pedestal it'd run off it)
	m.nodes[{ 0, 0, 0 }] = "ice";
	m.matter->heat({ 0, 0, 0 }, 360); // a cubic metre of ice: warming it this far melts it all (and not past boiling)
	for (int k = 0; k < 4000 && m.at(0, 0, 0) == "ice"; k++) {
		m.matter->step(1);
	}
	infostream << "world: the ice is now " << m.at(0, 0, 0) << ", below it " << m.at(0, -1, 0) << std::endl;
	// sprint 81: melted, it's water lying where the ice was (ice is 8% bigger)
	const auto melted = m.fills.find({ 0, 0, 0 });
	UASSERT(m.at(0, 0, 0) == "air" && melted != m.fills.end() && melted->second.share > 0.8);
	// a cubic metre of water takes a day to warm to the room: let the weather be its temperature, and it settles
	m.ambient = m.matter->temperature({ 0, 0, 0 });
	for (int k = 0; k < 2000 && m.matter->awakeCount(); k++)
		m.matter->step(1);
	UASSERT(m.matter->awakeCount() == 0);
	UASSERT(m.matter->temperature({ 0, 0, 0 }) == m.ambient); // asleep: the weather's temperature
}

void TestSandbox::testWorldSaves()
{
	Plot a;
	a.hole("granite");
	a.nodes[{ 0, 0, 0 }] = "log";
	a.matter->heat({ 0, 0, 0 }, 900);
	a.matter->step(20);
	Plot b;
	b.nodes = a.nodes;
	b.matter->load(a.matter->save());
	// (sprint 80: saved by state, a mixture's split between states rounds once on the way in; after that it's exact)
	Plot c;
	c.nodes = b.nodes;
	c.matter->load(b.matter->save());
	UASSERT(c.matter->save() == b.matter->save());
	a.matter->step(5);
	b.matter->step(5);
	UASSERT(std::fabs(a.matter->temperature({ 0, 0, 0 }) - b.matter->temperature({ 0, 0, 0 })) < 1e-6);
	EXCEPTION_CHECK(SandboxError, b.matter->load("nonsense"));
}

// Tin melted on a shelf runs down into the space below it.
void TestSandbox::testWorldPours()
{
	Plot m;
	m.nodes[{ 0, -2, 0 }] = "granite";
	for (int x = -1; x <= 1; x++) // a pit under the shelf to catch it (sprint 80: it runs)
		for (int z = -1; z <= 1; z++)
			if (x || z)
				m.nodes[{ x, -1, z }] = "granite";
	m.nodes[{ 0, 0, 0 }] = "tin";
	m.matter->heat({ 0, 0, 0 }, 650); // tin melts at 505 K
	for (int k = 0; k < 120 && m.at(0, -1, 0) == "air"; k++)
		m.matter->step(1);
	infostream << "world: after pouring, above " << m.at(0, 0, 0) << ", below " << m.at(0, -1, 0)
			<< (m.fills.find({ 0, -1, 0 }) != m.fills.end() ? " holding liquid" : "") << std::endl;
	// what melted ran down (sprint 81: lying in the pit's open space, or filling it as a block)
	const auto below = m.fills.find({ 0, -1, 0 });
	UASSERT(m.at(0, -1, 0) == "tin_liquid" || (below != m.fills.end() && below->second.substance == (int32_t)table().index("tin")));
	// (650 K only melts part of a cubic metre of tin before it drops to its melting point: the rest stays on the shelf)
}

// Sprint 79: what the world's matter costs. A slab of warm granite, 10 000
// blocks awake, stepped a second at a time: how long a step takes, which
// is the budget every awake block draws on (planning/sprint-79-matter.md).
void TestSandbox::testWorldBudget()
{
	Plot m;
	constexpr int SIDE = 22; // 22³ ≈ 10 000
	for (int x = 0; x < SIDE; x++)
		for (int y = -SIDE; y < 0; y++)
			for (int z = 0; z < SIDE; z++)
				m.nodes[{ x, y, z }] = "granite";
	for (int x = 0; x < SIDE; x++)
		for (int y = -SIDE; y < 0; y++)
			for (int z = 0; z < SIDE; z++)
				m.matter->heat({ x, y, z }, 400 + (x + y + z) % 7);
	m.matter->budget(1 << 20); // every one of them: this measures a full step
	const size_t awake = m.matter->awakeCount();
	m.matter->step(1); // the first wakes what's round them
	const auto start = std::chrono::steady_clock::now();
	constexpr int STEPS = 5;
	for (int i = 0; i < STEPS; i++)
		m.matter->step(1);
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / STEPS;
	infostream << "world: " << awake << " blocks awake (" << m.matter->awakeCount() << " now), " << ms
			<< " ms a 1 s step" << std::endl;
	UASSERT(awake >= 10000);
	// 22 ms on the machine it was tuned on (sprint 79: 142 before still
	// blocks stopped re-running their chemistry and breathing through
	// rock); twice that is a regression
	UTEST(ms < 45, "a step of 10 000 awake blocks took %f ms", ms);
}

// ---- sprint 80: liquids and powders that run

static const double WATER_PER_M3 = 997 / 0.018015; // mol

// A quarter of a cubic metre of water poured on a floor runs out into a
// puddle no thinner than a sixteenth, and none of it is lost.
void TestSandbox::testPuddleSpreads()
{
	Plot m;
	m.floor(8);
	const double poured = 0.25 * WATER_PER_M3;
	m.matter->pour({ 0, 0, 0 }, table().index("water"), poured, 293.15);
	for (int k = 0; k < 60; k++)
		m.matter->step(1);
	int wet = 0;
	double deepest = 0;
	for (int x = -8; x <= 8; x++)
		for (int z = -8; z <= 8; z++)
			if (auto f = m.fills.find({ x, 0, z }); f != m.fills.end()) {
				wet++;
				deepest = std::max(deepest, f->second.share);
			}
	const double now = m.held("water", 8, 0, 0);
	infostream << "world: a quarter cubic metre of water covers " << wet << " blocks, the deepest "
			<< deepest * 64 << "/64; " << now / poured * 100 << "% of it still there; "
			<< m.matter->awakeCount() << " awake" << std::endl;
	UASSERT(wet >= 3 && wet <= 4.5 / 0.0625);   // spread, but no thinner than a sixteenth
	UASSERT(deepest < 0.25);                   // it ran out of the block it landed in
	UASSERT(std::fabs(now / poured - 1) < 0.02); // a little evaporates in a minute; nothing vanishes
}

// Water poured in at one end of a ditch runs along it and comes level.
void TestSandbox::testDitchLevels()
{
	Plot m;
	for (int x = -1; x <= 10; x++)
		for (int z = -1; z <= 1; z++) {
			m.nodes[{ x, -1, z }] = "granite";
			if (z != 0 || x < 0 || x > 9)
				m.nodes[{ x, 0, z }] = "granite";
		}
	for (int k = 0; k < 40; k++) { // 40 s of a 50-litre-a-second pour into its end
		m.matter->pour({ 0, 0, 0 }, table().index("water"), 0.05 * WATER_PER_M3, 293.15);
		m.matter->step(1);
	}
	for (int k = 0; k < 60; k++)
		m.matter->step(1);
	double lo = 2, hi = 0;
	for (int x = 0; x <= 9; x++) {
		const double h = m.matter->mixture({ x, 0, 0 })->in(table().index("water"), matter::State::Liquid) / WATER_PER_M3;
		lo = std::min(lo, h);
		hi = std::max(hi, h);
	}
	infostream << "world: a ditch of 10 filled with 2 m³: from " << lo << " to " << hi << " deep" << std::endl;
	UASSERT(lo > 0.15 && hi - lo < 0.05); // level along its length
}

// Salt poured in one place heaps: it slides until its sides are no steeper
// than its angle of repose, so the middle stands higher than the edges.
void TestSandbox::testPowderHeaps()
{
	Plot m;
	m.floor(8);
	const size_t salt = table().index("salt");
	const double per_m3 = 2165 / 0.05844;
	for (int k = 0; k < 30; k++) {
		m.matter->pour({ 0, 2, 0 }, salt, 0.1 * per_m3, 293.15);
		m.matter->step(1);
	}
	for (int k = 0; k < 30; k++)
		m.matter->step(1);
	auto height = [&](int x, int z) {
		double h = 0;
		for (int y = 0; y < 4; y++)
			h += m.matter->mixture({ x, y, z })->in(salt, matter::State::Solid) / per_m3;
		return h;
	};
	infostream << "world: 3 m³ of salt heaped: middle " << height(0, 0) << ", beside " << height(1, 0)
			<< ", two away " << height(2, 0) << ", three away " << height(3, 0) << std::endl;
	UASSERT(height(0, 0) > height(2, 0) + 0.3); // a heap, not a flat spread
	UASSERT(height(1, 0) > 0);                  // and it slid
	UASSERT(std::fabs(m.held("salt", 8, -1, 4) / (3 * per_m3) - 1) < 1e-6); // every grain of it
}

// Ethanol boiled by heat leaves its block as vapour, and the air carries
// it off: cooled again, the block doesn't fill back up with liquid.
void TestSandbox::testVapourLeaves()
{
	Plot m;
	m.hole("granite");
	m.nodes[{ 0, 0, 0 }] = "ethanol_puddle";
	m.levels[{ 0, 0, 0 }] = 1.0 / 16; // 50 litres
	const double before = m.held("ethanol", 1, 0, 0);
	for (int k = 0; k < 120; k++) { // held at 360 K, over its boiling point, for two minutes
		m.matter->touch({ 0, 0, 0 }, 360);
		m.matter->step(1);
	}
	const double hot = m.held("ethanol", 1, 0, 0);
	for (int k = 0; k < 600; k++)
		m.matter->step(1);
	const double cooled = m.held("ethanol", 1, 0, 0);
	infostream << "world: ethanol boiled for 2 min: " << hot / before * 100 << "% left; cooled 10 min: "
			<< cooled / before * 100 << "%; the block is " << m.at(0, 0, 0) << std::endl;
	UASSERT(hot < before * 0.5);
	// what boiled off is carried off, not condensed back (a little that cooled in the
	// hole's still air falls back as dew: a fifth of the start, not the 40% it was)
	UASSERT(cooled < before * 0.2);
}

// Warm ethanol under a flame catches: its vapour burns in the air over it.
void TestSandbox::testSpiritBurns()
{
	Plot m;
	m.hole("granite");
	m.nodes[{ 0, 0, 0 }] = "ethanol_puddle";
	m.levels[{ 0, 0, 0 }] = 0.5;
	bool burnt = false;
	for (int k = 0; k < 30 && !burnt; k++) {
		m.matter->touch({ 0, 1, 0 }, 1200); // a torch's flame over it, kept lit
		m.matter->step(1);
		for (const auto &[at, reaction] : m.matter->happened())
			burnt = burnt || table().reactions()[reaction].id.find("ethanol") != std::string::npos;
	}
	infostream << "world: ethanol under a flame " << (burnt ? "caught" : "didn't catch") << "; the spirit's at "
			<< m.matter->temperature({ 0, 0, 0 }) << " K" << std::endl;
	UASSERT(burnt);
}

// A pool poured, left alone, settles and sleeps: nothing awake, its matter
// kept exactly (levels are drawn in sixty-fourths; what's in them isn't rounded).
void TestSandbox::testPoolSleeps()
{
	Plot m;
	m.floor(3);
	for (int x = -3; x <= 3; x++)
		for (int z = -3; z <= 3; z++)
			if (std::abs(x) == 3 || std::abs(z) == 3)
				m.nodes[{ x, 0, z }] = "granite";
	const double poured = 3.3 * WATER_PER_M3; // a little over 13 sixty-fourths over the 25 blocks inside
	// over the pool, not all in one block (sprint 82: more than a block holds rises out of it, over the walls)
	for (int x = -2; x <= 2; x++)
		for (int z = -2; z <= 2; z++)
			m.matter->pour({ x, 0, z }, table().index("water"), poured / 25, 293.15);
	m.matter->step(5);
	int k = 0;
	for (; k < 600 && m.matter->awakeCount() > 0; k++)
		m.matter->step(1);
	const double now = m.held("water", 3, 0, 0);
	infostream << "world: a pool slept after " << k << " s, " << m.matter->heldCount() << " blocks holding their own; "
			<< now / poured * 100 << "% of the water" << std::endl;
	UASSERT(m.matter->awakeCount() == 0);
	UASSERT(std::fabs(now / poured - 1) < 0.01);
	// and saved and loaded, it's still all there
	Plot again;
	again.nodes = m.nodes;
	again.levels = m.levels;
	again.matter->load(m.matter->save());
	UASSERT(std::fabs(again.held("water", 3, 0, 0) / now - 1) < 1e-9);
}

// More awake than the budget: a step runs only the busiest, and costs
// about what the budget's worth of blocks does.
void TestSandbox::testBudgetPicksBusiest()
{
	Plot m;
	constexpr int SIDE = 30; // 27 000 blocks of warm granite
	for (int x = 0; x < SIDE; x++)
		for (int y = -SIDE; y < 0; y++)
			for (int z = 0; z < SIDE; z++) {
				m.nodes[{ x, y, z }] = "granite";
				m.matter->heat({ x, y, z }, 400);
			}
	m.matter->budget(4096);
	m.matter->step(1);
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < 5; i++)
		m.matter->step(1);
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 5;
	infostream << "world: " << m.matter->awakeCount() << " awake, a budget of 4096: " << ms << " ms a step" << std::endl;
	UASSERT(m.matter->awakeCount() > 20000);
	UTEST(ms < 30, "a budgeted step took %f ms", ms);
}

// Sprint 81: liquid lies in a sculpted block's open sixteenths. A floor of
// granite with a pit cut a quarter deep into one block of it: water poured
// beside it runs in and fills the pit, the granite staying granite.
void TestSandbox::testPitFills()
{
	Plot m;
	m.floor(4);
	sandbox::Room pit = sandbox::Room::closed();
	for (int y = 12; y < 16; y++)
		pit.layers[y] = 256; // its top quarter is cut away
	m.rooms[{ 1, -1, 0 }] = pit;
	m.matter->pour({ 0, 0, 0 }, table().index("water"), 0.2 * WATER_PER_M3, 293.15);
	for (int k = 0; k < 60; k++)
		m.matter->step(1);
	const auto in = m.fills.find({ 1, -1, 0 });
	infostream << "world: a pit a quarter deep beside a pour holds "
			<< (in == m.fills.end() ? 0 : in->second.share) * 100 << "% of its room; the block is " << m.at(1, -1, 0) << std::endl;
	UASSERT(m.at(1, -1, 0) == "granite");
	UASSERT(in != m.fills.end() && in->second.share > 0.78); // all of it (0.2 of its 0.25 m³): the lowest place around
	UASSERT(std::fabs(m.held("water", 4, -1, 0) / (0.2 * WATER_PER_M3) - 1) < 0.01);
}

// Sprint 82: a pit filled past full overflows. Twice what a pit a quarter
// deep holds, put in it: the rest rises out and runs over the floor.
void TestSandbox::testPitOverflows()
{
	Plot m;
	m.floor(4);
	sandbox::Room pit = sandbox::Room::closed();
	for (int y = 12; y < 16; y++)
		pit.layers[y] = 256;
	m.rooms[{ 0, -1, 0 }] = pit;
	// into the pit's open space, more than it holds (add: a pour would make the granite a heap)
	m.matter->add({ 0, -1, 0 }, table().index("water"), 0.5 * WATER_PER_M3, 293.15);
	for (int k = 0; k < 60; k++)
		m.matter->step(1);
	const auto in = m.fills.find({ 0, -1, 0 });
	double beyond = 0;
	for (const auto &[at, f] : m.fills)
		if (at != std::make_tuple(0, -1, 0))
			beyond += f.share;
	infostream << "world: a pit filled twice full holds " << (in == m.fills.end() ? 0 : in->second.share) * 100
			<< "%; " << beyond << " blocks' worth of fills beyond it" << std::endl;
	UASSERT(in != m.fills.end() && in->second.share > 0.95);
	UASSERT(beyond > 0.1); // the rest ran out over the floor
	UASSERT(std::fabs(m.held("water", 4, -1, 0) / (0.5 * WATER_PER_M3) - 1) < 0.01);
}
