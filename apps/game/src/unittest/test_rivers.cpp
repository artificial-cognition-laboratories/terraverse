// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "porting.h"
#include "rivers/basin.h"
#include "rivers/rivers.h"
#include "terragen/grid.h"
#include "terragen/sphere.h"
#include "terragen/world.h"
#include "weather/weather.h"
#include <cmath>

using rivers::Network;

class TestRivers : public TestBase {
public:
	TestRivers() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestRivers"; }

	void runTests(IGameDef *gamedef);

	void testSettledStaysSettled();
	void testStormTravelsDown();
	void testRefusesLoops();
	void testSaves();
	void testWeatherFillsRivers();
	void testFlowMakesTheWater();
	void testToTheSea();
	void testForcing();
	void testDamFillsValley();
	void testSaddleSpillsFirst();
	void testOpenRiverDoesntRise();
	void testFloodSpreads();
	void testEvaporation();
	void testClosedLake();
};

static TestRivers g_test_instance;

static const double HOUR = 3600, DAY = 86400;

void TestRivers::runTests(IGameDef *gamedef)
{
	TEST(testSettledStaysSettled);
	TEST(testStormTravelsDown);
	TEST(testRefusesLoops);
	TEST(testSaves);
	TEST(testWeatherFillsRivers);
	TEST(testFlowMakesTheWater);
	TEST(testToTheSea);
	TEST(testForcing);
	TEST(testDamFillsValley);
	TEST(testSaddleSpillsFirst);
	TEST(testOpenRiverDoesntRise);
	TEST(testFloodSpreads);
	TEST(testEvaporation);
	TEST(testClosedLake);
}

// A river of four cells, a day across each, a km² each: 0 → 1 → 2 → 3 → the sea
static Network chain()
{
	return Network({ 1, 2, 3, -1 }, { 1e6, 1e6, 1e6, 1e6 }, { DAY, DAY, DAY, DAY }, 365 * DAY);
}

// Settled under steady rain, every cell lets go what comes into it: the
// mouth carries all four cells' runoff, and stepping changes nothing.
void TestRivers::testSettledStaysSettled()
{
	Network n = chain();
	const std::vector<rivers::Shed> rain(4, rivers::Shed(1e-8)); // m/s off each cell
	n.settle(rain);
	UASSERT(std::fabs(n.discharge(3) - 4 * 1e-8 * 1e6) < 1e-12);
	UASSERT(std::fabs(n.discharge(0) - 1e-8 * 1e6) < 1e-12);
	for (int k = 0; k < 40; k++)
		n.step(3 * HOUR, rain);
	UASSERT(std::fabs(n.discharge(3) / (4 * 1e-2) - 1) < 1e-9);
	UASSERT(std::fabs(n.mean(3) / (4 * 1e-2) - 1) < 1e-9);
}

// A storm over the head: its peak reaches the mouth later than it leaves
// the head, lower and spread out, and no water is lost on the way.
void TestRivers::testStormTravelsDown()
{
	Network n = chain();
	const std::vector<rivers::Shed> dry(4);
	std::vector<rivers::Shed> storm = dry;
	storm[0] = rivers::Shed(1e-5); // 36 mm/h, for six hours
	n.settle(dry);
	double head_peak = 0, mouth_peak = 0, head_at = 0, mouth_at = 0;
	double t = 0;
	for (int k = 0; k < 160; k++) {
		const double dt = 3 * HOUR;
		n.step(dt, k < 2 ? storm : dry);
		t += dt;
		if (n.discharge(0) > head_peak)
			head_peak = n.discharge(0), head_at = t;
		if (n.discharge(3) > mouth_peak)
			mouth_peak = n.discharge(3), mouth_at = t;
	}
	infostream << "rivers: a storm's peak at the head " << head_peak << " m³/s at " << head_at / HOUR << " h, at the mouth "
			<< mouth_peak << " m³/s at " << mouth_at / HOUR << " h" << std::endl;
	UASSERT(mouth_at > head_at + DAY);
	UASSERT(mouth_peak < head_peak);
	const double rained = 1e-5 * 1e6 * 6 * HOUR;
	UASSERT(std::fabs(n.rained() / rained - 1) < 1e-9);
	UASSERT(std::fabs((n.stored() + n.toSea()) / rained - 1) < 1e-9); // all of it held or at sea
}

