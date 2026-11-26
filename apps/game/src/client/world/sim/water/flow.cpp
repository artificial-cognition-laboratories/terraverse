// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/flow.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include "src/client/world/sim/water/shallow.h"
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
const float STILL = 1e-5;       // node heights moved in a step: still
const float DEEP_STILL = 20;    // times STILL a deep column must move to wake the tile beside (calm deep water stays asleep)
const float SLEEP = 1;          // s still before a near tile stops being run
const float SLEEP_FAR = 0.3;    // ... a far one (it's seen less)
const float NEAR = 10;          // nodes
const float WAKE = 4;           // per s: how fast what moves through water drives it at its own speed
} // namespace

Flow::Flow(Opts o) : m_grid(o.grid) {}

float Flow::dt() const
{
	return m_grid.res >= 16 ? 1 / 100.0f : 1 / 50.0f;
}

int Flow::mostSteps() const
{
	return m_grid.res >= 16 ? 3 : 2;
}

void Flow::push(const std::vector<Mover> &movers, float dtime)
{
	std::unordered_map<u32, v3f> now;
	for (const Mover &m : movers) {
		now[m.id] = m.feet;
		auto last = m_movers.find(m.id);
		if (last == m_movers.end() || dtime <= 0)
			continue;
		const v3f v = (m.feet - last->second) / dtime;
		if (v.getLength() > 30 || v.getLength() < 0.05f)
			continue; // a jump across the world, or standing
		pushOne(m, v, dtime);
	}
	m_movers.swap(now);
}

void Flow::pushOne(const Mover &m, v3f v, float dtime)
{
	Grid &g = m_grid;
	const int reach = int(std::ceil(m.radius * g.res));
	const int at = g.columnAt(m.feet.X, m.feet.Z);
	if (at < 0)
		return;
	for (int dz = -reach; dz <= reach; dz++)
		for (int dx = -reach; dx <= reach; dx++) {
			const int cx = at % g.cols + dx, cz = at / g.cols + dz;
			if (cx < 0 || cx >= g.cols || cz < 0 || cz >= g.cols || (dx * dx + dz * dz) > reach * reach)
				continue;
			const int c = cz * g.cols + cx;
			if (g.floor[c] == NONE || g.depth[c] <= 0)
				continue;
			const float surface = g.floor[c] / 16.0f - 0.5f + g.depth[c];
			if (surface < m.feet.Y - 0.1f || surface > m.feet.Y + m.height)
				continue;
			for (int s = 0; s < 4; s++) {
				const float ahead = std::max(0.0f, v.X * SIDES[s].X + v.Z * SIDES[s].Z) + std::max(0.0f, -v.Y) * 0.5f;
				// toward the flow it'd have at the mover's speed, never past it
				const float want = ahead * std::max(g.depth[c], 2 / 16.0f); // thin water's thrown too
				if (want > g.out[c][s])
					g.out[c][s] += (want - g.out[c][s]) * std::min(1.0f, WAKE * dtime);
			}
			g.wake(g.tileOf(c));
		}
}

// A step of shallow water by virtual pipes, one per column edge (Mei,
// Decaudin & Hu 2007): a flow is sped by the difference in the surfaces
// either side and carried from step to step, so water has momentum (waves,
// sloshing), and no column gives more than it has. Waves travel as over a
// node's depth at most (the sea's deep, and slow is cheap).
void Flow::step(float dt, v3f camera)
{
	Grid &g = m_grid;
	const float L = 1.0f / g.res;
	std::vector<int> awake;
	for (int t = 0; t < int(g.tile.size()); t++)
		if (g.tile[t].awake && g.tile[t].known)
			awake.push_back(t);
	for (int t : awake) {
		const int tx = t % g.tiles, tz = t / g.tiles;
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int gx = tx * 16 + cx, gz = tz * 16 + cz, c = gz * g.cols + gx;
				auto &out = g.out[c];
				const float d = g.depth[c];
				if (g.floor[c] == NONE || d <= 0) {
					out = {};
					continue;
				}
				const bool edge[4] = { gx + 1 >= g.cols, gx == 0, gz + 1 >= g.cols, gz == 0 };
				bool open[4];
				float higher[4] = {};
				for (int s = 0; s < 4; s++) {
					const int n = c + g.stepOf(s);
					open[s] = !edge[s] && g.floor[n] != NONE;
					if (open[s])
						higher[s] = g.higher(c, n);
				}
				shallow::flowOut(out, d, dt, L, open, higher);
			}
	}
	for (int t : awake) {
		const int first = g.firstOf(t);
		float moved = 0;
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int c = first + cz * g.cols + cx;
				for (int s = 0; s < 4; s++) {
					const float o = g.out[c][s];
					if (o <= 0)
						continue;
					const int n = c + g.stepOf(s);
					const float amount = o * dt / L;
					g.depth[c] -= amount;
					g.depth[n] += amount;
					moved = std::max(moved, amount);
					// into another tile: it's woken (calm deep water by more), and takes this one's colour if it has none
					const int nt = g.tileOf(n);
					if (nt != t && amount > (g.depth[c] > 1 ? STILL * DEEP_STILL : STILL)) {
						Tile &there = g.tile[nt];
						if (!there.awake)
							g.wake(nt);
						if (!there.coloured) {
							there.r = g.tile[t].r, there.g = g.tile[t].g, there.b = g.tile[t].b;
							there.coloured = true;
						}
					}
				}
			}
		Tile &k = g.tile[t];
		k.quiet = moved < STILL ? k.quiet + dt : 0;
	}
	for (int t : awake) {
		Tile &k = g.tile[t];
		const int first = g.firstOf(t);
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				float &d = g.depth[first + cz * g.cols + cx];
				d = std::max(0.0f, d);
			}
		const int per = g.per();
		const float away = std::hypot(g.origin.X + (t % g.tiles + 0.5f) * per - camera.X,
				g.origin.Z + (t / g.tiles + 0.5f) * per - camera.Z);
		if (k.quiet > (away > NEAR ? SLEEP_FAR : SLEEP)) {
			k.awake = false;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++)
					g.out[first + cz * g.cols + cx] = {};
		}
	}
}

} // namespace liquids
