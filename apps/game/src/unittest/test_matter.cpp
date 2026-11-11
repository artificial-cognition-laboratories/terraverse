// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "matter/grains.h"
#include "matter/matter.h"
#include "matter/mixture.h"
#include "porting.h"

using namespace matter;

class TestMatter : public TestBase {
public:
	TestMatter() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMatter"; }

	void runTests(IGameDef *gamedef);

	void testLoads();
	void testWater();
	void testOtherWorlds();
	void testCurves();
	void testReactionsBalance();
	void testBurning();
	void testDrivenReactions();
	void testHeatGated();
	void testRocksAndOres();
	void testAlloysAndCraft();
	void testMeltingTakesTime();
	void testHeatsFromData();
	void testWetChemistry();
	void testGrains();
};

static TestMatter g_test_instance;

static Matter table()
{
	return Matter::load(porting::path_share + DIR_DELIM "matter");
}

void TestMatter::runTests(IGameDef *gamedef)
{
	TEST(testLoads);
	TEST(testWater);
	TEST(testOtherWorlds);
	TEST(testCurves);
	TEST(testReactionsBalance);
	TEST(testBurning);
	TEST(testDrivenReactions);
	TEST(testHeatGated);
	TEST(testRocksAndOres);
	TEST(testAlloysAndCraft);
	TEST(testMeltingTakesTime);
	TEST(testHeatsFromData);
	TEST(testWetChemistry);
	TEST(testGrains);
}

void TestMatter::testLoads()
{
	Matter m = table();
	UASSERTEQ(size_t, m.elements().size(), 118u);
	UASSERT(m.element("Fe").number == 26);
	UASSERT(std::fabs(m.substance("water").molar_mass - 18.015) < 0.01); // from its elements
	EXCEPTION_CHECK(MatterError, m.substance("unobtainium"));
}

void TestMatter::testWater()
{
	const Substance &w = table().substance("water");
	UASSERT(w.state(263, ATMOSPHERE) == State::Solid);
	UASSERT(w.state(300, ATMOSPHERE) == State::Liquid);
	UASSERT(w.state(400, ATMOSPHERE) == State::Gas);
	UASSERT(w.state(700, 25e6) == State::Supercritical);
	UASSERT(w.state(300, 1000) == State::Gas);           // a near vacuum: it boils at room temperature
	UASSERT(w.state(273.1, 50e6) == State::Liquid);      // squeezed ice melts: the melting line leans back
	// CoolProp's curve (sprint 79): 373.124 K, not the old 373.15
	UASSERT(std::fabs(*w.boilingPoint(ATMOSPHERE) - 373.15) < 0.1);
	UASSERT(*w.boilingPoint(30000) < 350);               // on a high mountain it boils cooler
	UASSERT(!w.boilingPoint(100));                       // below the triple point there's no liquid
}

// What's liquid where: the seas of other worlds fall out of the same table.
void TestMatter::testOtherWorlds()
{
	Matter m = table();
	UASSERT(m.substance("methane").state(94, 1.5e5) == State::Liquid);      // Titan's lakes
	UASSERT(m.substance("nitrogen").state(94, 1.5e5) == State::Gas);        // Titan's air
	UASSERT(m.substance("carbon_dioxide").state(190, ATMOSPHERE) == State::Solid); // dry ice sublimes at 194.7 K
	UASSERT(m.substance("carbon_dioxide").state(230, ATMOSPHERE) == State::Gas); // dry ice: never liquid at 1 atm
	UASSERT(m.substance("carbon_dioxide").state(230, 1e6) == State::Liquid);
	UASSERT(m.substance("carbon_dioxide").state(740, 9.2e6) == State::Supercritical); // Venus's surface
	UASSERT(m.substance("mercury").state(293, ATMOSPHERE) == State::Liquid);
	UASSERT(m.substance("iron").state(293, ATMOSPHERE) == State::Solid);
	UASSERT(m.substance("iron").state(2000, ATMOSPHERE) == State::Liquid);
	UASSERT(m.substance("tin").state(600, ATMOSPHERE) == State::Liquid);    // melts in a campfire
	UASSERT(m.substance("copper").state(600, ATMOSPHERE) == State::Solid);  // copper doesn't
	UASSERT(m.substance("carbon").state(3000, ATMOSPHERE) == State::Solid);
}

