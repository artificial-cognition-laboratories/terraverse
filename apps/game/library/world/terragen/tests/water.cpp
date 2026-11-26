// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// The ground walked block by block over the north pole, and east across
// the date line (longitude ±180°), steps no more than the same walk on a
// meridian through mid-latitudes: no wall, no seam, nothing of a different
// kind there. (On the flat map both were edges.)
TEST_CASE("NoPoleOrSeam")
{
	World w(WorldCode::decode("TF24.dGVycmE"));
	const double R = w.radius() / (double)ONE;
	// the steepest step of a walk of 20° along a great circle in plane (a, b), from angle `from`
	const auto walk = [&](double from, std::array<double, 3> a, std::array<double, 3> b) {
		int32_t was = INT32_MIN, steepest = 0;
		bool was_dry = false;
		for (int32_t k = 0; k <= (int32_t)(R * M_PI / 9); k++) {
			const double t = from + k / R;
			const Dir d = normalise(std::llround((a[0] * std::cos(t) + b[0] * std::sin(t)) * (1 << 20)),
					std::llround((a[1] * std::cos(t) + b[1] * std::sin(t)) * (1 << 20)),
					std::llround((a[2] * std::cos(t) + b[2] * std::sin(t)) * (1 << 20)));
			const Ground g = w.ground(d, w.sunlight(d, std::nullopt));
			const int32_t h = g.terrain;
			// (not across a shore or into a river: a coast can stand as a low
			// cliff over the sea anywhere, a river cuts its bed below its banks,
			// and since TF23 this walk crosses both)
			const bool dry = h > 0 && g.kind == Water::None;
			if (was != INT32_MIN && dry && was_dry)
				steepest = std::max(steepest, std::abs(h - was));
			was = h;
			was_dry = dry;
		}
		return steepest;
	};
	const double ten = M_PI / 18;
	const int32_t pole = walk(M_PI / 2 - ten, { 1, 0, 0 }, { 0, 1, 0 });      // up longitude 0, over, down 180°
	const int32_t seam = walk(M_PI - ten, { 1, 0, 0 }, { 0, 0, 1 });          // the equator across ±180°
	const int32_t middle = walk(M_PI / 6 - ten, { 0, 0, 1 }, { 0, 1, 0 });    // longitude 90°, 20°–40° north
	note() << "steepest step over the pole " << pole << ", across the date line " << seam << ", mid-latitudes " << middle << std::endl;
	REQUIRE(pole <= std::max(2 * middle, 8));
	REQUIRE(seam <= std::max(2 * middle, 8));
}

// A flat picture's (u, v) is a place and back (the preview's map), to a
// few units; v 0 is the north pole whatever u.
TEST_CASE("Coordinates")
{
	for (int32_t i = 0; i < 16; i++)
		for (int32_t j = 1; j < 16; j++) {
			const fix u = ratio(i, 16), v = ratio(j, 16);
			const Dir d = World::place(u, v).dir();
			const fix du = World::u(d) - u;
			REQUIRE(std::min(std::abs(du), ONE - std::abs(du)) < 16); // wraps
			REQUIRE(std::abs(World::v(d) - v) < 16);
		}
	REQUIRE((World::place(0, 0).dir().y > ONE - 4 && World::place(HALF, 0).dir().y > ONE - 4)); // north
	REQUIRE(World::place(HALF, ONE).dir().y < -ONE + 4);                                 // south
}

TEST_CASE("LandShareExact")
{
	const Sphere &sphere = gridSphere();
	for (const char *text : WATER_CODES) {
		WorldCode code = WorldCode::decode(text);
		World w(code);
		const Grid &g = w.grid();
		int64_t land = 0, all = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			all += sphere.area(i);
			land += g.base[i] >= 0 ? sphere.area(i) : 0;
		}
		// within a fraction of a percent of the Land setting, by area
		fix share = (fix)((land << FRAC) / all);
		REQUIRE(near(share, code.land, ONE / 200));
	}
}

TEST_CASE("WaterDrainsToSea")
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		for (int32_t i = 0; i < g.size(); i++) {
			if (g.base[i] < 0)
				continue;
			int32_t c = i, steps = 0;
			while (g.receiver[c] >= 0 && g.base[c] >= 0 && steps < g.size()) {
				c = g.receiver[c];
				steps++;
			}
			// every land cell's water reaches the sea, or a closed basin's lake or
			// dry floor (where it evaporates: keepRealLakes)
			REQUIRE((g.base[c] < 0 || g.lake[c] || g.pan[c]));
		}
	}
}

TEST_CASE("WaterFlowsDownhill")
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		for (int32_t i = 0; i < g.size(); i++) {
			int32_t r = g.receiver[i];
			if (r >= 0)
				REQUIRE(g.filled[r] < g.filled[i]);
		}
	}
}

