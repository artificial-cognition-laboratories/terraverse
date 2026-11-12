// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "exceptions.h"
#include "flora/flora.h"
#include "flora/forest.h"
#include "mapblock.h"
#include "serialization.h"
#include "sprig/sprig.h"
#include <algorithm>
#include <cstring>
#include <sstream>

using namespace flora;
using terragen::ratio;

/*
	Flora (the flora folder): every plant is one generator's answer to its
	genes, bent by its site. The same seed grows the same plant; a kinder
	site grows a bigger one; a lineage dies outside what it bears, and says
	of what; each kind of plant keeps its place; and what the genes say
	shows in what grows.
*/
class TestFlora : public TestBase {
public:
	TestFlora() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFlora"; }

	void runTests(IGameDef *gamedef);

	void testSamePlantFromSameSeed();
	void testKinderSitesGrowMore();
	void testPlacesKeepTheirPlants();
	void testEveryPresetGrows();
	void testGenesShow();
	void testWorldsDiffer();
	void testNothingReachesTooFar();
	void testIdsKeepTheirPlants();
	void testSprigsKept(IGameDef *gamedef);
};

static TestFlora g_test_instance;

void TestFlora::runTests(IGameDef *gamedef)
{
	TEST(testSamePlantFromSameSeed);
	TEST(testKinderSitesGrowMore);
	TEST(testPlacesKeepTheirPlants);
	TEST(testEveryPresetGrows);
	TEST(testGenesShow);
	TEST(testWorldsDiffer);
	TEST(testNothingReachesTooFar);
	TEST(testIdsKeepTheirPlants);
	TEST(testSprigsKept, gamedef);
}

static const uint32_t WORLD = 0x7e22a;
static const uint32_t GOLDEN = 21898279u; // testSamePlantFromSameSeed

static Site site(int heat, int wet, int season = 0, int depth = 4)
{
	Site s;
	s.heat = ratio(heat, 100);
	s.wet = ratio(wet, 100);
	s.season = ratio(season, 100);
	s.depth = depth;
	return s;
}

static const Site TEMPERATE = site(5, 25), TAIGA = site(-35, 20), JUNGLE = site(70, 75), DESERT = site(70, -70);

static Lineage of(const char *preset)
{
	for (const Lineage &l : lineages(WORLD, (int)presets().size()))
		if (!strcmp(presets()[l.preset].id, preset))
			return l;
	throw BaseException(std::string("no preset ") + preset);
}

static uint32_t fingerprint(const Plant &p)
{
	uint32_t h = 2166136261u;
	for (const Voxel &v : p.voxels)
		for (const int32_t part : { (int32_t)v.x, (int32_t)v.y, (int32_t)v.z, (int32_t)v.part, (int32_t)v.size })
			h = (h ^ (uint32_t)part) * 16777619u;
	return h;
}

static fix height(const Lineage &l, const Site &s) { return traits(l, s).gene(Gene::Height); }

void TestFlora::testSamePlantFromSameSeed()
{
	const Traits t = traits(of("broadleaf"), TEMPERATE);
	const Plant one = grow(t, 7), again = grow(t, 7), other = grow(t, 8);
	UASSERT(!one.voxels.empty());
	UASSERTEQ(uint32_t, fingerprint(one), fingerprint(again));
	UASSERT(fingerprint(one) != fingerprint(other));
	// (pinned: the generator will plant these, and a world code must keep its plants)
	UASSERTEQ(uint32_t, fingerprint(one), GOLDEN);
}

