// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terra_flora.h"
#include "exceptions.h"
#include "map.h"
#include "nodedef.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace terra {

content_t PlantBlocks::of(flora::Part part) const
{
	switch (part) {
	case flora::Part::Wood:
	case flora::Part::Branch: return wood;
	case flora::Part::Flesh: return flesh;
	case flora::Part::Flower: return flower;
	default: return leaf;
	}
}

PlantBlocks plantBlocks(const NodeDefManager *ndef)
{
	const auto need = [&](const char *name) {
		content_t c;
		if (!ndef->getId(name, c))
			throw BaseException(std::string("terra: the game has no ") + name + " (plants are made of it)");
		return c;
	};
	return { need("tf_world:plant_wood"), need("tf_world:plant_leaf"), need("tf_world:plant_flesh"), need("tf_world:plant_flower"),
		need("tf_world:growth") };
}

// 16 hues × 16 shades (4 saturations × 4 values): its rows are shades, its columns hues
static const std::array<u32, 256> &palette()
{
	static const std::array<u32, 256> table = [] {
		std::array<u32, 256> out{};
		static const float S[4] = { 0.12f, 0.35f, 0.6f, 0.85f }, V[4] = { 0.28f, 0.48f, 0.7f, 0.92f };
		for (int i = 0; i < 256; i++) {
			const float h = (i % 16) / 16.f * 6, s = S[(i / 16) % 4], v = V[i / 64];
			const int sector = (int)h % 6;
			const float f = h - (int)h, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
			const float rgb[6][3] = { { v, t, p }, { q, v, p }, { p, v, t }, { p, q, v }, { t, p, v }, { v, p, q } };
			const auto byte = [](float c) { return (u32)std::lround(c * 255); };
			out[i] = (byte(rgb[sector][0]) << 16) | (byte(rgb[sector][1]) << 8) | byte(rgb[sector][2]);
		}
		return out;
	}();
	return table;
}

u32 paletteColour(u8 index)
{
	return palette()[index];
}

u8 paletteIndex(u32 rgb)
{
	int best = 0;
	long nearest = LONG_MAX;
	for (int i = 0; i < 256; i++) {
		const u32 c = palette()[i];
		long d = 0;
		for (int k = 0; k < 3; k++) {
			const long a = (long)((rgb >> (16 - 8 * k)) & 255) - (long)((c >> (16 - 8 * k)) & 255);
			d += a * a * (k == 1 ? 4 : 2); // (green counts most to the eye)
		}
		if (d < nearest) {
			nearest = d;
			best = i;
		}
	}
	return (u8)best;
}

static s32 floorDiv(s32 a, s32 b)
{
	return a >= 0 ? a / b : -((-a + b - 1) / b);
}