// What the plates push up and rivers wear away is laid down or carried out
// to the deep sea (creep only moves it): nothing is made or lost, and some
// of it builds land. Gold and tin settle out below the mountains.
// Rough land full of pits, a third of it sea: noise at a few scales.
static std::vector<fix> pittedLand()
{
	const Sphere &sphere = gridSphere();
	std::vector<fix> h(sphere.size());
	for (int32_t i = 0; i < sphere.size(); i++)
		h[i] = fbm(sphere.centre(i), 1234, 9, 5, HALF) + ratio(15, 100);
	return h;
}

// Cordonnier et al.'s routing (drainage.h): every cell reaches the sea, the
// order puts each after its receiver, and the lakes stand where a flood from
// the sea would leave them (a priority-flood without its epsilon).
TEST_CASE("Route")
{
	const Sphere &sphere = gridSphere();
	const std::vector<fix> h = pittedLand();
	const int32_t n = (int32_t)h.size();
	const Routing way = route(h);
	REQUIRE_EQ(size_t(way.order.size()), size_t((size_t)n));
	std::vector<int32_t> at(n, -1);
	for (int32_t k = 0; k < n; k++)
		at[way.order[k]] = k;
	int32_t pits = 0, lakes = 0;
	for (int32_t i = 0; i < n; i++) {
		const int32_t r = way.receiver[i];
		REQUIRE((r < 0) == (h[i] < 0)); // only the sea ends
		if (r < 0)
			continue;
		REQUIRE(at[r] < at[i]);
		// water's surface never rises downstream
		REQUIRE(way.surface[r] <= way.surface[i]);
		bool neighbour = false;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			neighbour = neighbour || *p == r;
		REQUIRE(neighbour);
		lakes += way.surface[i] > h[i];
		bool lower = false;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			lower = lower || std::max<fix>(h[*p], 0) < h[i];
		pits += !lower;
	}
	REQUIRE((pits > 50 && lakes > pits)); // (the land has pits to route, and lakes over them)

	using Entry = std::pair<fix, int32_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
	std::vector<fix> flooded = h;
	std::vector<uint8_t> seen(n, 0);
	for (int32_t i = 0; i < n; i++)
		if (h[i] < 0) {
			seen[i] = 1;
			open.push({ 0, i });
		}
	while (!open.empty()) {
		const auto [level, c] = open.top();
		open.pop();
		for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++)
			if (!seen[*p]) {
				seen[*p] = 1;
				flooded[*p] = std::max(h[*p], level);
				open.push({ flooded[*p], *p });
			}
	}
	for (int32_t i = 0; i < n; i++)
		if (h[i] >= 0)
			REQUIRE_EQ(fix(way.surface[i]), fix(flooded[i]));
}

// Braun & Willett's stream power (erosion.h): land wears toward what it
// drains to and never below it, nothing under a lake wears, the sea stays,
// and more water or a softer rock wears faster.
TEST_CASE("StreamPower")
{
	const std::vector<fix> start = pittedLand();
	const int32_t n = (int32_t)start.size();
	const Routing way = route(start);
	const std::vector<int64_t> area = accumulate(way.receiver, way.order, std::vector<fix>(n, ONE));
	std::vector<fix> soft = start, hard = start;
	streamPower(soft, way, area, std::vector<fix>(n, ratio(10, 100)));
	streamPower(hard, way, area, std::vector<fix>(n, ratio(2, 100)));
	int64_t soft_worn = 0, hard_worn = 0;
	for (int32_t i = 0; i < n; i++) {
		const int32_t r = way.receiver[i];
		if (r < 0 || way.surface[i] > start[i]) {
			REQUIRE_EQ(fix(soft[i]), fix(start[i]));
			continue;
		}
		const bool lake = way.surface[r] > start[r];
		REQUIRE((soft[i] <= start[i] && soft[i] >= std::max<fix>(lake ? way.surface[r] : soft[r], 0)));
		REQUIRE(hard[i] + 8 >= soft[i]); // (fixed point's rounding, a unit or two down a river: under a millimetre)
		soft_worn += start[i] - soft[i];
		hard_worn += start[i] - hard[i];
	}
	REQUIRE((hard_worn > 0 && soft_worn > hard_worn * 2));
}

TEST_CASE("Sediment")
{
	const Sphere &sphere = gridSphere();
	int32_t any_rich = 0;
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		int64_t lost = 0, laid = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			REQUIRE(g.sediment[i] >= 0);
			lost += (int64_t)(g.base[i] - g.eroded[i]) * sphere.area(i);
			if (g.eroded[i] >= 0)
				laid += g.sediment[i];
		}
		REQUIRE_EQ(int64_t(lost + g.uplifted + g.hollows), int64_t(g.to_deep));
		REQUIRE(laid > 0);
		// placers: only in what was laid, below the mountain belts' cores, and rare
		int32_t rich = 0, land = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			REQUIRE((g.placer[i] >= 0 && g.placer[i] <= ONE));
			if (g.placer[i] > 0)
				REQUIRE((g.sediment[i] > 0 && g.tectonics.belt[i] <= VEIN_CORE));
			land += g.eroded[i] >= 0;
			// (a fifth: the richest of these worlds stood 0.22–0.26, and since
			// sprint 112's ranges rise under the rivers, 0.21–0.23)
			rich += g.placer[i] >= ratio(1, 5);
		}
		REQUIRE(rich * 10 < land);
		any_rich += rich;
	}
	REQUIRE(any_rich > 0); // (a world without high mountains has none)
}

