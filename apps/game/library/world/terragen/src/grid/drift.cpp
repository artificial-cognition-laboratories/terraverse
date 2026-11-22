// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/drift.h"
#include "library/world/terragen/src/base/parallel.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/grid/plates.h"

namespace terragen::drift {

using namespace plates;

namespace {

Dir turn(const Dir &v, const Dir &k, fix s, fix c);
void open(Moved &moved, std::vector<uint8_t> &gap);
void tidy(std::vector<History::Cell> &cells);

} // namespace

Moved move(const std::vector<History::Cell> &was, const std::vector<Plate> &plates, const std::vector<Turn> &turns)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	const int count = (int)plates.size();
	Moved out;
	out.cells.resize(n);
	std::vector<uint8_t> gap(n);
	// Each cell takes crust from the plates near it (any within two cells: a
	// step moves crust at most a cell and a half). Each cell reads only the
	// last step; what it meets is told in cell order.
	std::vector<std::vector<Met>> met(PARTS);
	parallelParts(n, [&](int32_t part, int32_t begin, int32_t end) {
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
				const int32_t s = sphere.cell(turn(sphere.centre(j), plates[k].pole, turns[k].sine, turns[k].cosine));
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
			out.cells[j] = was[from[win]];
			for (int32_t f = 0; f < found; f++)
				if (f != win)
					met[part].push_back({ j, plate[win], plate[f], was[from[f]].continental });
		}
	});
	for (const std::vector<Met> &part : met)
		out.met.insert(out.met.end(), part.begin(), part.end());
	open(out, gap);
	tidy(out.cells);
	return out;
}

namespace {

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

// Gaps: new floor at a ridge, on the plate of the first neighbour that has
// crust (a gap inside gaps waits a pass)
void open(Moved &moved, std::vector<uint8_t> &gap)
{
	const Sphere &sphere = gridSphere();
	std::vector<History::Cell> &next = moved.cells;
	for (bool left = true; left;) {
		left = false;
		for (int32_t j = 0; j < sphere.size(); j++) {
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
			fresh.height = RIDGE_TOP;
			next[j] = fresh;
			gap[j] = 0;
			// a continent torn: its edges are rift margins
			for (const int32_t *q = sphere.neighboursBegin(j); q != sphere.neighboursEnd(j); q++)
				if (!gap[*q] && next[*q].continental) {
					next[*q].formed = Formed::Rift;
					next[*q].age = 0;
					next[*q].past.stretched = 0;
					for (const int32_t *r = sphere.neighboursBegin(j); r != sphere.neighboursEnd(j); r++)
						if (!gap[*r] && next[*r].plate != next[*q].plate)
							moved.opened.push_back({ j, next[*q].plate, next[*r].plate });
				}
		}
	}
}

// A cell left among another plate's (handing crust to the nearest cell
// leaves slivers a cell or two wide) goes with the plate round it
void tidy(std::vector<History::Cell> &next)
{
	const Sphere &sphere = gridSphere();
	for (int32_t j = 0; j < sphere.size(); j++) {
		int32_t count[32] = {}, best = next[j].plate;
		for (const int32_t *p = sphere.neighboursBegin(j); p != sphere.neighboursEnd(j); p++)
			if (++count[next[*p].plate] > count[best])
				best = next[*p].plate;
		if (count[best] * 3 >= sphere.neighbourCount(j) * 2 && count[next[j].plate] <= 2)
			next[j].plate = (uint8_t)best;
	}
}

} // namespace

} // namespace terragen::drift