void TestMatter::testCurves()
{
	const Matter m = table(); // kept: a loop over a temporary's substances reads freed memory
	for (const Substance &s : m.substances()) {
		// the curves meet at the triple point and rise with temperature;
		// where CoolProp's curve is used (sprint 79) it's its measurements,
		// the anchors the table's: they agree within a few percent
		const double close = s.saturation ? 0.03 : 1e-6;
		UTEST(std::fabs(s.vapourPressure(s.triple.t) / s.triple.p - 1) < close, "%s: %g at its triple point, not %g", s.id.c_str(), s.vapourPressure(s.triple.t), s.triple.p);
		UASSERT(std::fabs(s.sublimationPressure(s.triple.t) / s.triple.p - 1) < 1e-6);
		UASSERT(std::fabs(s.vapourPressure(s.critical.t) / s.critical.p - 1) < close);
		UASSERT(s.vapourPressure((s.triple.t + s.critical.t) / 2) > s.triple.p);
		if (s.boil)
			UASSERT(std::fabs(s.vapourPressure(*s.boil) / ATMOSPHERE - 1) < close);
		UASSERT(s.meltingPoint(1e10) > 0); // no melting line runs below absolute zero
	}
}

static const Conditions ROOM{ 293.15, ATMOSPHERE, 0, 0 };

void TestMatter::testReactionsBalance()
{
	Matter m = table(); // loading refuses any reaction that doesn't balance
	UASSERT(m.reactions().size() >= 12);
}

// Methane won't burn until lit; lit, it burns on its own heat until the oxygen runs out.
void TestMatter::testBurning()
{
	Matter m = table();
	size_t ch4 = m.index("methane"), o2 = m.index("oxygen"), co2 = m.index("carbon_dioxide"), h2o = m.index("water");
	Mixture mix(m);
	mix.amounts[ch4] = 2;
	mix.amounts[o2] = 2;
	mix.run(60, ROOM);
	UASSERT(std::fabs(mix.amounts[ch4] - 2) < 1e-6);   // unlit: nothing

	mix.temperature = 1100; // a spark
	double hottest = 0;
	for (int i = 0; i < 30; i++) {
		mix.run(1, ROOM);
		hottest = std::max(hottest, mix.temperature);
	}
	UASSERT(mix.amounts[o2] < 0.01);                    // the oxygen is gone…
	UASSERT(std::fabs(mix.amounts[ch4] - 1) < 0.01);   // …so half the fuel is left
	UASSERT(std::fabs(mix.amounts[co2] - 1) < 0.01);
	UASSERT(std::fabs(mix.amounts[h2o] - 2) < 0.01);
	UASSERT(hottest > 1300);                            // its own heat
}

// Electrolysis runs on electricity, photosynthesis on light; neither on heat.
void TestMatter::testDrivenReactions()
{
	Matter m = table();
	size_t water = m.index("water"), h2 = m.index("hydrogen"), o2 = m.index("oxygen");
	Mixture cell(m);
	cell.amounts[water] = 10;
	cell.run(60, ROOM);
	UASSERT(cell.amounts[h2] == 0);
	cell.run(60, Conditions{ 293.15, ATMOSPHERE, 1000, 0 });
	UASSERT(cell.amounts[h2] > 0.1);
	UASSERT(std::fabs(cell.amounts[h2] - 2 * cell.amounts[o2]) < 1e-9);  // two to one

	size_t co2 = m.index("carbon_dioxide"), sugar = m.index("glucose");
	Mixture leaf(m);
	leaf.amounts[co2] = 6;
	leaf.amounts[water] = 10;
	leaf.run(600, ROOM);
	UASSERT(leaf.amounts[sugar] == 0);
	leaf.run(600, Conditions{ 293.15, ATMOSPHERE, 0, 500 });
	UASSERT(leaf.amounts[sugar] > 0.05 && leaf.amounts[o2] > 0.3);
}

