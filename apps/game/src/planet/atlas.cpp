// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "atlas.h"
#include "log.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

#include <cstdlib>

namespace planet {

using Kind = terragen::Atlas::Region;

namespace {

constexpr double TAU = 6.283185307179586476925;
constexpr int32_t Y_LOW = -30912, Y_HIGH = 30927; // the generator's limits, as ORIGIN

bool inside(const Atlas::Rect &r, int32_t x, int32_t z)
{
	return x >= r.x && x < r.x + r.width && z >= r.z && z < r.z + r.depth;
}

uint64_t idKey(const Atlas::TileId &t)
{
	return ((uint64_t)(uint8_t)t.body << 56) | ((uint64_t)(uint8_t)t.region << 48) | ((uint64_t)(uint32_t)(t.ti & 0xffffff) << 24) |
			(uint64_t)(uint32_t)(t.tj & 0xffffff);
}

int64_t tileFloor(int64_t v)
{
	return v >= 0 ? v / Atlas::TILE : -((-v + Atlas::TILE - 1) / Atlas::TILE);
}

} // namespace

Atlas::Atlas(const std::vector<terragen::Atlas::Params> &bodies)
{
	if (bodies.empty() || bodies.size() > 256)
		throw std::invalid_argument("planet::Atlas: " + std::to_string(bodies.size()) + " bodies (1 to 256)");
	for (const terragen::Atlas::Params &params : bodies) {
		terragen::Atlas shape(params);
		// the shell's lowest layer, the same for every segment and cap
		const std::vector<terragen::Atlas::Grid> &grids = shape.grids();
		const auto lowest = std::find_if(grids.begin(), grids.end(), [](const auto &g) { return g.region != Kind::Core; });
		const bool coreStored = lowest->low[2] >= -CRUST;
		for (const terragen::Atlas::Grid &g : grids)
			if ((g.region != Kind::Core || coreStored) &&
					(std::max<int64_t>(g.low[2], -CRUST) < Y_LOW || g.high[2] >= Y_HIGH))
				throw std::invalid_argument("planet::Atlas: a region's layers don't fit the map (" + shape.code() + ")");
		m_bodies.push_back({ std::move(shape), coreStored });
	}
	auto empty = std::make_unique<Layout>();
	empty->slots.resize((size_t)SLOTS * SLOTS);
	empty->ids.resize((size_t)SLOTS * SLOTS);
	m_shared->current = empty.get();
	m_shared->layouts.push_back(std::move(empty));
}

// Each region's grid cut into tiles, a tile a slot, in rows across the map.
Atlas Atlas::resident(const terragen::Atlas::Params &body)
{
	Atlas a(body);
	const std::vector<TileId> all = a.allTiles(0);
	if (all.size() > (size_t)SLOTS * SLOTS)
		throw std::invalid_argument("planet::Atlas: the tiles (" + std::to_string(all.size()) + " of " +
				std::to_string(SLOTS * SLOTS) + " slots) don't fit the map (" + a.body(0).code() + ")");
	for (size_t s = 0; s < all.size(); s++)
		a.put((int)s, all[s]);
	return a;
}

const terragen::Atlas &Atlas::body(int b) const
{
	if (b < 0 || b >= bodies())
		throw std::invalid_argument("planet::Atlas: no body " + std::to_string(b));
	return m_bodies[b].shape;
}

std::optional<int> Atlas::bodyAt(Node node) const
{
	const std::optional<int> slot = slotAt(node);
	if (!slot)
		return std::nullopt;
	const std::optional<Tile> &tile = layout().slots[*slot];
	return tile ? std::optional(tile->body) : std::nullopt;
}

std::vector<Atlas::TileId> Atlas::allTiles(int b) const
{
	std::vector<TileId> out;
	for (const terragen::Atlas::Grid &g : body(b).grids()) {
		if (g.region == Kind::Core && !m_bodies[b].coreStored)
			continue;
		for (int64_t tj = tileFloor(g.low[1]); tj <= tileFloor(g.high[1]); tj++)
			for (int64_t ti = tileFloor(g.low[0]); ti <= tileFloor(g.high[0]); ti++) {
				out.push_back({ b, regionNumber(b, g.region, g.index), (int32_t)ti, (int32_t)tj });
				if (out.size() > (size_t)SLOTS * SLOTS * 4)
					return out; // far more than fit: enough to say so
			}
	}
	return out;
}

int Atlas::regionNumber(int b, Kind kind, int index) const
{
	const int segments = body(b).params().segments;
	return kind == Kind::Segment ? index : kind == Kind::Cap ? segments + (index > 0 ? 0 : 1) : segments + 2;
}

std::pair<Kind, int> Atlas::regionOfNumber(int b, int number) const
{
	const int segments = body(b).params().segments;
	if (number >= 0 && number < segments)
		return { Kind::Segment, number };
	if (number == segments || number == segments + 1)
		return { Kind::Cap, number == segments ? 1 : -1 };
	if (number == segments + 2)
		return { Kind::Core, 0 };
	throw std::invalid_argument("planet::Atlas: no region " + std::to_string(number));
}

namespace {

const terragen::Atlas::Grid &gridOf(const terragen::Atlas &atlas, Kind kind, int index)
{
	for (const terragen::Atlas::Grid &g : atlas.grids())
		if (g.region == kind && g.index == index)
			return g;
	throw std::invalid_argument("planet::Atlas: no such grid");
}

} // namespace

Atlas::Tile Atlas::tileIn(int slot, const TileId &id) const
{
	const auto [kind, index] = regionOfNumber(id.body, id.region);
	if (kind == Kind::Core && !m_bodies[id.body].coreStored)
		throw std::invalid_argument("planet::Atlas: the core isn't stored at this radius");
	const terragen::Atlas::Grid &g = gridOf(body(id.body), kind, index);
	const int64_t i0 = std::max(g.low[0], (int64_t)id.ti * TILE), i1 = std::min(g.high[0], (int64_t)id.ti * TILE + TILE - 1);
	const int64_t j0 = std::max(g.low[1], (int64_t)id.tj * TILE), j1 = std::min(g.high[1], (int64_t)id.tj * TILE + TILE - 1);
	if (i0 > i1 || j0 > j1)
		throw std::invalid_argument("planet::Atlas: tile " + std::to_string(id.ti) + "," + std::to_string(id.tj) +
				" isn't on region " + std::to_string(id.region) + "'s grid");
	// in its slot as in its square of the grid
	const int32_t x = ORIGIN + (slot % SLOTS) * SLOT + (int32_t)(i0 - (int64_t)id.ti * TILE);
	const int32_t z = ORIGIN + (slot / SLOTS) * SLOT + (int32_t)(j0 - (int64_t)id.tj * TILE);
	return { id.body, kind, index, { x, z, (int32_t)(i1 - i0 + 1), (int32_t)(j1 - j0 + 1) },
		(int32_t)std::max<int64_t>(g.low[2], -CRUST), (int32_t)g.high[2], i0, j0 };
}

const Atlas::Layout &Atlas::layout() const
{
	return *m_shared->current.load(std::memory_order_acquire);
}

Atlas::Found &Atlas::kept() const
{
	return layout().found;
}

std::vector<Atlas::Tile> Atlas::tiles() const
{
	std::vector<Tile> out;
	for (const std::optional<Tile> &t : layout().slots)
		if (t)
			out.push_back(*t);
	return out;
}

std::vector<std::optional<Atlas::TileId>> Atlas::window() const
{
	return layout().ids;
}

uint64_t Atlas::generation() const
{
	return layout().generation;
}

std::optional<int> Atlas::slotOf(const TileId &tile) const
{
	const Layout &l = layout();
	const auto it = l.slotOf.find(idKey(tile));
	return it == l.slotOf.end() ? std::nullopt : std::optional(it->second);
}

std::optional<int> Atlas::slotAt(Node node)
{
	const int64_t dx = (int64_t)node.x - ORIGIN, dz = (int64_t)node.z - ORIGIN;
	if (dx < 0 || dz < 0 || dx >= (int64_t)SLOTS * SLOT || dz >= (int64_t)SLOTS * SLOT)
		return std::nullopt;
	return (int)((dz / SLOT) * SLOTS + dx / SLOT);
}

void Atlas::put(int slot, std::optional<TileId> tile)
{
	if (slot < 0 || slot >= SLOTS * SLOTS)
		throw std::invalid_argument("planet::Atlas: no slot " + std::to_string(slot));
	std::lock_guard<std::mutex> hold(m_shared->changing);
	const Layout &was = layout();
	if (tile) {
		const auto it = was.slotOf.find(idKey(*tile));
		if (it != was.slotOf.end() && it->second != slot)
			throw std::invalid_argument("planet::Atlas: tile already in slot " + std::to_string(it->second));
	}
	auto next = std::make_unique<Layout>();
	next->slots = was.slots;
	next->ids = was.ids;
	next->slotOf = was.slotOf;
	next->generation = was.generation + 1;
	if (const std::optional<TileId> &old = next->ids[slot])
		next->slotOf.erase(idKey(*old));
	next->slots[slot] = tile ? std::optional(tileIn(slot, *tile)) : std::nullopt;
	next->ids[slot] = tile;
	if (tile)
		next->slotOf[idKey(*tile)] = slot;
	m_shared->current.store(next.get(), std::memory_order_release);
	m_shared->layouts.push_back(std::move(next));
	if (m_shared->layouts.size() > KEPT)
		m_shared->layouts.pop_front();
}

// "tiles-v2 slot:body,region,ti,tj …", the slots holding a tile
std::string Atlas::windowCode() const
{
	std::string out = "tiles-v2";
	const std::vector<std::optional<TileId>> &ids = layout().ids;
	for (size_t s = 0; s < ids.size(); s++)
		if (ids[s])
			out += " " + std::to_string(s) + ":" + std::to_string(ids[s]->body) + "," + std::to_string(ids[s]->region) + "," +
					std::to_string(ids[s]->ti) + "," + std::to_string(ids[s]->tj);
	return out;
}

void Atlas::setWindow(const std::string &code)
{
	const auto refuse = [&]() { throw std::invalid_argument("planet::Atlas: a window code that can't be read: " + code); };
	if (code.rfind("tiles-v2", 0) != 0)
		refuse();
	std::vector<std::pair<int, TileId>> placed;
	size_t at = 8;
	while (at < code.size()) {
		if (code[at] != ' ')
			refuse();
		int slot, b, region;
		long ti, tj;
		int used = 0;
		if (std::sscanf(code.c_str() + at, " %d:%d,%d,%ld,%ld%n", &slot, &b, &region, &ti, &tj, &used) != 5 || used <= 0)
			refuse();
		placed.push_back({ slot, TileId{ b, region, (int32_t)ti, (int32_t)tj } });
		at += used;
	}
	for (int s = 0; s < SLOTS * SLOTS; s++)
		if (layout().ids[s])
			put(s, std::nullopt);
	for (const auto &[slot, id] : placed)
		put(slot, id);
}

Atlas::TileId Atlas::tileOf(const Cell &c) const
{
	if (c.region == Kind::Core && !m_bodies.at(c.body).coreStored)
		throw std::invalid_argument("planet::Atlas: the core isn't stored at this radius");
	if (c.region != Kind::Core && c.k < -CRUST)
		throw std::invalid_argument("planet::Atlas: a cell below the crust");
	return { c.body, regionNumber(c.body, c.region, c.index), (int32_t)tileFloor(c.i), (int32_t)tileFloor(c.j) };
}

std::vector<Atlas::TileId> Atlas::tilesNear(int b, const terragen::Atlas::Vec &point, double radius) const
{
	// a square of points round it on the sphere through it, STEP apart
	constexpr double STEP = 256;
	const double r = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
	const terragen::Atlas::Vec up{ point.x / r, point.y / r, point.z / r };
	// any two directions square to up
	const terragen::Atlas::Vec a = std::abs(up.y) < 0.9 ? terragen::Atlas::Vec{ up.z, 0, -up.x } : terragen::Atlas::Vec{ 0, -up.z, up.y };
	const double al = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
	const terragen::Atlas::Vec e{ a.x / al, a.y / al, a.z / al };
	const terragen::Atlas::Vec n{ up.y * e.z - up.z * e.y, up.z * e.x - up.x * e.z, up.x * e.y - up.y * e.x };
	const terragen::Atlas &shape = body(b);
	const double surface = shape.params().radius - 1;
	std::vector<TileId> out;
	const int steps = (int)std::ceil(radius / STEP);
	for (int v = -steps; v <= steps; v++)
		for (int u = -steps; u <= steps; u++) {
			const double du = u * STEP, dv = v * STEP;
			if (du * du + dv * dv > radius * radius + STEP * STEP)
				continue;
			const terragen::Atlas::Vec d{ up.x * r + e.x * du + n.x * dv, up.y * r + e.y * du + n.y * dv,
				up.z * r + e.z * du + n.z * dv };
			const double dl = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
			const TileId t = tileOf(Cell(b, shape.locate({ d.x / dl * surface, d.y / dl * surface, d.z / dl * surface })));
			if (std::find(out.begin(), out.end(), t) == out.end())
				out.push_back(t);
		}
	return out;
}

Atlas::Spot Atlas::spotOf(const Cell &c) const
{
	return { c.body, (int32_t)(c.i + (int64_t)regionNumber(c.body, c.region, c.index) * STRIDE), (int32_t)c.k, (int32_t)c.j };
}

std::optional<Atlas::Spot> Atlas::spot(Node node) const
{
	const Tile *t = tileAt(node);
	if (!t)
		return std::nullopt;
	return spotOf(Cell(t->body, { t->kind, t->index, t->i0 + (node.x - t->rect.x), t->j0 + (node.z - t->rect.z), node.y }));
}

std::optional<Atlas::Spot> Atlas::blockSpot(Block block) const
{
	const Node corner{ block.x * BLOCK, block.y * BLOCK, block.z * BLOCK };
	const std::optional<int> slot = slotAt(corner);
	if (!slot)
		return std::nullopt;
	const std::optional<TileId> &id = layout().ids[*slot];
	const int32_t lx = (corner.x - ORIGIN - (*slot % SLOTS) * SLOT) / BLOCK;
	const int32_t lz = (corner.z - ORIGIN - (*slot / SLOTS) * SLOT) / BLOCK;
	if (!id || lx >= TILE / BLOCK || lz >= TILE / BLOCK)
		return std::nullopt;
	return Spot{ id->body, (int32_t)(((int64_t)id->region * STRIDE + (int64_t)id->ti * TILE) / BLOCK) + lx, block.y,
		(int32_t)((int64_t)id->tj * TILE / BLOCK) + lz };
}

const Atlas::Tile *Atlas::tileAt(Node node) const
{
	const std::optional<int> slot = slotAt(node);
	if (!slot)
		return nullptr;
	const std::optional<Tile> &tile = layout().slots[*slot];
	if (!tile)
		return nullptr;
	return inside(tile->rect, node.x, node.z) && node.y >= tile->k0 && node.y <= tile->k1 ? &*tile : nullptr;
}

std::optional<Atlas::Cell> Atlas::cell(Node node) const
{
	const Tile *t = tileAt(node);
	if (!t)
		return std::nullopt;
	const Cell c(t->body, { t->kind, t->index, t->i0 + (node.x - t->rect.x), t->j0 + (node.z - t->rect.z), node.y });
	return shapeOf(c).exists(c) ? std::optional(c) : std::nullopt;
}

std::optional<Atlas::Node> Atlas::charted(Node node) const
{
	if (cell(node))
		return node;
	const std::optional<int> b = bodyAt(node);
	if (!b)
		return std::nullopt;
	const int32_t top = body(*b).params().height - 1;
	if (node.y <= top)
		return std::nullopt;
	const Node below{ node.x, top, node.z };
	return cell(below) ? std::optional(below) : std::nullopt;
}

Atlas::Node Atlas::node(const Cell &c) const
{
	const std::optional<int> slot = slotOf(tileOf(c));
	if (!slot)
		throw std::invalid_argument("planet::Atlas: cell's tile isn't in the window");
	const Tile &r = *layout().slots[*slot];
	const Node n{ (int32_t)(r.rect.x + c.i - r.i0), (int32_t)c.k, (int32_t)(r.rect.z + c.j - r.j0) };
	const std::optional<Cell> back = cell(n);
	if (!back || !(*back == c))
		throw std::invalid_argument("planet::Atlas: cell isn't stored");
	return n;
}

std::optional<terragen::Place> Atlas::place(Node node) const
{
	const std::optional<terragen::Atlas::Vec> p = surface(node);
	return p ? std::optional<terragen::Place>(direction(*p)) : std::nullopt;
}

std::optional<terragen::Atlas::Vec> Atlas::surface(Node node) const
{
	const Tile *r = tileAt(node);
	if (!r || r->kind == Kind::Core)
		return std::nullopt;
	const Cell c(r->body, { r->kind, r->index, r->i0 + (node.x - r->rect.x), r->j0 + (node.z - r->rect.z), 0 });
	return shapeOf(c).centre({ c.region, c.index, c.i, c.j, 0 });
}

terragen::Place Atlas::place(const Cell &cell) const
{
	if (cell.region == Kind::Core)
		throw std::invalid_argument("planet::Atlas: the core has no place on the ground");
	return direction(shapeOf(cell).centre({ cell.region, cell.index, cell.i, cell.j, 0 }));
}

terragen::Place Atlas::direction(const terragen::Atlas::Vec &p)
{
	return terragen::placeOf(std::llround(p.x * 1024), std::llround(p.y * 1024), std::llround(p.z * 1024));
}

int Atlas::tileIndex(Node node) const
{
	return tileAt(node) ? *slotAt(node) : -2;
}

std::optional<terragen::Atlas::Vec> Atlas::point(Node from, double x, double y, double z) const
{
	const std::optional<Cell> c = cell(from);
	if (!c)
		return std::nullopt;
	return chart(*c, x - from.x, y - from.y, z - from.z);
}

Atlas::Node Atlas::seam() const
{
	// above every body's top: a layer no body stores
	int32_t top = 0;
	for (const Body &b : m_bodies)
		top = std::max(top, b.shape.params().height);
	return { ORIGIN, top, ORIGIN };
}

/*
	Charts. Storage axes: x the grid's i (east), y its k (up), z its j (north).
*/
terragen::Atlas::Vec Atlas::chart(const Cell &c, double dx, double dy, double dz) const
{
	return shapeOf(c).at(c, dx, dz, dy);
}

std::array<double, 3> Atlas::unchart(const Cell &c, const terragen::Atlas::Vec &p) const
{
	const std::array<double, 3> g = shapeOf(c).grid(c.region, c.index, p);
	return { g[0] - c.i - 0.5, g[2] - c.k - 0.5, g[1] - c.j - 0.5 };
}

// How `to`'s grid sits in `from`'s carried on: the angle from `from`'s east
// (its chart's x, there) to `to`'s, about the up at `to`.
double Atlas::turning(const Cell &from, const Cell &to) const
{
	using V = terragen::Atlas::Vec;
	const auto sub = [](V a, V b) { return V{ a.x - b.x, a.y - b.y, a.z - b.z }; };
	const auto dot = [](V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
	const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
	const V middle = shapeOf(to).centre(to);
	const double l = std::sqrt(dot(middle, middle));
	const V up{ middle.x / l, middle.y / l, middle.z / l };
	const std::array<double, 3> d = unchart(from, middle);
	const V near = sub(chart(from, d[0] + 0.5, d[1], d[2]), chart(from, d[0] - 0.5, d[1], d[2]));
	const V far = sub(chart(to, 0.5, 0, 0), chart(to, -0.5, 0, 0));
	return std::atan2(dot(cross(near, far), up), dot(near, far));
}

// Quarter turns, east toward north: a direction (dx, dz) of `from`'s grid
// is turned(dx, dz, turn) in `to`'s.
int Atlas::quarters(const Cell &from, const Cell &to) const
{
	return (int)std::lround(turning(from, to) / (TAU / 4));
}

int Atlas::face(const Cell &, int axis, int sign) const
{
	// terragen faces: 0/1 i, 2/3 j, 4/5 k; storage x is i, y is k, z is j
	const int along = axis == 0 ? 0 : axis == 1 ? 2 : 1;
	return 2 * along + (sign > 0 ? 1 : 0);
}

static uint64_t blockKey(const Atlas::Block &b)
{
	return ((uint64_t)(uint16_t)b.x << 32) | ((uint64_t)(uint16_t)b.y << 16) | (uint16_t)b.z;
}

std::optional<Atlas::Step> Atlas::step(Node from, int axis, int sign) const
{
	Found &cache = kept();
	if (interior(blockOf(from)))
		return std::nullopt;
	const std::optional<Cell> c = cell(from);
	if (!c)
		return std::nullopt;
	Node q = from;
	(axis == 0 ? q.x : axis == 1 ? q.y : q.z) += sign;
	const std::optional<Cell> plain = cell(q);
	// the same grid: the plain step is the neighbour; but from or to a cut
	// cell (at a seam), what's straight through the face may be across the
	// edge, and that's the neighbour that counts (from both sides alike, or
	// a liquid flows one way only)
	const bool cut = shapeOf(*c).retained(*c) < 1 || (plain && (plain->body != c->body || shapeOf(*plain).retained(*plain) < 1));
	if (plain && !cut)
		return std::nullopt;
	// at a seam: found once, kept (the planet never changes)
	const std::array<int32_t, 4> key{ from.x, from.y, from.z, axis * 2 + (sign > 0) };
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.steps.find(key);
		if (it != cache.steps.end())
			return it->second;
	}
	bool paged = true;
	const std::optional<Step> found = findStep(from, *c, axis, sign, paged);
	if (!paged)
		return std::nullopt; // not in the window yet: no step today, and not kept as the answer
	std::lock_guard<std::mutex> hold(cache.lock);
	return cache.steps.emplace(key, found).first->second;
}

std::optional<Atlas::Step> Atlas::findStep(Node from, const Cell &c, int axis, int sign, bool &paged) const
{
	Node q = from;
	(axis == 0 ? q.x : axis == 1 ? q.y : q.z) += sign;
	const std::optional<Cell> plain = cell(q);
	// straight through the face's middle (of perhaps several across a
	// T-junction; past a cut's edge where that comes first)
	const std::optional<terragen::Atlas::Cell> through = shapeOf(c).through(c, face(c, axis, sign));
	if (!through)
		return std::nullopt; // the top of the atlas: seam
	const Cell target = on(c, *through);
	if (plain && target == *plain)
		return std::nullopt; // straight through is the plain step after all
	if (!slotOf(tileOf(target))) {
		paged = false;
		return std::nullopt;
	}
	return Step{ node(target), quarters(c, target) };
}

std::optional<Atlas::Seen> Atlas::see(Node from, Node p) const
{
	if (!cell(from) || (tileIndex(p) == tileIndex(from) && cell(p)))
		return std::nullopt;
	return seeAt(from, p.x, p.y, p.z);
}

std::vector<Atlas::Block> Atlas::findAt(Node from, double x, double y, double z) const
{
	std::vector<Block> out;
	const double reach = BLOCK / 2 - 0.01;
	for (int k = 0; k < 9; k++) {
		const double dx = k == 8 ? 0 : (k & 1 ? reach : -reach);
		const double dy = k == 8 ? 0 : (k & 2 ? reach : -reach);
		const double dz = k == 8 ? 0 : (k & 4 ? reach : -reach);
		const std::optional<Seen> s = seeAt(from, x + dx, y + dy, z + dz);
		if (!s || !cell(s->node))
			continue; // seam, or past the top of the atlas
		const Block b = blockOf(s->node);
		if (std::find(out.begin(), out.end(), b) == out.end())
			out.push_back(b);
	}
	// and on across seams from them (another grid's block clipping a corner
	// of the box, between the samples), and from a seam block to its own
	// grid's neighbours, while a block reaches into the box: its corners,
	// put in the box's grid (its frame at the middle), not all past one of
	// the box's faces
	const std::optional<Frame> f = frame(from, { x, y, z });
	if (!f)
		return out;
	using V = terragen::Atlas::Vec;
	const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
	const std::array<V, 3> &j = f->axes;
	const V r0 = cross(j[1], j[2]), r1 = cross(j[2], j[0]), r2 = cross(j[0], j[1]);
	const double det = j[0].x * r0.x + j[0].y * r0.y + j[0].z * r0.z;
	const std::array<V, 3> inverse{ V{ r0.x / det, r0.y / det, r0.z / det }, V{ r1.x / det, r1.y / det, r1.z / det },
		V{ r2.x / det, r2.y / det, r2.z / det } };
	const double half = BLOCK / 2.0;
	const auto reaches = [&](const Block &b) {
		const std::optional<BlockChart> c = blockChart(b);
		if (!c)
			return false;
		std::array<double, 3> lo{ 1e30, 1e30, 1e30 }, hi{ -1e30, -1e30, -1e30 };
		for (int k = 0; k < 8; k++) {
			const V p = onPlanet(*c, b.x * BLOCK - 0.5 + (k & 1 ? BLOCK : 0), b.y * BLOCK - 0.5 + (k & 2 ? BLOCK : 0),
					b.z * BLOCK - 0.5 + (k & 4 ? BLOCK : 0));
			const V d{ c->centre.x + p.x - f->origin.x, c->centre.y + p.y - f->origin.y, c->centre.z + p.z - f->origin.z };
			for (int a = 0; a < 3; a++) {
				const double v = inverse[a].x * d.x + inverse[a].y * d.y + inverse[a].z * d.z;
				lo[a] = std::min(lo[a], v);
				hi[a] = std::max(hi[a], v);
			}
		}
		for (int a = 0; a < 3; a++)
			if (lo[a] > half || hi[a] < -half)
				return false;
		return true;
	};
	for (size_t i = 0; i < out.size(); i++) {
		// a seam block's own grid's neighbours too: past a turned grid's
		// edge the box can take a corner of one between the samples
		std::vector<Block> next = blocksAcross(out[i]);
		if (!interior(out[i]))
			for (const auto &d : { Block{ 1, 0, 0 }, Block{ -1, 0, 0 }, Block{ 0, 1, 0 }, Block{ 0, -1, 0 }, Block{ 0, 0, 1 },
						 Block{ 0, 0, -1 } })
				next.push_back({ out[i].x + d.x, out[i].y + d.y, out[i].z + d.z });
		for (const Block &b : next)
			if (std::find(out.begin(), out.end(), b) == out.end() && reaches(b))
				out.push_back(b);
	}
	return out;
}

std::optional<Atlas::BlockChart> Atlas::findChart(Block block) const
{
	const Node low{ block.x * BLOCK, block.y * BLOCK, block.z * BLOCK };
	const double middle = BLOCK / 2 - 0.5;
	std::optional<Node> cellIn;
	const Node mid{ low.x + BLOCK / 2, low.y + BLOCK / 2, low.z + BLOCK / 2 };
	if (cell(mid))
		cellIn = mid;
	for (int32_t z = 0; !cellIn && z < BLOCK; z++)
		for (int32_t y = 0; !cellIn && y < BLOCK; y++)
			for (int32_t x = 0; !cellIn && x < BLOCK; x++) {
				const Node n{ low.x + x, low.y + y, low.z + z };
				if (cell(n))
					cellIn = n;
			}
	if (!cellIn)
		return std::nullopt;
	return BlockChart{ *cellIn, *point(*cellIn, low.x + middle, low.y + middle, low.z + middle) };
}

/*
	Kept answers: looked up under the lock, found outside it (another
	thread may find the same meanwhile: the first kept wins; they agree).
*/

std::vector<Atlas::Block> Atlas::blocksAt(Node from, double x, double y, double z) const
{
	Found &cache = kept();
	// a grid carried on is the same from any of a tile's cells: keyed by the tile (its storage frame)
	const std::array<int64_t, 4> key{ tileIndex(from), std::llround(x * 2), std::llround(y * 2), std::llround(z * 2) };
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.at.find(key);
		if (it != cache.at.end())
			return it->second;
	}
	std::vector<Block> found = findAt(from, x, y, z);
	std::lock_guard<std::mutex> hold(cache.lock);
	return cache.at.emplace(key, std::move(found)).first->second;
}

