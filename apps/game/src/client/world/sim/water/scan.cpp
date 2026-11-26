// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/scan.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/render/map/clientmap.h"
#include "library/matter/fill/fill.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/items/itemgroup.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/map/shapes_down.h"
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
const int ABOVE = 8, BELOW = 24;       // nodes over and under the grid's height it looks for water and ground
const s64 SCAN_US = 1000;              // looking a frame, at most
} // namespace

Scan::Scan(Opts o) : m_ndef(o.ndef), m_grid(o.grid) {}

Scan::Handover Scan::frame(const std::unordered_set<v3s16> &blocks, const std::unordered_set<v3s16> &changed,
		ClientMap &map, const Frame &f, s64 since)
{
	Grid &g = m_grid;
	Handover handover;
	const int per = g.per();
	// the tiles a block lies over, found, not looked for (every tile for
	// every block changed was thousands a frame as blocks came in)
	const auto tilesOver = [&](int from, int origin) {
		const int lo = std::max(0, (int)std::floor((float)(from - origin) / per));
		const int hi = std::min(g.tiles, (int)std::ceil((float)(from + MAP_BLOCKSIZE - origin) / per));
		return std::pair<int, int>(lo, hi);
	};
	for (v3s16 bp : blocks) {
		const auto [z_lo, z_hi] = tilesOver(bp.Z * MAP_BLOCKSIZE, g.origin.Z);
		const auto [x_lo, x_hi] = tilesOver(bp.X * MAP_BLOCKSIZE, g.origin.X);
		for (int tz = z_lo; tz < z_hi; tz++)
			for (int tx = x_lo; tx < x_hi; tx++) {
				// a dry tile's ground is only needed when water comes (sprint 85:
				// ground churned by piles rescanned every dry tile round it)
				Tile &k = g.tile[tz * g.tiles + tx];
				const bool dry = !k.wet && !k.awake && !k.easing && !changed.count(bp);
				if (dry && k.known)
					k.stale = true;
				else
					k.scanned = false;
			}
	}
	// so many a frame, and no longer than this: the rest wait, nearest first, for the next
	int scans = 0;
	for (int t : g.nearest) {
		if (scans >= g.scans || micros() - since > SCAN_US)
			break;
		if (!g.tile[t].scanned) {
			scans++;
			tile(t, map, f, handover);
		}
	}
	return handover;
}

// A tile looked at, whole: what it found is kept only if every node in it
// could be read (a tile half looked at owned half its water, and the rest
// was drawn flat in the middle of it, a block high)
void Scan::tile(int t, ClientMap &map, const Frame &f, Handover &handover)
{
	Grid &g = m_grid;
	Tile &tile = g.tile[t];
	const int tx = t % g.tiles, tz = t / g.tiles, per = g.per();
	const s16 hi = s16(g.origin.Y + ABOVE), lo = s16(g.origin.Y - BELOW);
	std::vector<Found> found(size_t(per) * per);
	for (int nz = 0; nz < per; nz++) {
		for (int nx = 0; nx < per; nx++) {
			std::optional<Found> n = node(v3s16(s16(g.origin.X + tx * per + nx), 0, s16(g.origin.Z + tz * per + nz)), hi, lo, map, f);
			if (!n) {
				tile.scanned = true; // not here yet: looked at again when its block comes (it's in the changed blocks then)
				return;
			}
			found[nz * per + nx] = *n;
		}
	}

	const bool fresh = !tile.known;
	bool changed = fresh;
	for (int nz = 0; nz < per; nz++)
		for (int nx = 0; nx < per; nx++) {
			const Found &n = found[nz * per + nx];
			const int at = (tz * per + nz) * g.nodes + tx * per + nx;
			std::optional<v3s16> &kept = g.tops[at];
			if (kept != n.top) {
				if (kept)
					handover.let_go.push_back(getNodeBlockPos(*kept));
				if (n.top)
					handover.taken.push_back(getNodeBlockPos(*n.top));
			}
			kept = n.top;
			changed = changed || g.targets[at] != n.target || g.kinds[at] != n.kind || g.flows[at] != n.flow;
			g.targets[at] = n.target;
			g.kinds[at] = n.kind;
			g.flows[at] = n.flow;
			if (n.coloured && !tile.coloured)
				tile.r = n.r, tile.g = n.g, tile.b = n.b, tile.coloured = true;
			for (int iz = 0; iz < g.res; iz++)
				for (int ix = 0; ix < g.res; ix++) {
					const int c = (tz * 16 + nz * g.res + iz) * g.cols + tx * 16 + nx * g.res + ix;
					const s32 floor = n.floor[iz * g.res + ix];
					if (floor == g.floor[c])
						continue;
					changed = true;
					// a dry column's level lies on its ground (its water, when it comes, rises from it);
					// a wet one's stays where it stands, the ground moved under it
					const bool was_wet = g.wet(c);
					g.floor[c] = floor;
					if (floor == NONE || !was_wet || g.level[c] < floor) {
						g.level[c] = floor == NONE ? 0.0f : float(floor);
						g.eta[c] = 0;
						g.out[c] = {};
					}
				}
		}
	tile.known = true;
	tile.scanned = true;
	if (fresh)
		tile.snap = true;
	// its columns' targets come from its nodes' and its neighbours' (corners): all of them found again
	if (changed)
		for (int dz = -1; dz <= 1; dz++)
			for (int dx = -1; dx <= 1; dx++) {
				const int nx = tx + dx, nz = tz + dz;
				if (nx >= 0 && nx < g.tiles && nz >= 0 && nz < g.tiles && g.tile[nz * g.tiles + nx].known)
					g.tile[nz * g.tiles + nx].retarget = true;
			}
	tile.retarget = true;
}

