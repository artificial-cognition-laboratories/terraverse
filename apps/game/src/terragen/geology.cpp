// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "geology.h"
#include "grid.h"
#include "plates.h"
#include "noise.h"
#include "world.h"
#include <algorithm>
#include <array>
#include <queue>

namespace terragen {

const char *const PROVINCE_NAMES[(int)Province::Count] = {
	"shield", "orogen", "arc", "rift", "basin", "volcanic", "ocean floor", "highland", "mare",
};
const char *const ROCK_NAMES[(int)Rock::Count] = {
	"granite", "gneiss", "slate", "basalt", "andesite", "tuff", "obsidian", "sandstone", "shale", "limestone", "anorthosite", "breccia", "ice",
};
const char *const ORE_NAMES[(int)Ore::Count] = {
	"none", "copper", "tin", "lead", "gold", "iron", "magnetite", "coal", "salt", "sulfur", "ilmenite", "kreep", "ice", "flint",
};

using namespace plates;

namespace {

// Noise streams for geology (offsets from the world's seed hash).
enum GeoStream : uint32_t {
	VOLCANO = 107, HOTSPOT = 109, SEDIMENT = 113, FOLD = 127,
	PLUTON = 131, VEIN = 137, METAL = 139, MAGNETITE = 149, BANDED = 151, CAVE_A = 157, CAVE_B = 163,
	TUBE = 167, CAVERN = 173, SULFUR = 179, PHASE = 181, INLAND_BASIN = 191, ILMENITE = 193, KREEP = 197, FLINT = 433,
};

const int GIANT_SHIELDS = 5, PATERAE = 200;

} // namespace

// f averaged with its neighbours, `passes` times
static std::vector<fix> smoothed(std::vector<fix> f, int passes)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	for (int pass = 0; pass < passes; pass++) {
		const std::vector<fix> was = f;
		for (int32_t i = 0; i < n; i++) {
			int64_t sum = was[i];
			int32_t count = 1;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, count++)
				sum += was[*p];
			f[i] = (fix)(sum / count);
		}
	}
	return f;
}

// Each range's parts, from what its crust went through. Crust is handed over
// cell by cell, so what it carries is smoothed before it's read
static void styles(const History &h, Tectonics &t)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	std::vector<fix> over(n), under(n);
	for (int32_t i = 0; i < n; i++) {
		over[i] = h.cells[i].over;
		under[i] = h.cells[i].under;
	}
	over = smoothed(std::move(over), 3);
	under = smoothed(std::move(under), 3);

	// how far in from the belt's edge, by true distances along the cells
	const fix EDGE = ratio(3, 100);
	// how many cells in from the edge fold trains reach
	const fix FOLD_FRINGE = toFix(6);
	t.front.assign(n, 0);
	std::vector<fix> far(n, INT32_MAX);
	using Open = std::pair<fix, int32_t>;
	std::priority_queue<Open, std::vector<Open>, std::greater<Open>> open;
	for (int32_t i = 0; i < n; i++) {
		if (over[i] + under[i] <= EDGE) {
			far[i] = 0;
			open.push({ 0, i });
		}
	}
	while (!open.empty()) {
		const auto [at, i] = open.top();
		open.pop();
		if (at > far[i])
			continue;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++) {
			const Dir &a = sphere.centre(i), &b = sphere.centre(*p);
			const Dir gap{ a.x - b.x, a.y - b.y, a.z - b.z };
			const fix step = div(sqrtWide64(plates::dot(gap, gap) >> FRAC), sphere.spacing());
			if (at + step < far[*p]) {
				far[*p] = at + step;
				open.push({ far[*p], *p });
			}
		}
	}
	// (a planet that is all belt has no edge to be far from)
	for (int32_t i = 0; i < n; i++)
		t.front[i] = far[i] == INT32_MAX ? 0 : far[i];
	t.front = smoothed(std::move(t.front), 3);

	t.fold.assign(n, 0);
	t.plateau.assign(n, 0);
	t.oldness.assign(n, 0);
	for (int32_t i = 0; i < n; i++) {
		if (!h.cells[i].continental)
			continue;
		const fix all = over[i] + under[i];
		if (all <= EDGE)
			continue;
		const fix core = clamp(div(all - ratio(60, 100), ratio(60, 100)), 0, ONE);
		// mostly ahead of the front, less behind it; none in the crystalline core
		const fix ahead = div(under[i], all);
		t.fold[i] = mul(mul(clamp(div(all - EDGE, ratio(12, 100)), 0, ONE), ratio(25, 100) + mul(ahead, ratio(75, 100))), ONE - core);
		// a fringe of the belt, its outer FOLD_FRINGE cells (the Jura, the
		// Valley and Ridge): further in, the crust is thrust and stacked, not
		// folded in trains; and an old belt's are mostly worn away
		t.fold[i] = mul(t.fold[i], ONE - smooth(clamp(div(t.front[i] - FOLD_FRINGE / 2, FOLD_FRINGE / 2), 0, ONE)));
		t.fold[i] = mul(t.fold[i], ONE - mul(ratio(h.cells[i].age, 16) > ONE ? ONE : ratio(h.cells[i].age, 16), ratio(60, 100)));
		// (only the most thickened: a Tibet or an Altiplano is rare, most ranges stand in crests)
		t.plateau[i] = clamp(div(over[i] - ratio(35, 100), ratio(30, 100)), 0, ONE);
		t.oldness[i] = clamp(ratio(h.cells[i].age, 16), 0, ONE);
	}
	t.plateau = smoothed(std::move(t.plateau), 4);
	for (int32_t i = 0; i < n; i++)
		t.fold[i] = mul(t.fold[i], ONE - t.plateau[i]);
}