void place(const flora::Plant &plant, const flora::Colours &colours, v3s16 foot, int turn, bool mirror, v3s16 lo, v3s16 hi,
		const PlantBlocks &blocks, MMVManip &vm, const Open &open, std::map<v3s16, shape::Shape> &shapes)
{
	using shape::STEPS;
	const s32 cell = STEPS / plant.grain; // sixteenths a voxel spans
	if (plant.woody.size() != plant.voxels.size())
		throw std::logic_error("terra::place: a plant not grown by flora::grow (its voxels' woody neighbours unknown)");
	// (its sixteenths as rows along x, a row a (y, z): a box of them fills
	// a row at a time; a shape::Shape is made of it only if it's written partial)
	struct Node
	{
		v3s16 at;
		std::array<u16, STEPS * STEPS> rows{};
		std::array<s32, (int)flora::Part::Count> parts{};
	};
	static constexpr u16 FULL = 0xFFFF;
	std::unordered_map<u64, Node> nodes;
	const auto keyOf = [](v3s16 p) { return ((u64)(u16)p.X << 32) | ((u64)(u16)p.Y << 16) | (u64)(u16)p.Z; };
	const auto nodeAt = [&](v3s16 at) -> Node & {
		Node &n = nodes[keyOf(at)];
		n.at = at;
		return n;
	};
	// the chunk's nodes, in sixteenths from the foot node's corner: what's
	// outside is never written, so never filled
	const s32 lo_x = (lo.X - foot.X) * STEPS, lo_y = (lo.Y - foot.Y) * STEPS, lo_z = (lo.Z - foot.Z) * STEPS;
	const s32 hi_x = (hi.X - foot.X + 1) * STEPS, hi_y = (hi.Y - foot.Y + 1) * STEPS, hi_z = (hi.Z - foot.Z + 1) * STEPS;
	const auto turned = [&](s32 &x, s32 &z) {
		if (mirror)
			x = -x;
		for (int k = 0; k < turn; k++) {
			const s32 t = x;
			x = -z;
			z = t;
		}
	};
	// (voxels are sorted by y: only the layers that can reach the chunk's,
	// a voxel and the half voxel a thin branch reaches out from it)
	const auto by_y = [](const flora::Voxel &v, s32 y) { return v.y < y; };
	const size_t first = std::lower_bound(plant.voxels.begin(), plant.voxels.end(), floorDiv(lo_y, cell) - 2, by_y) - plant.voxels.begin();
	const size_t last = std::lower_bound(plant.voxels.begin(), plant.voxels.end(), floorDiv(hi_y, cell) + 2, by_y) - plant.voxels.begin();
	for (size_t i = first; i < last; i++) {
		const flora::Voxel &v = plant.voxels[i];
		s32 x = v.x, z = v.z;
		turned(x, z);
		// a thin branch fills the middle of its voxel
		const s32 side = v.part == flora::Part::Branch ? std::max(1, v.size * cell / 16) : cell;
		const s32 inset = (cell - side) / 2;
		// the voxel's sixteenths, from the foot node's corner (its middle is the foot's middle)
		const s32 x0 = x * cell + STEPS / 2 - cell / 2 + inset, z0 = z * cell + STEPS / 2 - cell / 2 + inset, y0 = v.y * cell + inset;
		const auto cube = [&](s32 ox, s32 oy, s32 oz) {
			// (clipped to the chunk: most of a neighbour's plant is outside)
			const s32 ax = std::max(x0 + ox, lo_x), bx = std::min(x0 + ox + side, hi_x);
			const s32 ay = std::max(y0 + oy, lo_y), by = std::min(y0 + oy + side, hi_y);
			const s32 az = std::max(z0 + oz, lo_z), bz = std::min(z0 + oz + side, hi_z);
			if (ax >= bx || ay >= by || az >= bz)
				return;
			// a whole node at once (a tree's every voxel)
			if (side == STEPS && ax == x0 + ox && ay == y0 + oy && az == z0 + oz && ax % STEPS == 0 && ay % STEPS == 0 && az % STEPS == 0) {
				Node &n = nodeAt(v3s16(foot.X + floorDiv(ax, STEPS), foot.Y + floorDiv(ay, STEPS), foot.Z + floorDiv(az, STEPS)));
				s32 had = 0;
				for (u16 &r : n.rows) {
					had += __builtin_popcount(r);
					r = FULL;
				}
				n.parts[(int)v.part] += shape::CELLS - had;
				return;
			}
			// node by node it spans, its sixteenths in each (one look-up a node, not a sixteenth)
			for (s32 ny = floorDiv(ay, STEPS); ny * STEPS < by; ny++)
				for (s32 nz = floorDiv(az, STEPS); nz * STEPS < bz; nz++)
					for (s32 nx = floorDiv(ax, STEPS); nx * STEPS < bx; nx++) {
						Node &n = nodeAt(v3s16(foot.X + nx, foot.Y + ny, foot.Z + nz));
						const s32 x_lo = std::max(ax, nx * STEPS) - nx * STEPS, x_hi = std::min(bx, (nx + 1) * STEPS) - nx * STEPS;
						const u16 mask = (u16)(((1u << (x_hi - x_lo)) - 1) << x_lo);
						s32 added = 0;
						for (s32 cy = std::max(ay, ny * STEPS); cy < std::min(by, (ny + 1) * STEPS); cy++)
							for (s32 cz = std::max(az, nz * STEPS); cz < std::min(bz, (nz + 1) * STEPS); cz++) {
								u16 &r = n.rows[(cy - ny * STEPS) * STEPS + (cz - nz * STEPS)];
								added += __builtin_popcount(mask & ~r);
								r |= mask;
							}
						n.parts[(int)v.part] += added;
					}
		};
		cube(0, 0, 0);
		if (side >= cell)
			continue;
		/* A thin branch's cube floats in its voxel's middle: reach halfway to
		   each woody neighbour so the branch is one piece, not a row of dots
		   (a diagonal one most of all). */
		const u32 woody = plant.woody[i];
		for (s32 ny = -1; ny <= 1; ny++)
			for (s32 nz = -1; nz <= 1; nz++)
				for (s32 nx = -1; nx <= 1; nx++) {
					if (!(woody >> ((ny + 1) * 9 + (nz + 1) * 3 + (nx + 1)) & 1))
						continue;
					s32 dx = nx, dz = nz;
					turned(dx, dz);
					for (s32 t = 1; t <= cell / 2; t++)
						cube(dx * t, ny * t, dz * t);
				}
	}
	static const s32 WEIGHT[(int)flora::Part::Count] = { 4, 4, 1, 2, 2, 1 }; // wood, branch, leaf, flesh, flower, vein
	// (its colours' nearest in the palette, each found once: a search of all 256)
	const u8 bark = paletteIndex(colours.bark), bloom = paletteIndex(colours.bloom), vein = paletteIndex(colours.vein),
			leaf = paletteIndex(colours.leaf);
	for (const auto &[key, n] : nodes) {
		const v3s16 at = n.at;
		if (at.X < lo.X || at.X > hi.X || at.Y < lo.Y || at.Y > hi.Y || at.Z < lo.Z || at.Z > hi.Z || !vm.m_area.contains(at) ||
				!open(at))
			continue;
		int part = 0;
		for (int p = 1; p < (int)flora::Part::Count; p++)
			if (n.parts[p] * WEIGHT[p] > n.parts[part] * WEIGHT[part])
				part = p;
		const flora::Part chosen = (flora::Part)part;
		const u8 colour = chosen == flora::Part::Wood || chosen == flora::Part::Branch ? bark
				: chosen == flora::Part::Flower ? bloom : chosen == flora::Part::Vein ? vein : leaf;
		vm.setNodeNoEmerge(at, MapNode(blocks.of(chosen), 0, colour));
		bool whole = true;
		for (const u16 r : n.rows)
			whole = whole && r == FULL;
		if (whole) {
			shapes.erase(at);
			continue;
		}
		shape::Shape out;
		for (s32 y = 0; y < STEPS; y++)
			for (s32 z = 0; z < STEPS; z++)
				out.setRow(y, z, n.rows[y * STEPS + z]);
		shapes[at] = out;
	}
}

} // namespace terra
