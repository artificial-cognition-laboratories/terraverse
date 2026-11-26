// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// Sprint 100: the drainage below the grid. Every channel runs down the land to
// what it joins, and the network branches as real ones do (Horton: each Strahler order
// has 3–5 times the streams of the next).
// Ranges by what built them (planning/sprint-101-ranges.md)
// A far tile's columns leave out what's narrower than its cells: nothing for a
// block-wide one, and what's left stands about where the whole land does.
// The sea's never invented: a far tile is sea only where the land is. An
// inlet narrower than the tile (a fjord) is left out of it, as a valley is;
// tiles as narrow as an inlet draw it (sprint 112).
TEST_CASE("Coarse" * doctest::test_suite("slow"))
{
	const WorldCode code = WorldCode::decode("TF24.a3U3eWd1Y2c.BQKAqoCAAG1mgICAgICAgICCVQ") /* 250k radius */;
	const World world(code);
	// (each column on its own: a Coarse read is the thread's)
	constexpr int N = 400;
	std::vector<Ground> whole(N), same(N), far(N), near(N);
	parallelFor(N, [&](int32_t k) {
		const Place at = placeOf(k * 7919 % 2001 - 1000, k * 6271 % 2003 - 1001, k * 4057 % 1999 - 999 + 1);
		const fix sun = world.sunlight(at, std::nullopt);
		whole[k] = world.ground(at, sun);
		{
			const World::Coarse one(world, 1);
			same[k] = world.ground(at, sun);
		}
		{
			const World::Coarse narrow(world, 256);
			near[k] = world.ground(at, sun);
		}
		const World::Coarse coarse(world, 4096);
		far[k] = world.ground(at, sun);
	});
	int64_t moved = 0;
	int worst = 0, seas = 0, invented = 0, near_seas = 0, n = 0;
	for (int k = 0; k < N; k++) {
		REQUIRE_EQ(int(same[k].terrain), int(whole[k].terrain));
		const int d = std::abs(far[k].terrain - whole[k].terrain);
		moved += d;
		worst = std::max(worst, d);
		seas += (far[k].kind == Water::Sea) != (whole[k].kind == Water::Sea);
		invented += far[k].kind == Water::Sea && whole[k].kind != Water::Sea;
		near_seas += (near[k].kind == Water::Sea) != (whole[k].kind == Water::Sea);
		n++;
	}
	note() << "coarse columns: moved " << moved / n << " blocks on average, at most " << worst << ", " << seas << " of " << n << " change sea" << std::endl;
	REQUIRE(moved / n < 150);
	REQUIRE(invented == 0);
	REQUIRE(seas * 100 <= n); // inlets narrower than the tile, left out
	REQUIRE(near_seas == 0);
}

// Sprint 110: on an Earth-sized planet (a block a metre) a rising range has
// a real range's relief: the highest ground less the lowest within 10 km is
// over 500 m in most of them (real ranges: 1.5–2.5 km; it was 167 m when the
// ground over the grid was held to 300 blocks on any planet)
// skipped: .agents/jobs/terragen-sprint-116-red-tests.md: relief from the streams, median 304 m of 500 in 10 km, sprint 116 phase 6b
TEST_CASE("RangesAtEarthsScale" * doctest::test_suite("slow") * doctest::skip())
{
	terragen::WorldCode code = terragen::WorldCode::decode("TF24.dGVycmE");
	code.size = (int32_t)std::llround(6371000.0 * 2 * M_PI);
	code.grid_key += "@earth";
	const terragen::World w(code);
	const terragen::Grid &g = w.grid();
	const terragen::Sphere &sphere = terragen::gridSphere();
	const double R = 6371000.0, span = 10000.0;
	std::vector<int> reliefs;
	for (int32_t c = 0; c < sphere.size() && reliefs.size() < 12; c += 97) {
		if (g.base[c] < 0 || g.tectonics.belt[c] < terragen::ratio(6, 10))
			continue;
		const Place m(sphere.centre(c));
		const double u0 = World::u(m) / 65536.0, v0 = World::v(m) / 65536.0;
		int lo = INT32_MAX, hi = INT32_MIN;
		for (int a = -5; a <= 5; a++)
			for (int b = -5; b <= 5; b++) {
				const Place p = World::place((fix)((u0 + a * span / 10 / (2 * M_PI * R)) * 65536), (fix)((v0 + b * span / 10 / (M_PI * R)) * 65536));
				const int h = w.ground(p, w.sunlight(p, std::nullopt)).terrain;
				lo = std::min(lo, h);
				hi = std::max(hi, h);
			}
		reliefs.push_back(hi - lo);
	}
	REQUIRE(reliefs.size() >= 8);
	std::sort(reliefs.begin(), reliefs.end());
	note() << "an Earth's ranges, relief in 10 km: median " << reliefs[reliefs.size() / 2] << " m" << std::endl;
	REQUIRE(reliefs[reliefs.size() / 2] > 500);
}