// Crust stands still (Tectonics::stable) from STILL_FROM steps after it last
// rose, fully STILL_STEPS later, and from STILL_EDGE cells off an active edge
// to three times that.
const int32_t STILL_FROM = 6, STILL_STEPS = 22, STILL_EDGE = 4;
// How fast a shield still rises, against a young land's 0.18 (Tectonics::belt)
const fix STILL_RISE = ratio(3, 100);

Tectonics tectonics(const WorldCode &c, const History &h)
{
	const Sphere &sphere = gridSphere();
	Tectonics t;
	size_t n = (size_t)sphere.size();
	t.plates = h.plates;
	const int count = (int)t.plates.size();
	t.plate.resize(n);
	t.formed.resize(n);
	t.age.resize(n);
	// where each plate is today: the middle of its cells, and whether it's
	// mostly ocean floor (the denser side sinks)
	std::vector<std::array<int64_t, 3>> middle(count, { 0, 0, 0 });
	std::vector<int64_t> cells(count, 0), continental(count, 0);
	for (size_t i = 0; i < n; i++) {
		const History::Cell &cell = h.cells[i];
		const Dir &d = sphere.centre((int32_t)i);
		t.plate[i] = cell.plate;
		t.formed[i] = cell.formed;
		t.age[i] = cell.age;
		middle[cell.plate][0] += d.x;
		middle[cell.plate][1] += d.y;
		middle[cell.plate][2] += d.z;
		cells[cell.plate]++;
		continental[cell.plate] += cell.continental;
	}
	for (int k = 0; k < count; k++)
		// (a plate swallowed whole keeps where it was; so does one covering the
		// whole body, a stagnant lid, whose cells have no middle)
		if (cells[k] > 0 && (middle[k][0] != 0 || middle[k][1] != 0 || middle[k][2] != 0)) {
			t.plates[k].centre = normalise(middle[k][0], middle[k][1], middle[k][2]);
			t.plates[k].oceanic = continental[k] * 2 < cells[k];
		}

	// boundaries: how each pair of plates moves across the line between their
	// centres, at its middle. Classified per pair, so a whole boundary has one
	// kind (per cell edge, a wandering boundary flickered between kinds)
	std::vector<Boundary> pair_kind((size_t)count * count, Boundary::None);
	std::vector<Side> pair_side((size_t)count * count, Side::None);
	for (int a = 0; a < count; a++)
		for (int b = 0; b < count; b++) {
			if (a == b)
				continue;
			const Plate &pa = t.plates[a], &pb = t.plates[b];
			const Dir &ca = pa.centre, &cb = pb.centre;
			if (dot(ca, cb) < -(int64_t)ONE * ONE * 99 / 100)
				continue; // opposite sides of the planet: they never meet
			const Dir m = normalise((int64_t)ca.x + cb.x, (int64_t)ca.y + cb.y, (int64_t)ca.z + cb.z);
			// from a toward b, along the ground at the middle
			const fix along = (fix)((dot(Dir{ cb.x - ca.x, cb.y - ca.y, cb.z - ca.z }, m)) >> FRAC);
			const Dir across{ cb.x - ca.x - mul(along, m.x), cb.y - ca.y - mul(along, m.y), cb.z - ca.z - mul(along, m.z) };
			if (across.x == 0 && across.y == 0 && across.z == 0)
				continue;
			const Dir towards = normalise(across.x, across.y, across.z);
			const Dir va = velocity(pa, m), vb = velocity(pb, m);
			const fix closing = (fix)(dot(Dir{ va.x - vb.x, va.y - vb.y, va.z - vb.z }, towards) >> FRAC);
			Boundary kind = closing > ratio(3, 10) ? Boundary::Convergent
					: closing < -ratio(3, 10) ? Boundary::Divergent : Boundary::Transform;
			pair_kind[(size_t)a * count + b] = kind;
			if (kind == Boundary::Convergent) // the denser plate sinks: ocean under continent; between oceans, the lower index
				pair_side[(size_t)a * count + b] = pa.oceanic == pb.oceanic
						? (pa.oceanic ? (a < b ? Side::Overriding : Side::Subducting) : Side::Collision)
						: (pa.oceanic ? Side::Subducting : Side::Overriding);
		}
	t.boundary.assign(n, Boundary::None);
	t.side.assign(n, Side::None);
	t.distance.assign(n, 255);
	std::queue<int32_t> frontier;
	for (int32_t i = 0; i < (int32_t)n; i++) {
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++) {
			int32_t j = *p;
			if (t.plate[j] == t.plate[i])
				continue;
			size_t pair = (size_t)t.plate[i] * count + t.plate[j];
			Boundary kind = pair_kind[pair];
			if (t.boundary[i] == Boundary::None || kind == Boundary::Convergent ||
					(kind == Boundary::Divergent && t.boundary[i] == Boundary::Transform)) {
				t.boundary[i] = kind;
				t.side[i] = pair_side[pair];
			}
		}
		if (t.boundary[i] != Boundary::None) {
			t.distance[i] = 0;
			frontier.push(i);
		}
	}
	// distance to the nearest boundary of the cell's own plate, carrying its kind and side inward
	while (!frontier.empty()) {
		int32_t i = frontier.front();
		frontier.pop();
		if (t.distance[i] >= 40)
			continue;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++) {
			int32_t j = *p;
			if (t.plate[j] != t.plate[i] || t.distance[j] <= t.distance[i] + 1)
				continue;
			t.distance[j] = t.distance[i] + 1;
			t.boundary[j] = t.boundary[i];
			t.side[j] = t.side[i];
			frontier.push(j);
		}
	}

	// what each boundary does to the land: how hard it's pushed up lately
	// (a little everywhere: the land rises slowly as it wears), trenches
	// where ocean sinks, rift valleys where land is pulled apart. (Mid-ocean
	// ridges come from the floor's age.)
	t.belt.resize(n);
	t.relief.assign(n, 0);
	for (size_t i = 0; i < n; i++) {
		int32_t d = t.distance[i];
		bool sea = !h.cells[i].continental;
		t.belt[i] = h.cells[i].rate;
		if (t.boundary[i] == Boundary::Convergent && t.side[i] == Side::Subducting && sea && d < 2)
			t.relief[i] = -mul(ratio(22, 100), falloff(d, 2));
		else if (t.boundary[i] == Boundary::Divergent && !sea) {
			// A continental rift isn't a trough the boundary's length: it opens
			// in basins strung along it, each deepest at its middle and closed
			// at its ends by a step of higher ground (the East African lakes,
			// the Rhine graben's), and its width wanders. Noise along it
			// decides where (a few basins a boundary), and how far each reaches.
			const Dir at = sphere.centre((int32_t)i);
			const fix basins = clamp(fbm(at, c.seed_hash + RIFTS, 24, 2, HALF) * 3 + ratio(20, 100), 0, ONE);
			const fix reach = toFix(2) + fbm(at, c.seed_hash + RIFTS + 1, 48, 2, HALF) * 3; // cells, about 1 to 3
			const fix fade = d * ONE >= reach ? 0 : smooth(ONE - div(toFix(d), std::max<fix>(reach, ONE)));
			t.relief[i] = -mul(mul(ratio(5, 100), smooth(basins)), fade);
		}
	}
	styles(h, t);
	// Shields and platforms: crust that last rose long ago, isn't being
	// pushed up now, and is far from an edge that's doing anything
	t.stable.assign(n, 0);
	for (size_t i = 0; i < n; i++) {
		if (!h.cells[i].continental)
			continue;
		const fix old = smooth(clamp(ratio(t.age[i] - STILL_FROM, STILL_STEPS), 0, ONE));
		const fix quiet = ONE - clamp(h.cells[i].rate * 3, 0, ONE);
		const fix away = t.boundary[i] == Boundary::Transform || t.boundary[i] == Boundary::None ? ONE
				: smooth(clamp(ratio(t.distance[i] - STILL_EDGE, STILL_EDGE * 2), 0, ONE));
		t.stable[i] = mul(mul(old, quiet), away);
	}
	t.stable = smoothed(std::move(t.stable), 10);
	// (the slow rise everywhere that keeps land from wearing flat is the
	// young crust's: a shield barely rises, so it's worn to a plain)
	for (size_t i = 0; i < n; i++)
		t.belt[i] = std::max(t.belt[i], STILL_RISE + mul(ratio(18, 100) - STILL_RISE, ONE - t.stable[i]));
	return t;
}

