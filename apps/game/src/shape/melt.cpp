// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "shape.h"
#include <algorithm>
#include <cmath>
#include <vector>

// (apart from the rest of Shape: the sandbox is built without the engine)
namespace shape {

Shape melted(const Shape *now, int cells, unsigned exposed, bool melting)
{
	Shape out;
	if (now)
		out = *now;
	else
		for (s16 y = 0; y < STEPS; y++)
			for (s16 z = 0; z < STEPS; z++)
				for (s16 x = 0; x < STEPS; x++)
					out.set(v3s16(x, y, z), true);
	cells = std::clamp(cells, 1, CELLS);
	const int have = (int)out.count();
	if (!exposed || (melting ? have <= cells : have >= cells))
		return out;
	struct Scored { float depth; v3s16 at; };
	std::vector<Scored> cands;
	for (s16 y = 0; y < STEPS; y++)
		for (s16 z = 0; z < STEPS; z++)
			for (s16 x = 0; x < STEPS; x++) {
				if (out.has(v3s16(x, y, z)) != melting)
					continue;
				const float away[6] = { STEPS - x - 0.5f, x + 0.5f, STEPS - y - 0.5f, y + 0.5f, STEPS - z - 0.5f, z + 0.5f };
				float nearness = 0;
				for (int f = 0; f < 6; f++)
					if (exposed & (1u << f))
						nearness += 1 / (away[f] * away[f]);
				// (a little by where it is: ice doesn't go in perfectly even shells)
				const u32 hash = ((u32)x * 73856093u) ^ ((u32)y * 19349663u) ^ ((u32)z * 83492791u);
				cands.push_back({ 1 / std::sqrt(nearness) + (hash % 256) / 256.f * 0.6f, v3s16(x, y, z) });
			}
	const size_t change = (size_t)std::abs(have - cells);
	std::nth_element(cands.begin(), cands.begin() + change - 1, cands.end(), [&](const Scored &a, const Scored &b) {
		return melting ? a.depth < b.depth : a.depth > b.depth;
	});
	for (size_t i = 0; i < change; i++)
		out.set(cands[i].at, !melting);
	if (!melting)
		return out;
	// only its biggest piece stays
	std::vector<int> piece(CELLS, 0), sizes(1, 0);
	const auto index = [](v3s16 c) { return (c.Y * STEPS + c.Z) * STEPS + c.X; };
	static const v3s16 NEAR[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	std::vector<v3s16> todo;
	for (s16 y = 0; y < STEPS; y++)
		for (s16 z = 0; z < STEPS; z++)
			for (s16 x = 0; x < STEPS; x++) {
				const v3s16 first(x, y, z);
				if (!out.has(first) || piece[index(first)])
					continue;
				sizes.push_back(0);
				piece[index(first)] = (int)sizes.size() - 1;
				todo.push_back(first);
				while (!todo.empty()) {
					const v3s16 c = todo.back();
					todo.pop_back();
					sizes.back()++;
					for (const v3s16 &d : NEAR) {
						const v3s16 n = c + d;
						if (Shape::inside(n) && out.has(n) && !piece[index(n)]) {
							piece[index(n)] = (int)sizes.size() - 1;
							todo.push_back(n);
						}
					}
				}
			}
	const int biggest = (int)(std::max_element(sizes.begin(), sizes.end()) - sizes.begin());
	for (s16 y = 0; y < STEPS; y++)
		for (s16 z = 0; z < STEPS; z++)
			for (s16 x = 0; x < STEPS; x++)
				if (out.has(v3s16(x, y, z)) && piece[index(v3s16(x, y, z))] != biggest)
					out.set(v3s16(x, y, z), false);
	return out;
}

} // namespace shape
