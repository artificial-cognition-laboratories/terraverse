// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "flora.h"
#include <algorithm>
#include <map>

// What the generators share (not for flora's callers).
namespace flora {

using namespace terragen;
using terragen::div; // (not stdlib's)

constexpr fix R(int64_t hundredths) { return ratio(hundredths, 100); }

struct Rng
{
	uint32_t s;
	uint32_t next()
	{
		s += 0x9e3779b9u;
		uint32_t h = s;
		h = (h ^ (h >> 16)) * 0x85ebca6bu;
		h = (h ^ (h >> 13)) * 0xc2b2ae35u;
		return h ^ (h >> 16);
	}
	fix unit() { return (fix)(next() >> 16); } // 0 .. just under 1
	fix in(fix lo, fix hi) { return lo + mul(hi - lo, unit()); }
};

inline uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	uint32_t h = seed ^ ((uint32_t)x * 0x27d4eb2du) ^ ((uint32_t)y * 0x165667b1u) ^ ((uint32_t)z * 0x9e3779b1u);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

struct V
{
	fix x, y, z;
};
inline V operator+(V a, V b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V operator-(V a, V b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V scaled(V a, fix k) { return { mul(a.x, k), mul(a.y, k), mul(a.z, k) }; }
inline int64_t square(V v) { return (int64_t)v.x * v.x + (int64_t)v.y * v.y + (int64_t)v.z * v.z; } // Q32
inline fix length(V v) { return (fix)isqrt((uint64_t)square(v)); }
inline V cross(V a, V b) { return { mul(a.y, b.z) - mul(a.z, b.y), mul(a.z, b.x) - mul(a.x, b.z), mul(a.x, b.y) - mul(a.y, b.x) }; }
inline V unit(V v, V otherwise = { 0, ONE, 0 })
{
	const fix len = length(v);
	if (len < 64)
		return otherwise;
	return { div(v.x, len), div(v.y, len), div(v.z, len) };
}

// A plant's voxels as they're laid: where two fall on one voxel, the part that shows through wins.
class Canvas
{
public:
	explicit Canvas(int32_t grain) { m_plant.grain = grain; }
	void set(int32_t x, int32_t y, int32_t z, Part part, uint8_t size = 16)
	{
		if (y < 0 || y > 4000 || x < -4000 || x > 4000 || z < -4000 || z > 4000)
			return;
		const uint64_t key = ((uint64_t)y << 32) | ((uint64_t)(z + 4096) << 16) | (uint64_t)(x + 4096);
		const auto found = m_cells.find(key);
		if (found == m_cells.end())
			m_cells.emplace(key, std::make_pair(part, size));
		else if (RANK[(int)part] > RANK[(int)found->second.first] || (part == found->second.first && size > found->second.second))
			found->second = { part, size };
	}
	void set(V at, Part part, uint8_t size = 16) { set(roundInt(at.x), roundInt(at.y), roundInt(at.z), part, size); }
	// Sorted by y, z, x, as the keys are.
	Plant finish()
	{
		for (const auto &[key, cell] : m_cells) {
			const int32_t x = (int32_t)(key & 0xffff) - 4096, z = (int32_t)((key >> 16) & 0xffff) - 4096, y = (int32_t)(key >> 32);
			m_plant.voxels.push_back({ (int16_t)x, (int16_t)y, (int16_t)z, cell.first, cell.second });
			m_plant.count[(int)cell.first]++;
			m_plant.reach = std::max({ m_plant.reach, x < 0 ? -x : x, z < 0 ? -z : z });
			m_plant.top = std::max(m_plant.top, y);
		}
		return std::move(m_plant);
	}

private:
	// by Part: wood, branch, leaf, flesh, flower, vein
	static constexpr int RANK[(int)Part::Count] = { 5, 3, 1, 2, 4, 1 };
	Plant m_plant;
	std::map<uint64_t, std::pair<Part, uint8_t>> m_cells;
};

Plant growPlant(const Traits &traits, uint32_t seed);
Plant growMat(const Traits &traits, uint32_t seed);

} // namespace flora