void TestRivers::testRefusesLoops()
{
	EXCEPTION_CHECK(rivers::RiversError, Network({ 1, 0 }, { 1, 1 }, { 1, 1 }, 1));
}

// Saved and loaded, a network carries on exactly as one never saved.
void TestRivers::testSaves()
{
	Network a = chain(), b = chain();
	std::vector<rivers::Shed> rain{ { 1e-6, 1e-7 }, {}, { 2e-6, 0 }, {} };
	a.settle(rain);
	a.step(HOUR, rain);
	b.load(a.save());
	a.step(5 * HOUR, rain);
	b.step(5 * HOUR, rain);
	UASSERT(a.save() == b.save());
	Network other({ -1 }, { 1 }, { 1 }, 1);
	EXCEPTION_CHECK(rivers::RiversError, other.load(a.save()));
}

// A real world's rivers, run on its weather's own steps from its settling
// year: water's conserved, the grid's rivers carry far more than a dry
// cell, and their flow changes with the weather.
void TestRivers::testWeatherFillsRivers()
{
	static const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	terragen::World world(terragen::WorldCode::decode("TF23.dGVycmE"));
	const terragen::Grid &grid = world.grid();
	const terragen::Sphere &sphere = terragen::gridSphere();
	std::optional<Network> net;
	bool settled = false;
	int steps = 0;
	auto each = [&](const weather::Weather &w, double dt) {
		const std::vector<rivers::Shed> runoff = rivers::runoff(w, grid, sphere, w.now());
		if (!settled) {
			net->settle(rivers::climate(grid, &w, &sphere));
			settled = true;
		} else {
			net->step(dt, runoff);
		}
		steps++;
	};
	double now = weather::clock(3, 0.5);
	weather::Weather::Opts opts{ world, table, "" };
	opts.each_step = each;
	// (made before the weather: its settling year runs the rivers too)
	net.emplace(Network::ofGrid(grid, sphere, 6.371e6, 1.0, 365 * DAY));
	auto w = weather::Weather::open(opts, now);
	UASSERT(steps > 100);
	// the rivers' cells against the rest
	double river = 0, other = 0;
	int rivers_n = 0, others_n = 0;
	for (int32_t i = 0; i < grid.size(); i++) {
		if (grid.base[i] < 0)
			continue;
		(grid.river(i) ? river : other) += net->mean(i);
		(grid.river(i) ? rivers_n : others_n)++;
	}
	infostream << "rivers: " << rivers_n << " river cells average " << river / std::max(1, rivers_n) << " m³/s, "
			<< others_n << " others " << other / std::max(1, others_n) << " m³/s" << std::endl;
	UASSERT(rivers_n > 0 && river / rivers_n > 5 * other / std::max(1, others_n));
	// its lakes: how many the air takes everything that comes in from (closed basins)
	int lakes = 0, closed = 0;
	for (int32_t i = 0; i < grid.size(); i++)
		if (grid.base[i] >= 0 && grid.lake[i]) {
			lakes++;
			closed += net->mean(i) < 1e-3;
		}
	infostream << "rivers: " << lakes << " lake cells, " << closed << " closed (nothing goes on from them)" << std::endl;
	// a week on: rivers move with the weather, and every m³ is accounted for
	const double held = net->stored(), sea = net->toSea(), rained = net->rained(), gone = net->evaporated();
	std::vector<double> before(grid.size());
	for (int32_t i = 0; i < grid.size(); i++)
		before[i] = net->discharge(i);
	w->advance(now + 7 * DAY);
	double changed = 0;
	for (int32_t i = 0; i < grid.size(); i++)
		changed = std::max(changed, std::fabs(net->discharge(i) - before[i]) / std::max(before[i], 1.0));
	UASSERT(changed > 0.05);
	const double in = net->rained() - rained,
			out = (net->stored() - held) + (net->toSea() - sea) + (net->evaporated() - gone);
	UASSERT(std::fabs(out - in) <= 1e-9 * std::max(1.0, rained));
}