const std::vector<Atlas::Block> &Atlas::blocksAcross(Block block) const
{
	Found &cache = kept();
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.across.find(blockKey(block));
		if (it != cache.across.end())
			return it->second;
	}
	std::vector<Block> found = findAcross(block);
	std::lock_guard<std::mutex> hold(cache.lock);
	return cache.across.emplace(blockKey(block), std::move(found)).first->second;
}

const std::vector<Atlas::RimNode> &Atlas::rim(Block block) const
{
	Found &cache = kept();
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.rims.find(blockKey(block));
		if (it != cache.rims.end())
			return it->second;
	}
	std::vector<RimNode> found = findRim(block);
	std::lock_guard<std::mutex> hold(cache.lock);
	return cache.rims.emplace(blockKey(block), std::move(found)).first->second;
}

std::vector<Atlas::RimNode> Atlas::findRim(Block block) const
{
	constexpr int32_t W = BLOCK + 2;
	const std::vector<bool> isCell = cellsAround(block);
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	const auto at = [&](int32_t x, int32_t y, int32_t z) { return isCell[((size_t)z * W + y) * W + x]; };
	const auto inBlock = [](int32_t v) { return v >= 1 && v <= BLOCK; };

	std::vector<RimNode> out;
	static const int32_t DIRS[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (int32_t z = 0; z < W; z++)
		for (int32_t y = 0; y < W; y++)
			for (int32_t x = 0; x < W; x++) {
				if (at(x, y, z))
					continue; // a cell: stored where it is
				for (const auto &d : DIRS) {
					const int32_t fx = x + d[0], fy = y + d[1], fz = z + d[2];
					if (!inBlock(fx) || !inBlock(fy) || !inBlock(fz) || !at(fx, fy, fz))
						continue;
					// seen from the cell of the block it borders: straight across, then
					// every cell its own (from's grid's) cell overlaps there, at its
					// middle and near its corners (a cell is at least ~3/4 of a node wide)
					const Node from{ low.x + fx, low.y + fy, low.z + fz }, q{ low.x + x, low.y + y, low.z + z };
					const Cell c = *cell(from);
					const double ox = q.x - from.x, oy = q.y - from.y, oz = q.z - from.z;
					const terragen::Atlas::Vec there = chart(c, ox, oy, oz);
					const double r = std::sqrt(there.x * there.x + there.y * there.y + there.z * there.z);
					const terragen::Atlas &shape = shapeOf(c);
					if (r >= shape.params().radius + shape.params().height)
						break; // past the top: nothing across
					const Cell straight = on(c, shape.locate(there));
					std::vector<Cell> cells{ straight };
					// across a tile's edge the grid goes on: the cell straight across is the one
					const bool sameGrid = straight.region == c.region && straight.index == c.index;
					for (int k = 0; k < 8 && !sameGrid; k++) {
						const double e = 0.45;
						const terragen::Atlas::Vec p = chart(c, ox + (k & 1 ? e : -e), oy + (k & 2 ? e : -e), oz + (k & 4 ? e : -e));
						if (std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z) >= shape.params().radius + shape.params().height)
							continue;
						const Cell s = on(c, shape.locate(p));
						if (std::find(cells.begin(), cells.end(), s) == cells.end())
							cells.push_back(s);
					}
					RimNode rim{ q, quarters(c, straight), {} };
					for (const Cell &s : cells)
						rim.across.push_back(node(s));
					out.push_back(std::move(rim));
					break;
				}
			}
	return out;
}

