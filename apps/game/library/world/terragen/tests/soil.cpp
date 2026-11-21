// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// Soil as a composition: every point round-trips through its index, and the
// soils follow why they differ (sprint 111): weathering makes clay where it's
// warm and wet, dry ground keeps sand and stones, cold wet ground keeps
// peat, steep ground is thin and stony, hollows deep.
TEST_CASE("Soil")
{
	for (int32_t i = 0; i < SOIL_POINTS; i++) {
		Soil s = soilPoint((uint8_t)i);
		REQUIRE(soilIndex(s) == i);
		REQUIRE(std::abs(s.sand + s.silt + s.clay - ONE) < 8);
	}
	REQUIRE_THROWS_AS(soilPoint((uint8_t)SOIL_POINTS), std::invalid_argument);

	World w(WorldCode::decode("TF24.dGVycmE"));
	int32_t size = w.code().size;
	struct Mean
	{
		double sum = 0;
		int n = 0;
		void add(double v) { sum += v; n++; }
		double of() const { return n ? sum / n : 0; }
	};
	Mean wet_clay, dry_clay, cold_organic, warm_organic, steep_stones, flat_stones, steep_depth, flat_depth, lake_clay, dry_stones, wet_stones;
	for (int32_t k = 0; k < 24000; k++) {
		int32_t x = (int32_t)((int64_t)k * 7919 % size) - size / 2, z = (int32_t)((int64_t)k * 104729 % (size * 8 / 10)) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		if (g.kind == Water::Lake) {
			lake_clay.add(w.soil(at, g, w.strata(at, g)).clay / 65536.0);
			continue;
		}
		if (g.kind != Water::None)
			continue;
		Soil s = w.soil(at, g, w.strata(at, g));
		const double clay = s.clay / 65536.0, stones = s.stones / 65536.0, organic = s.organic / 65536.0;
		if (g.column.heat > ratio(30, 100) && g.column.wet > ratio(30, 100)) { wet_clay.add(clay); wet_stones.add(stones); }
		if (g.column.wet < -ratio(30, 100)) { dry_clay.add(clay); dry_stones.add(stones); }
		// (rot by climate: soil made where it lies, not a swamp's or a
		// floodplain's, which hold peat in any climate, the Congo's too)
		if (!s.laid && g.column.heat < -ratio(20, 100) && g.column.wet > ratio(10, 100)) cold_organic.add(organic);
		if (!s.laid && g.column.heat > ratio(30, 100)) warm_organic.add(organic);
		// (slopes in one climate: deserts are flat and stony, and so are the
		// till plains the ice left, where it's cold)
		const bool temperate = g.column.wet > -ratio(15, 100) && g.column.wet < ratio(40, 100) && g.column.heat > ratio(5, 100);
		// (the ground's own slope, from its neighbours four blocks off: what soil
		// reads where the drainage shapes the land, not the grid's)
		const auto height = [&](int32_t dx, int32_t dz) {
			const Place q = flat(w, x + dx, z + dz);
			return w.ground(q, w.sunlight(q, std::nullopt)).terrain;
		};
		const double local = std::hypot(height(4, 0) - height(-4, 0), height(0, 4) - height(0, -4)) / 8.0;
		// (soil made where it lies: what water laid is sorted, a riverbed's gravel)
		if (temperate && !s.laid && local > 0.3) steep_stones.add(stones);
		if (temperate && !s.laid && local < 0.05) flat_stones.add(stones);
		// (depth by the hillside the soil model reads: the ground's own slope
		// also sees valley walls cut below it, where depth isn't decided)
		const double hill = w.soilSlope(at, g) / 65536.0;
		if (temperate && !s.laid && hill > 0.3) steep_depth.add(s.depth);
		if (temperate && !s.laid && hill < 0.05) flat_depth.add(s.depth);
	}
	note() << "soil: clay warm-wet " << wet_clay.of() << ", dry " << dry_clay.of() << "; stones warm-wet " << wet_stones.of()
			<< ", dry " << dry_stones.of() << "; organic cold-wet " << cold_organic.of() << ", warm " << warm_organic.of()
			<< "; stones steep " << steep_stones.of() << ", flat " << flat_stones.of() << "; depth steep " << steep_depth.of()
			<< ", flat " << flat_depth.of() << "; lake floors' clay " << lake_clay.of() << std::endl;
	REQUIRE((wet_clay.n && dry_clay.n && cold_organic.n && warm_organic.n && steep_stones.n && flat_stones.n && lake_clay.n && steep_depth.n && flat_depth.n));
	REQUIRE(wet_clay.of() > dry_clay.of());
	REQUIRE(wet_stones.of() < dry_stones.of());
	REQUIRE(cold_organic.of() > warm_organic.of());
	REQUIRE(steep_stones.of() > flat_stones.of());
	REQUIRE(steep_depth.of() < flat_depth.of());
	REQUIRE(lake_clay.of() > 0.5); // still water lays clay

	// the maps' soil, a cell at a time from the grid: the same order, nothing at sea
	const World::RegionalSoil &r = w.regionalSoil();
	const Grid &grid = w.grid();
	Mean cell_wet, cell_dry;
	for (int32_t c = 0; c < grid.size(); c++) {
		if (grid.base[c] < 0) {
			REQUIRE((r.sand[c] == 0 && r.clay[c] == 0 && r.stones[c] == 0));
			continue;
		}
		const Column col = w.column(Place(gridSphere().centre(c)), w.sunlight(Place(gridSphere().centre(c)), std::nullopt));
		if (col.heat > ratio(30, 100) && col.wet > ratio(30, 100)) cell_wet.add(r.clay[c] / 65536.0);
		if (col.wet < -ratio(30, 100)) cell_dry.add(r.clay[c] / 65536.0);
	}
	note() << "regional soil: clay warm-wet " << cell_wet.of() << " (" << cell_wet.n << "), dry " << cell_dry.of() << std::endl;
	REQUIRE((cell_wet.n && cell_dry.n && cell_wet.of() > cell_dry.of()));
}

