// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Growing the tree of Streams (streams.h): the grid's runs, and the walks
// that join each finer level to the coarser.
#include "streams.h"
#include "streams_detail.h"
#include "grid.h"
#include "world.h"

namespace terragen {

using namespace streams;

namespace {

V cross(const V &a, const V &b)
{
	return { (a.y * b.z - a.z * b.y) >> FRAC, (a.z * b.x - a.x * b.z) >> FRAC, (a.x * b.y - a.y * b.x) >> FRAC };
}
int64_t length(const V &a) { return (int64_t)isqrt((uint64_t)(a.x * a.x + a.y * a.y + a.z * a.z)); }
V unit(const V &a)
{
	const int64_t l = length(a);
	return l == 0 ? V{ 0, 0, 0 } : V{ (a.x << FRAC) / l, (a.y << FRAC) / l, (a.z << FRAC) / l };
}
V along(const V &p, const V &dir, int64_t by) // by: at SUB bits; dir a unit (fixed)
{
	return { p.x + (dir.x * by >> FRAC), p.y + (dir.y * by >> FRAC), p.z + (dir.z * by >> FRAC) };
}
V lerpV(const V &a, const V &b, int num, int den)
{
	return { a.x + (b.x - a.x) * num / den, a.y + (b.y - a.y) * num / den, a.z + (b.z - a.z) * num / den };
}

// Round a run (Chaikin, twice), its ends kept; bed and width carried as
// averages, so a bed falling through the points falls all along
void round(Streams::Branch &b)
{
	for (int pass = 0; pass < 2; pass++) {
		Streams::Branch r;
		r.level = b.level;
		r.drawn = b.drawn;
		r.pts.push_back(b.pts.front());
		r.bed.push_back(b.bed.front());
		r.width.push_back(b.width.front());
		for (size_t s = 0; s + 1 < b.pts.size(); s++)
			for (int k : { 1, 3 }) {
				r.pts.push_back(lerpV(b.pts[s], b.pts[s + 1], k, 4));
				r.bed.push_back(b.bed[s] + (b.bed[s + 1] - b.bed[s]) * k / 4);
				r.width.push_back(b.width[s] + (b.width[s + 1] - b.width[s]) * k / 4);
			}
		r.pts.push_back(b.pts.back());
		r.bed.push_back(b.bed.back());
		r.width.push_back(b.width.back());
		b = std::move(r);
	}
}

} // namespace

Streams::V Streams::toV(const Place &p) const
{
	return { (int64_t)p.x * m_radius >> (PLACE_FRAC - SUB), (int64_t)p.y * m_radius >> (PLACE_FRAC - SUB),
		(int64_t)p.z * m_radius >> (PLACE_FRAC - SUB) };
}

Place Streams::toPlace(const V &v) const
{
	return placeOf(v.x, v.y, v.z);
}

Streams::V Streams::ground(V v) const
{
	const int64_t l = length(v);
	const __int128 r = (__int128)m_radius << SUB;
	return { (int64_t)(v.x * r / l), (int64_t)(v.y * r / l), (int64_t)(v.z * r / l) };
}

// A grid cell's run to its receiver: a drawn river's line as ground() draws
// it; any other, a curve through the centres (Catmull–Rom: through its
// biggest donor's centre before and its receiver's receiver's after, so a
// stem of them runs smooth through the cells), its bed the grid's water
std::shared_ptr<const Streams::Branch> Streams::run(int32_t c) const
{
	std::call_once(m_kept->ran[(size_t)c], [&] {
		const Grid &g = m_grid;
		const int32_t r = g.receiver[c];
		if (r < 0 || g.base[c] < 0)
			return;
		const Sphere &sphere = gridSphere();
		Branch b;
		b.level = 0;
		b.drawn = m_world.drawn(c);
		const auto centre = [&](int32_t i) { return toV(Place(sphere.centre(i))); };
		const V p1 = centre(c), p2 = centre(r);
		const V p0 = m_main[c] >= 0 ? centre(m_main[c]) : V{ 2 * p1.x - p2.x, 2 * p1.y - p2.y, 2 * p1.z - p2.z };
		const V p3 = g.receiver[r] >= 0 && g.base[r] >= 0 ? centre(g.receiver[r]) : V{ 2 * p2.x - p1.x, 2 * p2.y - p1.y, 2 * p2.z - p1.z };
		const int64_t cell = m_cell_blocks >> FRAC;
		const int64_t width = m_world.riverWidth(mulShift(g.drained[c], cell * cell, FRAC));
		const bool wet = (int64_t)g.drained[c] >= WET;
		const V up = unit(p1), side = unit(cross(up, unit({ p2.x - p1.x, p2.y - p1.y, p2.z - p1.z })));
		const int64_t run_len = length({ p2.x - p1.x, p2.y - p1.y, p2.z - p1.z });
		const uint64_t hc = mix((uint64_t)c * 0x9e3779b97f4a7c15ull ^ ((uint64_t)m_seed << 32));
		const fix a1 = mul((fix)(hc & 0xffff) * 2 - ONE, BEND), a2 = mul((fix)(hc >> 16 & 0xffff) * 2 - ONE, BEND / 2);
		for (int k = 0; k <= RUN_PIECES; k++) {
			const fix t = ratio(k, RUN_PIECES);
			V at;
			if (b.drawn) {
				at = toV(m_world.unwarped(m_world.onRun(c, t)));
			} else {
				// (Catmull–Rom, in the cell's units: each coordinate a cubic in t)
				const fix t2 = mul(t, t), t3 = mul(t2, t);
				const auto cr = [&](int64_t a0, int64_t a1, int64_t a2, int64_t a3) {
					return a1 + (mulShift(a2 - a0, t, FRAC) + mulShift(2 * a0 - 5 * a1 + 4 * a2 - a3, t2, FRAC) + mulShift(3 * a1 - a0 - 3 * a2 + a3, t3, FRAC)) / 2;
				};
				const V on{ cr(p0.x, p1.x, p2.x, p3.x), cr(p0.y, p1.y, p2.y, p3.y), cr(p0.z, p1.z, p2.z, p3.z) };
				// bent across the run, its ends kept (so a stem stays joined):
				// a hashed swing of up to BEND of its length, and half that twice as often
				const fix s1 = sin(mul(PI, t)), s2 = sin(mul(TWO_PI, t));
				const int64_t off = mulShift(run_len, mul(a1, s1) + mul(a2, s2), FRAC);
				at = ground(along(on, side, off));
			}
			b.pts.push_back(at);
			b.bed.push_back(std::min(b.bed.empty() ? INT64_MAX : b.bed.back(), m_world.linkHeight(c, t)));
			b.width.push_back(wet ? width : 0);
		}
		b.bounds();
		m_kept->runs[(size_t)c] = std::make_shared<const Branch>(std::move(b));
	});
	return m_kept->runs[(size_t)c];
}

// A finer level's branch from its cube: a point hashed in the cube, if it
// lies on the ground there, walks down the land the coarser levels carved,
// a quarter lattice a step, till a coarser channel takes it
std::shared_ptr<const Streams::Branch> Streams::walk(int level, int64_t i, int64_t j, int64_t l) const
{
	const int64_t S = m_spacing[(size_t)level] << SUB;
	const uint64_t h = mix(((uint64_t)i * 0x9e3779b97f4a7c15ull) ^ ((uint64_t)j * 0xc2b2ae3d27d4eb4full) ^
			((uint64_t)l * 0x165667b19e3779f9ull) ^ ((uint64_t)m_seed << 32 | (uint64_t)level));
	const V jit{ i * S + (int64_t)((h & 0xfffff) * (uint64_t)S >> 20), j * S + (int64_t)((h >> 20 & 0xfffff) * (uint64_t)S >> 20),
		l * S + (int64_t)((h >> 40 & 0xfffff) * (uint64_t)S >> 20) };
	const V p = ground(jit);
	if (floorDiv(p.x, S) != i || floorDiv(p.y, S) != j || floorDiv(p.z, S) != l)
		return nullptr; // (the ground doesn't pass here: another cube's)
	const Here at = here(p);
	if (at.of(m_grid.eroded) <= 0)
		return nullptr; // (the sea's)
	const int64_t cut = depth(at, level);
	const auto land = [&](const V &q) { return carve(q, FROM_GRID, level, nullptr, 0); };
	// a coarser channel within reach of a place, the lowest
	const auto joinAt = [&](const V &q, bool any) {
		Near best;
		const Here hq = here(q);
		for (int m = 0; m < level; m++)
			near(q, hq, m, [&](const Branch &b) {
				if (!any && !b.within(q, std::max(b.widest / 2, (S << (FRAC - SUB)) / 8)))
					return;
				const Near n = nearest(b, q);
				if (any ? n.d < best.d : n.d < std::max(n.width / 2, (S << (FRAC - SUB)) / 8) && (!best.b || n.bed < best.bed))
					best = n;
			});
		return best;
	};
	Branch b;
	b.level = level;
	b.pts.push_back(p);
	int64_t height = land(p);
	b.bed.push_back(0); // (its bed: below, once it's joined)
	Near join;
	for (int step = 0; step < STEPS; step++) {
		const V c = b.pts.back();
		if ((join = joinAt(c, false)).b)
			break;
		// down the land: its slope read a sixteenth of the lattice either way
		const V n = unit(c);
		const V axis = std::abs(n.y) < ratio(9, 10) ? V{ 0, ONE, 0 } : V{ ONE, 0, 0 };
		const V t1 = unit(cross(axis, n)), t2 = cross(n, t1);
		const int64_t e = S / 16;
		// (forward: the land here is known)
		const int64_t g1 = land(ground(along(c, t1, e))) - height;
		const int64_t g2 = land(ground(along(c, t2, e))) - height;
		const V down = unit({ -(t1.x * g1 + t2.x * g2) >> FRAC, -(t1.y * g1 + t2.y * g2) >> FRAC, -(t1.z * g1 + t2.z * g2) >> FRAC });
		if (down.x == 0 && down.y == 0 && down.z == 0)
			break; // (flat: no way down)
		const V next = ground(along(c, down, S / 4));
		const int64_t there = land(next);
		if (there > height)
			break; // (a hollow: no way out of it)
		b.pts.push_back(next);
		b.bed.push_back(0);
		height = there;
	}
	if (!join.b && !(join = joinAt(b.pts.back(), true)).b)
		return nullptr; // (none coarser near at all)
	// Its bed, from where it joins up: rising at its channel's slope (stream
	// power at its balance, S = k_s · A^−½: a level's lattice drains about L²,
	// so k_s · cell / L), held a valley's depth under the land before its
	// valleys, never falling upstream. (Not the land the coarser carved: that's
	// their valley's floor, and every branch on it was laid flat at its join.)
	b.pts.push_back(join.q);
	b.bed.push_back(join.bed);
	{
		const int64_t L = m_spacing[(size_t)level];
		const fix slope = (fix)std::min<int64_t>(CRITICAL, mulShift(std::max<fix>(0, at.of(m_amplifier.steepnesses())), m_cell_blocks / L, FRAC) >> FRAC);
		for (size_t k = b.pts.size() - 1; k-- > 0;) {
			const Here hk = here(b.pts[k]);
			const int64_t top = ((int64_t)hk.of(m_grid.eroded) * m_height >> FRAC) + mulShift(depth(hk, 0), UP, FRAC) - cut;
			const V &a = b.pts[k], &c = b.pts[k + 1];
			const int64_t step = length({ a.x - c.x, a.y - c.y, a.z - c.z }) << (FRAC - SUB);
			b.bed[k] = std::max(b.bed[k + 1], std::min(b.bed[k + 1] + mulShift(step, slope, FRAC), top));
		}
	}
	// water where what it drains passes WET, and what it joins carries water
	const int64_t L = m_spacing[(size_t)level], cell = m_cell_blocks >> FRAC;
	const fix share = ratio(L, cell);
	const fix cells = mul(mul(share, share), std::max<fix>(ratio(5, 100), rain(at)));
	const int64_t width = cells >= WET && join.width > 0 ? m_world.riverWidth(mulShift(cells, cell * cell, FRAC)) : 0;
	b.width.assign(b.pts.size(), width);
	round(b);
	b.bounds();
	return std::make_shared<const Branch>(std::move(b));
}

} // namespace terragen
