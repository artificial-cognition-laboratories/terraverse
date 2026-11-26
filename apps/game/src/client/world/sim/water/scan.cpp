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
const float SLACK = 1 / 128.0f;        // nodes³ a tile's water may be off before it's run
const float FILM = 1 / 32.0f;          // node heights: thinner isn't water the ground moved under
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
				const bool dry = !k.wet && !k.sea && !k.awake && k.volume <= 0 && k.target <= 0 && !changed.count(bp);
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
	// held levels found again where nodes' changed: a river risen or fallen moves its water with it
	for (int t = 0; t < int(g.tile.size()); t++) {
		if (!g.tile[t].relevel)
			continue;
		if (g.relevel(t))
			g.tile[t].drawn = false; // (its water moved with it: nothing to run)
	}
	return handover;
}

// A tile looked at
void Scan::tile(int t, ClientMap &map, const Frame &f, Handover &handover)
{
	Grid &g = m_grid;
	Tile &tile = g.tile[t];
	const int tx = t % g.tiles, tz = t / g.tiles, per = g.per();
	const s16 hi = s16(g.origin.Y + ABOVE), lo = s16(g.origin.Y - BELOW);
	struct Node
	{
		enum { OPEN, SOLID, SHAPED } kind;
		std::optional<shape::Shape> shape;
	};
	const bool fresh = !tile.known;
	bool moved = false;
	bool levelled = fresh; // a node's held level changed (or it's new): its columns' and its neighbours' found again
	bool sea = false;
	float target = 0, above = 0;
	const float area = 1.0f / (g.res * g.res); // a column's, nodes²
	for (int nz = 0; nz < per; nz++)
		for (int nx = 0; nx < per; nx++) {
			const s16 x = s16(g.origin.X + tx * per + nx), z = s16(g.origin.Z + tz * per + nz);
			std::vector<Node> nodes;
			std::optional<v3s16> top;
			bool is_sea = false;
			float runs = 0;
			std::array<float, 2> way{};
			float surface = 0; // world sixteenths
			ShapesDown shapes({ /*map*/ map, /*top*/ v3s16(x, hi, z) });
			for (s16 y = hi; y >= lo; y--) {
				const v3s16 p(x, y, z);
				const MapNode n = map.getNode(p);
				if (n.getContent() == CONTENT_IGNORE) {
					// not here yet: looked at again when its block comes (it's in the changed blocks then)
					tile.scanned = true;
					return;
				}
				const ContentFeatures &cf = m_ndef.get(n);
				Node node{ Node::OPEN, shapes.next() };
				if (node.shape)
					node.kind = Node::SHAPED;
				else if (cf.walkable && cf.param_type_2 != CPT2_LEVELED)
					node.kind = Node::SOLID;
				if (!top) {
					const fill::Fill *fill = map.getFill(p);
					if (cf.drawtype == NDT_LIQUID && itemgroup_get(cf.groups, "liquid_layer")) {
						// full water: the sea's at or under its level (a reservoir), and a
						// river's or lake's held at its own (sprint 115); otherwise (blocks
						// of water placed) it's water like a fill's
						top = p;
						const bool held = n.getParam2() & fill::HELD;
						is_sea = (f.sea && y <= *f.sea) || held;
						surface = y * 16.0f + (held ? fill::partOf(n.getParam2()) : 16);
						// a river's level and speed as the weather fills it (sprint 115)
						if (held && f.rivers) {
							surface += std::lround(riseAt(*f.rivers, p));
							auto r = f.rivers->find(getNodeBlockPos(p));
							if (r != f.rivers->end()) {
								runs = r->second.speed;
								way = r->second.way;
							}
						}
						if (!tile.coloured || !tile.sea)
							tile.r = cf.color.getRed(), tile.g = cf.color.getGreen(), tile.b = cf.color.getBlue();
					} else if (fill && fill->amount > 0) {
						top = p;
						surface = y * 16 + 16 * fill::Room::of(node.shape ? &*node.shape : nullptr).surface(fill->amount / 255.0f);
						if (!tile.coloured)
							tile.r = fill->r, tile.g = fill->g, tile.b = fill->b;
					}
					if (top)
						tile.coloured = true;
				}
				const bool ground = node.kind == Node::SOLID;
				nodes.push_back(std::move(node));
				if (ground && !top)
					break; // ground: no water of ours under it
			}
			std::optional<v3s16> &kept = g.tops[(tz * per + nz) * g.nodes + tx * per + nx];
			if (kept != top) {
				if (kept)
					handover.let_go.push_back(getNodeBlockPos(*kept));
				if (top)
					handover.taken.push_back(getNodeBlockPos(*top));
			}
			kept = top;
			// its held level (the sea's, a river's with its rise): its columns' come from
			// it and its neighbours' (Grid::relevel)
			const s32 level = is_sea ? s32(std::lround(surface)) : NONE;
			s32 &kept_level = g.levels[(tz * per + nz) * g.nodes + tx * per + nx];
			if (level != kept_level) {
				kept_level = level;
				levelled = true;
			}
			g.runs[(tz * per + nz) * g.nodes + tx * per + nx] = runs;
			g.ways[(tz * per + nz) * g.nodes + tx * per + nx] = way;
			sea = sea || is_sea;
			for (int iz = 0; iz < g.res; iz++)
				for (int ix = 0; ix < g.res; ix++) {
					const int c = (tz * 16 + nz * g.res + iz) * g.cols + tx * 16 + nx * g.res + ix;
					// the sixteenth at the column's middle
					const int sx = ix * 16 / g.res + 8 / g.res, sz = iz * 16 / g.res + 8 / g.res;
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
					if (floor == NONE && top)
						floor = lo * 16; // deep water: as far down as it looks is its bottom
					moved = moved || floor != g.floor[c];
					// the ground moved under water: its surface stays where it was (only
					// under water: a dry rut dug got water as deep as it, sprint 84)
					if (!fresh && floor != g.floor[c] && g.floor[c] != NONE && floor != NONE && g.depth[c] > FILM)
						g.depth[c] = std::max(0.0f, g.depth[c] + (g.floor[c] - floor) / 16.0f);
					g.floor[c] = floor;
					g.want[c] = top && floor != NONE ? s32(std::lround(surface)) : NONE;
					if (floor == NONE) {
						g.depth[c] = 0;
						continue;
					}
					const float over = top ? std::max(0.0f, (surface - floor) / 16) : 0;
					if (!is_sea) {
						target += over * area;
						// counted as the coast's pools count what they hold: over the sea's level only
						const float under = g.sea_level == INT32_MIN ? 0 : std::max(0.0f, (g.sea_level - floor) / 16.0f);
						above += std::max(0.0f, over - under) * area;
					}
					if (fresh)
						g.depth[c] = over; // first seen: as the server has it
				}
		}
	if (fresh)
		besideSea(t, sea);
	moved = !fresh && (moved || std::fabs(target - tile.target) > SLACK);
	tile.target = target;
	tile.above = above;
	tile.sea = sea;
	tile.known = true;
	tile.scanned = true;
	if (fresh)
		tile.drawn = false;
	if (levelled)
		for (int dz = -1; dz <= 1; dz++)
			for (int dx = -1; dx <= 1; dx++) {
				const int nx = tx + dx, nz = tz + dz;
				if (nx >= 0 && nx < g.tiles && nz >= 0 && nz < g.tiles)
					g.tile[nz * g.tiles + nx].relevel = true;
			}
	// first seen, it's as the server has it: run only if it doesn't meet its neighbours level
	if (moved) {
		tile.drawn = false;
		g.wake(t);
	}
}