bool Atlas::interior(Block block) const
{
	Found &cache = kept();
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.interior.find(blockKey(block));
		if (it != cache.interior.end())
			return it->second;
	}
	// its middle's cell more than the block's half diagonal and a step in,
	// and the block and a step round it inside its tile (a tile's edge is a
	// seam too: the crust's floor, the gap to the next tile)
	const Node middle{ block.x * BLOCK + BLOCK / 2, block.y * BLOCK + BLOCK / 2, block.z * BLOCK + BLOCK / 2 };
	const std::optional<Cell> c = cell(middle);
	const Tile *t = tileAt(middle);
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	const Node high{ low.x + BLOCK + 1, low.y + BLOCK + 1, low.z + BLOCK + 1 };
	const bool found = c && shapeOf(*c).clearance(*c) > BLOCK + 2 &&
			c->k + BLOCK < shapeOf(*c).params().height &&
			inside(t->rect, low.x, low.z) && inside(t->rect, high.x, high.z) && low.y >= t->k0 && high.y <= t->k1;
	std::lock_guard<std::mutex> hold(cache.lock);
	return cache.interior.emplace(blockKey(block), found).first->second;
}

std::optional<Atlas::Cut> Atlas::cut(Node n) const
{
	Found &cache = kept();
	if (interior(blockOf(n)))
		return std::nullopt;
	const std::optional<Cell> c = cell(n);
	// a cell clear of its territory's edge by more than its half diagonal
	if (!c || shapeOf(*c).clearance(*c) > 0.9)
		return std::nullopt;
	const std::array<int32_t, 3> key{ n.x, n.y, n.z };
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.cuts.find(key);
		if (it != cache.cuts.end())
			return it->second;
	}
	const std::optional<Cut> found = findCut(*c);
	std::lock_guard<std::mutex> hold(cache.lock);
	return cache.cuts.emplace(key, found).first->second;
}