void TestFlora::testKinderSitesGrowMore()
{
	const Lineage oak = of("broadleaf");
	UASSERT(height(oak, JUNGLE) > height(oak, TEMPERATE));
	UASSERT(strategy(TAIGA).limit == Limit::Cold);
	UASSERT(strategy(DESERT).limit == Limit::Drought);
	Site rock = TEMPERATE, gale = TEMPERATE, oasis = DESERT;
	rock.depth = 0;
	gale.wind = terragen::ONE;
	oasis.bank = true;
	UASSERT(height(oak, rock) < height(oak, TEMPERATE));
	UASSERT(height(oak, gale) < height(oak, TEMPERATE));
	UASSERT(traits(oak, oasis).lives);
	// shade grows bigger leaves
	Site shade = JUNGLE;
	shade.light = ratio(15, 100);
	const Lineage leaf = of("monstera");
	UASSERT(traits(leaf, shade).gene(Gene::Size) > traits(leaf, JUNGLE).gene(Gene::Size));
}

void TestFlora::testPlacesKeepTheirPlants()
{
	Site floor = JUNGLE, marsh = site(5, -10);
	floor.light = ratio(15, 100);
	marsh.bank = true;
	UASSERT(traits(of("giant"), TAIGA).why == Limit::Cold);
	UASSERT(traits(of("weeping"), DESERT).why == Limit::Drought);
	UASSERT(traits(of("fern"), floor).lives);
	UASSERT(traits(of("monstera"), floor).lives);
	UASSERT(traits(of("grass"), floor).why == Limit::Shade);
	UASSERT(traits(of("cactus"), DESERT).lives);
	UASSERT(traits(of("agave"), DESERT).lives);
	UASSERT(traits(of("cactus"), JUNGLE).why == Limit::Wet);
	UASSERT(!traits(of("reed"), site(5, -10)).lives);
	UASSERT(traits(of("reed"), marsh).lives);
	UASSERT(traits(of("palm"), TAIGA).why == Limit::Cold);
	UASSERT(grow(traits(of("weeping"), DESERT), 1).voxels.empty());
}

void TestFlora::testEveryPresetGrows()
{
	// each where it does best of a few places, standing on the ground
	const Site places[] = { TEMPERATE, TAIGA, JUNGLE, DESERT, site(60, -15, 70) };
	for (const Lineage &l : lineages(WORLD, (int)presets().size())) {
		const Site *home = nullptr;
		fix best = -1;
		for (const Site &s : places)
			if (traits(l, s).lives && height(l, s) > best) {
				best = height(l, s);
				home = &s;
			}
		UASSERT(home);
		const Plant p = grow(traits(l, *home), l.seed);
		UASSERT(p.voxels.size() > 20);
		bool footed = false;
		for (const Voxel &v : p.voxels) {
			UASSERT(v.y >= 0);
			footed = footed || v.y <= 1;
		}
		UASSERT(footed);
	}
}

void TestFlora::testGenesShow()
{
	Site floor = JUNGLE;
	floor.light = ratio(30, 100);
	Life spring;
	spring.season = ratio(25, 100);
	const Plant cactus = grow(traits(of("cactus"), DESERT, spring), 1), monstera = grow(traits(of("monstera"), floor), 1),
			oak = grow(traits(of("broadleaf"), TEMPERATE), 1), grass = grow(traits(of("grass"), TEMPERATE), 1);
	const auto n = [](const Plant &p, Part part) { return p.count[(int)part]; };
	// a cactus is flesh and flowers, no leaves; an oak is wood and leaves
	UASSERT(n(cactus, Part::Flesh) > 0 && n(cactus, Part::Flower) > 0 && n(cactus, Part::Leaf) == 0);
	UASSERT(n(oak, Part::Wood) > 0 && n(oak, Part::Leaf) > 100);
	UASSERT(n(grass, Part::Wood) == 0 && n(grass, Part::Leaf) > 0);
	// small plants are grown fine, big ones coarser
	UASSERTEQ(int, monstera.grain, 16);
	UASSERT(oak.grain < monstera.grain);
	// a split leaf has fewer voxels than the same leaf whole
	Traits whole = traits(of("monstera"), floor);
	whole.genes[(int)Gene::Splits] = 0;
	whole.genes[(int)Gene::Holes] = 0;
	UASSERT(n(grow(whole, 1), Part::Leaf) > n(monstera, Part::Leaf));
}

