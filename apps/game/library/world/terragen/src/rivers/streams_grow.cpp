// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

// Growing the tree of Streams (streams.h): the grid's runs, and the walks
// that join each finer level to the coarser.
#include "library/world/terragen/src/rivers/streams.h"
#include "library/world/terragen/src/rivers/streams_detail.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/world.h"

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

// whether the line a–b crosses any of the channels, seen from above a
bool crosses(const V &a, const V &b, const std::vector<const Streams::Branch *> &channels)
{
	const V up = unit(a);
	const V axis = std::abs(up.y) < ratio(9, 10) ? V{ 0, ONE, 0 } : V{ ONE, 0, 0 };
	const V t1 = unit(cross(axis, up)), t2 = cross(up, t1);
	struct P { int64_t x, y; };
	const auto flat = [&](const V &v) {
		const int64_t dx = v.x - a.x, dy = v.y - a.y, dz = v.z - a.z;
		return P{ (dx * t1.x + dy * t1.y + dz * t1.z) >> FRAC, (dx * t2.x + dy * t2.y + dz * t2.z) >> FRAC };
	};
	const auto side = [](const P &o, const P &p, const P &q) {
		const __int128 c = (__int128)(p.x - o.x) * (q.y - o.y) - (__int128)(p.y - o.y) * (q.x - o.x);
		return c > 0 ? 1 : c < 0 ? -1 : 0;
	};
	const P pa{ 0, 0 }, pb = flat(b);
	const V lo{ std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }, hi{ std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
	for (const Streams::Branch *ch : channels) {
		if (ch->hi.x < lo.x || ch->lo.x > hi.x || ch->hi.y < lo.y || ch->lo.y > hi.y || ch->hi.z < lo.z || ch->lo.z > hi.z)
			continue;
		P prev = flat(ch->pts[0]);
		for (size_t k = 1; k < ch->pts.size(); k++) {
			const P cur = flat(ch->pts[k]);
			if (side(pa, pb, prev) * side(pa, pb, cur) < 0 && side(prev, cur, pa) * side(prev, cur, pb) < 0)
				return true;
			prev = cur;
		}
	}
	return false;
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
		// (what it drains grows across its cell: half the cell's own at its centre)
		const auto widthAt = [&](fix t) {
			const fix a = (fix)(g.discharge[c] - mulShift(std::min<int64_t>(g.discharge[c], ONE), ONE - t, FRAC) / 2);
			return m_world.riverWidth(mulShift(a, cell * cell, FRAC));
		};
		const bool wet = (int64_t)g.discharge[c] >= WET && !frozen(Here{ { c, c, c }, { ONE, 0, 0 } });
		const V up = unit(p1), side = unit(cross(up, unit({ p2.x - p1.x, p2.y - p1.y, p2.z - p1.z })));
		const int64_t run_len = length({ p2.x - p1.x, p2.y - p1.y, p2.z - p1.z });
		const uint64_t hc = mix((uint64_t)c * 0x9e3779b97f4a7c15ull ^ ((uint64_t)m_seed << 32));
		const fix a1 = mul((fix)(hc & 0xffff) * 2 - ONE, BEND), a2 = mul((fix)(hc >> 16 & 0xffff) * 2 - ONE, BEND / 2);
		// a drawn river's line followed loop by loop (DRAWN_PIECES a loop), so
		// its valley bends with it and what joins it meets its water; and its
		// valley's floor, free of the hills, as wide as its meanders swing
		// (FLOODPLAIN of its width either side), held, its own width
		const World::RunLook look = b.drawn ? m_world.runLook(c) : World::RunLook{};
		const int pieces = b.drawn ? std::max(RUN_PIECES, look.loops * DRAWN_PIECES) : RUN_PIECES;
		const int64_t plain = b.drawn ? mulShift(mulShift(look.width, FLOODPLAIN, FRAC), ONE - look.confined, FRAC) : 0;
		for (int k = 0; k <= pieces; k++) {
			const fix t = ratio(k, pieces);
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
			b.width.push_back(wet ? widthAt(t) + 2 * plain : 0);
		}
		b.bounds();
		m_kept->runs[(size_t)c] = std::make_shared<const Branch>(std::move(b));
	});
	return m_kept->runs[(size_t)c];
}