// Smelting and kilns need heat; rust needs water and time.
void TestMatter::testHeatGated()
{
	Matter m = table();
	size_t ore = m.index("hematite"), co = m.index("carbon_monoxide"), iron = m.index("iron");
	Mixture furnace(m);
	furnace.amounts[ore] = 1;
	furnace.amounts[co] = 3;
	furnace.run(600, ROOM);
	UASSERT(furnace.amounts[iron] < 1e-3);
	furnace.temperature = 1300;
	furnace.run(600, Conditions{ 1300, ATMOSPHERE, 0, 0 });
	UASSERT(furnace.amounts[iron] > 1);

	size_t calcite = m.index("calcite"), lime = m.index("lime");
	Mixture kiln(m);
	kiln.amounts[calcite] = 1;
	kiln.run(600, ROOM);
	UASSERT(kiln.amounts[lime] < 1e-3);
	kiln.run(1200, Conditions{ 1300, ATMOSPHERE, 0, 0 });
	UASSERT(kiln.amounts[lime] > 0.5);

	size_t o2 = m.index("oxygen"), water = m.index("water");
	Mixture dry(m), wet(m);
	for (Mixture *nail : { &dry, &wet }) {
		nail->amounts[iron] = 4;
		nail->amounts[o2] = 3;
	}
	wet.amounts[water] = 5;
	dry.run(86400, ROOM);
	wet.run(86400, ROOM);
	UASSERT(dry.amounts[ore] < 1e-6);   // no water, no rust
	UASSERT(wet.amounts[ore] > 0.01);   // a day in the wet
}

// Every rock is a mixture of known minerals; every ore the world places
// smelts to its metal in a furnace, and clay fires to pottery in a kiln.
void TestMatter::testRocksAndOres()
{
	Matter m = table();
	UASSERT(m.rocks().size() >= 10);

	auto run = [&](std::vector<std::pair<const char *, double>> start, double kelvin, double seconds) {
		Mixture mix(m);
		for (auto &[id, n] : start)
			mix.amounts[m.index(id)] = n;
		mix.temperature = kelvin;
		mix.run(seconds, Conditions{ kelvin, ATMOSPHERE, 0, 0 });
		return mix;
	};
	UASSERT(run({ { "cassiterite", 1 }, { "carbon_monoxide", 2 } }, 1250, 900).amounts[m.index("tin")] > 0.5);
	UASSERT(run({ { "cassiterite", 1 }, { "carbon_monoxide", 2 } }, 293, 900).amounts[m.index("tin")] < 1e-3);
	UASSERT(run({ { "chalcopyrite", 4 }, { "oxygen", 13 } }, 900, 900).amounts[m.index("copper_oxide")] > 2);
	UASSERT(run({ { "galena", 2 }, { "oxygen", 3 } }, 900, 900).amounts[m.index("lead_oxide")] > 1);
	UASSERT(run({ { "lead_oxide", 1 }, { "carbon_monoxide", 1 } }, 900, 900).amounts[m.index("lead")] > 0.5);
	UASSERT(run({ { "magnetite", 1 }, { "carbon_monoxide", 4 } }, 1300, 900).amounts[m.index("iron")] > 1.5);
	UASSERT(run({ { "kaolinite", 1 } }, 1000, 1800).amounts[m.index("metakaolin")] > 0.5);
	UASSERT(run({ { "kaolinite", 1 } }, 293, 1800).amounts[m.index("metakaolin")] < 1e-3);
	UASSERT(m.substance("sulfur").state(400, ATMOSPHERE) == State::Liquid); // molten sulfur at a vent
}

// Alloys form only when their metals are molten; wood chars short of air;
// sand and soda make glass when hot; mortar sets slowly in the air.
void TestMatter::testAlloysAndCraft()
{
	Matter m = table();
	auto run = [&](std::vector<std::pair<const char *, double>> start, double kelvin, double seconds) {
		Mixture mix(m);
		for (auto &[id, n] : start)
			mix.amounts[m.index(id)] = n;
		mix.temperature = kelvin;
		mix.run(seconds, Conditions{ kelvin, ATMOSPHERE, 0, 0 });
		return mix;
	};
	UASSERT(run({ { "copper", 15 }, { "tin", 1 } }, 1450, 120).amounts[m.index("bronze")] > 0.5);
	UASSERT(run({ { "copper", 15 }, { "tin", 1 } }, 1200, 120).amounts[m.index("bronze")] == 0); // copper still solid
	UASSERT(run({ { "iron", 22 }, { "carbon", 4 } }, 1900, 600).amounts[m.index("cast_iron")] > 0.5);
	UASSERT(run({ { "cast_iron", 25 }, { "oxygen", 39 } }, 1600, 600).amounts[m.index("steel")] > 5);
	UASSERT(run({ { "tin", 9 }, { "lead", 1 } }, 700, 120).amounts[m.index("pewter")] > 0.5);

	Mixture kiln = run({ { "wood", 2 }, { "oxygen", 0.5 } }, 700, 1800); // short of air
	UASSERT(kiln.amounts[m.index("carbon")] > 3); // most of the wood charred: 3 charcoal from each (the rest leaves as gas)
	UASSERT(run({ { "silica", 1 }, { "soda_ash", 1 } }, 1400, 1800).amounts[m.index("glass")] > 0.5);
	UASSERT(run({ { "silica", 1 }, { "soda_ash", 1 } }, 293, 1800).amounts[m.index("glass")] < 1e-9); // Arrhenius: negligible, never quite 0
	UASSERT(run({ { "lime", 1 }, { "water", 2 } }, 293, 600).amounts[m.index("slaked_lime")] > 0.5);
	UASSERT(run({ { "slaked_lime", 1 }, { "carbon_dioxide", 2 } }, 293, 86400 * 3).amounts[m.index("calcite")] > 0.2);
}