bool Atlas::prepared(Block block) const
{
	Found &cache = kept();
	std::lock_guard<std::mutex> hold(cache.lock);
	const uint64_t key = blockKey(block);
	return cache.rims.count(key) && cache.blockCuts.count(key) && cache.charts.count(key);
}

void Atlas::prepare(Block block) const
{
	rim(block);
	cutsIn(block);
	blockChart(block);
}

const Atlas::Cuts &Atlas::cutsIn(Block block) const
{
	Found &cache = kept();
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.blockCuts.find(blockKey(block));
		if (it != cache.blockCuts.end())
			return *it->second;
	}
	auto found = std::make_shared<Cuts>();
	if (!interior(block))
		for (int32_t z = 0; z < BLOCK; z++)
			for (int32_t y = 0; y < BLOCK; y++)
				for (int32_t x = 0; x < BLOCK; x++) {
					const Node n{ block.x * BLOCK + x, block.y * BLOCK + y, block.z * BLOCK + z };
					if (const std::optional<Cut> c = cut(n))
						found->push_back({ n, *c, findAcross(n, *c) });
				}
	std::lock_guard<std::mutex> hold(cache.lock);
	return *cache.blockCuts.emplace(blockKey(block), std::move(found)).first->second;
}

// The nodes just past each of a cut cell's faces (cutsIn)
std::vector<std::vector<Atlas::Node>> Atlas::findAcross(Node n, const Cut &cut) const
{
	constexpr double PAST = 0.05; // nodes past the face
	constexpr double E = 0.35;
	static const double PLACES[5][2] = { { 0, 0 }, { -E, -E }, { E, -E }, { -E, E }, { E, E } };
	std::vector<std::vector<Node>> out;
	for (size_t plane = 0; plane < cut.planes.size(); plane++) {
		const Cut::Plane &face = cut.planes[plane];
		const int a = face.axis(), b = (a + 1) % 3, c = (a + 2) % 3;
		std::vector<Node> nodes;
		for (const auto &uv : PLACES) {
			std::array<double, 3> d{};
			d[b] = uv[0];
			d[c] = uv[1];
			d[a] = -(face.normal[b] * uv[0] + face.normal[c] * uv[1] + face.offset) / face.normal[a];
			if (std::abs(d[a]) > 0.5)
				continue; // the plane leaves the node here
			bool kept = true; // and the other planes leave it on the solid
			for (size_t k = 0; k < cut.planes.size(); k++)
				kept &= k == plane || cut.planes[k].at(d[0], d[1], d[2]) >= 0;
			if (!kept)
				continue;
			for (int k = 0; k < 3; k++)
				d[k] -= face.normal[k] * PAST;
			const std::optional<Seen> seen = seeAt(n, n.x + d[0], n.y + d[1], n.z + d[2]);
			if (!seen)
				throw std::logic_error("planet::Atlas: a cut cell sees nothing across its face");
			nodes.push_back(seen->node);
		}
		out.push_back(std::move(nodes));
	}
	return out;
}