std::vector<Volcano> volcanoes(const WorldCode &c, const Grid &g)
{
	const Sphere &sphere = gridSphere();
	const Tectonics &t = g.tectonics;
	std::vector<Volcano> out;
	if (c.processes.stiff < ONE)
		return out; // an ice crust has no lava to build cones
	// a cell across, in blocks: the old grid's sizes are kept in cells
	const int32_t cell = (int32_t)std::max<int64_t>(1, (World::cellBlocksOf(c) + HALF) >> FRAC);
	const fix spacing = sphere.spacing();
	const fix scale = blocksPerMetre(c);
	uint32_t seed = c.seed_hash + VOLCANO;
	auto place = [&](const Dir &at, VolcanoKind kind, int age, uint32_t h) {
		if (out.size() >= 250)
			return;
		Volcano v;
		v.kind = kind;
		// somewhere in its cell, not at the centre
		const fix dx = mul(unit(h) - HALF, spacing), dy = mul(unit(h >> 8) - HALF, spacing), dz = mul(unit(h >> 16) - HALF, spacing);
		v.at = normalise((int64_t)at.x + dx, (int64_t)at.y + dy, (int64_t)at.z + dz);
		uint32_t a = mix(h, 11, seed), b = mix(h, 13, seed);
		// sizes in metres (a height and how many heights wide), made blocks by
		// the body's scale, and never wider than a few cells
		int32_t metres, wide;
		switch (kind) {
		case VolcanoKind::Arc: // steep cones
			metres = 1500 + (int32_t)(b % 2000);
			wide = 4;
			v.active = (a >> 12) % 100 < 40;
			break;
		case VolcanoKind::Rift: // low fissure cones
			metres = 500 + (int32_t)(b % 800);
			wide = 6;
			v.active = (a >> 12) % 100 < 30;
			break;
		case VolcanoKind::Hotspot: // broad shields, older ones worn down
			metres = (2000 + (int32_t)(b % 4000)) * (5 - age) / 5;
			wide = 12;
			v.active = age == 0;
			break;
		default: throw std::logic_error("terragen::volcanoes: a lid's kind on plates");
		}
		v.height = std::max(4, floorInt(scale * metres));
		v.radius = std::min(v.height * wide * (8 + (int32_t)(a % 5)) / 10, cell * 3);
		v.radius = std::max(v.radius, std::min(v.height * 2, cell * 3)); // (no steeper than a cone half as wide as high)
		// (a cone's slopes are worked in fixed blocks: World::volcanic)
		v.radius = std::min(v.radius, 16000);
		v.crater = std::max(5, v.radius / 7);
		v.depth = v.crater * 3 / 4;
		fix e = g.eroded[sphere.cell(v.at)];
		v.base = floorInt(mul(e, heightScale(c)));
		out.push_back(v);
	};

	// a lid's: wide vents that aren't limited by a cell's cones
	auto lidVent = [&](const Dir &at, VolcanoKind kind, int32_t height, int32_t radius, int32_t crater, int32_t depth, bool active) {
		const fix e = g.eroded[sphere.cell(at)];
		out.push_back({ at, radius, height, crater, depth, floorInt(mul(e, heightScale(c))), active, kind });
	};
	// Shields: a plume stays under a lid, so one vent grows until the rock can't
	// bear more (tallest under the least pull: Olympus Mons, 22 km at 0.38 g);
	// slopes of 1 in 14. Silent now, their calderas shallow.
	if (c.processes.plumes > 0) {
		const int32_t tallest = floorInt(div(toFix(8), c.processes.gravity)) * 1000;
		for (int k = 0; k < GIANT_SHIELDS; k++) {
			const uint32_t a = mix(k, 21, seed), b = mix(k, 23, seed), d = mix(k, 25, seed);
			const Dir way = scattered(a, b, d);
			const fix off = k == 0 ? ratio(30, 100) : ratio(18, 100); // the tallest stands off the rise
			const Dir at = normalise((int64_t)g.rise.x + mul(way.x, off), (int64_t)g.rise.y + mul(way.y, off), (int64_t)g.rise.z + mul(way.z, off));
			const int32_t h = std::max(8, floorInt(scale * (tallest * (100 - k * 18) / 100)));
			const int32_t radius = std::min(h * 14, 16000);
			lidVent(at, VolcanoKind::Giant, h, radius, radius / 7, h / 8, false);
		}
	}
	// Paterae: where eruptions bury the land, wide flat calderas (a hundred km
	// and more) barely raised, half of them lava lakes now
	if (c.processes.eruptions > 0)
		for (int k = 0; k < PATERAE; k++) {
			const uint32_t a = mix(k, 31, seed), b = mix(k, 33, seed), d = mix(k, 35, seed);
			const int32_t height = std::max(6, floorInt(scale * (300 + (int32_t)(a % 500))));
			const int32_t radius = std::min(std::max(16, floorInt(scale * (25000 + (int32_t)(b % 60000)))), 16000);
			const Dir at = scattered(a, b, d);
			if (g.eroded[sphere.cell(at)] > ratio(30, 100))
				continue; // on the plains: the thrust blocks stand where lava doesn't reach
			lidVent(at, VolcanoKind::Patera, height, radius, radius * 6 / 10, height, d % 2 == 0);
		}

	for (int32_t i = 0; i < sphere.size(); i++) {
		uint32_t h = mix(i, 1, seed);
		bool land = g.eroded[i] >= 0;
		if (t.boundary[i] == Boundary::Convergent && t.side[i] == Side::Overriding && t.distance[i] == 3 && h % 9 == 0)
			place(sphere.centre(i), VolcanoKind::Arc, 0, h);
		else if (t.boundary[i] == Boundary::Divergent && t.distance[i] == 0 && land && h % 16 == 0)
			place(sphere.centre(i), VolcanoKind::Rift, 0, h);
	}

	// hot spots: plumes from deep down, leaving a chain behind as the plate drifts over them
	int spots = 2 + (int)(c.seed_hash % 3);
	for (int k = 0; k < spots; k++) {
		uint32_t hs = c.seed_hash + HOTSPOT;
		const Dir spot = scattered(mix(k, 1, hs), mix(k, 2, hs), mix(k, 3, hs));
		const Plate &p = t.plates[t.plate[sphere.cell(spot)]];
		const Dir drift = velocity(p, spot);
		if (drift.x == 0 && drift.y == 0 && drift.z == 0)
			continue; // at its plate's pole: it doesn't move over it
		const Dir ahead = normalise(drift.x, drift.y, drift.z);
		for (int age = 0; age < 4; age++) {
			// four cells back along where the plate came from, per age
			const fix back = spacing * 4 * age;
			const Dir at = normalise((int64_t)spot.x - mul(ahead.x, back), (int64_t)spot.y - mul(ahead.y, back),
					(int64_t)spot.z - mul(ahead.z, back));
			place(at, VolcanoKind::Hotspot, age, mix(k, 7 + age, hs));
		}
	}
	return out;
}

