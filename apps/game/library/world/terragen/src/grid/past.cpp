// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/past.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/grid/plates.h"
#include "library/world/terragen/src/base/noise.h"
#include <queue>

namespace terragen::past {

using namespace plates;

namespace {

// Noise stream for the plumes (geology.cpp and plates.h have the others)
const uint32_t PLUMES = 443, TERRANES = 457;
// Terranes on the whole sphere (a continent of a quarter of it holds a
// dozen); their edges wander by WANDER of a radian. An old belt is OLD_BELT
// cells wide either side of its seam.
const int TERRANE_COUNT = 48;
const fix WANDER = ratio(12, 100);
const int32_t OLD_BELT = 2;
// The share of terranes under a cover of flat beds (the rest bare basement:
// a shield)
const uint32_t COVERED_OF_8 = 4;
// How far a plume reaches, in cells (a cell is some 90 km on an Earth): its
// swell (about 1000 km across), its head's basalt (a Deccan, some 500 km),
// its rift arms' length and half width
const int32_t SWELL = 7, FLOOD = 5, ARM = 10;
const fix ARM_WIDTH = ratio(7, 10);
// Plates grow from their centres at paces from SLOWEST_GROWTH to 1 (a plate's
// size goes as its pace squared: some ten times between the least and the
// greatest), over ground whose roughness varies by ROUGH either way; the
// smallest plate turns SLOWEST of its speed, the biggest all of it
const fix SLOWEST_GROWTH = ratio(30, 100), ROUGH = ratio(60, 100), SLOWEST = ratio(35, 100);
const uint32_t GROWTH = 461;
// A plume lasts this many steps (of 15 Myr) at least, and up to four times as long (Réunion has burned 66 Myr, Kerguelen 130)
const int32_t LIFE = 10;

Dir offAxis(const Dir &p)
{
	// a direction across p: crossed with whichever axis p is least along
	const Dir axis = std::abs(p.x) < std::abs(p.z) ? Dir{ ONE, 0, 0 } : Dir{ 0, 0, ONE };
	const Dir c = cross(p, axis);
	return normalise(c.x, c.y, c.z);
}

} // namespace

std::vector<Plume> plumes(const WorldCode &c)
{
	const uint32_t seed = c.seed_hash + PLUMES;
	const int count = 8 + (int)(mix(0, 0, seed) % 7);
	std::vector<Plume> out;
	for (int k = 0; k < count; k++) {
		Plume p;
		p.at = scattered(mix(k, 1, seed), mix(k, 2, seed), mix(k, 3, seed));
		p.arrives = (int32_t)(mix(k, 4, seed) % (HISTORY_STEPS + LIFE)) - LIFE;
		p.ends = p.arrives + LIFE + (int32_t)(mix(k, 5, seed) % (3 * LIFE + 1));
		p.turn = mul(unit(mix(k, 6, seed)), TWO_PI);
		out.push_back(p);
	}
	return out;
}

Terranes terranes(const WorldCode &c)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	const uint32_t seed = c.seed_hash + TERRANES;
	Terranes out;
	out.seed = seed;
	for (int k = 0; k < TERRANE_COUNT; k++)
		out.centres.push_back(scattered(mix(k, 1, seed), mix(k, 2, seed), mix(k, 3, seed)));
	out.of.resize(n);
	for (int32_t i = 0; i < n; i++) {
		const Dir &d = sphere.centre(i);
		const Dir w = normalise((int64_t)d.x + mul(fbm(d, seed + 1, 3, 4, ratio(55, 100)), WANDER),
				(int64_t)d.y + mul(fbm(d, seed + 2, 3, 4, ratio(55, 100)), WANDER),
				(int64_t)d.z + mul(fbm(d, seed + 3, 3, 4, ratio(55, 100)), WANDER));
		int64_t best = INT64_MIN;
		for (int k = 0; k < TERRANE_COUNT; k++)
			if (dot(w, out.centres[k]) > best) {
				best = dot(w, out.centres[k]);
				out.of[i] = (uint8_t)k;
			}
	}
	return out;
}