// A plane for each edge of the territory some corner of the cell is past,
// fitted (least squares) to how far inside that edge each corner is: exact
// where the edge is flat over the cell.
std::optional<Atlas::Cut> Atlas::findCut(const Cell &c) const
{
	std::array<std::array<double, 4>, 8> t;
	for (int k = 0; k < 8; k++)
		t[k] = shapeOf(c).edges(c, chart(c, k & 1 ? 0.5 : -0.5, k & 2 ? 0.5 : -0.5, k & 4 ? 0.5 : -0.5));
	Cut cut;
	for (int e = 0; e < 4; e++) {
		std::array<double, 3> g{ 0, 0, 0 };
		double sum = 0, least = std::numeric_limits<double>::infinity();
		for (int k = 0; k < 8; k++) {
			least = std::min(least, t[k][e]);
			sum += t[k][e];
			const std::array<double, 3> d{ k & 1 ? 0.5 : -0.5, k & 2 ? 0.5 : -0.5, k & 4 ? 0.5 : -0.5 };
			for (int a = 0; a < 3; a++)
				g[a] += t[k][e] * d[a] / 2; // Σ t·d / Σ d² (Σ d² = 8 · ¼)
		}
		if (least >= 0)
			continue; // every corner inside this edge (or the cell has no such edge)
		const double length = std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
		if (!(length > 1e-9))
			throw std::logic_error("planet::Atlas: an edge crosses a cell without falling off anywhere");
		cut.planes.push_back({ { g[0] / length, g[1] / length, g[2] / length }, sum / 8 / length });
	}
	if (cut.planes.empty())
		return std::nullopt;
	return cut;
}