std::vector<Province> provinces(const WorldCode &c, const Grid &g)
{
	const Sphere &sphere = gridSphere();
	const Tectonics &t = g.tectonics;
	size_t n = (size_t)sphere.size();
	std::vector<Province> out(n);
	for (size_t i = 0; i < n; i++) {
		// a lid: the old crust, and the lava that flooded it (all of it, where
		// eruptions keep burying it)
		if (c.processes.eruptions > 0) {
			out[i] = Province::Volcanic;
			continue;
		}
		if (c.processes.regime == Regime::Lid) {
			out[i] = g.mare[i] > 0 ? Province::Mare : Province::Highland;
			continue;
		}
		int32_t d = t.distance[i];
		fix e = g.eroded[i];
		if (e < 0) {
			out[i] = t.boundary[i] == Boundary::Divergent && d <= 2 ? Province::Rift : Province::OceanFloor;
			continue;
		}
		// what the history built here, or the boundary at work now; ranges
		// older than most of the history are worn to the shield's basement
		const Formed f = t.formed[i];
		const bool young = t.age[i] < HISTORY_STEPS * 3 / 4;
		const bool converging = t.boundary[i] == Boundary::Convergent && d <= 4;
		if ((f == Formed::Arc && young) || (converging && t.side[i] == Side::Overriding))
			out[i] = Province::Arc;
		else if ((f == Formed::Orogen && young) || converging)
			out[i] = Province::Orogen;
		else if ((f == Formed::Rift && t.age[i] < 8) || (t.boundary[i] == Boundary::Divergent && d <= 3))
			out[i] = Province::Rift;
		else if (e < ratio(12, 100) || (e < ratio(22, 100) && g.rain((int32_t)i) > ONE) ||
				// broad sagging basins inside plates, where old seas and swamps laid down their beds
				(t.distance[i] > 8 && noise(sphere.centre((int32_t)i), c.seed_hash + INLAND_BASIN, 4) > ratio(30, 100)))
			out[i] = Province::Basin;
		else
			out[i] = Province::Shield;
	}
	// lava fields around every volcano
	const int64_t radius = World::radiusOf(c);
	for (const Volcano &v : g.volcanoes) {
		const int64_t reach = (int64_t)v.radius * 3 / 2 * ONE;
		for (int32_t cell : sphere.around(sphere.cell(v.at), [&](int32_t k) {
				return World::blocksOf(chord(sphere.centre(k), v.at), radius) <= reach; }))
			out[cell] = Province::Volcanic;
	}
	return out;
}