// Rivers sort what they carry (sprint 111): a channel keeps what its current
// can't move (stony where it runs steep, sand where it's gentle), its banks
// and bars the next finer, its floodplain silt and clay. On an Earth-sized
// planet, where streams below the grid are drawn as well as its rivers.
// Sprint 112: an ice sheet's snow never melts, so it feeds no river; and a
// closed basin's salt is where its water stands in the wet years, a share of
// its floor, not all of it; and a minority of rivers braid.
TEST_CASE("ColdAndDryWater")
{
	const auto g = gridFor(WorldCode::decode("TF24.YmZ2bmVmZzQ"));
	// (what it adds of its own: its runoff less what drains into it; a river
	// from warmer ground upstream may still cross it, as the Lena's lower
	// reaches cross the frozen north)
	std::vector<int64_t> inflow(g->size(), 0);
	for (int32_t c = 0; c < g->size(); c++)
		if (g->receiver[c] >= 0)
			inflow[g->receiver[c]] += g->runoff[c];
	int fed_by_ice = 0, pan = 0, land = 0;
	for (int32_t c = 0; c < g->size(); c++) {
		land += g->base[c] >= 0;
		pan += g->pan[c];
		const fix celsius = celsiusOf(g->heat[c] - mul(std::max<fix>(0, g->eroded[c]), LAPSE));
		if (g->base[c] >= 0 && celsius < -toFix(15) && g->runoff[c] > inflow[c])
			fed_by_ice++;
	}
	REQUIRE_EQ(fed_by_ice, 0);
	REQUIRE(pan > 0);
	REQUIRE(pan * 100 < land); // under 1% of the land
	// and some rivers braid, steep for what they carry, a minority (sprint 112)
	const World w(WorldCode::decode("TF24.dGVycmE"));
	int drawn = 0, braided = 0;
	for (int32_t c = 0; c < w.grid().size(); c++)
		if (w.drawn(c)) {
			drawn++;
			braided += w.braided(c);
		}
	REQUIRE((braided * 100 > drawn * 3 && braided * 100 < drawn * 25));
}