// A level's node: its cube's hashed point, if the ground passes the cube
// there and it's land, and the land under it as the coarser levels left it
Streams::Node Streams::node(int level, int64_t i, int64_t j, int64_t l) const
{
	Kept::Shard &shard = m_kept->levels[(size_t)level][mix(keyOf(i, j, l) ^ 0x2545f491u) % SHARDS];
	const uint64_t key = keyOf(i, j, l);
	{
		std::lock_guard<std::mutex> hold(shard.lock);
		const auto it = shard.nodes.find(key);
		if (it != shard.nodes.end())
			return it->second;
	}
	Node n;
	const int64_t S = m_spacing[(size_t)level] << SUB;
	const uint64_t h = mix(((uint64_t)i * 0x9e3779b97f4a7c15ull) ^ ((uint64_t)j * 0xc2b2ae3d27d4eb4full) ^
			((uint64_t)l * 0x165667b19e3779f9ull) ^ ((uint64_t)m_seed << 32 | (uint64_t)level));
	const V jit{ i * S + (int64_t)((h & 0xfffff) * (uint64_t)S >> 20), j * S + (int64_t)((h >> 20 & 0xfffff) * (uint64_t)S >> 20),
		l * S + (int64_t)((h >> 40 & 0xfffff) * (uint64_t)S >> 20) };
	n.p = ground(jit);
	// (the ground doesn't pass here: another cube's; or the sea's)
	if (floorDiv(n.p.x, S) == i && floorDiv(n.p.y, S) == j && floorDiv(n.p.z, S) == l && here(n.p).of(m_grid.eroded) > 0) {
		n.real = true;
		n.height = carve(n.p, FROM_GRID, level, nullptr, 0);
	}
	std::lock_guard<std::mutex> hold(shard.lock);
	if (shard.nodes.size() >= SHARD_MOST * 4)
		shard.nodes.clear();
	shard.nodes.emplace(key, n);
	return n;
}