// ---- a column's geology ---------------------------------------------------

namespace {

// a hash of a block, for scattered things (sulfur crystals)
uint32_t blockHash(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	return mix((uint32_t)x, mix((uint32_t)y, (uint32_t)z, seed), seed);
}

// How far down the beds a fold valley's top lies from its crest's: from
// sandstone (the first beds) to shale
const int32_t CREST_TO_VALLEY = 14;

// position in the repeating sequence of sedimentary beds (36 blocks: sand, mud, lime)
int32_t bed(int32_t depth, int32_t phase)
{
	return ((depth + phase) % 36 + 36) % 36;
}

} // namespace

Strata World::strata(const Place &at, const Ground &ground) const
{
	const Grid &g = *m_grid;
	int32_t i = cell(at);
	Strata s;
	s.at = at;
	s.province = g.province[i];
	int32_t size = m_code.size;
	fix lumpy = noise(at, m_code.seed_hash + SEDIMENT, size / 300); // −1..1
	fix e = ground.column.elevation;

	switch (s.province) {
	case Province::Basin: {
		// deepest in the lowest basins: up to ~120 blocks of sediment (inland
		// basins on high ground keep at least 30)
		fix low = clamp(div(ratio(18, 100) - e, ratio(18, 100)), 0, ONE);
		s.sediment = 40 + floorInt(mul(toFix(70), low)) + floorInt(mul(toFix(12), lumpy));
		s.fold = 4;
		break;
	}
	case Province::OceanFloor: s.sediment = 6 + floorInt(mul(toFix(6), lumpy)); s.fold = 2; break;
	case Province::Shield: s.sediment = std::max(0, floorInt(mul(toFix(6), lumpy))); s.fold = 3; break;
	case Province::Orogen: s.sediment = std::max(0, floorInt(mul(toFix(10), lumpy))); s.fold = 20; break;
	case Province::Arc: s.sediment = std::max(0, floorInt(mul(toFix(5), lumpy))); s.fold = 10; break;
	case Province::Highland:
	case Province::Mare: s.sediment = 0; s.fold = 0; break; // nothing laid down by water
	default: s.sediment = std::max(0, floorInt(mul(toFix(5), lumpy))); s.fold = 6; break;
	}
	if (m_code.processes.eruptions > 0)
		s.sediment = 0; // lava on lava
	// beds rise and fall across the land: the folds, broad and gentle away from mountains
	s.tilt = floorInt(mul(toFix(s.fold), noise(at, m_code.seed_hash + FOLD, std::max(8, size / 400))));
	s.phase = (int32_t)((m_code.seed_hash >> 4) % 36) + floorInt(mul(toFix(10), noise(at, m_code.seed_hash + PHASE, 6)));
	s.pluton = (s.province == Province::Orogen || s.province == Province::Arc || s.province == Province::Shield) &&
			noise(at, m_code.seed_hash + PLUTON, std::max(8, size / 700)) > ratio(35, 100);
	// Fold ridges are their rock: thick folded sediment, hard sandstone cropping
	// out on the crests and weak shale along the valleys (each column's top bed
	// is set by where it lies on the ridge)
	s.crumpled = s.province == Province::Orogen;
	if (const Folds f = folds(at); f.height > 0 && m_code.processes.eruptions <= 0) {
		s.sediment = std::max(s.sediment, 60);
		s.tilt = 0;
		s.crumpled = false;
		s.phase = 36 + floorInt(mul(toFix(CREST_TO_VALLEY), ONE - f.crest));
	}
	s.humid = ground.column.wet > ratio(15, 100);
	s.arid = ground.column.wet < -ratio(20, 100);
	s.placer = between(g.placer, at);
	s.lava_province = s.province == Province::Volcanic || s.province == Province::Rift || s.province == Province::Arc;

	// a lid: shattered by impacts some 2 km down (Hiesinger & Head 2006), the
	// lava as deep as it pooled
	s.breccia = s.mare = 0;
	s.titanium = s.thin = 0;
	s.cold_trap = false;
	if (m_code.processes.regime == Regime::Lid) {
		s.breccia = std::max(1, floorInt(mul(m_height, ratio(2, 5) + lumpy / 10)));
		s.mare = ground.mare;
		s.titanium = between(g.titanium, at);
		s.thin = between(g.thin, at);
		// ice in the cold traps: crater floors 2 km down or more near a pole
		// of a body with no air to carry heat in, in shadow all year (a tenth
		// or so of the ground within 6° of the pole: Mazarico et al. 2011)
		s.cold_trap = !m_code.processes.water && fabsx(latitudeOf(at.dir())) > ratio(1466, 1000) && // 84°
				craters(at, Age::All) < -mul(m_height, ratio(40, 100));
	}

	// the nearest active crater's rim
	s.crater = 1 << 20;
	for (uint32_t k = g.volcano_start[i]; k < g.volcano_start[i + 1]; k++) {
		const Volcano &vo = g.volcanoes[g.volcano_list[k]];
		if (!vo.active)
			continue;
		const int64_t d = blocksFine(chord(at, Place(vo.at))) >> FRAC;
		s.crater = (int32_t)std::min<int64_t>(s.crater, std::abs(d - vo.crater));
	}
	return s;
}