// A lake has one surface: touching lake cells share a level, so real
// (flowing) water has nothing to spill down across it.
TEST_CASE("LakesAreFlat")
{
	const Sphere &sphere = gridSphere();
	// (lakes on the worlds together, so the test isn't empty: an archipelago
	// of low islands may hold none at the grid's scale, sprint 120)
	int lakes = 0;
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		for (int32_t i = 0; i < g.size(); i++) {
			if (!g.lake[i])
				continue;
			lakes++;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
				if (g.lake[*p])
					REQUIRE_EQ(fix(g.lake_level[i]), fix(g.lake_level[*p]));
		}
	}
	REQUIRE(lakes > 0);
}

// A river's surface runs down its run smoothly (sprint 122): side by side, two
// columns of one run stand apart by about what its grade drops over a block,
// not a whole block at every step (a staircase of level pools, which the water
// the player sees drained down and over its banks). Only a fall over a hard lip
// is more; every top node is filled to a sixteenth it can hold; and the water
// is a block deep at least everywhere, its bed cut down from its surface.
TEST_CASE("RiversSlopeSmoothly")
{
	World w(WorldCode::decode("TF24.dGVycmE"));
	const auto at = [&](int32_t x, int32_t z) { return w.ground(flat(w, x, z), w.sunlight(flat(w, x, z), std::nullopt)); };
	const auto top = [](const Ground &g) { return g.water * 16 + g.water_part; }; // sixteenths over the sea
	// a grid river's water, east along a few parallels
	int32_t found_x = INT32_MIN, found_z = 0;
	for (int32_t z : { 0, w.code().size / 16, -w.code().size / 16, w.code().size / 8, -w.code().size / 8 })
		for (int32_t x = 0; x < w.code().size && found_x == INT32_MIN; x += 3) {
			const Ground g = at(x, z);
			if (g.kind == Water::River && g.river_cell >= 0 && g.river_width >= toFix(3) && g.water > 2) {
				found_x = x;
				found_z = z;
			}
		}
	REQUIRE(found_x != INT32_MIN);
	// every side-by-side pair of one run's water in the square round it
	const int32_t R = 40;
	std::vector<Ground> square;
	for (int32_t z = -R; z <= R; z++)
		for (int32_t x = -R; x <= R; x++)
			square.push_back(at(found_x + x, found_z + z));
	const int32_t across = 2 * R + 1;
	int pairs = 0, differ = 0, steady = 0, whole = 0;
	int32_t shallowest = INT32_MAX;
	for (int32_t z = 0; z < across; z++)
		for (int32_t x = 0; x < across; x++) {
			const Ground &g = square[(size_t)(z * across + x)];
			if (g.kind != Water::River)
				continue;
			REQUIRE(g.water_part >= 1);
			REQUIRE(g.water_part <= 16);
			// as deep as its channel, never a film over its bed (the bed's cut from
			// the surface down, not from its top node)
			const int32_t deep = top(g) - (g.terrain + 1) * 16;
			REQUIRE(deep >= 16);
			shallowest = std::min(shallowest, deep);
			for (const auto &[dx, dz] : { std::pair{ 1, 0 }, std::pair{ 0, 1 } }) {
				if (x + dx >= across || z + dz >= across)
					continue;
				const Ground &n = square[(size_t)((z + dz) * across + x + dx)];
				if (n.kind != Water::River || n.river_cell != g.river_cell)
					continue;
				pairs++;
				const int32_t d = std::abs(top(g) - top(n));
				const double grade = std::max(g.river_slope, n.river_slope) / (double)ONE;
				differ += d > 0;
				steady += d <= 16 * grade * 1.5 + 2; // (a block's drop at its grade, with the run's bends and rounding)
				whole += d == 16;
			}
		}
	note() << "river at " << found_x << ", " << found_z << ": " << pairs << " pairs of its water side by side, " << differ
		   << " at different heights, " << steady << " within their grade, " << whole << " a block apart; its shallowest water " << shallowest << " sixteenths" << std::endl;
	REQUIRE(pairs > 20);
	REQUIRE(differ > 0);                // it runs downhill
	REQUIRE(steady * 20 >= pairs * 19); // as steeply as its run, no more
	REQUIRE(whole * 10 < differ);       // by sixteenths, not whole blocks
}

