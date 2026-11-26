// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/pile/piles.h"
#include "src/shared/game/items/itemgroup.h"
#include "src/shared/map/map.h"
#include "src/server/world/run/ground/ground.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/platform/system/profiler.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pile {

std::optional<Material> materialOf(const ContentFeatures &f)
{
	const int bearing = itemgroup_get(f.groups, "pile_bearing");
	if (bearing <= 0)
		return std::nullopt;
	return Material{ itemgroup_get(f.groups, "pile_repose"), itemgroup_get(f.groups, "pile_yield"), bearing,
		itemgroup_get(f.groups, "pile_give") };
}

Piles::Piles(Map &map, const NodeDefManager &ndef, std::function<void(v3s16)> changed, bool predict,
		std::function<void(const std::vector<v3s16> &)> settled) :
	m_map(map), m_ndef(ndef), m_changed(std::move(changed)), m_predict(predict), m_settled(std::move(settled))
{}

std::optional<Piles::Cell> Piles::cell(v3s16 p)
{
	const MapNode n = m_map.getNode(p);
	if (n.getContent() == CONTENT_IGNORE)
		return std::nullopt;
	if (const std::optional<shape::Shape> s = m_map.solidShape(p)) {
		// cut or smoothed ground; or air the smoothed ground below rises into, the ground's
		if (m_ndef.get(n).walkable)
			return Cell{ *s, n };
		const MapNode below = m_map.getNode(p - v3s16(0, 1, 0));
		if (below.getContent() == CONTENT_IGNORE)
			return std::nullopt;
		return Cell{ *s, below };
	}
	return Cell{ m_ndef.get(n).walkable ? shape::Shape::whole() : shape::Shape(), n };
}

std::optional<Piles::Region> Piles::read(v3s16 lo, v3s16 hi, content_t c, const Material &m)
{
	ScopeProfiler sp(g_profiler, m_predict ? "Client: piles read" : "Server: piles read", SPT_ADD, PRECISION_MICRO);
	const int nw = hi.X - lo.X + 1, nd = hi.Z - lo.Z + 1, h = hi.Y - lo.Y + 1;
	Region r{ lo, hi, c, Field(nw * UNIT, nd * UNIT, m), std::vector<MapNode>(size_t(nw) * nd) };
	// columns by the world's sixteenths: node x's first is x·16 − 8
	r.field.origin(lo.X * UNIT - UNIT / 2, lo.Z * UNIT - UNIT / 2);
	std::vector<Cell> cells(h);
	for (s16 nz = 0; nz < nd; nz++)
		for (s16 nx = 0; nx < nw; nx++) {
			MapNode &made = r.node[size_t(nz) * nw + nx];
			made = MapNode(c);
			for (s16 y = 0; y < h; y++) {
				std::optional<Cell> ce = cell(v3s16(lo.X + nx, lo.Y + y, lo.Z + nz));
				if (!ce)
					return std::nullopt; // not all loaded: no region (the press is dropped)
				cells[y] = *ce;
			}
			// what a cell filled here becomes: the column's own (its colour, its param2)
			for (s16 y = h - 1; y >= 0; y--)
				if (cells[y].node.getContent() == c) {
					made = cells[y].node;
					break;
				}
			const auto solid = [&](int i, s16 sx, s16 sz) {
				return cells[i / UNIT].solid.has(v3s16(sx, i % UNIT, sz));
			};
			for (s16 sz = 0; sz < UNIT; sz++)
				for (s16 sx = 0; sx < UNIT; sx++) {
					const int x = nx * UNIT + sx, z = nz * UNIT + sz;
					int top = h * UNIT - 1;
					while (top >= 0 && !solid(top, sx, sz))
						top--;
					if (top < 0) {
						r.field.set(x, z, 0, 0); // nothing in reach: what slides in lies at the bottom
						continue;
					}
					// buried past the top, or not this ground: it's floor
					if (top == h * UNIT - 1 || cells[top / UNIT].node.getContent() != c) {
						r.field.set(x, z, top + 1, 0);
						continue;
					}
					int b = top;
					while (b >= 0 && solid(b, sx, sz) && cells[b / UNIT].node.getContent() == c)
						b--;
					r.field.set(x, z, b + 1, top - b);
				}
		}
	r.moved.assign(size_t(nw) * nd, 0);
	r.shown.resize(size_t(r.field.width()) * r.field.depth());
	for (int z = 0; z < r.field.depth(); z++)
		for (int x = 0; x < r.field.width(); x++)
			r.shown[size_t(z) * r.field.width() + x] = r.field.top(x, z);
	return r;
}

