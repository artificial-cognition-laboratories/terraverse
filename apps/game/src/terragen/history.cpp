// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "drainage.h"
#include "geology.h"
#include "grid.h"
#include "noise.h"
#include "plates.h"
#include <algorithm>
#include <queue>
#include <stdexcept>

/*
	The plates' history (planning/sprint-89-plates.md; the ideas of Cortial
	et al., "Procedural Tectonic Planets", 2019). The grid's cells stay put
	and crust is handed between them: each step every cell asks each plate
	near it which of its crust the plate's turn brings here. Two plates
	bringing crust to one cell meet: ocean floor sinks under the other side
	(the older floor, between two oceans) and an arc rises behind the edge;
	two continents collide, and the crust between them is folded up into a
	broad range. A cell no plate reaches is a gap opening between them: new
	ocean floor at a ridge, sinking as it ages. Between steps rivers wear
	the land (stream power, without the sediment: the finished land's
	erosion keeps that account), so old ranges come down while new ones rise.
*/
#include "parallel.h"
#include <atomic>

namespace terragen {

using namespace plates;

namespace {

// Noise stream for the arcs' volcanoes (geology.cpp and plates.h have the others).
const uint32_t ARC_CONES = 197;

// The fastest plate turns this many cells a step (at 90° from its pole):
// more than half a cell, or the nearest cell would hand a cell its own crust back.
const fix STEP_CELLS = ratio(3, 2);
// What a step of collision or subduction pushes the land up by, at its
// peak (× the Mountains setting, 0.5..1.5), and how far it reaches, in
// cells: into the plate on top (the Himalaya with Tibet behind), less far
// into the one going under (the foreland), an arc's peak a few cells in.
const fix COLLIDE = ratio(7, 100);
const fix ARC = ratio(9, 100); // (at a cone's peak; between cones, nothing)
const int32_t COLLIDE_REACH = 9, UNDER_REACH = 5, ARC_PEAK = 3, ARC_REACH = 7;
// The highest crust stands: a plateau as thick as crust gets.
const fix TOP = ratio(18, 10);
// Ocean floor: shallow at the ridge where it forms, sinking as it cools
// (as √age) to the abyss.
const fix YOUNG = -ratio(8, 100), ABYSS = -ratio(40, 100);
const int32_t ABYSS_AGE = 16;
// Ocean floor that arcs have built up this close under the sea becomes
// continent (island arcs grow into land).
const fix SHALLOW = ratio(4, 100);
// How far the finished history's lines are bent (cells, at most about), and
// how many bends a radian: no deeper than a bend's length over 2π, or the warp
// folds over itself and scatters each plate's cells into its neighbours'.
const fix WANDER = toFix(5);
const int32_t WANDER_FREQUENCY = 2;
// How fast recent uplift fades from `rate`, per step.
const fix FADE = ratio(3, 4);
// The rivers between steps (as the finished land's: grid.cpp).
const fix ERODIBILITY = ratio(6, 100);
const fix CREEP = ratio(1, 50);

fix floorDepth(int32_t age)
{
	return YOUNG + mul(ABYSS - YOUNG, sqrt(ratio(std::min(age, ABYSS_AGE), ABYSS_AGE)));
}

// v turned about the unit axis k by the angle whose sine and cosine are s, c (Rodrigues).
Dir turn(const Dir &v, const Dir &k, fix s, fix c)
{
	const fix along = (fix)(dot(k, v) >> FRAC);
	const Dir across = cross(k, v);
	const fix rest = mul(along, ONE - c);
	return normalise((int64_t)mul(v.x, c) + mul(across.x, s) + mul(k.x, rest),
			(int64_t)mul(v.y, c) + mul(across.y, s) + mul(k.y, rest),
			(int64_t)mul(v.z, c) + mul(across.z, s) + mul(k.z, rest));
}

// The level with `share` of the planet's area above it.
fix levelAbove(const std::vector<fix> &height, fix share)
{
	const Sphere &sphere = gridSphere();
	const size_t n = height.size();
	const std::vector<int32_t> by = lowestFirst(height);
	const int64_t below = (int64_t)n * (ONE - share);
	int64_t under = 0;
	size_t k = 0;
	while (k + 1 < n && under + sphere.area(by[k]) <= below)
		under += sphere.area(by[k++]);
	return height[by[k]];
}

// Each cell's distance (in cells, up to `reach`) to the nearest site, over
// cells of the site's own plate (or any: a collision folds both sides);
// `reach` + 1 where none is that near.
std::vector<int32_t> spread(const std::vector<int32_t> &sites, const std::vector<History::Cell> &cells, int32_t reach, bool own_plate)
{
	const Sphere &sphere = gridSphere();
	std::vector<int32_t> out(cells.size(), reach + 1);
	std::queue<int32_t> open;
	for (int32_t s : sites)
		if (out[s] != 0) {
			out[s] = 0;
			open.push(s);
		}
	while (!open.empty()) {
		const int32_t i = open.front();
		open.pop();
		if (out[i] >= reach)
			continue;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			if ((!own_plate || cells[*p].plate == cells[i].plate) && out[*p] > out[i] + 1) {
				out[*p] = out[i] + 1;
				open.push(*p);
			}
	}
	return out;
}

// One round of rivers wearing the land (heights relative to the sea).
void wear(std::vector<fix> &h)
{
	const Sphere &sphere = gridSphere();
	const size_t n = h.size();
	const std::vector<fix> surface = fill(h);
	const std::vector<int32_t> down = receivers(surface);
	const std::vector<int32_t> order = upstreamOrder(down);
	const std::vector<int64_t> area = accumulate(down, order, std::vector<fix>(n, ONE));
	for (int32_t c : order) {
		const int32_t r = down[c];
		if (r < 0 || surface[c] > h[c])
			continue;
		const fix F = mul(ERODIBILITY, sqrtWide64(area[c]));
		const fix floor = std::max<fix>(surface[r], 0);
		h[c] = std::max(div(h[c] + mul(F, floor), ONE + F), floor + 1);
	}
	std::vector<fix> moved(n, 0);
	for (int32_t i = 0; i < (int32_t)n; i++) {
		if (h[i] < 0)
			continue;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			if (*p > i && h[*p] >= 0) {
				const fix q = mul(CREEP, h[*p] - h[i]);
				moved[i] += q;
				moved[*p] -= q;
			}
	}
	for (size_t i = 0; i < n; i++)
		h[i] += moved[i];
}

} // namespace

History history(const WorldCode &c)
{
	const Sphere &sphere = gridSphere();
	const size_t n = (size_t)sphere.size();
	History out;

	// the plates: scattered centres, each turning about its own pole
	const uint32_t seed = c.seed_hash + PLATE;
	// (a stagnant lid is one plate that doesn't turn: the crust stays as it
	// formed, its highlands and its low plains, and only ages)
	const bool lid = c.processes.regime == Regime::Lid;
	const int count = lid ? 1 : 6 + c.continents;
	if (count > 32)
		throw std::logic_error("history: plates are kept in a 32-bit mask");
	for (int k = 0; k < count; k++) {
		Plate p;
		p.centre = scattered(mix(k, 1, seed), mix(k, 2, seed), mix(k, 3, seed));
		p.pole = scattered(mix(k, 4, seed), mix(k, 5, seed), mix(k, 6, seed));
		p.spin = lid ? 0 : HALF + unit(mix(k, 7, seed)) / 2;
		p.oceanic = false; // (tectonics() says, from the crust today)
		out.plates.push_back(p);
	}

	// The start: continents where the landmass field is highest, a little
	// more of them than the Land share (their edges are drowned shelves),
	// old and level; everything else old ocean floor. Each cell goes to its
	// nearest plate centre, through a warp so the edges wander.
	std::vector<fix> mass(n);
	parallelFor((int32_t)n, [&](int32_t i) { mass[i] = landmass(c, sphere.centre(i)); });
	const fix shore = levelAbove(mass, c.land);
	const fix shelf = levelAbove(mass, std::min<fix>(c.land + ratio(6, 100), ratio(95, 100)));
	out.cells.resize(n);
	// (rough enough that edges wander and step like real ones, not run in
	// the long straight lines between centres)
	const fix WARP = ratio(30, 100);
	parallelFor((int32_t)n, [&](int32_t i) {
		const Dir &d = sphere.centre(i);
		History::Cell &cell = out.cells[i];
		cell.home = d;
		const Dir w = normalise((int64_t)d.x + mul(fbm(d, c.seed_hash + PLATE_WARP, 4, 5, ratio(55, 100)), WARP),
				(int64_t)d.y + mul(fbm(d, c.seed_hash + PLATE_WARP + 1, 4, 5, ratio(55, 100)), WARP),
				(int64_t)d.z + mul(fbm(d, c.seed_hash + PLATE_WARP + 2, 4, 5, ratio(55, 100)), WARP));
		int64_t best = INT64_MIN;
		for (int k = 0; k < count; k++) {
			const int64_t near = dot(w, out.plates[k].centre);
			if (near > best) {
				best = near;
				cell.plate = (uint8_t)k;
			}
		}
		cell.age = HISTORY_STEPS;
		cell.continental = mass[i] >= shelf;
		if (cell.continental) {
			// plains that level off inland (land / (land + 0.3) · 0.4), shelves under the sea
			const fix land = mass[i] - shore;
			cell.height = land < 0 ? land : div(mul(land, ratio(4, 10)), land + ratio(3, 10)) + ratio(1, 160);
			cell.formed = Formed::Craton;
		} else {
			cell.age = 2 * ABYSS_AGE;
			cell.height = floorDepth(cell.age);
		}
	});
	for (size_t i = 0; i < n; i++)
		if (out.cells[i].continental)
			out.continental_start += sphere.area((int32_t)i);

	// how far each plate turns a step, as the sine and cosine of minus that
	// (a cell asks where its crust came from)
	std::vector<fix> sine(count), cosine(count);
	for (int k = 0; k < count; k++)
		sincos(-mul(mul(STEP_CELLS, sphere.spacing()), out.plates[k].spin), sine[k], cosine[k]);
	const fix lift = HALF + c.mountains;
	std::vector<uint8_t> told(3 * 32 * 32, 0);
	auto record = [&](int32_t step, History::Kind kind, int a, int b, int32_t at) {
		const size_t key = (size_t)kind * 1024 + (size_t)std::min(a, b) * 32 + (size_t)std::max(a, b);
		if (told[key])
			return;
		told[key] = 1;
		out.events.push_back({ step, kind, sphere.centre(at) });
	};

	std::vector<History::Cell> next(n);
	std::vector<uint8_t> gap(n);
	for (int32_t step = 0; step < HISTORY_STEPS; step++) {
		const std::vector<History::Cell> &was = out.cells;
		std::vector<int32_t> collisions, arcs;
		// Move: each cell takes crust from the plates near it (any within
		// two cells: a step moves crust at most a cell and a half)
		// (each cell reads only the last step; what it meets is told in cell order)
		struct Met
		{
			int32_t at;
			int win, other;
			bool continent;
		};
		std::vector<std::vector<Met>> met(PARTS);
		parallelParts((int32_t)n, [&](int32_t part, int32_t begin, int32_t end) {
			for (int32_t j = begin; j < end; j++) {
				uint32_t near = 1u << was[j].plate;
				for (const int32_t *p = sphere.neighboursBegin(j); p != sphere.neighboursEnd(j); p++) {
					near |= 1u << was[*p].plate;
					for (const int32_t *q = sphere.neighboursBegin(*p); q != sphere.neighboursEnd(*p); q++)
						near |= 1u << was[*q].plate;
				}
				int32_t from[32], plate[32], found = 0, win = -1;
				for (int k = 0; k < count; k++) {
					if (!(near >> k & 1))
						continue;
					const int32_t s = sphere.cell(turn(sphere.centre(j), out.plates[k].pole, sine[k], cosine[k]));
					if (was[s].plate != k)
						continue;
					from[found] = s;
					plate[found] = k;
					// the winner stays on top: continent over ocean floor, the
					// higher continent, the younger floor (the older sinks)
					if (win < 0)
						win = found;
					else {
						const History::Cell &a = was[s], &b = was[from[win]];
						if (a.continental != b.continental ? a.continental
								: a.continental ? a.height > b.height : a.age < b.age)
							win = found;
					}
					found++;
				}
				gap[j] = found == 0;
				if (found == 0)
					continue;
				next[j] = was[from[win]];
				for (int32_t f = 0; f < found; f++)
					if (f != win)
						met[part].push_back({ j, plate[win], plate[f], was[from[f]].continental });
			}
		});
		for (const std::vector<Met> &part : met)
			for (const Met &m : part) {
				if (m.continent) { // (so the winner is too: continent can't sink)
					collisions.push_back(m.at);
					out.lost += sphere.area(m.at);
					record(step, History::Kind::Collision, m.win, m.other, m.at);
				} else {
					arcs.push_back(m.at);
					record(step, History::Kind::Arc, m.win, m.other, m.at);
				}
			}
		// Gaps: new floor at a ridge, on the plate of the first neighbour
		// that has crust (a gap inside gaps waits a pass)
		for (bool left = true; left;) {
			left = false;
			for (int32_t j = 0; j < (int32_t)n; j++) {
				if (!gap[j])
					continue;
				const int32_t *p = sphere.neighboursBegin(j);
				while (p != sphere.neighboursEnd(j) && gap[*p])
					p++;
				if (p == sphere.neighboursEnd(j)) {
					left = true;
					continue;
				}
				History::Cell fresh;
				fresh.plate = next[*p].plate;
				fresh.home = sphere.centre(j);
				fresh.age = 0;
				fresh.height = YOUNG;
				next[j] = fresh;
				gap[j] = 0;
				// a continent torn: its edges are rift margins
				for (const int32_t *q = sphere.neighboursBegin(j); q != sphere.neighboursEnd(j); q++)
					if (!gap[*q] && next[*q].continental) {
						next[*q].formed = Formed::Rift;
						next[*q].age = 0;
						for (const int32_t *r = sphere.neighboursBegin(j); r != sphere.neighboursEnd(j); r++)
							if (!gap[*r] && next[*r].plate != next[*q].plate)
								record(step, History::Kind::Opening, next[*q].plate, next[*r].plate, j);
					}
			}
		}
		// a cell left among another plate's (handing crust to the nearest cell
		// leaves slivers a cell or two wide) goes with the plate round it
		for (int32_t j = 0; j < (int32_t)n; j++) {
			int32_t count[32] = {}, best = next[j].plate;
			for (const int32_t *p = sphere.neighboursBegin(j); p != sphere.neighboursEnd(j); p++)
				if (++count[next[*p].plate] > count[best])
					best = next[*p].plate;
			if (count[best] * 3 >= sphere.neighbourCount(j) * 2 && count[next[j].plate] <= 2)
				next[j].plate = (uint8_t)best;
		}
		out.cells.swap(next);
		std::vector<History::Cell> &cells = out.cells;
		for (History::Cell &cell : cells)
			if (cell.age < INT16_MAX)
				cell.age++;

		// Uplift: ranges where continents collide (broad, highest at the
		// suture), arcs a few cells into the plate above sinking floor
		const std::vector<int32_t> to_collision = spread(collisions, cells, COLLIDE_REACH, false);
		const std::vector<int32_t> to_arc = spread(arcs, cells, ARC_REACH, true);
		// (a collision's cells keep the winner's crust, so this is the side on top)
		const std::vector<int32_t> behind = spread(collisions, cells, COLLIDE_REACH, true);
		const fix sea = levelAbove([&] {
			std::vector<fix> h(n);
			for (size_t i = 0; i < n; i++)
				h[i] = cells[i].height;
			return h;
		}(), c.land);
		std::atomic<int64_t> made{ 0 };
		parallelFor((int32_t)n, [&](int32_t i) {
			History::Cell &cell = cells[i];
			const fix folded = mul(COLLIDE, falloff(to_collision[i], behind[i] <= COLLIDE_REACH ? COLLIDE_REACH : UNDER_REACH));
			// an arc is built by its volcanoes, each a cone a cell or two across
			// carried with the crust (drawn where the crust formed), so it rises
			// as a chain of islands that only merges where they crowd
			const fix cones = to_arc[i] <= ARC_REACH ? clamp((noise(cell.home, c.seed_hash + ARC_CONES, 36) - ratio(15, 100)) * 3, 0, ONE) : 0;
			const fix arc = to_arc[i] <= ARC_REACH ? mul(mul(ARC, cones), falloff(std::abs(to_arc[i] - ARC_PEAK), ARC_REACH - ARC_PEAK + 1)) : 0;
			// ridges along the range, sharp where their noise crosses zero
			const fix ridge = ONE - fabsx(fbm(sphere.centre((int32_t)i), c.seed_hash + RIDGE, c.continents * 4, 3, HALF));
			const fix up = mul(mul(folded + arc, lift), HALF + ridge / 2);
			// (a push held step after step settles at itself, not 1/(1 − FADE)
			// of it: summed raw it filled the cap across the whole reach, and a
			// belt was peaks from edge to edge with no foothills)
			cell.rate = std::min<fix>(ONE, mul(cell.rate, FADE) + mul(div(up, COLLIDE), ONE - FADE));
			if (cell.continental) {
				(behind[i] <= COLLIDE_REACH ? cell.over : cell.under) += mul(folded, lift);
				// inland of an arc on a continent the crust is squeezed too (the Subandes)
				if (to_arc[i] > ARC_PEAK && to_arc[i] <= ARC_REACH)
					cell.under += mul(mul(ARC, falloff(to_arc[i] - ARC_PEAK, ARC_REACH - ARC_PEAK + 1)), lift) / 2;
			}
			if (up < ratio(1, 200))
				return;
			const Formed built = folded >= arc ? Formed::Orogen : Formed::Arc;
			if (cell.continental) {
				cell.height = std::min(TOP, cell.height + up);
				cell.formed = built;
				cell.age = 0;
			} else {
				cell.build += up;
				if (floorDepth(cell.age) + cell.build >= sea - SHALLOW) {
					cell.continental = true;
					cell.height = floorDepth(cell.age) + cell.build;
					cell.formed = Formed::Arc;
					cell.age = 0;
					made += sphere.area(i);
				}
			}
		});
		out.made += made;
		// the floor sinks as it ages; rivers wear the land (where there are any)
		std::vector<fix> h(n);
		for (size_t i = 0; i < n; i++) {
			if (!cells[i].continental)
				cells[i].height = floorDepth(cells[i].age) + cells[i].build;
			h[i] = cells[i].height - sea;
		}
		if (c.processes.water)
			wear(h);
		for (size_t i = 0; i < n; i++)
			if (cells[i].continental)
				cells[i].height = h[i] + sea;
	}
	// Plates turn rigidly and are read back a cell at a time, so over the
	// steps their shapes smear into streaks along their motion, and coasts,
	// rifts and sutures run in ruler lines a thousand km long. Real ones
	// wander (rifts follow old weaknesses, transforms offset ridges): the
	// whole history read through a warp a few cells deep, every line bent,
	// what's either side of it kept.
	{
		const fix deep = mul(WANDER, sphere.spacing());
		const std::vector<History::Cell> was = out.cells;
		parallelFor((int32_t)n, [&](int32_t i) {
			const Dir &d = sphere.centre(i);
			const Dir w = normalise((int64_t)d.x + mul(fbm(d, c.seed_hash + HISTORY_WARP, WANDER_FREQUENCY, 4, ratio(45, 100)), deep),
					(int64_t)d.y + mul(fbm(d, c.seed_hash + HISTORY_WARP + 1, WANDER_FREQUENCY, 4, ratio(45, 100)), deep),
					(int64_t)d.z + mul(fbm(d, c.seed_hash + HISTORY_WARP + 2, WANDER_FREQUENCY, 4, ratio(45, 100)), deep));
			out.cells[i] = was[sphere.cell(w)];
		});
	}
	for (size_t i = 0; i < n; i++)
		if (out.cells[i].continental)
			out.continental_end += sphere.area((int32_t)i);
	return out;
}

} // namespace terragen