double Atlas::Cut::at(double x, double y, double z) const
{
	double least = std::numeric_limits<double>::infinity();
	for (const Plane &p : planes)
		least = std::min(least, p.at(x, y, z));
	return least;
}

int Atlas::Cut::Plane::axis() const
{
	int a = 0;
	for (int k = 1; k < 3; k++)
		if (std::abs(normal[k]) > std::abs(normal[a]))
			a = k;
	return a;
}

std::vector<Atlas::Cut::Box> Atlas::Cut::boxes(int n) const
{
	const int a = planes.front().axis(), b = (a + 1) % 3, c = (a + 2) % 3;
	std::vector<Box> out;
	for (int u = 0; u < n; u++)
		for (int v = 0; v < n; v++) {
			Box box;
			box.low[b] = -0.5 + (double)u / n;
			box.high[b] = -0.5 + (double)(u + 1) / n;
			box.low[c] = -0.5 + (double)v / n;
			box.high[c] = -0.5 + (double)(v + 1) / n;
			box.low[a] = -0.5;
			box.high[a] = 0.5;
			const double mb = (box.low[b] + box.high[b]) / 2, mc = (box.low[c] + box.high[c]) / 2;
			for (const Plane &p : planes) {
				// where the plane is along a, at the column's middle
				const double rest = p.normal[b] * mb + p.normal[c] * mc + p.offset;
				if (std::abs(p.normal[a]) < 1e-9) {
					if (rest < 0)
						box.high[a] = box.low[a]; // the column is past it
					continue;
				}
				const double edge = -rest / p.normal[a];
				if (p.normal[a] > 0)
					box.low[a] = std::max(box.low[a], edge);
				else
					box.high[a] = std::min(box.high[a], edge);
			}
			if (box.high[a] - box.low[a] > 1e-6)
				out.push_back(box);
		}
	return out;
}

