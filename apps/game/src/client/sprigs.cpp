// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sprigs.h"
#include "flora/forest.h"
#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace sprig {

namespace {

const size_t KEPT = 2000; // plants' faces kept before they're all let go

u64 cellKey(s32 x, s32 y, s32 z) { return ((u64)(u16)x << 32) | ((u64)(u16)y << 16) | (u64)(u16)z; }

u32 colourOf(flora::Part part, const flora::Colours &c)
{
	switch (part) {
	case flora::Part::Wood:
	case flora::Part::Branch:
		return c.bark;
	case flora::Part::Flower:
		return c.bloom;
	case flora::Part::Vein:
		return c.vein;
	default:
		return c.leaf;
	}
}

s32 floorDiv(s32 a, s32 b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// The plant in at most `grain` voxels to a block: each coarse voxel the part most of its fine ones are.
flora::Plant coarsened(const flora::Plant &plant, s32 grain)
{
	if (plant.grain <= grain)
		return plant;
	const s32 k = plant.grain / grain; // (grains are powers of two)
	std::map<std::array<s32, 3>, std::array<s32, (int)flora::Part::Count>> votes;
	for (const flora::Voxel &v : plant.voxels)
		// x, z centred on the foot: a coarse voxel's middle on a fine one's
		votes[{ floorDiv(v.x + k / 2, k), v.y / k, floorDiv(v.z + k / 2, k) }][(int)v.part]++;
	flora::Plant out;
	out.grain = grain;
	for (const auto &[at, n] : votes) {
		const int part = (int)(std::max_element(n.begin(), n.end()) - n.begin());
		out.voxels.push_back({ (int16_t)at[0], (int16_t)at[1], (int16_t)at[2], (flora::Part)part, 16 });
	}
	return out;
}

std::vector<Face> grow(u32 forest, u32 id, s32 grain)
{
	const std::shared_ptr<flora::Forest> f = flora::forestFor(forest);
	const flora::Plant plant = coarsened(*f->plant(id), grain);
	const flora::Colours colours = f->traitsOf(id).colours;
	std::unordered_map<u64, flora::Part> cells;
	for (const flora::Voxel &v : plant.voxels)
		cells[cellKey(v.x, v.y, v.z)] = v.part;
	const float g = (float)plant.grain;
	static const v3s16 DIRS[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

	std::vector<Face> out;
	for (const v3s16 &d : DIRS) {
		const int axis = d.X ? 0 : d.Y ? 1 : 2;
		const int ua = axis == 0 ? 2 : 0, va = axis == 1 ? 2 : 1;
		// each plane's open faces: (a, u, v) → part
		std::map<std::array<s32, 3>, flora::Part> open;
		for (const flora::Voxel &v : plant.voxels) {
			const s32 c[3] = { v.x, v.y, v.z };
			if (cells.count(cellKey(v.x + d.X, v.y + d.Y, v.z + d.Z)))
				continue;
			open[{ c[axis], c[ua], c[va] }] = v.part;
		}
		// rectangles: a run along v, grown along u, of one part
		std::unordered_set<u64> used;
		const auto has = [&](s32 a, s32 u, s32 v, flora::Part part) {
			const auto it = open.find({ a, u, v });
			return it != open.end() && it->second == part && !used.count(cellKey(a, u, v));
		};
		const int sign = d.X + d.Y + d.Z;
		for (const auto &[at, part] : open) {
			const auto [a, u, v] = at;
			if (used.count(cellKey(a, u, v)))
				continue;
			s32 v1 = v + 1;
			while (has(a, u, v1, part))
				v1++;
			s32 u1 = u + 1;
			for (;; u1++) {
				bool row = true;
				for (s32 k = v; k < v1 && row; k++)
					row = has(a, u1, k, part);
				if (!row)
					break;
			}
			for (s32 i = u; i < u1; i++)
				for (s32 k = v; k < v1; k++)
					used.insert(cellKey(a, i, k));
			const float plane = (float)(a + (sign > 0 ? 1 : 0));
			const auto corner = [&](s32 cu, s32 cv) {
				v3f c;
				c[axis] = plane;
				c[ua] = (float)cu;
				c[va] = (float)cv;
				// voxels from the foot: x and z centred on the node's middle
				return v3f((c.X - 0.5f) / g, c.Y / g, (c.Z - 0.5f) / g);
			};
			out.push_back({ { corner(u, v), corner(u1, v), corner(u1, v1), corner(u, v1) }, d, part, colourOf(part, colours) });
		}
	}
	return out;
}

} // namespace

std::shared_ptr<const std::vector<Face>> facesOf(u32 forest, u32 id, s32 grain)
{
	static std::mutex lock;
	static std::map<std::tuple<u32, u32, s32>, std::shared_ptr<const std::vector<Face>>> kept;
	const std::tuple<u32, u32, s32> key(forest, id, grain);
	{
		std::lock_guard<std::mutex> hold(lock);
		const auto it = kept.find(key);
		if (it != kept.end())
			return it->second;
	}
	// (grown outside the lock: the mesh threads share it)
	auto faces = std::make_shared<const std::vector<Face>>(grow(forest, id, grain));
	std::lock_guard<std::mutex> hold(lock);
	if (kept.size() >= KEPT)
		kept.clear();
	return kept.emplace(key, faces).first->second;
}

} // namespace sprig
