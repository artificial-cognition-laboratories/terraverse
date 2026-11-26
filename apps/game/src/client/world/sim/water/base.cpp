// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/base.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
const float EASE = 1.5f;      // per s: how fast a level closes on its target
const float EASE_LEAST = 2;   // sixteenths/s it moves at least (the last of it isn't a crawl)
const float SETTLED = 0.01f;  // sixteenths: at its target
float flowAt(const Grid &g, int c, int axis);
} // namespace

Base::Base(Opts o) : m_grid(o.grid) {}

void Base::retarget()
{
	for (int t = 0; t < int(m_grid.tile.size()); t++)
		if (m_grid.tile[t].retarget)
			retargetTile(t);
}

// Each column stands between its node's four corners (each the mean of the
// levels of the nodes there in its cluster: held::corners), so neighbours
// meet and a river ramps a sixteenth at a time; a fall past a block stays a step
void Base::retargetTile(int t)
{
	Grid &g = m_grid;
	Tile &k = g.tile[t];
	k.retarget = false;
	if (!k.known)
		return;
	m_retargeted++;
	const int tx = t % g.tiles, tz = t / g.tiles, p = g.per();
	const auto targetOf = [&](int nx, int nz) {
		return nx < 0 || nx >= g.nodes || nz < 0 || nz >= g.nodes ? NONE : g.targets[nz * g.nodes + nx];
	};
	for (int nz = 0; nz < p; nz++)
		for (int nx = 0; nx < p; nx++) {
			const int gx = tx * p + nx, gz = tz * p + nz;
			const s32 own = targetOf(gx, gz);
			std::array<float, 4> corners{};
			if (own != NONE) {
				held::Around around;
				for (int dz = -1; dz <= 1; dz++)
					for (int dx = -1; dx <= 1; dx++)
						around[dz + 1][dx + 1] = targetOf(gx + dx, gz + dz);
				corners = held::corners(around);
			}
			const Kind kind = g.kinds[gz * g.nodes + gx];
			for (int iz = 0; iz < g.res; iz++)
				for (int ix = 0; ix < g.res; ix++) {
					const int c = (tz * 16 + nz * g.res + iz) * g.cols + tx * 16 + nx * g.res + ix;
					s32 to = own == NONE || g.floor[c] == NONE ? NONE
							: s32(std::lround(held::at(corners, (ix + 0.5f) / g.res, (iz + 0.5f) / g.res)));
					if (to != NONE && to <= g.floor[c])
						to = NONE; // its ground's over where the water stands: dry
					g.target[c] = to;
					g.kind[c] = to == NONE ? Kind::None : kind;
					g.vel[c] = { flowAt(g, c, 0), flowAt(g, c, 1) };
					if (k.snap && g.floor[c] != NONE)
						g.level[c] = to == NONE ? float(g.floor[c]) : float(to);
				}
		}
	k.easing = !k.snap;
	k.snap = false;
	k.drawn = false;
}

void Base::ease(float dtime)
{
	Grid &g = m_grid;
	const float rate = std::min(1.0f, EASE * dtime), least = EASE_LEAST * dtime;
	for (int t = 0; t < int(g.tile.size()); t++) {
		Tile &k = g.tile[t];
		if (!k.easing || !k.known)
			continue;
		bool easing = false;
		const int first = g.firstOf(t);
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int c = first + cz * g.cols + cx;
				if (g.floor[c] == NONE)
					continue;
				float &level = g.level[c];
				const float goal = g.target[c] == NONE ? float(g.floor[c]) : float(g.target[c]);
				const float gap = goal - level;
				if (std::fabs(gap) <= SETTLED) {
					level = goal;
					continue;
				}
				level += std::copysign(std::min(std::fabs(gap), std::max(std::fabs(gap) * rate, least)), gap);
				easing = true;
			}
		k.easing = easing;
	}
}

namespace {

// Its river's current at a column, between its nodes' middles: slowing to
// nothing toward a bank, and never a step at a node's edge
float flowAt(const Grid &g, int c, int axis)
{
	const float fx = ((c % g.cols) + 0.5f) / g.res - 0.5f, fz = ((c / g.cols) + 0.5f) / g.res - 0.5f;
	const int x0 = int(std::floor(fx)), z0 = int(std::floor(fz));
	const float tx = fx - x0, tz = fz - z0;
	float sum = 0;
	for (int k = 0; k < 4; k++) {
		const int x = std::clamp(x0 + (k & 1), 0, g.nodes - 1), z = std::clamp(z0 + (k >> 1), 0, g.nodes - 1);
		sum += ((k & 1) ? tx : 1 - tx) * ((k >> 1) ? tz : 1 - tz) * g.flows[z * g.nodes + x][axis];
	}
	return sum;
}

} // namespace

} // namespace liquids