// A grid river's column: at its mean flow it stands as generated; a flood
// raises and quickens it (never past the block over it), a drought lowers
// and slows it (never under its bed); a lake stands still.
void TestRivers::testFlowMakesTheWater()
{
	const terragen::Sphere &sphere = terragen::gridSphere();
	terragen::Ground g{};
	g.kind = terragen::Water::River;
	g.terrain = 10;
	g.water = g.river_level = 12; // two blocks deep
	g.river_cell = 3;
	g.river_width = 8 * terragen::ONE;
	g.river_dist = 0;
	g.river_slope = terragen::ONE / 200;
	const terragen::Place at(terragen::PLACE_ONE, 0, 0);
	Network n = chain();
	n.settle(std::vector<rivers::Shed>(4, rivers::Shed(1e-8, 1e-8)));
	const rivers::Flow mean = rivers::flowAt(n, sphere, g, at);
	UASSERT(std::fabs(mean.ratio - 1) < 1e-9 && std::fabs(mean.rise) < 1e-9 && mean.speed > 0);
	for (int k = 0; k < 8; k++)
		n.step(DAY, std::vector<rivers::Shed>(4, rivers::Shed(1e-6, 1e-7))); // a hundred times the rain
	const rivers::Flow flood = rivers::flowAt(n, sphere, g, at);
	UASSERT(flood.ratio > 10 && flood.rise == 15.0 / 16 && flood.speed > 2 * mean.speed);
	UASSERT(flood.surge > 2); // over its banks: a flood's
	UASSERT(std::fabs(mean.surge) < 1e-9);
	for (int k = 0; k < 30; k++)
		n.step(DAY, std::vector<rivers::Shed>(4));
	const rivers::Flow dry_season = rivers::flowAt(n, sphere, g, at); // its ground water still running
	UASSERT(dry_season.ratio > 0.2 && dry_season.rise < 0);
	for (int k = 0; k < 300; k++)
		n.step(DAY, std::vector<rivers::Shed>(4));
	const rivers::Flow dry = rivers::flowAt(n, sphere, g, at);
	infostream << "rivers: mean " << mean.speed << " nodes/s; flood x" << flood.ratio << " +" << flood.rise
			<< " blocks (" << flood.surge << " unbounded) at " << flood.speed << "; a dry month x" << dry_season.ratio << " " << dry_season.rise << "; a dry year x" << dry.ratio << " " << dry.rise << " at " << dry.speed << std::endl;
	UASSERT(dry.ratio < 0.5 && dry.rise < -0.5 && dry.rise >= -2 && dry.speed < mean.speed);
	g.kind = terragen::Water::Lake;
	UASSERT(rivers::flowAt(n, sphere, g, at).rise == 0);
}

// Water leaving the head crosses every cell below it: four days to the sea.
void TestRivers::testToTheSea()
{
	const Network n = chain();
	UASSERT(n.toSeaTime(0) == 4 * DAY && n.toSeaTime(3) == DAY);
}