void TestFlora::testWorldsDiffer()
{
	// the same preset in two worlds: different genes, different names
	const std::vector<Lineage> a = lineages(1, (int)presets().size()), b = lineages(2, (int)presets().size());
	int same = 0;
	for (size_t i = 0; i < a.size(); i++) {
		UASSERT(a[i].preset == b[i].preset);
		same += a[i].genes == b[i].genes;
	}
	UASSERTEQ(int, same, 0);
	UASSERT(a[0].name != b[0].name);
}

void TestFlora::testNothingReachesTooFar()
{
	// the generator grows plants this far into a chunk from its neighbours: past it, they'd be cut at the seam
	const Site places[] = { TEMPERATE, TAIGA, JUNGLE, DESERT, site(90, 90), site(60, -15, 70) };
	for (uint32_t world = 1; world <= 8; world++)
		for (const Lineage &l : lineages(world, 2 * (int)presets().size()))
			for (const Site &s : places) {
				const Traits t = traits(l, s);
				if (!t.lives)
					continue;
				const Plant p = grow(t, world);
				const int32_t reach = (p.reach + p.grain - 1) / p.grain + 1;
				UASSERT(reach <= MOST_REACH);
				// (Forest's ground layer: under a tree's height)
				UASSERT(l.genes[(int)Gene::Height] >= terragen::toFix(3) || reach <= MOST_GROUND_REACH);
			}
}

void TestFlora::testIdsKeepTheirPlants()
{
	// a plant's id is all of it: round its site and it's the same id, the same plant
	Forest forest(WORLD);
	for (const Site &s : { TEMPERATE, TAIGA, JUNGLE, DESERT }) {
		for (int lineage = 0; lineage < (int)forest.all().size(); lineage += 7) {
			const uint32_t id = Forest::idOf(lineage, s, 12345);
			UASSERTEQ(int, Forest::lineageOf(id), lineage);
			UASSERTEQ(uint32_t, Forest::idOf(lineage, Forest::siteOf(id), 12345), id);
		}
	}
	// grown from the rounded site, whatever site first asked
	Site near = TEMPERATE;
	near.heat += ratio(1, 100);
	const int oak = 0;
	UASSERTEQ(uint32_t, Forest::idOf(oak, near, 3), Forest::idOf(oak, TEMPERATE, 3));
	Forest other(WORLD);
	other.plant(Forest::idOf(oak, near, 3));
	UASSERTEQ(uint32_t, fingerprint(*forest.plant(Forest::idOf(oak, TEMPERATE, 3))),
			fingerprint(*other.plant(Forest::idOf(oak, TEMPERATE, 3))));
}

// A block's small plants survive the disk and the network; one from before (31) has none.
void TestFlora::testSprigsKept(IGameDef *gamedef)
{
	const sprig::Sprig s{ 0xdeadbeef, Forest::idOf(3, JUNGLE, 5), 6 };
	for (bool disk : { true, false }) {
		MapBlock block({}, gamedef);
		block.setNodeNoCheck({ 2, 3, 4 }, MapNode(CONTENT_AIR));
		block.m_sprigs.set({ 2, 3, 4 }, s);
		std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
		block.serialize(ss, SER_FMT_VER_HIGHEST_WRITE, disk, -1);
		MapBlock back({}, gamedef);
		back.deSerialize(ss, SER_FMT_VER_HIGHEST_WRITE, disk);
		const sprig::Sprig *got = back.m_sprigs.get({ 2, 3, 4 });
		UASSERT(got && *got == s);
		UASSERT(!back.m_sprigs.get({ 2, 3, 5 }));
	}
	MapBlock block({}, gamedef);
	block.m_sprigs.set({ 1, 1, 1 }, s);
	std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
	block.serialize(ss, 31, true, -1);
	MapBlock back({}, gamedef);
	back.deSerialize(ss, 31, true);
	UASSERT(back.m_sprigs.empty());
}