std::optional<Atlas::BlockChart> Atlas::blockChart(Block block) const
{
	Found &cache = kept();
	{
		std::lock_guard<std::mutex> hold(cache.lock);
		const auto it = cache.charts.find(blockKey(block));
		if (it != cache.charts.end())
			return it->second;
	}
	std::optional<BlockChart> found = findChart(block);
	std::lock_guard<std::mutex> hold(cache.lock);
	return cache.charts.emplace(blockKey(block), found).first->second;
}

std::vector<bool> Atlas::cellsAround(Block block) const
{
	constexpr int32_t W = BLOCK + 2;
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	// Its corners' cells all one region's, well inside its territory (two
	// blocks clear: a box this small bulges no further into the core's
	// sphere, and a region's territory is otherwise convex): every node is.
	const Tile *region = tileAt(low);
	bool whole = region != nullptr;
	for (int k = 0; whole && k < 8; k++) {
		const Node corner{ low.x + (k & 1 ? W - 1 : 0), low.y + (k & 2 ? W - 1 : 0), low.z + (k & 4 ? W - 1 : 0) };
		const std::optional<Cell> c = tileAt(corner) == region ? cell(corner) : std::nullopt;
		whole = c && shapeOf(*c).clearance(*c) >= 2;
	}
	if (whole)
		return std::vector<bool>((size_t)W * W * W, true);
	std::vector<bool> isCell((size_t)W * W * W);
	for (int32_t z = 0; z < W; z++)
		for (int32_t y = 0; y < W; y++)
			for (int32_t x = 0; x < W; x++)
				isCell[((size_t)z * W + y) * W + x] = cell({ low.x + x, low.y + y, low.z + z }).has_value();
	return isCell;
}