// A node's edge: from its point to the neighbour node it drains to (the
// lowest by slope), or to a coarser channel when that's nearer than the
// neighbour or steeper; a curve leaving and arriving along the water's way,
// so a chain of edges runs on smooth, and a tributary meets its trunk aslant
// Where a node drains (cached; edge() is the curve, its bed and water): to
// its neighbour with the steepest fall, one that crosses no coarser channel
// (one past a channel drains into it, not over it), climbs no rise between
// and drains on itself; or to the nearest coarser channel, when that's
// nearer than the neighbour or steeper; or nowhere
Streams::Choice Streams::choice(int level, int64_t i, int64_t j, int64_t l) const
{
	Kept::Shard &shard = m_kept->levels[(size_t)level][mix(keyOf(i, j, l) ^ 0x68e31da4u) % SHARDS];
	const uint64_t key = keyOf(i, j, l);
	{
		std::lock_guard<std::mutex> hold(shard.lock);
		const auto it = shard.choices.find(key);
		if (it != shard.choices.end())
			return it->second;
	}
	Choice out;
	const Node n = node(level, i, j, l);
	if (n.real) {
		const auto span = [](const V &a, const V &b) { return length({ b.x - a.x, b.y - a.y, b.z - a.z }); }; // SUB bits
	// the coarser channels round it, and the nearest of them
		Near join;
		std::vector<const Branch *> coarser;
		{
			const Here h = here(n.p);
			for (int m = 0; m < level; m++)
				near(n.p, h, m, [&](const Branch &b) {
					coarser.push_back(&b);
					const Near q = nearest(b, n.p);
					if (q.d < join.d)
						join = q;
				});
		}
		// the neighbour it drains to: the steepest fall that crosses no coarser
		// channel (one past a channel drains into it, not over it) and no rise
		struct Way
		{
			int64_t fall, d; // fall per SUB unit (fixed), SUB units
			int64_t a, b, c;
			Node m;
		};
		std::vector<Way> ways;
		for (int64_t a = i - 1; a <= i + 1; a++)
			for (int64_t b = j - 1; b <= j + 1; b++)
				for (int64_t c = l - 1; c <= l + 1; c++) {
					if (a == i && b == j && c == l)
						continue;
					const Node m = node(level, a, b, c);
					if (!m.real || m.height >= n.height)
						continue;
					const int64_t d = std::max<int64_t>(1, span(n.p, m.p));
					ways.push_back({ (n.height - m.height) / d, d, a, b, c, m });
				}
		std::sort(ways.begin(), ways.end(), [](const Way &x, const Way &y) { return x.fall != y.fall ? x.fall > y.fall : std::tie(x.a, x.b, x.c) < std::tie(y.a, y.b, y.c); });
		int64_t best_fall = -1, best_d = 0;
		int64_t bi = 0, bj = 0, bl = 0;
		// (and never over a rise between: the land along the line no higher than
		// where it starts, or its valley's a trench through a hill)
		const auto downhill = [&](const Node &m) {
			for (int k = 1; k < 4; k++) {
				const V q = ground({ n.p.x + (m.p.x - n.p.x) * k / 4, n.p.y + (m.p.y - n.p.y) * k / 4, n.p.z + (m.p.z - n.p.z) * k / 4 });
				if (carve(q, FROM_GRID, level, nullptr, 0) > n.height)
					return false;
			}
			return true;
		};
		for (const Way &w : ways)
			if (!crosses(n.p, w.m.p, coarser) && downhill(w.m) && choice(level, w.a, w.b, w.c).real) {
				best_fall = w.fall;
				best_d = w.d;
				bi = w.a; bj = w.b; bl = w.c;
				break;
			}
		const int64_t join_d = join.b ? std::max<int64_t>(1, join.d >> (FRAC - SUB)) : INT64_MAX;
		if (best_fall >= 0 && !(join.b && (join_d * 4 <= best_d * 3 || (n.height - join.bed) / join_d >= best_fall))) {
			out.real = out.down = true;
			out.a = bi; out.b = bj; out.c = bl;
		} else
			out.real = join.b != nullptr;
	}
	std::lock_guard<std::mutex> hold(shard.lock);
	if (shard.choices.size() >= SHARD_MOST * 4)
		shard.choices.clear();
	shard.choices.emplace(key, out);
	return out;
}

// What drains through a node (cells of a cell's average rain): its own
// lattice's square of what runs off there, and all its level's nodes that
// drain to it, so water adds up through the tree as it does on the land
// (cached; upstream is uphill, so this ends)
int64_t Streams::area(int level, int64_t i, int64_t j, int64_t l) const
{
	Kept::Shard &shard = m_kept->levels[(size_t)level][mix(keyOf(i, j, l) ^ 0x1b873593u) % SHARDS];
	const uint64_t key = keyOf(i, j, l);
	{
		std::lock_guard<std::mutex> hold(shard.lock);
		const auto it = shard.areas.find(key);
		if (it != shard.areas.end())
			return it->second;
	}
	const Node n = node(level, i, j, l);
	int64_t sum = 0;
	if (n.real) {
		const fix share = ratio(m_spacing[(size_t)level], m_cell_blocks >> FRAC);
		sum = mul(mul(share, share), water(here(n.p)));
		for (int64_t a = i - 1; a <= i + 1; a++)
			for (int64_t b = j - 1; b <= j + 1; b++)
				for (int64_t c = l - 1; c <= l + 1; c++) {
					if (a == i && b == j && c == l)
						continue;
					const Node m = node(level, a, b, c);
					if (!m.real || m.height <= n.height)
						continue;
					const Choice up = choice(level, a, b, c);
					if (up.down && up.a == i && up.b == j && up.c == l)
						sum += area(level, a, b, c);
				}
	}
	std::lock_guard<std::mutex> hold(shard.lock);
	if (shard.areas.size() >= SHARD_MOST * 4)
		shard.areas.clear();
	shard.areas.emplace(key, sum);
	return sum;
}