// Coasts and dunes (sprint 112): waves only on the coast, sheltered and
// exposed; sandy beach on some of the shore, not all; dunes only as sand,
// on dry land, no higher than sand stands
TEST_CASE("Coasts")
{
	const World w(WorldCode::decode("TF24.YmZ2bmVmZzQ"));
	const Grid &g = w.grid();
	int calm = 0, rough = 0;
	for (int32_t c = 0; c < g.size(); c++) {
		bool by_sea = false;
		for (const int32_t *p = gridSphere().neighboursBegin(c); p != gridSphere().neighboursEnd(c); p++)
			by_sea |= g.base[*p] < 0;
		if (g.base[c] < 0 || !by_sea) {
			REQUIRE_EQ(fix(g.surf[c]), 0);
			continue;
		}
		calm += g.surf[c] < ratio(15, 100);
		rough += g.surf[c] > ratio(40, 100);
	}
	REQUIRE((calm > 0 && rough > 0));
	int shore = 0, beach = 0, desert = 0, dunes = 0;
	const int U = 300, V = 150;
	for (int j = 1; j < V; j++)
		for (int i = 0; i < U; i++) {
			const Place p = World::place((fix)((int64_t)i * ONE / U), (fix)((int64_t)j * ONE / V));
			const Ground gr = w.ground(p, w.sunlight(p, std::nullopt));
			if (gr.dune > 0) {
				dunes++;
				REQUIRE(gr.kind == Water::None);
				REQUIRE((gr.column.biome == Biome::Desert || gr.column.biome == Biome::Beach));
				REQUIRE(gr.dune <= 10);
			}
			if (gr.kind != Water::None || gr.terrain < 1)
				continue;
			desert += gr.column.biome == Biome::Desert;
			if (g.surf[w.cell(p)] > 0 && gr.terrain <= 3) {
				shore++;
				beach += gr.column.biome == Biome::Beach;
			}
		}
	REQUIRE((shore > 0 && beach * 10 > shore && beach * 10 < shore * 6));
	REQUIRE((dunes * 10 > desert && dunes < desert));
}

// (sprint 112) quiet craters where it rains hold lakes; a humid basin's
// limestone platforms are karst, their ground limestone
TEST_CASE("CraterLakesAndKarst")
{
	const World w(WorldCode::decode("TF24.YmZ2bmVmZzQ"));
	const Grid &g = w.grid();
	int lakes = 0;
	for (const Volcano &v : g.volcanoes) {
		const Ground at = w.ground(Place(v.at), 0);
		lakes += at.kind == Water::Lake;
		REQUIRE(!(v.active && at.kind == Water::Lake)); // an active one's is lava
	}
	REQUIRE(lakes > 0);
	int karst = 0, lime = 0;
	const int U = 200, V = 100;
	for (int j = 1; j < V; j++)
		for (int i = 0; i < U; i++) {
			const Place p = World::place((fix)((int64_t)i * ONE / U), (fix)((int64_t)j * ONE / V));
			const Ground gr = w.ground(p, 0);
			if (gr.terrain < 1)
				continue;
			if (w.karst(p, gr.column.wet) <= HALF)
				continue;
			karst++;
			lime += w.rock(w.strata(p, gr), 1, gr.terrain) == Rock::Limestone;
		}
	REQUIRE((karst > 0 && lime * 10 > karst * 7));
}