void Piles::write(Region &r)
{
	ScopeProfiler sp(g_profiler, m_predict ? "Client: piles write" : "Server: piles write", SPT_ADD, PRECISION_MICRO);
	const v3s16 up(0, 1, 0);
	const int nw = r.hi.X - r.lo.X + 1, nd = r.hi.Z - r.lo.Z + 1;
	// only node columns whose ground changed since it was last written, and
	// those beside them (a node's smoothing reads its neighbours): what it
	// didn't move it never writes over, so a client's prediction leaves the
	// server's word alone where they agree, and the server spares the rest
	std::vector<char> touch(size_t(nw) * nd, 0);
	const int w = r.field.width();
	for (int z = 0; z < r.field.depth(); z++)
		for (int x = 0; x < w; x++) {
			int &shown = r.shown[size_t(z) * w + x];
			if (shown == r.field.top(x, z))
				continue;
			shown = r.field.top(x, z);
			const int nx = x / UNIT, nz = z / UNIT;
			for (int dz = -1; dz <= 1; dz++)
				for (int dx = -1; dx <= 1; dx++)
					if (nx + dx >= 0 && nx + dx < nw && nz + dz >= 0 && nz + dz < nd)
						touch[size_t(nz + dz) * nw + nx + dx] = 1;
		}
	auto wrote = [&](v3s16 p) {
		if (m_changed)
			m_changed(p);
	};
	// a node written changes the smoothing of those round it, so again till
	// it's as the field says (twice is plenty: it only reaches a node)
	for (int pass = 0; pass < 2; pass++) {
		bool changed = false;
		for (s16 nz = 0; nz < nd; nz++)
			for (s16 nx = 0; nx < nw; nx++)
				for (s16 y = r.lo.Y; y <= r.hi.Y && touch[size_t(nz) * nw + nx]; y++) { // bottom up: a cell's shape reads the one below
					const v3s16 p(r.lo.X + nx, y, r.lo.Z + nz);
					const std::optional<Cell> now = cell(p);
					if (!now)
						continue;
					const content_t nc = m_map.getNode(p).getContent();
					const bool ours = nc == r.c;
					// open: air (or what's built over), not showing other ground
					const bool open = !ours && m_ndef.get(nc).buildable_to
							&& (now->solid.empty() || now->node.getContent() == r.c);
					if (!ours && !open)
						continue;
					if (m_predict && !ours)
						continue; // a node made only here would never be put right
					shape::Shape want;
					const int base = (y - r.lo.Y) * UNIT;
					for (s16 sz = 0; sz < UNIT; sz++)
						for (s16 sx = 0; sx < UNIT; sx++) {
							const int x = nx * UNIT + sx, z = nz * UNIT + sz;
							const int from = std::max(0, r.field.floor(x, z) - base);
							const int to = std::min(UNIT, r.field.top(x, z) - base);
							for (int k = from; k < to; k++)
								want.set(v3s16(sx, k, sz), true);
						}
					if (want == now->solid)
						continue;
					if (want.empty()) {
						if (m_predict)
							continue;
						if (ours) {
							m_map.removeNodeWithEvent(p);
							wrote(p);
							changed = true;
						} else if (!m_map.getShape(p - up)) {
							// the ground below rises into it as smoothing: kept as cut, it doesn't
							const std::optional<Cell> below = cell(p - up);
							if (below && !below->solid.isWhole() && !below->solid.empty()) {
								m_map.setShape(p - up, below->solid);
								wrote(p - up);
								changed = true;
							}
						}
						continue;
					}
					r.moved[size_t(nz) * nw + nx] = 1;
					if (want.isWhole()) {
						// whole is never kept as a cut: a cut node made whole again drops it;
						// one smoothed is drawn by its neighbours
						if (open) {
							m_map.addNodeWithEvent(p, r.node[size_t(nz) * nw + nx]);
							wrote(p);
							changed = true;
						} else if (m_map.getShape(p)) {
							m_map.addNodeWithEvent(p, m_map.getNode(p), false);
							wrote(p);
							changed = true;
						}
						continue;
					}
					if (open)
						m_map.addNodeWithEvent(p, r.node[size_t(nz) * nw + nx]);
					m_map.setShape(p, want);
					wrote(p);
					changed = true;
				}
		if (!changed)
			break;
	}
}