std::vector<uint8_t> grow(std::vector<Plate> &plates, const WorldCode &c)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	const uint32_t seed = c.seed_hash + GROWTH;
	std::vector<fix> pace(plates.size());
	for (size_t k = 0; k < plates.size(); k++) {
		const fix share = unit(mix((uint32_t)k, 1, seed));
		pace[k] = SLOWEST_GROWTH + mul(ONE - SLOWEST_GROWTH, share);
		plates[k].spin = mul(plates[k].spin, SLOWEST + mul(ONE - SLOWEST, share));
	}
	// (each cell's roughness, the same whichever plate crosses it)
	std::vector<fix> rough(n);
	for (int32_t i = 0; i < n; i++)
		rough[i] = ONE + mul(clamp(fbm(sphere.centre(i), seed + 1, 6, 4, ratio(55, 100)) * 3, -ONE, ONE), ROUGH);
	// time to reach each cell, nearest first (Dijkstra); ties by plate, then cell
	std::vector<int64_t> at(n, INT64_MAX);
	std::vector<uint8_t> out(n, 0);
	using Open = std::tuple<int64_t, uint8_t, int32_t>;
	std::priority_queue<Open, std::vector<Open>, std::greater<Open>> open;
	for (size_t k = 0; k < plates.size(); k++) {
		const int32_t start = sphere.cell(plates[k].centre);
		if (at[start] == 0)
			continue;
		at[start] = 0;
		out[start] = (uint8_t)k;
		open.push({ 0, (uint8_t)k, start });
	}
	while (!open.empty()) {
		const auto [t, k, i] = open.top();
		open.pop();
		if (t > at[i] || out[i] != k)
			continue;
		const fix *d = sphere.distancesBegin(i);
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, d++) {
			const int64_t next = t + div(mul(*d, (rough[i] + rough[*p]) / 2), pace[k]);
			if (next < at[*p]) {
				at[*p] = next;
				out[*p] = k;
				open.push({ next, k, *p });
			}
		}
	}
	return out;
}

void inherit(std::vector<History::Cell> &cells, const Terranes &t)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	const uint32_t seed = t.seed;
	const std::vector<uint8_t> &terrane = t.of;
	// the seam: a continental cell beside another terrane's; the belt, its
	// cells within OLD_BELT of one
	std::vector<int32_t> to_seam(n, OLD_BELT + 1);
	std::vector<int32_t> open;
	for (int32_t i = 0; i < n; i++)
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			if (cells[i].continental && cells[*p].continental && terrane[*p] != terrane[i] && to_seam[i] != 0) {
				to_seam[i] = 0;
				open.push_back(i);
			}
	for (int32_t ring = 1; ring <= OLD_BELT; ring++) {
		std::vector<int32_t> next;
		for (int32_t i : open)
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
				if (to_seam[*p] > ring) {
					to_seam[*p] = ring;
					next.push_back(*p);
				}
		open.swap(next);
	}
	for (int32_t i = 0; i < n; i++) {
		if (!cells[i].continental)
			continue;
		cells[i].past.covered = mix(terrane[i], 4, seed) % 8 < COVERED_OF_8;
		if (to_seam[i] > OLD_BELT)
			continue;
		cells[i].formed = Formed::Orogen;
		cells[i].past.suture = to_seam[i] == 0;
	}
}

void age(std::vector<History::Cell> &cells)
{
	for (History::Cell &cell : cells) {
		if (cell.past.plumed < Past::NEVER)
			cell.past.plumed++;
		if (cell.past.stretched < Past::NEVER)
			cell.past.stretched++;
	}
}

void lift(std::vector<History::Cell> &cells, const std::vector<Plume> &plumes, int32_t step)
{
	const Sphere &sphere = gridSphere();
	const fix spacing = sphere.spacing();
	for (const Plume &p : plumes) {
		if (step < p.arrives || step >= p.ends)
			continue;
		const bool head = step == p.arrives;
		const int32_t reach = head ? std::max(SWELL, ARM) : SWELL;
		// within `reach` cells: dot ≥ cos(reach · spacing)
		const int64_t near = (int64_t)cos(reach * spacing) * ONE;
		const int32_t centre = sphere.cell(p.at);
		const std::vector<int32_t> under = sphere.around(centre, [&](int32_t k) { return dot(sphere.centre(k), p.at) >= near; });
		// (the arms, in the plane tangent at the plume)
		const Dir e1 = offAxis(p.at), e2 = cross(p.at, e1);
		for (int32_t i : under) {
			History::Cell &cell = cells[i];
			if (!cell.continental)
				continue;
			const Dir &d = sphere.centre(i);
			// how far out, in cells (the chord, near enough the angle here)
			const Dir off{ d.x - p.at.x, d.y - p.at.y, d.z - p.at.z };
			const fix far = div(sqrtWide64(dot(off, off) >> FRAC), spacing);
			if (far <= toFix(SWELL))
				cell.past.plumed = 0;
			if (!head)
				continue;
			if (far <= toFix(FLOOD))
				cell.past.flooded = true;
			// three arms at 120°: along one, within its width, the crust is stretched
			for (int a = 0; a < 3; a++) {
				const fix angle = p.turn + (fix)((int64_t)TWO_PI * a / 3);
				const fix s = sin(angle), co = cos(angle);
				const Dir arm{ mul(e1.x, co) + mul(e2.x, s), mul(e1.y, co) + mul(e2.y, s), mul(e1.z, co) + mul(e2.z, s) };
				const Dir side = cross(p.at, arm);
				const fix along = div((fix)(dot(off, arm) >> FRAC), spacing);
				const fix across = div((fix)(dot(off, side) >> FRAC), spacing);
				if (along > 0 && along <= toFix(ARM) && std::abs(across) <= ARM_WIDTH)
					cell.past.stretched = 0;
			}
		}
	}
}

} // namespace terragen::past
