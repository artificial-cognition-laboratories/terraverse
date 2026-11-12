// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terra_flora.h"
#include "exceptions.h"
#include "map.h"
#include "nodedef.h"
#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

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
	struct Node
	{
		shape::Shape shape;
		std::array<s32, (int)flora::Part::Count> parts{};
	};
	std::unordered_map<u64, Node> nodes;
	const auto keyOf = [](v3s16 p) { return ((u64)(u16)p.X << 32) | ((u64)(u16)p.Y << 16) | (u64)(u16)p.Z; };
	std::unordered_map<u64, v3s16> where;
	const auto fill = [&](s32 cx, s32 cy, s32 cz, flora::Part part) {
		const v3s16 at(foot.X + floorDiv(cx, STEPS), foot.Y + floorDiv(cy, STEPS), foot.Z + floorDiv(cz, STEPS));
		const u64 key = keyOf(at);
		Node &n = nodes[key];
		where[key] = at;
		const v3s16 sub(cx - floorDiv(cx, STEPS) * STEPS, cy - floorDiv(cy, STEPS) * STEPS, cz - floorDiv(cz, STEPS) * STEPS);
		if (!n.shape.has(sub)) {
			n.shape.set(sub, true);
			n.parts[(int)part]++;
		}
	};
	const auto turned = [&](s32 &x, s32 &z) {
		if (mirror)
			x = -x;
		for (int k = 0; k < turn; k++) {
			const s32 t = x;
			x = -z;
			z = t;
		}
	};
	// the plant's wood, to join a thin branch to what it grows from
	const auto voxelKey = [](s32 x, s32 y, s32 z) { return ((u64)(u16)x << 32) | ((u64)(u16)y << 16) | (u64)(u16)z; };
	std::unordered_set<u64> wood;
	for (const flora::Voxel &v : plant.voxels)
		if (v.part == flora::Part::Wood || v.part == flora::Part::Branch)
			wood.insert(voxelKey(v.x, v.y, v.z));
	for (const flora::Voxel &v : plant.voxels) {
		s32 x = v.x, z = v.z;
		turned(x, z);
		// a thin branch fills the middle of its voxel
		const s32 side = v.part == flora::Part::Branch ? std::max(1, v.size * cell / 16) : cell;
		const s32 inset = (cell - side) / 2;
		// the voxel's sixteenths, from the foot node's corner (its middle is the foot's middle)
		const s32 x0 = x * cell + STEPS / 2 - cell / 2 + inset, z0 = z * cell + STEPS / 2 - cell / 2 + inset, y0 = v.y * cell + inset;
		const auto cube = [&](s32 ox, s32 oy, s32 oz) {
			const s32 cx = x0 + ox, cy = y0 + oy, cz = z0 + oz;
			// (most of a neighbour's plant is outside: skip it before its sixteenths)
			if (foot.X + floorDiv(cx + side - 1, STEPS) < lo.X || foot.X + floorDiv(cx, STEPS) > hi.X ||
					foot.Y + floorDiv(cy + side - 1, STEPS) < lo.Y || foot.Y + floorDiv(cy, STEPS) > hi.Y ||
					foot.Z + floorDiv(cz + side - 1, STEPS) < lo.Z || foot.Z + floorDiv(cz, STEPS) > hi.Z)
				return;
			// a whole node at once (a tree's every voxel)
			if (side == STEPS && cx % STEPS == 0 && cy % STEPS == 0 && cz % STEPS == 0) {
				const v3s16 at(foot.X + floorDiv(cx, STEPS), foot.Y + floorDiv(cy, STEPS), foot.Z + floorDiv(cz, STEPS));
				const u64 key = keyOf(at);
				Node &n = nodes[key];
				where[key] = at;
				n.parts[(int)v.part] += (s32)(shape::CELLS - n.shape.count());
				n.shape = shape::Shape::whole();
				return;
			}
			for (s32 dy = 0; dy < side; dy++)
				for (s32 dz = 0; dz < side; dz++)
					for (s32 dx = 0; dx < side; dx++)
						fill(x0 + ox + dx, y0 + oy + dy, z0 + oz + dz, v.part);
		};
		cube(0, 0, 0);
		if (side >= cell)
			continue;
		/* A thin branch's cube floats in its voxel's middle: reach halfway to
		   each woody neighbour so the branch is one piece, not a row of dots
		   (a diagonal one most of all). */
		for (s32 ny = -1; ny <= 1; ny++)
			for (s32 nz = -1; nz <= 1; nz++)
				for (s32 nx = -1; nx <= 1; nx++) {
					if ((!nx && !ny && !nz) || !wood.count(voxelKey(v.x + nx, v.y + ny, v.z + nz)))
						continue;
					s32 dx = nx, dz = nz;
					turned(dx, dz);
					for (s32 t = 1; t <= cell / 2; t++)
						cube(dx * t, ny * t, dz * t);
				}
	}
	static const s32 WEIGHT[(int)flora::Part::Count] = { 4, 4, 1, 2, 2, 1 }; // wood, branch, leaf, flesh, flower, vein
	for (const auto &[key, n] : nodes) {
		const v3s16 at = where[key];
		if (at.X < lo.X || at.X > hi.X || at.Y < lo.Y || at.Y > hi.Y || at.Z < lo.Z || at.Z > hi.Z || !vm.m_area.contains(at) ||
				!open(at))
			continue;
		int part = 0;
		for (int p = 1; p < (int)flora::Part::Count; p++)
			if (n.parts[p] * WEIGHT[p] > n.parts[part] * WEIGHT[part])
				part = p;
		const flora::Part chosen = (flora::Part)part;
		const u32 rgb = chosen == flora::Part::Wood || chosen == flora::Part::Branch ? colours.bark
				: chosen == flora::Part::Flower ? colours.bloom : chosen == flora::Part::Vein ? colours.vein : colours.leaf;
		vm.setNodeNoEmerge(at, MapNode(blocks.of(chosen), 0, paletteIndex(rgb)));
		if (!n.shape.isWhole())
			shapes[at] = n.shape;
		else
			shapes.erase(at);
	}
}

} // namespace terra