Piles::Region *Piles::regionAt(v3s16 p, content_t c, const Material &m)
{
	for (Region &r : m_regions)
		if (r.c == c && p.X - r.lo.X >= 2 && r.hi.X - p.X >= 2 && p.Z - r.lo.Z >= 2 && r.hi.Z - p.Z >= 2
				&& p.Y - r.lo.Y >= 1 && r.hi.Y - p.Y >= 1)
			return &r;
	const v3s16 lo = p - v3s16(REACH, BELOW, REACH), hi = p + v3s16(REACH, ABOVE, REACH);
	// none holds it: one round it, and any of this ground it'd overlap let
	// go first, so no two write the same ground
	for (auto it = m_regions.begin(); it != m_regions.end();) {
		const bool overlaps = it->c == c && it->lo.X <= hi.X && it->hi.X >= lo.X && it->lo.Y <= hi.Y
				&& it->hi.Y >= lo.Y && it->lo.Z <= hi.Z && it->hi.Z >= lo.Z;
		if (!overlaps) {
			++it;
			continue;
		}
		write(*it);
		it = m_regions.erase(it);
	}
	std::optional<Region> r = read(lo, hi, c, m);
	if (!r)
		return nullptr;
	m_regions.push_back(std::move(*r));
	return &m_regions.back();
}

void Piles::press(u64 who, v3f at, float radius, float kpa)
{
	auto last = m_last.find(who);
	if (last != m_last.end()) {
		const float dx = at.X - last->second.X, dz = at.Z - last->second.Z;
		if (dx * dx + dz * dz < 1.f / (UNIT * UNIT))
			return; // hasn't moved a sixteenth: pressed already
	}
	m_last[who] = at;
	// the node it touches: the one its point is in, or under it if that's open
	v3s16 p(std::lround(at.X), std::lround(at.Y - 1.f / (2 * UNIT)), std::lround(at.Z));
	std::optional<Cell> ce = cell(p);
	if (ce && ce->solid.empty()) {
		p.Y--;
		ce = cell(p);
	}
	if (!ce)
		return;
	const content_t c = ce->node.getContent();
	const std::optional<Material> m = materialOf(m_ndef.get(c));
	if (!m)
		return;
	Region *r = regionAt(p, c, *m);
	if (!r)
		return;
	const float cx = (at.X - (r->lo.X - 0.5f)) * UNIT, cz = (at.Z - (r->lo.Z - 0.5f)) * UNIT;
	const int ix = int(std::floor(cx)), iz = int(std::floor(cz));
	if (ix < 0 || iz < 0 || ix >= r->field.width() || iz >= r->field.depth())
		throw std::logic_error("pile::Piles::press: a region round a press doesn't hold it");
	// off the ground (flying, jumping): more than a quarter over it
	const float h = (at.Y - (r->lo.Y - 0.5f)) * UNIT;
	if (h - r->field.top(ix, iz) > UNIT / 4)
		return;
	if (r->field.press(cx, cz, radius * UNIT, kpa) > 0)
		r->dirty = true;
	r->still = 0;
}