// skipped: .agents/jobs/terragen-sprint-116-red-tests.md: plateaus cut by rivers, sprint 116 phase 4
TEST_CASE("Ranges" * doctest::skip())
{
	terragen::WorldCode code = terragen::WorldCode::decode("TF24.dGVycmE");
	code.size = 628320;
	code.grid_key += "@wide"; // (what the grid's kept by)
	terragen::World world(code);
	const std::shared_ptr<const terragen::Grid> grid = terragen::gridFor(code);
	const terragen::Tectonics &t = grid->tectonics;
	const terragen::Sphere &sphere = terragen::gridSphere();
	using terragen::ratio;

	// all parts present, and a plateau stands high and level: far less rough
	// across its neighbours than a range still rising without one (its core:
	// belts are graded to foothills since TF20)
	int folded = 0, plains = 0;
	int64_t plain_rough = 0, range_rough = 0;
	int ranges = 0;
	for (int32_t i = 0; i < sphere.size(); i++) {
		folded += grid->eroded[i] > 0 && t.fold[i] > ratio(1, 2);
		const bool plain = grid->eroded[i] > 0 && t.plateau[i] > ratio(95, 100); // (its interior: the margins are cut by rivers)
		const bool range = grid->eroded[i] > 0 && t.plateau[i] == 0 && t.fold[i] == 0 && t.belt[i] > ratio(4, 10);
		if (!plain && !range)
			continue;
		int64_t rough = 0;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			rough += std::abs(grid->eroded[*p] - grid->eroded[i]);
		if (plain) {
			plains++;
			plain_rough += rough;
			REQUIRE(grid->eroded[i] > ratio(6, 10));
		} else {
			ranges++;
			range_rough += rough;
		}
	}
	REQUIRE(folded > 500);
	REQUIRE(plains > 50);
	REQUIRE(ranges > 50);
	REQUIRE(plain_rough / plains * 2 < range_rough / ranges);

	// fold ridges run along the belt: crossing its contours crosses many, and
	// a young belt's stand taller than an old one's
	int across = 0, along = 0, tested = 0;
	for (int32_t i = 0; i < sphere.size() && tested < 40; i++) {
		if (grid->eroded[i] <= 0 || t.fold[i] < ratio(9, 10))
			continue;
		// the neighbours most up and most level along the contours
		int32_t up = i, level = i;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++) {
			if (t.front[*p] > t.front[up])
				up = *p;
			if (level == i || std::abs(t.front[*p] - t.front[i]) < std::abs(t.front[level] - t.front[i]))
				level = *p;
		}
		if (up == i || t.fold[up] < ratio(1, 2))
			continue;
		const terragen::Place a(sphere.centre(i));
		const auto changes = [&](const terragen::Place &b) {
			int count = 0;
			terragen::fix was = terragen::ONE / 2;
			for (int k = 0; k <= 64; k++) {
				const terragen::Place p = terragen::placeOf((int64_t)a.x + ((int64_t)b.x - a.x) * k / 64,
						(int64_t)a.y + ((int64_t)b.y - a.y) * k / 64, (int64_t)a.z + ((int64_t)b.z - a.z) * k / 64);
				const terragen::fix crest = world.ridgesAt(p);
				if ((crest > terragen::ONE / 2) != (was > terragen::ONE / 2))
					count++;
				was = crest;
			}
			return count;
		};
		across += changes(terragen::Place(sphere.centre(up)));
		along += changes(terragen::Place(sphere.centre(level)));
		tested++;
	}
	REQUIRE(tested >= 20);
	// (by a good deal, not twice: they bend, split and close, sprint 106)
	REQUIRE(across > along * 5 / 4);
}