Rock World::rock(const Strata &s, int32_t depth, int32_t y) const
{
	if (m_code.processes.stiff < ONE)
		return Rock::Ice; // an ice crust, as deep as anyone digs
	if (s.crater < 8 && depth < 6)
		return Rock::Obsidian; // glass where lava met the air at an active rim
	int32_t d = depth + s.tilt;
	if (s.crumpled) { // beds folded on themselves
		const std::array<int32_t, 3> p = solid(s.at, y);
		d += floorInt(mul(toFix(6), value3(p[0], p[1], p[2], m_code.seed_hash + FOLD, 24)));
	}
	if (d < s.sediment) {
		int32_t b = bed(d, s.phase);
		if (s.arid) // deserts lay down sand
			return b < 18 ? Rock::Sandstone : b < 26 ? Rock::Shale : Rock::Limestone;
		if (s.humid || s.province == Province::OceanFloor) // warm shallow seas lay down lime
			return b < 8 ? Rock::Sandstone : b < 18 ? Rock::Shale : Rock::Limestone;
		return b < 12 ? Rock::Sandstone : b < 22 ? Rock::Shale : Rock::Limestone;
	}
	int32_t under = d - s.sediment;
	if (s.province == Province::Highland || s.province == Province::Mare) {
		// basalt as deep as the lava, then the shattered crust, then the
		// crust: anorthosite, the light rock that floated on the young
		// body's molten surface (the lunar magma ocean)
		if (depth < s.mare)
			return Rock::Basalt;
		return depth < s.mare + s.breccia ? Rock::Breccia : Rock::Anorthosite;
	}
	switch (s.province) {
	case Province::Shield: return s.pluton ? Rock::Granite : Rock::Gneiss;
	case Province::Orogen: return under < 28 ? Rock::Slate : s.pluton ? Rock::Granite : Rock::Gneiss;
	case Province::Arc: return under < 30 ? (under % 11 < 2 ? Rock::Tuff : Rock::Andesite) : Rock::Granite;
	case Province::Volcanic:
	case Province::Rift: return under < 70 ? (under % 13 < 2 ? Rock::Tuff : Rock::Basalt) : Rock::Granite;
	case Province::Basin: return Rock::Granite;
	case Province::OceanFloor: return Rock::Basalt;
	default: return Rock::Granite;
	}
}

