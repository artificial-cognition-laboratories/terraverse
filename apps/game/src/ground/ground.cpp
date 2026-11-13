// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ground.h"

#include "constants.h"

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

	// the clusters: from the lowest up, a new one where a surface is more
	// than a block above its cluster's lowest
	std::array<int, 4> tops;
	size_t n = 0;
	for (const Column &c : around)
		if (c.kind != Kind::None)
			tops[n++] = c.top;
	std::sort(tops.begin(), tops.begin() + n);
	int sum = 0, count = 0;
	for (size_t i = 0, start = 0; i < n; i++) {
		if (tops[i] - tops[start] > 1) {
			if (mine >= tops[start] && mine < tops[i])
				break;
			start = i;
			sum = count = 0;
		}
		sum += tops[i];
		count++;
	}
	// the nearest sixteenth of the cluster's average
	int height = int(std::lround(double(sum) * STEPS / count));

	// a face hidden against a full block stays hidden
	for (size_t i = 0; i < 4; i++) {
		const Column &c = around[i];
		if (i != own && (c.top > mine || (c.top == mine && c.kind == Kind::Solid)))
			height = std::max(height, mine * STEPS);
	}
	return height;
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
