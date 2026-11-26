// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/faces.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/views/game/water/liquidlook.h"
#include "src/client/render/map/clientmap.h"
#include "library/matter/fill/fill.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/map/block/mapblock.h"
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
const u8 ALPHA = u8(look::ALPHA * 255 + 0.5f);
void side(scene::SMeshBuffer *mesh, v3f o, v3s16 d, float from, float to, video::SColor colour);
} // namespace

Faces::Faces(Opts o) : m_ndef(o.ndef), m_grid(o.grid) {}

void Faces::build(const Block &b, ClientMap &map)
{
	sea(b, map);
	fills(b, map);
}

// The sea: tops alike along x in one quad; sides as looked at. A river's
// top slopes through its corners' heights (held::corners, as the grid's
// columns stand), with no side against the river beside it.
void Faces::sea(const Block &b, ClientMap &map)
{
	scene::SMeshBuffer *mesh = b.mesh;
	const v3s16 base = b.blockpos * MAP_BLOCKSIZE;
	const u32 daynight = b.daynight;
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
		for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
			for (s16 x = 0; x < MAP_BLOCKSIZE;) {
				auto it = b.nodes.find(keys::of(v3s16(x, y, z)));
				const v3s16 p = base + v3s16(x, y, z);
				if (it == b.nodes.end() || !it->second.sea || m_grid.owns(p)) {
					x++;
					continue;
				}
				const Shown &s = it->second;
				const float shade = light::shade(map, m_ndef, p, daynight);
				const v3f o(x - 0.5f, y - 0.5f, z - 0.5f);
				const video::SColor colour = light::tint(s.r, s.g, s.b, shade, 0.85f, false);
				const video::SColor top = light::tint(s.r, s.g, s.b, shade, 0.65f, true, u8(ALPHA + 60));
				if (s.held && s.top) {
					riverTop(b, map, p, o, s, colour, top);
					x++;
					continue;
				}
				const float height = s.level;
				for (int i = 0; i < 4; i++)
					if (s.from[i] >= 0 && s.from[i] < height * 16 && !m_grid.owns(p + SIDES[i]))
						side(mesh, o, SIDES[i], o.Y + s.from[i] / 16.0f, o.Y + height, colour);
				s16 end = x + 1;
				if (s.top) {
					// the run of tops lit alike beside it, at its height
					while (end < MAP_BLOCKSIZE) {
						auto next = b.nodes.find(keys::of(v3s16(end, y, z)));
						const v3s16 q = base + v3s16(end, y, z);
						if (next == b.nodes.end() || !next->second.sea || !next->second.top || next->second.level != s.level ||
								next->second.held || m_grid.owns(q) || light::shade(map, m_ndef, q, daynight) != shade)
							break;
						end++;
					}
					const float h = o.Y + height;
					quads::into(mesh, v3f(o.X, h, o.Z), v3f(o.X, h, o.Z + 1), v3f(o.X + (end - x), h, o.Z + 1),
							v3f(o.X + (end - x), h, o.Z), v3f(0, 1, 0), top);
				}
				x = end;
			}
}

// A river's top node: its surface through its corners, and its sides where
// what's beside isn't its river (a bank's open air, a fall's foot)
void Faces::riverTop(const Block &b, ClientMap &map, v3s16 p, v3f o, const Shown &s, video::SColor colour, video::SColor top)
{
	held::Around around;
	for (int dz = -1; dz <= 1; dz++)
		for (int dx = -1; dx <= 1; dx++)
			around[dz + 1][dx + 1] = held::levelAt(map, m_ndef, b.rivers, p + v3s16(dx, 0, dz));
	scene::SMeshBuffer *mesh = b.mesh;
	const s32 own = around[1][1];
	if (own == NONE)
		return; // (not its column's top after all)
	const std::array<float, 4> c = held::corners(around);
	// node-local heights: the node's foot is p.Y * 16 world sixteenths
	auto h = [&](int k) { return o.Y + (c[k] - p.Y * 16) / 16.0f; };
	quads::into(mesh, v3f(o.X, h(0), o.Z), v3f(o.X, h(2), o.Z + 1), v3f(o.X + 1, h(3), o.Z + 1), v3f(o.X + 1, h(1), o.Z),
			v3f(0, 1, 0), top);
	// each side's two corners, as SIDES runs: +x (1, 3), −x (0, 2), +z (2, 3), −z (0, 1)
	static const int EDGE[4][2] = { { 1, 3 }, { 0, 2 }, { 2, 3 }, { 0, 1 } };
	for (int i = 0; i < 4; i++) {
		const v3s16 n = p + SIDES[i];
		const s32 there = around[1 + SIDES[i].Z][1 + SIDES[i].X];
		if (s.from[i] < 0 || m_grid.owns(n) || (there != NONE && std::abs(there - own) <= 16))
			continue; // against ground, the grid's, or its own river: no side
		const float from = o.Y + s.from[i] / 16.0f, a = h(EDGE[i][0]), d = h(EDGE[i][1]);
		const v3f nv(SIDES[i].X, 0, SIDES[i].Z);
		// the side's foot along the edge, under its two corners' tops
		auto at = [&](int k) {
			return v3f(o.X + ((k & 1) ? 1 : 0), 0, o.Z + ((k >> 1) ? 1 : 0));
		};
		const v3f e0 = at(EDGE[i][0]), e1 = at(EDGE[i][1]);
		quads::into(mesh, v3f(e0.X, from, e0.Z), v3f(e0.X, a, e0.Z), v3f(e1.X, d, e1.Z), v3f(e1.X, from, e1.Z), nv, colour);
	}
}