TEST_CASE("Streams")
{
	terragen::WorldCode code = terragen::WorldCode::decode("TF24.dGVycmE");
	code.size = 628320;
	code.grid_key += "@wide"; // (what the grid's kept by)
	terragen::World world(code);
	const std::shared_ptr<const terragen::Grid> grid = terragen::gridFor(code);
	REQUIRE((world.streams() && world.streams()->levels() >= 4));
	// every drawn run's water falls downstream, and meets the next where it ends (sprint 107)
	parallelFor(grid->size(), [&](int32_t c) { world.profile(c); }); // (each run's read once, on every core)
	for (int32_t c = 0; c < grid->size(); c++) {
		const int32_t *p = world.profile(c);
		if (!p)
			continue;
		for (int j = 0; j < terragen::World::PROFILE; j++)
			REQUIRE(p[j + 1] <= p[j]);
		const int32_t *next = world.profile(grid->receiver[c]);
		if (next && !grid->lake[grid->receiver[c]])
			REQUIRE(next[0] <= p[terragen::World::PROFILE]);
	}
	// Streams below the grid (sprint 116, phase 6b), across a patch of a
	// range: some carry water, their water never stands over its land, and
	// what's read is the same from a world that has kept nothing
	// (the most pushed-up land a stream crosses: the most pushed-up of all can
	// be a small island at its own peak, with none)
	int32_t centre = -1;
	for (int32_t i = 0; i < grid->size(); i++)
		if (grid->base[i] > 0 && grid->runoff[i] >= grid->stream_threshold &&
				(centre < 0 || grid->tectonics.belt[i] > grid->tectonics.belt[centre]))
			centre = i;
	const terragen::Place at(terragen::gridSphere().centre(centre));
	const int64_t radius = world.radius() >> terragen::FRAC;
	const fix sun = world.sunlight(at, std::nullopt);
	terragen::World fresh(code);
	int wet = 0, read = 0;
	for (int y = -40; y < 40; y++)
		for (int x = -40; x < 40; x++) {
			// a column every 8 blocks, east and north of the cell's centre
			const terragen::Place p = terragen::placeOf((int64_t)at.x + ((int64_t)x * 8 << terragen::PLACE_FRAC) / radius,
					(int64_t)at.y + ((int64_t)y * 8 << terragen::PLACE_FRAC) / radius, at.z);
			const terragen::Ground g = world.ground(p, sun);
			read++;
			if (g.kind == terragen::Water::River && (g.river_dist == INT64_MAX || g.river_dist > g.river_width / 2 + terragen::toFix(1))) {
				wet++;
				REQUIRE(g.water > g.terrain);
				REQUIRE(g.water - g.terrain <= 4); // (a stream's bed: never a hollow under it)
			}
			if ((x + y) % 9 == 0) {
				const terragen::Ground h = fresh.ground(p, sun);
				REQUIRE((h.terrain == g.terrain && h.water == g.water && h.kind == g.kind));
			}
		}
	REQUIRE((wet > 0 && wet < read / 4));
}
