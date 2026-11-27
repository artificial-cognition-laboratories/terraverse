// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/ground/ground.h"

#include "src/shared/base/constants.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ground {

Kind Ground::kind(const MapNode &n) const
{
	const ContentFeatures &f = m_ndef.get(n);
	if (!f.walkable)
		return Kind::None;
	return f.loose ? Kind::Loose : Kind::Solid;
}

int corner(const std::array<Column, 4> &around, size_t own)
{
	const int mine = around[own].top;

	// natural ground all round: one field, the fine heights' plain average
	// (no higher than the cell over the column's top: where it would be, the
	// walls meet it; no lower than its loose blocks reach, which the top clamps)
	if (std::all_of(around.begin(), around.end(), [](const Column &c) { return c.natural(); })) {
		int sum = 0;
		for (const Column &c : around)
			sum += c.fine();
		return std::clamp(int(std::lround(sum / 4.0)), (mine - REACH) * STEPS, (mine + 1) * STEPS);
	}

	// the clusters: from the lowest up, a new one where a surface is more
	// than a block above its cluster's lowest (by their levels: a rise
	// moves a surface within its block, never across a cliff)
	std::array<int, 4> tops, fines;
	size_t n = 0;
	// loose ground never climbs what's solid (a block placed on it, a trunk, rock): only
	// a solid at or below its level joins the average; one above just keeps its face hidden
	for (const Column &c : around)
		if (c.kind == Kind::Loose || (c.kind == Kind::Solid && c.top <= mine)) {
			tops[n] = c.top;
			fines[n++] = c.kind == Kind::Loose ? c.fine() : c.top * STEPS;
		}
	// (by level, each its fine height alongside)
	std::array<size_t, 4> order{ 0, 1, 2, 3 };
	std::sort(order.begin(), order.begin() + n, [&](size_t a, size_t b) { return tops[a] < tops[b]; });
	int sum = 0, count = 0;
	for (size_t i = 0, start = 0; i < n; i++) {
		const int top = tops[order[i]], first = tops[order[start]];
		if (top - first > 1) {
			if (mine >= first && mine < top)
				break;
			start = i;
			sum = count = 0;
		}
		sum += fines[order[i]];
		count++;
	}
	// the nearest sixteenth of the cluster's average, within the two cells
	// the column's top may take (its own and the one above)
	int height = std::clamp(int(std::lround(double(sum) / count)), (mine - 1) * STEPS, (mine + 1) * STEPS);

	// a face hidden against a full block stays hidden
	for (size_t i = 0; i < 4; i++) {
		const Column &c = around[i];
		if (i != own && (c.top > mine || (c.top == mine && c.kind == Kind::Solid)))
			height = std::max(height, mine * STEPS);
	}
	return height;
}

bool Top::build(int ceiling)
{
	bool crossed = false;
	for (int x = 0; x < STEPS; x++)
		for (int z = 0; z < STEPS; z++) {
			const int raw = int(std::lround(field((x + 0.5f) / STEPS, (z + 0.5f) / STEPS)));
			cells[x][z] = u8(std::clamp(raw, 0, ceiling));
			// (at its base, the block under takes it, if the field may run there)
			surface[x][z] = raw <= ceiling && (open > 0 ? raw > 0 : raw >= 0);
			crossed |= raw <= ceiling;
		}
	return crossed;
}

std::optional<Top> Top::slice(int k) const
{
	Top s = *this;
	for (int &c : s.corners)
		c += k * STEPS;
	for (auto &side : s.sides)
		for (int &c : side)
			c += k * STEPS;
	s.below = below + k;
	s.open = open - k;
	if (!s.build(STEPS))
		return std::nullopt;
	return s;
}

float Top::field(float x, float z) const
{
	auto h = [&](int i) { return float(corners[i]); };
	// split from corner 0 (0, 0) to corner 3 (1, 1)
	if (x >= z)
		return h(0) + x * (h(1) - h(0)) + z * (h(3) - h(1));
	return h(0) + z * (h(2) - h(0)) + x * (h(3) - h(2));
}

float Top::height(float x, float z) const
{
	auto voxel = [](float v) { return std::clamp(int(v * STEPS), 0, STEPS - 1); };
	return cells[voxel(x)][voxel(z)] / float(STEPS);
}

int Top::beside(int s, int k) const
{
	const float f = (k + 0.5f) / STEPS;
	const float line = sides[s][0] + f * (sides[s][1] - sides[s][0]);
	return std::max(0, int(std::floor(line)) - EDGE);
}

void Top::boxes(std::vector<aabb3f> &out) const
{
	auto at = [](int v) { return (v / float(STEPS) - 0.5f) * BS; };
	// rectangles of one height: a run along x, grown along z while the
	// rows past it are the same height there (flat ground is one box, not
	// a row each: physics built each block of it from thousands)
	std::array<std::array<bool, STEPS>, STEPS> taken{}; // [x][z]
	for (int z = 0; z < STEPS; z++)
		for (int x = 0; x < STEPS;) {
			if (taken[x][z]) {
				x++;
				continue;
			}
			const int h = cells[x][z];
			int end = x + 1;
			while (end < STEPS && !taken[end][z] && cells[end][z] == h)
				end++;
			int far = z + 1;
			auto same = [&](int row) {
				for (int i = x; i < end; i++)
					if (taken[i][row] || cells[i][row] != h)
						return false;
				return true;
			};
			while (far < STEPS && same(far))
				far++;
			for (int i = x; i < end; i++)
				for (int j = z; j < far; j++)
					taken[i][j] = true;
			if (h > 0)
				out.emplace_back(at(x), at(0), at(z), at(end), at(h), at(far));
			x = end;
		}
}

shape::Shape Top::shape(int layer) const
{
	shape::Shape s;
	for (s16 x = 0; x < STEPS; x++)
		for (s16 z = 0; z < STEPS; z++) {
			const int h = std::clamp(int(cells[x][z]) - layer * STEPS, 0, STEPS);
			for (s16 y = 0; y < h; y++)
				s.set({ x, y, z }, true);
		}
	return s;
}

} // namespace ground