void Piles::changed(v3s16 at, MapNode was)
{
	const v3s16 up(0, 1, 0);
	// dug (or a scoop off it): each neighbour as it was smoothed just before, kept so (a whole one
	// too), so a hole doesn't ramp what's round it. Built where it was open, nothing round it
	// changes (ground never climbs a solid).
	const bool built = !m_ndef.get(was).walkable && m_ndef.get(m_map.getNode(at)).walkable;
	for (s16 dz = built ? 2 : -1; dz <= 1; dz++)
		for (s16 dx = -1; dx <= 1; dx++)
			for (s16 dy = -ground::REACH; dy <= ground::REACH; dy++) {
				const v3s16 q = at + v3s16(dx, dy, dz);
				if (q == at)
					continue;
				const MapNode n = m_map.getNode(q);
				if (n.getContent() == CONTENT_IGNORE || !m_ndef.get(n).loose)
					continue;
				const std::optional<ground::Top> top = m_map.groundTopAs(q, at, was);
				if (!top)
					continue;
				const shape::Shape low = top->shape(0), high = top->shape(1);
				if (!low.empty())
					m_map.keepShape(q, low);
				const v3s16 over = q + up;
				if (!high.empty() && over != at && m_ndef.get(m_map.getNode(over)).buildable_to) {
					m_map.addNodeWithEvent(over, n);
					m_map.keepShape(over, high);
				}
			}
	// a hole's floor: loose under what was dug, smoothed now it's open; kept flat
	const MapNode floor = m_map.getNode(at - up);
	if (m_ndef.get(floor).loose && !m_map.getShape(at - up) && !m_ndef.get(m_map.getNode(at)).walkable)
		m_map.keepShape(at - up, shape::Shape::whole());
	// stirred: each loose material round it, its region woken round the change
	for (v3s16 d : { v3s16(1, 0, 0), v3s16(-1, 0, 0), v3s16(0, 0, 1), v3s16(0, 0, -1), v3s16(0, -1, 0) }) {
		const MapNode n = m_map.getNode(at + d);
		const std::optional<Material> m = materialOf(m_ndef.get(n));
		if (!m)
			continue;
		Region *r = regionAt(at, n.getContent(), *m);
		if (!r)
			continue;
		for (int z = (at.Z - 1 - r->lo.Z) * UNIT; z < (at.Z + 2 - r->lo.Z) * UNIT; z++)
			for (int x = (at.X - 1 - r->lo.X) * UNIT; x < (at.X + 2 - r->lo.X) * UNIT; x++)
				r->field.stir(x, z);
		r->still = 0;
	}
}

void Piles::step(float dtime)
{
	m_behind = std::min(m_behind + dtime, 4 * TICK); // behind further: dropped, not caught up
	while (m_behind >= TICK) {
		m_behind -= TICK;
		for (Region &r : m_regions) {
			bool moved = false;
			for (int i = 0; i < 2; i++)
				moved = r.field.step() || moved;
			if (moved) {
				r.dirty = true;
				r.still = 0;
			} else {
				r.still += TICK;
			}
			r.unwritten += TICK;
		}
	}
	for (auto it = m_regions.begin(); it != m_regions.end();) {
		Region &r = *it;
		const bool asleep = r.still >= SLEEP;
		if (r.dirty && (r.unwritten >= (m_predict ? PREDICT : WRITE) || asleep)) {
			write(r);
			r.dirty = false;
			r.unwritten = 0;
		}
		if (asleep && m_settled) {
			std::vector<v3s16> nodes;
			const int nw = r.hi.X - r.lo.X + 1, nd = r.hi.Z - r.lo.Z + 1;
			for (s16 nz = 0; nz < nd; nz++)
				for (s16 nx = 0; nx < nw; nx++)
					if (r.moved[size_t(nz) * nw + nx])
						for (s16 y = r.lo.Y; y <= r.hi.Y; y++)
							nodes.emplace_back(r.lo.X + nx, y, r.lo.Z + nz);
			if (!nodes.empty())
				m_settled(nodes);
		}
		it = asleep ? m_regions.erase(it) : it + 1;
	}
}

} // namespace pile