std::vector<Atlas::Block> Atlas::findAcross(Block block) const
{
	// every cell of the block beside a node that isn't one (seam, or trimmed,
	// inside the block or past its faces): the cell across.
	// Which nodes of the block and its rim are cells, found once.
	constexpr int32_t W = BLOCK + 2;
	const std::vector<bool> isCell = cellsAround(block);
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	const auto at = [&](int32_t x, int32_t y, int32_t z) { return isCell[((size_t)z * W + y) * W + x]; };

	std::vector<Block> out;
	static const int32_t DIRS[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (int32_t z = 1; z <= BLOCK; z++)
		for (int32_t y = 1; y <= BLOCK; y++)
			for (int32_t x = 1; x <= BLOCK; x++) {
				// every other cell (a checkerboard): a block across is wider
				// than one, and its neighbours find it too
				if (!at(x, y, z) || (x + y + z) % 2)
					continue;
				for (int d = 0; d < 6; d++) {
					if (at(x + DIRS[d][0], y + DIRS[d][1], z + DIRS[d][2]))
						continue; // a plain step: the same grid
					// what's just past the face's middle, in that cell's grid
					// carried on (where a block is, not which neighbour exactly)
					const Node n{ low.x + x, low.y + y, low.z + z };
					const std::optional<Seen> s = seeAt(n, n.x + 0.501 * DIRS[d][0], n.y + 0.501 * DIRS[d][1],
							n.z + 0.501 * DIRS[d][2]);
					if (!s || !cell(s->node))
						continue; // past the top
					const Block there = blockOf(s->node);
					if (!(there == block) && std::find(out.begin(), out.end(), there) == out.end())
						out.push_back(there);
				}
			}
	return out;
}

std::optional<Atlas::Seen> Atlas::seeAt(Node from, double x, double y, double z) const
{
	const std::optional<Cell> c = cell(from);
	if (!c)
		return std::nullopt;
	const terragen::Atlas::Vec there = chart(*c, x - from.x, y - from.y, z - from.z);
	const terragen::Atlas &shape = shapeOf(*c);
	const auto &params = shape.params();
	const double r = std::sqrt(there.x * there.x + there.y * there.y + there.z * there.z);
	const Seen nothing{ seam(), 0, x, y, z };
	if (r >= params.radius + params.height)
		return nothing;
	const Cell target = on(*c, shape.locate(there));
	// the far cell's own centre (its chart's origin), not its corners'
	// average: on a curved cell that sits a hair inside the arc, and a floor
	// seen there would be a ledge
	const std::array<double, 3> at = unchart(*c, chart(target, 0, 0, 0));
	if (!slotOf(tileOf(target)))
		return nothing; // not paged in yet: from here, nothing there
	return Seen{ node(target), quarters(*c, target),
		from.x + at[0], from.y + at[1], from.z + at[2] };
}

std::optional<Atlas::Carried> Atlas::carry(double wasX, double wasY, double wasZ, double x, double y, double z) const
{
	// above the top too, charted from the column under it (an orbit over a
	// low sky crosses seams like anything else)
	const std::optional<Node> from = charted(moverNode(wasX, wasY, wasZ));
	if (!from)
		return std::nullopt;
	const Cell c = *cell(*from);
	// still in its grid (above the top, by the column under it: there's no tile up there)
	const std::optional<Node> now = charted(moverNode(x, y, z));
	if (now && tileIndex(*now) == tileIndex(*from))
		return std::nullopt;
	// located a centimetre up, as moverNode: a point on a face is the node above's
	const terragen::Atlas::Vec there = chart(c, x - from->x, y + UP - from->y, z - from->z);
	const terragen::Atlas &shape = shapeOf(c);
	const auto &params = shape.params();
	// above the top it's found in the top layer's cell under it
	const double r = std::sqrt(there.x * there.x + there.y * there.y + there.z * there.z);
	const double top = params.radius + params.height;
	const bool above = r >= top - 0.5;
	const double k = above ? (top - 0.5) / r : 1;
	const Cell target = on(c, shape.locate({ there.x * k, there.y * k, there.z * k }));
	// A mover can reach a tile before the server pages it in: it stays charted where
	// it is and asks again next step, rather than ending the session.
	if (!slotOf(tileOf(target))) {
		warningstream << "planet::Atlas: a mover reached a tile that isn't in the window yet" << std::endl;
		return std::nullopt;
	}
	const Node n = node(target);
	std::array<double, 3> at = unchart(target, there);
	if (above) {
		// up there the grid's columns lean from the one found at the top:
		// it's over the column its own grid says, if that's stored here
		const std::array<double, 3> g = shape.grid(target.region, target.index, there);
		Cell column = target;
		column.i = (int32_t)std::floor(g[0]);
		column.j = (int32_t)std::floor(g[1]);
		if (const std::optional<int> slot = slotOf(tileOf(column))) {
			const Tile &r = *layout().slots[*slot];
			const Node m{ (int32_t)(r.rect.x + column.i - r.i0), target.k, (int32_t)(r.rect.z + column.j - r.j0) };
			if (const std::optional<Cell> back = cell(m); back && *back == column) {
				const std::array<double, 3> d = unchart(column, there);
				return Carried{ m.x + d[0], m.y + d[1] - UP, m.z + d[2], turning(c, column) };
			}
		}
	}
	// in the cell's box (a sliver merged into it lies a little outside it:
	// it's the cell's); up, as far over it as it is. Above the top that can
	// move it: between leaning columns of different grids are wedges in none.
	for (int i = 0; i < 3; i++)
		if (i != 1 || !above)
			at[i] = std::clamp(at[i], -0.499, 0.499);
	return Carried{ n.x + at[0], n.y + at[1] - UP, n.z + at[2], turning(c, target) };
}

std::optional<Atlas::Frame> Atlas::frame(Node from, const std::array<double, 3> &at) const
{
	const std::optional<terragen::Atlas::Vec> o = point(from, at[0], at[1], at[2]);
	if (!o)
		return std::nullopt;
	Frame f{ *bodyAt(from), at, *o, {} };
	for (int i = 0; i < 3; i++) {
		std::array<double, 3> hi = at, lo = at;
		hi[i] += 0.5;
		lo[i] -= 0.5;
		const terragen::Atlas::Vec h = *point(from, hi[0], hi[1], hi[2]), l = *point(from, lo[0], lo[1], lo[2]);
		f.axes[i] = { h.x - l.x, h.y - l.y, h.z - l.z };
	}
	return f;
}

Atlas::Affine Atlas::view(const Frame &eye, const Frame &block)
{
	if (eye.body != block.body)
		throw std::invalid_argument("planet::Atlas: a block of body " + std::to_string(block.body) +
				" seen from body " + std::to_string(eye.body));
	using V = terragen::Atlas::Vec;
	const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
	const auto dot = [](V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
	// the eye's axes inverted (rows of its inverse, by cofactors)
	const std::array<V, 3> &je = eye.axes;
	const V r0 = cross(je[1], je[2]), r1 = cross(je[2], je[0]), r2 = cross(je[0], je[1]);
	const double det = dot(je[0], r0);
	const std::array<V, 3> inv{ V{ r0.x / det, r0.y / det, r0.z / det }, V{ r1.x / det, r1.y / det, r1.z / det },
		V{ r2.x / det, r2.y / det, r2.z / det } };

	Affine m;
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			m.a[3 * r + c] = dot(inv[r], block.axes[c]);
	const V d{ block.origin.x - eye.origin.x, block.origin.y - eye.origin.y, block.origin.z - eye.origin.z };
	const std::array<double, 3> &c = block.at;
	for (int r = 0; r < 3; r++) {
		// the block's centre lands at the eye plus the eye's grid's way there;
		// the map is a·(s − centre) round it
		const double landed = eye.at[r] + dot(inv[r], d);
		m.t[r] = landed - (m.a[3 * r] * c[0] + m.a[3 * r + 1] * c[1] + m.a[3 * r + 2] * c[2]);
	}
	return m;
}

} // namespace planet