// A tile first seen beside the sea: its ground under the sea's level where
// the server has water starts full to it, as the sea would fill it (else it
// starts dry and is seen filling: the shallows animating); and a sea tile
// first seen does that for the tiles beside it seen before it
void Scan::besideSea(int t, bool sea)
{
	Grid &g = m_grid;
	if (g.sea_level == INT32_MIN)
		return;
	const int tx = t % g.tiles, tz = t / g.tiles;
	auto fillTo = [&](int tile, bool redraw) {
		const int first = g.firstOf(tile);
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int c = first + cz * g.cols + cx;
				if (g.floor[c] == NONE || g.want[c] == NONE || g.floor[c] >= g.sea_level)
					continue;
				const float full = (g.sea_level - g.floor[c]) / 16.0f;
				if (g.depth[c] < full) {
					g.depth[c] = full;
					if (redraw)
						g.tile[tile].drawn = false;
				}
			}
	};
	bool beside = sea;
	for (int s = 0; s < 4 && !beside; s++) {
		const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
		beside = nx >= 0 && nx < g.tiles && nz >= 0 && nz < g.tiles && g.tile[nz * g.tiles + nx].sea;
	}
	if (beside)
		fillTo(t, false);
	if (!sea)
		return;
	for (int s = 0; s < 4; s++) {
		const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
		if (nx < 0 || nx >= g.tiles || nz < 0 || nz >= g.tiles)
			continue;
		const Tile &n = g.tile[nz * g.tiles + nx];
		if (n.known && !n.sea)
			fillTo(nz * g.tiles + nx, true);
	}
}

} // namespace liquids