std::shared_ptr<const Streams::Branch> Streams::edge(int level, int64_t i, int64_t j, int64_t l) const
{
	const Choice to = choice(level, i, j, l);
	if (!to.real)
		return nullptr;
	const Node n = node(level, i, j, l);
	const int64_t L = m_spacing[(size_t)level];
	const auto span = [](const V &a, const V &b) { return length({ b.x - a.x, b.y - a.y, b.z - a.z }); }; // SUB bits
	// the nearest coarser channel (where it joins one)
	Near join;
	{
		const Here h = here(n.p);
		for (int m = 0; m < level; m++)
			near(n.p, h, m, [&](const Branch &b) {
				const Near q = nearest(b, n.p);
				if (q.d < join.d)
					join = q;
			});
	}
	const Node down = to.down ? node(level, to.a, to.b, to.c) : Node{};
	const std::shared_ptr<const Branch> below = to.down ? grown(level, to.a, to.b, to.c) : nullptr;
	if (to.down && !below)
		return nullptr; // (a cache's eviction mid-way: never, the choice drains on)
	// where it arrives, the water's way there, its bed and width there, and
	// the land as the coarser left it
	V foot, way;
	int64_t foot_bed, foot_width, foot_land;
	if (below) {
		foot = below->pts.front();
		const V &on = below->pts[std::min<size_t>(2, below->pts.size() - 1)];
		way = unit({ on.x - foot.x, on.y - foot.y, on.z - foot.z });
		foot_bed = below->bed.front();
		foot_width = below->width.front();
		foot_land = down.height;
	} else {
		// down the trunk from its nearest point, so it meets it aslant
		const Branch &t = *join.b;
		size_t seg = join.seg;
		V q = join.q;
		int64_t left = mulShift(span(n.p, join.q), ADVANCE, FRAC);
		while (left > 0) {
			const int64_t rest = span(q, t.pts[seg + 1]);
			if (rest >= left) {
				const V &e = t.pts[seg + 1];
				q = { q.x + (e.x - q.x) * left / rest, q.y + (e.y - q.y) * left / rest, q.z + (e.z - q.z) * left / rest };
				break;
			}
			left -= rest;
			q = t.pts[seg + 1];
			if (seg + 2 >= t.pts.size())
				break;
			seg++;
		}
		const V &a = t.pts[seg], &e = t.pts[seg + 1];
		const int64_t whole = span(a, e);
		const fix at = whole > 0 ? (fix)std::clamp<int64_t>((span(a, q) << FRAC) / whole, 0, ONE) : 0;
		foot = q;
		way = unit({ e.x - a.x, e.y - a.y, e.z - a.z });
		foot_bed = t.bed[seg] + mulShift(t.bed[seg + 1] - t.bed[seg], at, FRAC);
		foot_width = t.width[seg] + mulShift(t.width[seg + 1] - t.width[seg], at, FRAC);
		foot_land = foot_bed;
	}
	// the curve (Hermite): leaving toward its foot, arriving along the way
	// the water goes there, bent a little across (sin², so its ends keep
	// their tangents)
	const int64_t d = span(n.p, foot);
	const int64_t reach = mulShift(d, TANGENT, FRAC);
	const V out = unit({ foot.x - n.p.x, foot.y - n.p.y, foot.z - n.p.z });
	const V t0 = { out.x * reach >> FRAC, out.y * reach >> FRAC, out.z * reach >> FRAC };
	const V t1 = { way.x * reach >> FRAC, way.y * reach >> FRAC, way.z * reach >> FRAC };
	const V side = unit(cross(unit(n.p), out));
	const uint64_t hb = mix(keyOf(i, j, l) ^ ((uint64_t)m_seed << 20 | (uint64_t)level) * 0x9e3779b97f4a7c15ull);
	const fix bend = mul((fix)(hb & 0xffff) * 2 - ONE, BEND), wave = mul((fix)(hb >> 16 & 0xffff) * 2 - ONE, BEND);
	Branch b;
	b.level = level;
	for (int k = 0; k <= EDGE_PIECES; k++) {
		const fix t = ratio(k, EDGE_PIECES), t2 = mul(t, t), t3 = mul(t2, t);
		const fix h00 = 2 * t3 - 3 * t2 + ONE, h10 = t3 - 2 * t2 + t, h01 = 3 * t2 - 2 * t3, h11 = t3 - t2;
		const auto hermite = [&](int64_t p0, int64_t m0, int64_t p1, int64_t m1) {
			return mulShift(p0, h00, FRAC) + mulShift(m0, h10, FRAC) + mulShift(p1, h01, FRAC) + mulShift(m1, h11, FRAC);
		};
		V at{ hermite(n.p.x, t0.x, foot.x, t1.x), hermite(n.p.y, t0.y, foot.y, t1.y), hermite(n.p.z, t0.z, foot.z, t1.z) };
		if (k > 0 && k < EDGE_PIECES) {
			// (a swing and an S, each flat at the ends)
			const fix sn = sin(mul(PI, t)), sn2 = mul(sn, sn);
			at = ground(along(at, side, mulShift(d, mul(bend, sn2) + mul(wave, mul(sn2, cos(mul(PI, t)))) * 2, FRAC)));
		}
		b.pts.push_back(k == 0 ? n.p : k == EDGE_PIECES ? foot : at);
	}
	// its bed, from its foot up: rising at its channel's slope (stream power
	// at its balance, S = k_s · A^−½: a level's lattice drains about L², so
	// k_s · cell / L), held a valley's depth under the land before its
	// valleys, never falling upstream
	const Here hn = here(n.p);
	const int64_t cut = depth(hn, level);
	const fix slope = (fix)std::min<int64_t>(CRITICAL, mulShift(std::max<fix>(0, hn.of(m_amplifier.steepnesses())), m_cell_blocks / L, FRAC) >> FRAC);
	// valleys, and never over the land the coarser left there (across a
	// floodplain it runs nearly flat), never falling upstream
	b.bed.assign(b.pts.size(), foot_bed);
	std::vector<int64_t> under(b.pts.size(), foot_land);
	under[0] = n.height;
	for (size_t k = b.pts.size() - 1; k-- > 0;) {
		const Here hk = here(b.pts[k]);
		const int64_t top = ((int64_t)hk.of(m_grid.eroded) * m_height >> FRAC) + mulShift(depth(hk, 0), UP, FRAC) - cut;
		if (k > 0)
			under[k] = carve(b.pts[k], FROM_GRID, level, nullptr, 0);
		const int64_t step = span(b.pts[k], b.pts[k + 1]) << (FRAC - SUB);
		b.bed[k] = std::max(b.bed[k + 1], std::min({ b.bed[k + 1] + mulShift(step, slope, FRAC), top, under[k] }));
	}
	// the water: all that drains through its node (area(): its own lattice's
	// and every node's upstream), so a stream that's gathered many is a
	// river; wet where that drains enough (wet(): fewer the finer) and isn't under ice, never wider than what it runs into,
	// never over the land the coarser left (a bed caught over a coarser
	// valley: dry there)
	const int64_t cell = m_cell_blocks >> FRAC;
	const int64_t cells = area(level, i, j, l);
	const int64_t width = wet(level, i, j, l, cells) && foot_width > 0 && !frozen(hn) ? std::min(m_world.riverWidth(mulShift(cells, cell * cell, FRAC)), foot_width) : 0;
	b.width.assign(b.pts.size(), 0);
	for (size_t k = 0; k < b.pts.size(); k++)
		if (b.bed[k] <= under[k] + ONE)
			b.width[k] = width;
	b.bounds();
	return std::make_shared<const Branch>(std::move(b));
}

} // namespace terragen
