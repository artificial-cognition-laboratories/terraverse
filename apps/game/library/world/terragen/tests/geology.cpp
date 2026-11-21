// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// Plates cover the world; boundaries of every kind exist; mountains stand
// along the collisions.
TEST_CASE("Plates")
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		const Tectonics &t = g.tectonics;
		REQUIRE(t.plates.size() >= 7);
		int kinds[4] = {};
		double near = 0, far = 0;
		int nn = 0, nf = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			REQUIRE(t.plate[i] < t.plates.size());
			if (t.distance[i] == 0)
				kinds[(int)t.boundary[i]]++;
			if (g.eroded[i] < 0)
				continue;
			bool collision = t.boundary[i] == Boundary::Convergent && t.distance[i] < 5;
			(collision ? near : far) += g.eroded[i];
			(collision ? nn : nf)++;
		}
		REQUIRE((kinds[(int)Boundary::Convergent] > 0 && kinds[(int)Boundary::Divergent] > 0 && kinds[(int)Boundary::Transform] > 0));
		if (nn > 50)
			REQUIRE(near / nn > far / nf); // ranges along the collisions
	}
}

// The plates' history: continental crust is kept (what collisions fold
// away and arcs build is counted; the rest moves cell to cell, which the
// nearest-cell handover only keeps roughly), ocean floor forms where plates
// part, and things happen: collisions, openings, arcs.
TEST_CASE("History")
{
	const Sphere &sphere = gridSphere();
	for (const char *text : WATER_CODES) {
		const History h = history(WorldCode::decode(text));
		REQUIRE(h.cells.size() == (size_t)sphere.size());
		int64_t continental = 0, young = 0;
		for (size_t i = 0; i < h.cells.size(); i++) {
			const History::Cell &c = h.cells[i];
			REQUIRE(c.plate < h.plates.size());
			REQUIRE(c.continental == (c.formed != Formed::Ocean));
			if (c.continental)
				continental += sphere.area((int32_t)i);
			else if (c.age < HISTORY_STEPS)
				young++;
		}
		REQUIRE(continental == h.continental_end);
		// within a fifth of what the rules alone would leave
		const int64_t expected = h.continental_start + h.made - h.lost;
		REQUIRE(std::abs(h.continental_end - expected) * 5 < expected);
		REQUIRE(young > 0);
		bool kinds[3] = {};
		for (const History::Event &e : h.events) {
			REQUIRE((e.step >= 0 && e.step < HISTORY_STEPS));
			kinds[(int)e.kind] = true;
		}
		REQUIRE((kinds[(int)History::Kind::Collision] || kinds[(int)History::Kind::Arc]));
		REQUIRE(kinds[(int)History::Kind::Opening]);
	}
}

// Volcanoes stand where plates allow them, and some are active; a column
// at an active vent holds lava.
TEST_CASE("Volcanoes")
{
	World w(WorldCode::decode(WATER_CODES[0]));
	const Grid &g = w.grid();
	REQUIRE(!g.volcanoes.empty());
	int active = 0, calderas = 0, old = 0;
	for (const Volcano &v : g.volcanoes) {
		active += v.active;
		calderas += v.caldera;
		old += v.age > 0;
		REQUIRE((v.radius > 0 && v.height > 0 && v.crater < v.radius));
		REQUIRE((!v.active || v.age == 0)); // only the youngest erupt
	}
	REQUIRE(active > 0);
	// (sprint 112) some tops fell in, and most have been worn a while
	REQUIRE((calderas > 0 && old * 2 > (int)g.volcanoes.size()));
	bool lava = false;
	for (const Volcano &v : g.volcanoes) {
		if (!v.active)
			continue;
		// beside it: past its slopes, to the north
		const Dir north = dirAt(latitudeOf(v.at) + w.angle(v.radius + 20), longitudeOf(v.at));
		Ground at = w.ground(v.at, w.sunlight(v.at, std::nullopt));
		Ground beside = w.ground(north, w.sunlight(north, std::nullopt));
		lava |= at.kind == Water::Lava;
		REQUIRE((at.terrain > beside.terrain || at.kind == Water::Lava)); // the cone stands up
	}
	REQUIRE(lava);
}

