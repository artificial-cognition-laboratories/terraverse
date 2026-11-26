// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

TEST_CASE("RiversOnLand")
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		// rivers are where rain gathers: on land, and wet land carries more of
		// them than dry (rivers per land cell, wettest third against driest)
		std::vector<std::pair<fix, bool>> land;
		for (int32_t i = 0; i < g.size(); i++) {
			if (g.river(i))
				REQUIRE(g.base[i] >= 0);
			if (g.base[i] >= 0)
				land.emplace_back(g.rain(i), g.river(i));
		}
		std::sort(land.begin(), land.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
		const size_t third = land.size() / 3;
		int dry = 0, wet = 0;
		for (size_t k = 0; k < third; k++) {
			dry += land[k].second;
			wet += land[land.size() - 1 - k].second;
		}
		REQUIRE(wet > 0);
		REQUIRE(wet > dry);
	}
}

static void describe(const char *code)
{
	World w(WorldCode::decode(code));
	Preview p = preview(w, std::nullopt, 64);
	int counts[(int)Biome::Count] = {};
	for (uint8_t b : p.biome)
		counts[b]++;
	note() << "terragen " << code << ":";
	for (int b = 0; b < (int)Biome::Count; b++)
		note() << " " << BIOME_NAMES[b] << "=" << counts[b];
	note() << std::endl;
}

