// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

// terragen's one test that reads the game's data (assets/matter), so it runs
// in the engine. The rest of terragen's tests are tests/library/terragen.
#include "tests/unit/test.h"

#include "library/world/terragen/src/grid/geology.h"
#include "library/world/terragen/src/world.h"
#include "library/matter/matter/grains.h"
#include "src/shared/platform/system/porting.h"
#include <cmath>
#include <string>

using namespace terragen;

class TestTerragenMatter : public TestBase {
public:
	TestTerragenMatter() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestTerragenMatter"; }

	void runTests(IGameDef *gamedef);
	void testSoilMatchesMatter();
};

static TestTerragenMatter g_test_instance;

void TestTerragenMatter::runTests(IGameDef *gamedef)
{
	TEST(testSoilMatchesMatter);
}

// terragen weathers rock by its own copy of the matter data (it reads no
// files): the copy must be the data
void TestTerragenMatter::testSoilMatchesMatter()
{
	const matter::Matter m = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	for (int r = 0; r < (int)Rock::Count; r++) {
		if ((Rock)r == Rock::Ice)
			continue;
		const ParentRock &p = parentRock((Rock)r);
		const matter::RockKind &kind = m.rock(ROCK_NAMES[r]);
		size_t named = 0;
		for (const ParentMineral &pm : p.minerals) {
			if (!pm.id)
				continue;
			named++;
			const size_t index = m.index(pm.id);
			double share = -1;
			for (const matter::RockKind::Part &part : kind.minerals)
				if (part.substance == index)
					share = part.share;
			UTEST(std::fabs(share - pm.share / 65536.0) < 0.002, "%s: %s is %g of it in the data", ROCK_NAMES[r], pm.id, share);
			const matter::Substance &sub = m.substances()[index];
			const double rate = sub.weathers ? sub.weathers->rate : 0, keep = sub.weathers ? sub.weathers->keep : 0;
			UTEST(std::fabs(rate - pm.rate / 65536.0) < 0.002 && std::fabs(keep - pm.keep / 65536.0) < 0.002,
					"%s weathers at %g keeping %g in the data", pm.id, rate, keep);
			const int finest = sub.grain ? (int)matter::bandOf(*sub.grain) : -1;
			UTEST(finest == pm.finest, "%s comes apart at band %d in the data", pm.id, finest);
		}
		UTEST(named == kind.minerals.size(), "%s: %zu minerals in the data", ROCK_NAMES[r], kind.minerals.size());
		UTEST((int)matter::bandOf(kind.grain) == p.crystals, "%s: its crystals' band", ROCK_NAMES[r]);
		UTEST(std::fabs(kind.crumbles - p.crumbles / 65536.0) < 0.002, "%s crumbles at %g in the data", ROCK_NAMES[r], kind.crumbles);
	}
}