// Fills the grid doesn't draw: a top over the open sixteenths of the layer it
// stands in, and sides where it stands above what's beside it (open air, a lower fill)
void Faces::fills(const Block &b, ClientMap &map)
{
	scene::SMeshBuffer *mesh = b.mesh;
	const v3s16 base = b.blockpos * MAP_BLOCKSIZE;
	const u32 daynight = b.daynight;
	for (const auto &[k, s] : b.nodes) {
		if (s.sea || s.level <= 0)
			continue;
		const v3s16 rel = keys::rel(k);
		const v3s16 p = base + rel;
		if (m_grid.owns(p))
			continue;
		const float shade = light::shade(map, m_ndef, p, daynight);
		const v3f o(rel.X - 0.5f, rel.Y - 0.5f, rel.Z - 0.5f);
		const float above = besideLevel(map, m_ndef, p + v3s16(0, 1, 0));
		const bool underneath = above >= 0 && above <= 1;
		const std::optional<shape::Shape> solid = map.solidShape(p); // cut, or smoothed ground
		const shape::Shape *cut = solid ? &*solid : nullptr;
		const float h = fill::Room::of(cut).surface(s.level);
		if (h <= 0)
			continue;
		// as deep as the grid reckons it (so they meet in one colour)
		const float below = besideLevel(map, m_ndef, p - v3s16(0, 1, 0));
		const float deep = std::min(1.0f, h + (below >= 0.999f && below <= 1 ? 1 : 0)); // over liquid, not ground
		const video::SColor top_colour = light::tint(s.r, s.g, s.b, shade, 1 - 0.35f * deep, true, u8(ALPHA + 60 * deep));
		const video::SColor colour = light::tint(s.r, s.g, s.b, shade, 0.85f, false);
		const float top = o.Y + h;
		if (!(h >= 0.999f && underneath)) {
			if (!cut) {
				quads::into(mesh, v3f(o.X, top, o.Z), v3f(o.X, top, o.Z + 1), v3f(o.X + 1, top, o.Z + 1), v3f(o.X + 1, top, o.Z),
						v3f(0, 1, 0), top_colour);
			} else {
				const int layer = std::clamp(int(std::ceil(h * 16)) - 1, 0, 15);
				for (int z = 0; z < 16; z++)
					for (int x = 0; x < 16;) {
						if (cut->has(v3s16(x, layer, z))) {
							x++;
							continue;
						}
						int end = x;
						while (end < 16 && !cut->has(v3s16(end, layer, z)))
							end++;
						const float x0 = o.X + x / 16.0f, x1 = o.X + end / 16.0f;
						const float z0 = o.Z + z / 16.0f, z1 = o.Z + (z + 1) / 16.0f;
						quads::into(mesh, v3f(x0, top, z0), v3f(x0, top, z1), v3f(x1, top, z1), v3f(x1, top, z0), v3f(0, 1, 0),
								top_colour);
						x = end;
					}
			}
		}
		for (const v3s16 &d : SIDES) {
			const v3s16 n = p + d;
			if (m_grid.owns(n))
				continue;
			const float there = besideLevel(map, m_ndef, n);
			if (there >= 2 || there >= h || (there < 0 && map.solidShape(n)))
				continue;
			side(mesh, o, d, o.Y + std::max(0.0f, there), top, colour);
		}
	}
}

namespace {

// A side's quad: facing d, from `from` up to `to`, on the node whose low corner is o
void side(scene::SMeshBuffer *mesh, v3f o, v3s16 d, float from, float to, video::SColor colour)
{
	const v3f nv(d.X, 0, d.Z);
	if (d.X) {
		const float x = d.X > 0 ? o.X + 1 : o.X;
		quads::into(mesh, v3f(x, from, o.Z), v3f(x, to, o.Z), v3f(x, to, o.Z + 1), v3f(x, from, o.Z + 1), nv, colour);
	} else {
		const float z = d.Z > 0 ? o.Z + 1 : o.Z;
		quads::into(mesh, v3f(o.X, from, z), v3f(o.X, to, z), v3f(o.X + 1, to, z), v3f(o.X + 1, from, z), nv, colour);
	}
}

} // namespace

} // namespace liquids