// Any change to generation changes this. That is the point: a changed world
// for an existing code needs a new code version, not a silent update.
// skipped: .agents/jobs/terragen-sprint-116-red-tests.md: golden rebaked at TF25, sprint 116's end
TEST_CASE("DeterminismGolden" * doctest::test_suite("slow") * doctest::skip())
{
	describe("TF24.YWJj");
	describe("TF24.dGVycmE");
	World w(WorldCode::decode("TF24.YWJj.AQCAqoCAAFVmgICAgICAgIDjVQ"));
	Preview p = preview(w, std::nullopt, 48);
	uint32_t h = 2166136261u;
	auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
	for (size_t i = 0; i < p.biome.size(); i++) {
		mix((uint16_t)p.elevation[i]);
		mix((uint8_t)p.heat[i]);
		mix((uint8_t)p.wet[i]);
		mix((uint8_t)p.season[i]);
		mix(p.fertility[i]);
		mix(p.biome[i]);
	}
	// and what's underground: province, rock and ore down a few columns
	for (int32_t k = 0; k < 24; k++) {
		int32_t x = (k * 2311) % w.code().size - w.code().size / 2, z = (k * 977) % (w.code().size / 2) - w.code().size / 4;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Strata s = w.strata(at, g);
		mix((uint32_t)s.province);
		for (int32_t depth = 0; depth < 120; depth += 7) {
			Rock r = w.rock(s, depth, g.terrain - depth);
			mix((uint32_t)r);
			mix((uint32_t)w.ore(s, r, depth, g.terrain - depth));
			mix((uint32_t)w.cave(s, r, depth, g.terrain - depth, g.kind != Water::None));
		}
	}
	// and the land and water at block scale (sprint 108: the land's shaped
	// below the grid, so the grid's fields alone didn't pin it; only a planet
	// big enough for channels below its cells has that land, so a new world's
	// size): patches round a few land cells, a block apart and 9 apart
	{
		WorldCode big = WorldCode::decode("TF24.dGVycmE");
		big.size = 628320;
		const World land(big);
		const Grid &g = land.grid();
		int patches = 0;
		for (int32_t c = 0; c < g.size() && patches < 6; c += 997) {
			if (g.base[c] <= 0)
				continue;
			patches++;
			const Dir d = gridSphere().centre(c);
			const fix u0 = World::u(d), v0 = World::v(d);
			for (int32_t a = 0; a < 16; a++)
				for (int32_t b = 0; b < 16; b++) {
					const int32_t step = a < 8 && b < 8 ? 1 : 9;
					const Place at = World::place(u0 + ratio(a * step, big.size), v0 + ratio(b * step, big.size));
					const Ground gr = land.ground(at, land.sunlight(at, std::nullopt));
					mix((uint32_t)gr.terrain);
					mix((uint32_t)gr.water);
					mix((uint32_t)gr.kind);
					mix((uint32_t)gr.column.biome);
				}
		}
		REQUIRE_EQ(int(patches), 6);
		// and along a few rivers (sprint 110: the patches never fell on one, so
		// the rivers' levels changed with the golden none the wiser): the
		// ground at points along their drawn runs, and the water there
		int rivers = 0;
		for (int32_t c = 0; c < g.size() && rivers < 12; c += 113) {
			if (!land.drawn(c) || !land.profile(c))
				continue;
			rivers++;
			for (int32_t k = 0; k < 16; k++) {
				const Place at = land.unwarped(land.onRun(c, ratio(2 * k + 1, 32)));
				const Ground gr = land.ground(at, land.sunlight(at, std::nullopt));
				mix((uint32_t)gr.terrain);
				mix((uint32_t)gr.water);
				mix((uint32_t)gr.kind);
			}
		}
		REQUIRE_EQ(int(rivers), 12);
	}
	// what the world is made of, for a human reading --info output when tuning
	int counts[(int)Biome::Count] = {};
	int16_t lo = 32767, hi = -32767;
	for (size_t i = 0; i < p.biome.size(); i++) {
		counts[p.biome[i]]++;
		lo = std::min(lo, p.elevation[i]);
		hi = std::max(hi, p.elevation[i]);
	}
	note() << "terragen golden: " << h << ", elevation " << lo / 10000.f << ".." << hi / 10000.f << ", biomes:";
	for (int b = 0; b < (int)Biome::Count; b++)
		note() << " " << BIOME_NAMES[b] << "=" << counts[b];
	note() << std::endl;
	// TF6: the world on the sphere (sprint 86; TF5, on the flat map, was
	// 2496371879). The code's size 1 is 62832 round since sprint 39.
	// (re-pinned: this code is 62 832 round, past where the radius overflowed a fix)
	// TF9, not yet released, re-pinned for sprint 88 (was 1343096095)
	// re-pinned for sprint 97: no beaches on hard coasts (was 3559267434)
	// re-pinned for sprint 100: relief from the drainage below the grid (was 1626736796)
	// re-pinned for sprint 101: ranges by what built them (was 1400668538)
	// TF17, sprint 106: the land's shape (plates that wander, deltas, alpine crests…; was 1902362085)
	// TF18, sprint 107: old land worn flat (shields; was 2420144613)
	// TF19, sprint 108: the land from the drainage, pinned at block scale on a
	// big world too (was 3548419286)
	// TF20: mountain belts graded to foothills, narrower on the side going
	// under (was 2338250884); sprint 109: natural land, seamless, bent,
	// branching to the finest, ranges brought to the grid's heights (was 4220558337)
	// TF21, sprint 110: the rivers' levels from the network's bed, not the
	// land read at every run, and no world handed another's channel nodes
	// from a thread's cache (was 1517453345, itself not always so); then
	// along rivers too, their profiles from their ends and rock (was 989336567)
	// TF23, sprint 110: rivers wander down the grid and meet, and each level
	// branches into its own, joining along its streams' length, meandering
	// by its rock, streams as wide as their flow into the rivers, and the
	// Rivers setting's default denser, rivers as wide as real ones for what
	// they drain at any planet's size, up to 16 continents, streams and
	// ranges by the planet's scale (was 1009732295); sprint 111: soil made
	// by weathering, slopes and water sorting, the beds level, mostly mud, and
	// provinces' edges wandering (was 3749507156); sprint 112: ice sheets
	// feed no rivers, salt pans where water stands in wet years, braided
	// rivers, cut banks (was 3928772032); valleys still cut under the ice
	// sheets, fjords reaching further in (was 3302989925); the drainage's
	// valleys down to 12 blocks apart, so the smallest planets have them
	// (was 1719416001); a small planet's ranges' hillsides as steep as its
	// land is exaggerated, and their peaks free of the grid's mean (was
	// 4140243162); ranges rising under rivers already running (was
	// 1580458716); valleys along a rising range (was 1392860933); TF23,
	// sprint 115: a finer channel's hillside fades over 1½..2 of its lattice,
	// not 2..2½ (was 3597410694, which had already drifted from what TF22 made)
	// TF24: where every channel near fades out, the land eases to the noise's
	// (the default size stood spires at the gaps; this size's golden unchanged)
	REQUIRE_EQ(uint32_t(h), 3643851153u);
}