Ore World::ore(const Strata &s, Rock rock, int32_t depth, int32_t y) const
{
	if (depth < 3)
		return Ore::None;
	int32_t size = m_code.size;
	const std::array<int32_t, 3> p = solid(s.at, y);
	fix scale = HALF + m_code.ores; // the Ore richness setting: half to one and a half

	// a lid's: ice in the cold traps just under the dust, ilmenite in the
	// mare basalt (as rich as its lava's titanium), KREEP in the crust under
	// the thin side (the last of the magma ocean to freeze, where the
	// maria's lava came up through it)
	if (s.cold_trap && depth < 8)
		return Ore::Ice;
	if (rock == Rock::Basalt && depth < s.mare &&
			value3(p[0], p[1], p[2], m_code.seed_hash + ILMENITE, 8) > ONE - mul(mul(ratio(30, 100), s.titanium), scale))
		return Ore::Ilmenite;
	if ((rock == Rock::Anorthosite || rock == Rock::Breccia) && s.thin > ratio(60, 100) &&
			value3(p[0], p[1], p[2], m_code.seed_hash + KREEP, 16) > ONE - mul(s.thin - ratio(60, 100), scale))
		return Ore::Kreep;

	// sulfur crystals around an active vent
	if (s.crater < 12 && depth < 5 && blockHash(p[0], p[1], p[2], m_code.seed_hash + SULFUR) % 5 == 0)
		return Ore::Sulfur;

	// hydrothermal veins: hot water from the granite below left metals in cracks
	if ((s.province == Province::Orogen || s.province == Province::Arc || s.pluton) && depth > 5) {
		fix n = value3(p[0], p[1], p[2], m_code.seed_hash + VEIN, 28);
		if (fabsx(n) < mul(ratio(3, 100), scale)) {
			fix r = noise(s.at, m_code.seed_hash + METAL, std::max(8, size / 300));
			if (r < -ratio(35, 100))
				return rock == Rock::Granite ? Ore::Tin : Ore::Copper;
			if (r < ratio(25, 100))
				return Ore::Copper;
			if (r < ratio(55, 100))
				return Ore::Lead;
			return depth > 18 ? Ore::Gold : Ore::Copper;
		}
	}
	// magnetite pods in basalt
	if (rock == Rock::Basalt && value3(p[0], p[1], p[2], m_code.seed_hash + MAGNETITE, 10) > ONE - mul(ratio(16, 100), scale))
		return Ore::Magnetite;
	// banded iron: old sea floors in the shields, rusted by life's first oxygen
	if (s.province == Province::Shield && depth >= 8 && depth < 22 && (depth + s.phase) % 4 < 2 &&
			value3(p[0], p[1], p[2], m_code.seed_hash + BANDED, 48) > ratio(5, 100))
		return Ore::Iron;
	bool sedimentary = rock == Rock::Sandstone || rock == Rock::Shale || rock == Rock::Limestone;
	if (sedimentary) {
		int32_t b = bed(depth + s.tilt, s.phase);
		// coal: swamp forests buried in the mud of humid basins
		if (rock == Rock::Shale && s.humid && s.sediment > 20 && b >= 14 && b < 16)
			return Ore::Coal;
		// salt: seas that dried in arid basins
		if (s.arid && s.province == Province::Basin && b >= 4 && b < 7)
			return Ore::Salt;
		// flint: silica gathered into nodules in the chalk, along its beds
		// (a stone age's tools where the limestone comes up)
		if (rock == Rock::Limestone && b >= 9 && b < 11 &&
				value3(p[0], p[1], p[2], m_code.seed_hash + FLINT, 3) > ONE - mul(ratio(30, 100), scale))
			return Ore::Flint;
	}
	return Ore::None;
}