// A storm put by hand rains on the cells within its reach, on saturated
// ground; a drought there sheds nothing; the cells past it keep the weather's.
void TestRivers::testForcing()
{
	const terragen::Sphere &sphere = terragen::gridSphere();
	std::vector<rivers::Shed> shed(sphere.size(), rivers::Shed(1e-8, 1e-9));
	rivers::Forcing storm;
	storm.centre = terragen::Place(sphere.centre(0));
	storm.radius = 0.05;
	storm.rain = 1e-5;
	rivers::force(shed, sphere, storm);
	UASSERT(std::fabs(shed[0].quick / 1e-5 - 0.9) < 1e-9);
	int touched = 0;
	for (const rivers::Shed &s : shed)
		touched += s.quick > 1e-6;
	UASSERT(touched > 1 && touched < sphere.size() / 100);
	rivers::Forcing drought = storm;
	drought.rain = 0;
	rivers::force(shed, sphere, drought);
	UASSERT(shed[0].quick == 0 && shed[0].soak == 0);
	UASSERT(shed[sphere.size() - 1].quick == 1e-8);
}

// A valley running down +x: a V across it (walls rising a block a column
// from its middle), its floor falling a block every 4 columns, a river in
// it a block deep (the pool between steps), and what's built on it
static int32_t valley(int32_t x, int32_t z)
{
	const int32_t floor = 20 - x / 4;
	return floor + std::abs(z);
}

// A dam across the valley at x = 10, 6 blocks over its floor there: the pool
// above it fills the valley back up to its crest, and pours over it.
void TestRivers::testDamFillsValley()
{
	auto top = [](int32_t x, int32_t z) {
		if (x == 10 && std::abs(z) <= 30)
			return 20 - 10 / 4 + 6; // the dam: 24 (the floor there is 18)
		const int32_t ground = valley(x, z);
		return z == 0 && x >= 8 && x < 12 ? ground + 1 : ground; // the river's pool from 8 to 11, its water at 19
	};
	const rivers::Basin b = rivers::Basin::find(9, 0, 19, top, 100000);
	UASSERT(b.dammed() && !b.capped());
	UASSERT(b.spill() == 24);
	UASSERT(b.outX() > 10); // over the crest, down the far side
	UASSERT(b.volume(19) == 0 && b.volume(24) > b.volume(22) && b.volume(22) > 0);
	UASSERT(b.area(23.5) > b.area(20.5)); // a V: wider as it rises
	const double half = b.levelOf(b.volume(24) / 2);
	UASSERT(half > 19 && half < 24 && std::fabs(b.volume(half) / (b.volume(24) / 2) - 1) < 1e-6);
	// nothing past the dam is in it (its crest is: under the water once it pours over)
	for (const rivers::Basin::Column &c : b.columns())
		UASSERT(c.x < 10 || (c.x == 10 && c.top == 24));
	// it pours out only over its spill, and over it by a weir's 1.5 power:
	// its crest a block wide where the valley floor meets the dam
	UASSERT(b.outflow(24) == 0 && b.outflow(23) == 0);
	const double q1 = b.outflow(25), q2 = b.outflow(26);
	UASSERT(q1 > 0 && q2 > q1 * 2.5); // (wider and deeper as it rises)
}

// The same dam, with a saddle in the valley's side lower than its crest:
// the lake pours out over the saddle.
void TestRivers::testSaddleSpillsFirst()
{
	auto top = [](int32_t x, int32_t z) {
		if (x == 10 && std::abs(z) <= 30)
			return 30;
		if (x == 4 && z >= 1 && z < 40)
			return std::min(valley(x, z), 21 + (z > 25 ? 25 - z : 0)); // a notch in the north wall, falling away past it
		return z == 0 && x >= 8 && x < 12 ? valley(x, z) + 1 : valley(x, z);
	};
	const rivers::Basin b = rivers::Basin::find(9, 0, 19, top, 100000);
	UASSERT(b.dammed() && b.spill() == 21 && b.outX() == 4);
	// the saddle passes it, never the dam: its crest is 9 blocks higher
	for (const rivers::Basin::Rim &r : b.rim())
		UASSERT(r.x == 4 && r.top == 21);
	UASSERT(b.outflow(21) == 0 && b.outflow(22) > 0);
}