TEST_CASE("SoilByWater" * doctest::test_suite("slow"))
{
	WorldCode code = WorldCode::decode("TF24.dGVycmE");
	code.size = (int32_t)std::llround(6371000.0 * 2 * M_PI);
	World w(code);
	const Grid &g = w.grid();
	const Sphere &sphere = gridSphere();
	// round the biggest rivers: a window across each, every few blocks
	std::vector<int32_t> rivers;
	for (int32_t i = 0; i < g.size(); i++)
		if (g.river(i) && g.receiver[i] >= 0 && g.base[g.receiver[i]] >= 0)
			rivers.push_back(i);
	std::sort(rivers.begin(), rivers.end(), [&](int32_t a, int32_t b) { return g.drained[a] > g.drained[b]; });
	// each river's window on its own thread (a ground read is milliseconds at
	// this size), its sums joined in river order so the totals don't depend
	// on the machine
	struct Sums {
		double bed_fine = 0, plain_fine = 0, steep_stones = 0, gentle_stones = 0;
		int beds = 0, plains = 0, steep = 0, gentle = 0;
	};
	std::vector<int32_t> chosen;
	for (size_t r = 0; r < rivers.size() && r < 400; r += 4)
		chosen.push_back(rivers[r]);
	std::vector<Sums> each(chosen.size());
	parallelFor((int32_t)chosen.size(), [&](int32_t r) {
		Sums &m = each[r];
		const Place c(sphere.centre(chosen[r]));
		for (int k = 0; k < 400; k++) {
			// a 20 km square round the cell's centre, at its own precision
			const int64_t step = (int64_t)(PLACE_ONE / (w.radius() / 65536.0) * 1000);
			const Place at = placeOf((int64_t)c.x + (k % 20 - 10) * step, (int64_t)c.y + (k / 20 - 10) * step, c.z);
			const Ground gr = w.ground(at, w.sunlight(at, std::nullopt));
			if (gr.kind == Water::Sea || gr.kind == Water::Lake)
				continue;
			const Soil s = w.soil(at, gr, w.strata(at, gr));
			const double fine = (s.silt + s.clay) / 65536.0 * (1 - s.stones / 65536.0), stones = s.stones / 65536.0;
			if (gr.kind == Water::River && gr.river_width > 0 && gr.river_dist < gr.river_width / 2) {
				m.bed_fine += fine;
				m.beds++;
				if (gr.river_slope > ratio(3, 1000)) { m.steep_stones += stones; m.steep++; }
				else if (gr.river_slope < ratio(5, 10000)) { m.gentle_stones += stones; m.gentle++; }
			} else if (gr.kind == Water::None && gr.river_width > 0 && gr.river_dist > gr.river_width * 2 &&
					gr.river_dist < gr.river_width * 4 && gr.terrain - gr.river_level <= 2) {
				m.plain_fine += fine;
				m.plains++;
			}
		}
	});
	double bed_fine = 0, plain_fine = 0, steep_stones = 0, gentle_stones = 0;
	int beds = 0, plains = 0, steep = 0, gentle = 0;
	for (const Sums &m : each) {
		bed_fine += m.bed_fine; plain_fine += m.plain_fine;
		steep_stones += m.steep_stones; gentle_stones += m.gentle_stones;
		beds += m.beds; plains += m.plains; steep += m.steep; gentle += m.gentle;
	}
	note() << "soil by water: fines in beds " << bed_fine / std::max(beds, 1) << " (" << beds << "), on floodplains "
			<< plain_fine / std::max(plains, 1) << " (" << plains << "); stones in steep beds " << steep_stones / std::max(steep, 1)
			<< " (" << steep << "), gentle " << gentle_stones / std::max(gentle, 1) << " (" << gentle << ")" << std::endl;
	REQUIRE((beds > 20 && plains > 20));
	REQUIRE(plain_fine / plains > 2 * bed_fine / beds);
	if (steep > 5 && gentle > 5)
		REQUIRE(steep_stones / steep > gentle_stones / gentle);
}