// Ice in a warm room sits at its melting point while it melts, part solid
// and part liquid, until the heat to melt all of it has come in.
void TestMatter::testMeltingTakesTime()
{
	Matter m = table();
	size_t water = m.index("water");
	Mixture ice(m);
	ice.temperature = 263;
	ice.add(water, State::Solid, 10); // 180 g
	ice.run(30, Conditions{ 293.15, ATMOSPHERE, 0, 0 }); // warms to 0 °C in ~12 s; melting all of it takes ~60 s more
	UASSERT(std::fabs(ice.temperature - 273.15) < 0.05);
	UASSERT(ice.in(water, State::Solid) > 0 && ice.in(water, State::Liquid) > 0);
	ice.run(3600, Conditions{ 293.15, ATMOSPHERE, 0, 0 });
	UASSERT(ice.in(water, State::Solid) < 1e-9 && ice.temperature > 274);
	// and back: water in a freezer holds at 0 °C while it freezes
	ice.run(60, Conditions{ 250, ATMOSPHERE, 0, 0 });
	ice.run(60, Conditions{ 250, ATMOSPHERE, 0, 0 });
	if (ice.in(water, State::Solid) > 0 && ice.in(water, State::Liquid) > 0)
		UASSERT(std::fabs(ice.temperature - 273.15) < 0.05);
}

// Sprint 79: a reaction's heat is its substances' formation enthalpies
// (NASA's), not a number typed in: methane burnt to liquid water gives
// 890 kJ/mol, as measured.
void TestMatter::testHeatsFromData()
{
	const Matter m = table();
	for (const Reaction &r : m.reactions())
		if (r.id == "methane_burns")
			UTEST(std::fabs(r.heat - 890.6) < 1, "methane burns at %f kJ", r.heat);
	UASSERT(std::fabs(m.substance("water").formation + 285.83) < 0.01);
	UASSERT(m.substance("iron").formation == 0); // an element as it is
	// CoolProp's curve: water boils near 70 °C at 0.31 bar
	UASSERT(std::fabs(*m.substance("water").boilingPoint(31200) - 343.15) < 0.5);
}

// Sprint 79: chemistry in water. Sodium dropped in water gives off
// hydrogen and heat; aluminium in hydrochloric acid dissolves, fizzing
// hydrogen; caustic soda and the acid cancel into salt water.
void TestMatter::testWetChemistry()
{
	const Matter m = table();
	const size_t water = m.index("water"), h2 = m.index("hydrogen");
	{
		Mixture pot(m);
		pot.add(water, State::Liquid, 10); // 180 g
		pot.add(m.index("sodium"), State::Solid, 0.2); // 4.6 g
		const double was = pot.temperature;
		pot.run(5, Conditions{ 293.15, ATMOSPHERE, 0, 0, 0 });
		UTEST(pot.amounts[h2] > 0.09, "sodium gave %f mol of hydrogen", pot.amounts[h2]);
		// 28 kJ into 180 g of water and its pot (1 kJ/K): about 16 K
		UTEST(pot.temperature > was + 12, "it warmed only to %f K", pot.temperature);
	}
	{
		Mixture pot(m);
		pot.add(water, State::Liquid, 20);
		pot.add(m.index("hydrogen_chloride"), State::Gas, 3);
		pot.add(m.index("aluminium"), State::Solid, 0.5);
		pot.run(60, Conditions{ 293.15, ATMOSPHERE, 0, 0, 0 });
		UTEST(pot.amounts[m.index("aluminium_chloride")] > 0.4, "only %f mol dissolved", pot.amounts[m.index("aluminium_chloride")]);
		UTEST(pot.amounts[h2] > 0.6, "only %f mol of hydrogen", pot.amounts[h2]);
	}
	{
		Mixture pot(m);
		pot.add(water, State::Liquid, 20);
		pot.add(m.index("sodium_hydroxide"), State::Solid, 1);
		pot.add(m.index("hydrogen_chloride"), State::Gas, 1);
		pot.run(10, Conditions{ 293.15, ATMOSPHERE, 0, 0, 0 });
		UTEST(pot.amounts[m.index("salt")] > 0.95, "only %f mol of salt", pot.amounts[m.index("salt")]);
	}
	{
		// dry, sodium sits there
		Mixture jar(m);
		jar.add(m.index("sodium"), State::Solid, 1);
		jar.add(m.index("nitrogen"), State::Gas, 1);
		jar.run(60, Conditions{ 293.15, ATMOSPHERE, 0, 0, 0 });
		UASSERT(jar.amounts[h2] == 0);
	}
}