// A pool with the river's next step down beside it isn't dammed: it pours
// on at its own level.
void TestRivers::testOpenRiverDoesntRise()
{
	auto top = [](int32_t x, int32_t z) {
		const int32_t ground = valley(x, z);
		return z == 0 ? ground + 1 : ground; // the river all along
	};
	const rivers::Basin b = rivers::Basin::find(9, 0, 19, top, 100000);
	UASSERT(!b.dammed() && b.spill() == 19 && b.columns().empty());
}

// A river in a flat-floored valley (a floodplain a block over its water,
// walls past it), its surface risen 2 blocks: the flood covers the river and
// its floodplain, not the walls, and not the lower ground downstream.
void TestRivers::testFloodSpreads()
{
	auto top = [](int32_t x, int32_t z) {
		if (x > 20)
			return 10; // downstream: lower, another pool's
		if (x < -20)
			return 30; // upstream: the next step up, its own pool
		if (z == 0)
			return 12; // the river's water
		return std::abs(z) <= 6 ? 13 : 13 + std::abs(z) - 6; // the floodplain, then the walls
	};
	const rivers::Basin b = rivers::Basin::spread(0, 0, 12, 14, top, 100000);
	bool plain = false;
	for (const rivers::Basin::Column &c : b.columns()) {
		UASSERT(c.x <= 20 && std::abs(c.z) <= 6);
		plain = plain || std::abs(c.z) == 6;
	}
	UASSERT(plain && !b.capped());
	UASSERT(b.volume(14) > b.volume(13) && b.volume(13) > 0);
}

// Open water's evaporation: a few mm a day on a warm dry windy day, more
// than on a cool damp still one, none into saturated air.
void TestRivers::testEvaporation()
{
	fluid::Sample hot{};
	hot.temperature = 273.15 + 30;
	hot.humidity = 0.3;
	hot.wind_u = 4;
	fluid::Sample cool = hot;
	cool.temperature = 273.15 + 8;
	cool.humidity = 0.8;
	cool.wind_u = 0.5;
	fluid::Sample fog = cool;
	fog.humidity = 1;
	const double h = weather::openWaterEvaporation(hot) * 24, c = weather::openWaterEvaporation(cool) * 24;
	infostream << "rivers: evaporation " << h << " mm/day hot and dry, " << c << " cool and damp" << std::endl;
	UASSERT(h > 5 && h < 30 && c > 0 && c < 1.5 && h > 10 * c);
	UASSERT(weather::openWaterEvaporation(fog) == 0);
}

// A river into a lake cell (cell 2 of the chain): the lake evaporating
// more than the river brings lets nothing go on, and its mouth runs dry; in
// a wetter year (less taken off it) it fills and runs on. Nothing's lost
// that isn't at sea, held or evaporated.
void TestRivers::testClosedLake()
{
	Network n = chain();
	std::vector<rivers::Shed> dry(4, rivers::Shed(1e-8, 0));
	dry[2] = rivers::Shed(0, 0, 5e-8); // 4 mm a day off it, against 0.9 mm a day off each cell above
	n.settle(dry);
	UASSERT(n.discharge(2) == 0 && n.discharge(3) == std::fabs(1e-8 * 1e6));
	const double held = n.stored(); // (settled: what it starts with, never rained)
	for (int k = 0; k < 200; k++)
		n.step(3 * HOUR, dry);
	UASSERT(n.discharge(2) == 0 && n.evaporated() > 0);
	std::vector<rivers::Shed> wet = dry;
	wet[2].lose = 5e-9;
	for (int k = 0; k < 200; k++)
		n.step(3 * HOUR, wet);
	UASSERT(n.discharge(2) > 0.01);
	UASSERT(std::fabs((n.stored() - held + n.toSea() + n.evaporated()) / n.rained() - 1) < 1e-9);
}