// A node's column looked down: the highest liquid in reach (the sea's top, a
// river's or lake's held level with its rise, a fill), and the ground under
// each of its columns
std::optional<Scan::Found> Scan::node(v3s16 at, s16 hi, s16 lo, ClientMap &map, const Frame &f) const
{
	Found out;
	struct Node
	{
		enum { OPEN, SOLID, SHAPED } kind;
		std::optional<shape::Shape> shape;
	};
	const s16 x = at.X, z = at.Z;
	std::vector<Node> nodes;
	float surface = 0; // world sixteenths
	s16 bottom = lo;   // where looking stopped: an unloaded block under water is its bottom
	ShapesDown shapes({ /*map*/ map, /*top*/ v3s16(x, hi, z) });
	for (s16 y = hi; y >= lo; y--) {
		const v3s16 p(x, y, z);
		const MapNode n = map.getNode(p);
		if (n.getContent() == CONTENT_IGNORE) {
			if (!out.top)
				return std::nullopt;
			bottom = y + 1;
			break;
		}
		const ContentFeatures &cf = m_ndef.get(n);
		Node node{ Node::OPEN, shapes.next() };
		if (node.shape)
			node.kind = Node::SHAPED;
		else if (cf.walkable && cf.param_type_2 != CPT2_LEVELED)
			node.kind = Node::SOLID;
		if (!out.top) {
			const fill::Fill *fill = map.getFill(p);
			if (cf.drawtype == NDT_LIQUID && itemgroup_get(cf.groups, "liquid_layer")) {
				// full water: a river's or lake's held at its own level (sprint 115), the
				// sea's at or under its level; otherwise (blocks of water placed) it's water like a fill's
				const bool held = n.getParam2() & fill::HELD;
				surface = y * 16.0f + (held && f.rivers ? fill::surface::of(n.getParam2(), riseAt(*f.rivers, p))
								: held ? fill::partOf(n.getParam2()) : 16);
				out.kind = held ? Kind::Lake : f.sea && y <= *f.sea ? Kind::Sea : Kind::Fill;
				if (held && f.rivers) {
					auto r = f.rivers->find(getNodeBlockPos(p));
					if (r != f.rivers->end() && r->second.speed > 0 && (r->second.way[0] != 0 || r->second.way[1] != 0)) {
						out.kind = Kind::River;
						out.flow = { r->second.way[0] * r->second.speed, r->second.way[1] * r->second.speed };
					}
				}
				out.top = p;
				out.r = cf.color.getRed(), out.g = cf.color.getGreen(), out.b = cf.color.getBlue();
				out.coloured = true;
			} else if (fill && fill->amount > 0) {
				surface = y * 16 + 16 * fill::Room::of(node.shape ? &*node.shape : nullptr).surface(fill->amount / 255.0f);
				out.kind = Kind::Fill;
				out.top = p;
				out.r = fill->r, out.g = fill->g, out.b = fill->b;
				out.coloured = true;
			}
		}
		const bool ground = node.kind == Node::SOLID;
		nodes.push_back(std::move(node));
		if (ground && !out.top)
			break; // ground: no water of ours under it
	}
	out.target = out.top ? s32(std::lround(surface)) : NONE;
	const int res = m_grid.res;
	for (int iz = 0; iz < res; iz++)
		for (int ix = 0; ix < res; ix++) {
			// the sixteenth at the column's middle
			const int sx = ix * 16 / res + 8 / res, sz = iz * 16 / res + 8 / res;
			s32 floor = NONE;
			for (size_t i = 0; i < nodes.size() && floor == NONE; i++) {
				const s32 y = hi - s32(i);
				if (nodes[i].kind == Node::SOLID)
					floor = (y + 1) * 16;
				else if (nodes[i].kind == Node::SHAPED)
					for (int l = 15; l >= 0; l--)
						if (nodes[i].shape->has(v3s16(sx, l, sz))) {
							floor = y * 16 + l + 1;
							break;
						}
			}
			if (floor == NONE && out.top)
				floor = bottom * 16; // deep water: as far down as it looks is its bottom
			out.floor[iz * res + ix] = floor;
		}
	return out;
}

} // namespace liquids