Cave World::cave(const Strata &s, Rock rock, int32_t depth, int32_t y, bool water_above) const
{
	fix amount = m_code.caves;
	if (amount < ratio(1, 64) || depth < 4 || (water_above && depth < 14))
		return Cave::None;
	const std::array<int32_t, 3> p = solid(s.at, y);
	fix scale = HALF + amount;

	// tunnels where two noise sheets cross; water dissolves limestone into networks
	fix width = mul(rock == Rock::Limestone ? ratio(7, 100) : ratio(28, 1000), scale);
	fix a = value3(p[0], p[1], p[2], m_code.seed_hash + CAVE_A, 20);
	if (fabsx(a) < width && fabsx(value3(p[0], p[1], p[2], m_code.seed_hash + CAVE_B, 20)) < width)
		return Cave::Air;

	// lava tubes: flat tunnels just under basalt flows near volcanoes, and in
	// the maria (their roofs fallen in are the Moon's skylights)
	if (rock == Rock::Basalt && depth < 16 &&
			(s.province == Province::Volcanic || s.province == Province::Rift || s.province == Province::Mare) &&
			[&] {
				// squashed in height (a block's place with its height stretched): flat tunnels
				const std::array<int32_t, 3> a = solid(s.at, y * 3), b = solid(s.at, y * 2);
				return fabsx(value3(a[0], a[1], a[2], m_code.seed_hash + TUBE, 24)) < ratio(5, 100) &&
						fabsx(value3(b[0], b[1], b[2], m_code.seed_hash + TUBE + 1, 30)) < ratio(12, 100);
			}())
		return Cave::Air;

	// deep caverns, their floors flooded: water, or lava under volcanic ground
	if (depth > 60 && value3(p[0], p[1], p[2], m_code.seed_hash + CAVERN, 56) > ratio(86, 100) - mul(amount, ratio(8, 100))) {
		if (s.lava_province && y < -90)
			return Cave::Lava;
		return y < -50 && m_code.processes.water ? Cave::Water : Cave::Air;
	}
	return Cave::None;
}

} // namespace terragen