// Each place has a history (sprint 120): every world on a plate planet has
// six or more tectonic settings on its land, none over 35% of it.
TEST_CASE("Settings")
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		const Sphere &sphere = gridSphere();
		double share[(int)Setting::Count] = {}, land = 0;
		for (int32_t i = 0; i < g.size(); i++)
			if (g.eroded[i] >= 0) {
				share[(int)g.setting[i]] += sphere.area(i);
				land += sphere.area(i);
			}
		int kinds = 0;
		for (int s = 0; s < (int)Setting::Count; s++) {
			INFO(std::string(text), " ", SETTINGS[s].name);
			CHECK(share[s] <= 0.35 * land);
			kinds += share[s] > 0.01 * land;
		}
		INFO(std::string(text));
		CHECK(kinds >= 6);
	}
}

// Rock follows the province: sediment over basement in basins, basalt under
// the sea floor, granite or gneiss in the shields.
TEST_CASE("Strata")
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	int seen[(int)Province::Count] = {};
	for (int32_t k = 0; k < 4000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Strata s = w.strata(at, g);
		seen[(int)s.province]++;
		Rock deep = w.rock(s, 400, g.terrain - 400);
		switch (s.province) {
		case Province::OceanFloor: REQUIRE(deep == Rock::Basalt); break;
		case Province::Shield: REQUIRE((deep == Rock::Granite || deep == Rock::Gneiss)); break;
		case Province::Basin: {
			REQUIRE(s.sediment >= 20);
			Rock top = w.rock(s, 0, g.terrain);
			REQUIRE((top == Rock::Sandstone || top == Rock::Shale || top == Rock::Limestone || top == Rock::Obsidian));
			break;
		}
		default: break;
		}
	}
	// every kind of ground somewhere, but a lid's (on a plate world: none)
	for (int p = 0; p < (int)Province::Count; p++)
		REQUIRE((seen[p] > 0) == (p != (int)Province::Highland && p != (int)Province::Mare));
}

// Ores where their processes put them, and caves most of all in limestone.
TEST_CASE("OresAndCaves")
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	int ores[(int)Ore::Count] = {};
	int carved[(int)Rock::Count] = {}, blocks[(int)Rock::Count] = {};
	for (int32_t k = 0; k < 3000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Strata s = w.strata(at, g);
		for (int32_t depth = 0; depth < 140; depth++) {
			int32_t y = g.terrain - depth;
			Rock r = w.rock(s, depth, y);
			ores[(int)w.ore(s, r, depth, y)]++;
			// tunnels, above where deep caverns open (those are in the basement, whatever the rock)
			const bool cave = w.cave(s, r, depth, y, g.kind != Water::None) != Cave::None;
			if (depth <= 60) {
				blocks[(int)r]++;
				carved[(int)r] += cave;
			}
		}
	}
	for (Ore o : { Ore::Copper, Ore::Lead, Ore::Iron, Ore::Magnetite, Ore::Coal })
		REQUIRE(ores[(int)o] > 0);
	REQUIRE(ores[(int)Ore::None] > 50 * (ores[(int)Ore::Copper] + ores[(int)Ore::Iron])); // ore is the exception
	auto share = [&](Rock r) { return blocks[(int)r] ? (double)carved[(int)r] / blocks[(int)r] : 0.0; };
	// (against all the other rock: granite alone is too few blocks where the
	// shields are platforms' cover, sprint 120)
	int64_t rest = 0, rest_carved = 0;
	for (int r = 0; r < (int)Rock::Count; r++)
		if (r != (int)Rock::Limestone) {
			rest += blocks[r];
			rest_carved += carved[r];
		}
	REQUIRE((blocks[(int)Rock::Limestone] > 0 && share(Rock::Limestone) > 2.0 * rest_carved / std::max<int64_t>(1, rest)));
}