// Grain size (sprint 111): rock weathers into ground by what it's made of,
// and every split keeps the mass.
void TestMatter::testGrains()
{
	Matter m = table();
	UASSERT(bandOf(100) == Band::Stone && bandOf(2) == Band::Gravel && bandOf(1) == Band::Sand);
	UASSERT(bandOf(0.01) == Band::Silt && bandOf(0.001) == Band::Clay);
	// feldspar keeps half a mole of kaolinite a mole: 0.46 kg a kg, from the masses
	UASSERT(std::fabs(m.substance("feldspar").weathers->keep - 0.464) < 0.01);
	UASSERT(!m.substance("silica").weathers); // quartz lasts

	auto weathered = [&](const char *rock, double extent, double *dissolved) {
		const RockKind &r = m.rock(rock);
		Grains g = Grains::broken(m, r, 1000, Band::Stone);
		*dissolved = g.weather(extent, bandOf(r.grain), r.crumbles);
		UASSERT(std::fabs(g.mass() + *dissolved - 1000) < 1e-6);
		return g;
	};
	double gone;
	// granite rots to quartz sand and kaolinite clay: a sandy loam where
	// weathering is mild (temperate), clay with quartz sand in it where
	// it's deep (the tropics)
	Grains granite = weathered("granite", 4, &gone);
	Texture t = granite.texture();
	UTEST(t.sand > 0.6 && t.clay > 0.1 && t.clay < 0.3, "mild: granite weathered to sand %.2f, clay %.2f", t.sand, t.clay);
	UASSERT(granite.stones() < 0.05);
	Texture deep = weathered("granite", 15, &gone).texture();
	UTEST(deep.clay > 0.4 && deep.sand > 0.3, "deep: granite weathered to sand %.2f, clay %.2f", deep.sand, deep.clay);
	// basalt has no quartz: clay or nothing
	Grains basalt = weathered("basalt", 4, &gone);
	UTEST(basalt.texture().clay > t.clay, "basalt's clay %.2f under granite's %.2f", basalt.texture().clay, t.clay);
	// limestone goes into the water, leaving its clay (terra rossa)
	Grains lime = weathered("limestone", 4, &gone);
	UTEST(gone > 850, "limestone left %.0f kg of 1000", 1000 - gone);
	UASSERT(lime.texture().clay > 0.4);
	// young weathering: still mostly the rock, in pieces
	Grains young = weathered("granite", 0.1, &gone);
	UASSERT(young.stones() > 0.8);

	// splits keep everything, water included
	Grains soil = granite;
	soil.water = 100;
	const double before = soil.mass();
	Grains fine = soil.finer(Band::Silt);
	UASSERT(fine.mass(Band::Sand) == 0 && soil.mass(Band::Clay) == 0);
	UASSERT(std::fabs(fine.mass() + soil.mass() - before) < 1e-9 && std::fabs(fine.water + soil.water - 100) < 1e-9);
	soil.add(fine);
	Grains spade = soil.share(0.25);
	UASSERT(std::fabs(spade.mass() * 3 - soil.mass()) < 1e-6);
	young.crush(Band::Sand);
	UASSERT(young.stones() == 0);
	// organic matter counts in the mass, never the texture
	Grains peat(m);
	peat.add(m.index("wood"), Band::Silt, 10);
	peat.add(m.index("kaolinite"), Band::Clay, 10);
	UASSERT(peat.organic() == 0.5 && peat.texture().clay == 1);
}
