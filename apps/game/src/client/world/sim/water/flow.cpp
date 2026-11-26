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
const float QUIET = 1 / 512.0f; // nodes: a disturbance smaller than this is none (a tile of only that sleeps)
const float STILL = 1e-5;       // node heights moved in a step: still
const float SLEEP = 1;          // s quiet before a near tile stops being run
const float SLEEP_FAR = 0.3;    // ... a far one (it's seen less)
const float NEAR = 10;          // nodes
const float WAKE = 4;           // per s: how fast what moves through water drives it at its own speed
const float HIGHEST = 1;        // nodes: the most a disturbance stands over its level
// per s, by Kind: how fast a disturbance dies back to its level (a river's slower than the sea's,
// so a wake's seen drifting downstream)
constexpr float DECAY[] = { /*None*/ 8, /*Sea*/ 1.2f, /*River*/ 0.5f, /*Lake*/ 0.8f, /*Fill*/ 1 };
float sample(const Grid &g, float x, float z, int c);
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
			if (!g.wet(c))
				continue;
			const float surface = g.surface(c) - 0.5f;
			if (surface < m.feet.Y - 0.1f || surface > m.feet.Y + m.height)
				continue;
			for (int s = 0; s < 4; s++) {
				const float ahead = std::max(0.0f, v.X * SIDES[s].X + v.Z * SIDES[s].Z) + std::max(0.0f, -v.Y) * 0.5f;
				// toward the flow it'd have at the mover's speed, never past it
				const float want = ahead * std::max(g.depth(c), 2 / 16.0f); // thin water's thrown too
				if (want > g.out[c][s])
					g.out[c][s] += (want - g.out[c][s]) * std::min(1.0f, WAKE * dtime);
			}
			g.wake(g.tileOf(c));
		}
}

// A step of the disturbance: shallow water by virtual pipes (Mei, Decaudin &
// Hu 2007) on how far each column is off its level, one pipe per edge between
// wet columns, so a river's slope at rest is still and a wake never climbs a
// bank; then carried by the current, and dying back to its level. Nothing
// else moves η: the server's water is the level, never pulled on here.
void Flow::step(float dt, v3f camera)
{
	Grid &g = m_grid;
	const float L = 1.0f / g.res;
	std::vector<int> awake;
	for (int t = 0; t < int(g.tile.size()); t++)
		if (g.tile[t].awake && g.tile[t].known)
			awake.push_back(t);
	for (int t : awake)
		pipes(t, dt, L);
	for (int t : awake)
		move(t, dt, L);
	carry(awake, dt);
	for (int t : awake)
		settle(t, dt, camera);
}

// Each column's flows out, from how much higher its disturbance stands than each neighbour's
void Flow::pipes(int t, float dt, float L)
{
	Grid &g = m_grid;
	const int tx = t % g.tiles, tz = t / g.tiles;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int gx = tx * 16 + cx, gz = tz * 16 + cz, c = gz * g.cols + gx;
			auto &out = g.out[c];
			if (!g.wet(c)) {
				out = {};
				continue;
			}
			const bool edge[4] = { gx + 1 >= g.cols, gx == 0, gz + 1 >= g.cols, gz == 0 };
			bool open[4];
			float higher[4] = {};
			for (int s = 0; s < 4; s++) {
				const int n = c + g.stepOf(s);
				open[s] = !edge[s] && g.wet(n);
				if (open[s])
					higher[s] = g.eta[c] - g.eta[n];
			}
			shallow::flowOut(out, g.depth(c), dt, L, open, higher);
		}
}

// The flows moved: what crosses into a sleeping tile wakes it
void Flow::move(int t, float dt, float L)
{
	Grid &g = m_grid;
	const int first = g.firstOf(t);
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			for (int s = 0; s < 4; s++) {
				const float o = g.out[c][s];
				if (o <= 0)
					continue;
				const int n = c + g.stepOf(s), nt = g.tileOf(n);
				const float amount = o * dt / L;
				if (nt != t && !g.tile[nt].awake) {
					// too little to wake a sleeping tile goes nowhere (left there, it'd never die)
					if (amount <= STILL)
						continue;
					g.wake(nt);
				}
				g.eta[c] -= amount;
				g.eta[n] += amount;
			}
		}
}

// The current carries the disturbance (semi-Lagrangian: each column takes
// what stood upstream of it a step ago), so a wake drifts downstream; what
// it carries over a tile's edge wakes the tile there
void Flow::carry(const std::vector<int> &awake, float dt)
{
	Grid &g = m_grid;
	m_carried.clear();
	for (int t : awake) {
		const int first = g.firstOf(t);
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int c = first + cz * g.cols + cx;
				const auto &v = g.vel[c];
				if ((v[0] == 0 && v[1] == 0) || !g.wet(c))
					continue;
				const float x = c % g.cols - v[0] * dt * g.res, z = c / g.cols - v[1] * dt * g.res;
				m_carried.push_back({ c, sample(g, x, z, c) });
			}
	}
	for (const auto &[c, eta] : m_carried) {
		g.eta[c] = eta;
		const int gx = c % g.cols, gz = c / g.cols;
		if (std::fabs(eta) <= QUIET || (gx % 16 && gx % 16 != 15 && gz % 16 && gz % 16 != 15))
			continue;
		const int t = g.tileOf(c);
		for (int s = 0; s < 4; s++) {
			const int nx = gx + SIDES[s].X, nz = gz + SIDES[s].Z;
			if (nx < 0 || nx >= g.cols || nz < 0 || nz >= g.cols)
				continue;
			const int n = nz * g.cols + nx, nt = g.tileOf(n);
			if (nt != t && !g.tile[nt].awake && g.wet(n))
				g.wake(nt);
		}
	}
}

// Each disturbance dies back toward its level, stays within its water, and a
// tile quiet long enough sleeps (sooner the further from the camera), at rest
void Flow::settle(int t, float dt, v3f camera)
{
	Grid &g = m_grid;
	Tile &k = g.tile[t];
	const int first = g.firstOf(t);
	float most = 0;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			float &eta = g.eta[c];
			if (!g.wet(c)) {
				eta = 0;
				continue;
			}
			eta *= std::max(0.0f, 1 - DECAY[int(g.kind[c])] * dt);
			eta = std::clamp(eta, -g.rest(c), HIGHEST);
			most = std::max(most, std::fabs(eta));
		}
	k.quiet = most < QUIET ? k.quiet + dt : 0;
	const int per = g.per();
	const float away = std::hypot(g.origin.X + (t % g.tiles + 0.5f) * per - camera.X,
			g.origin.Z + (t / g.tiles + 0.5f) * per - camera.Z);
	if (k.quiet <= (away > NEAR ? SLEEP_FAR : SLEEP))
		return;
	k.awake = false;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			g.eta[c] = 0;
			g.out[c] = {};
		}
}

namespace {

// The disturbance at (x, z) in columns, between the four columns round it;
// a dry one counts as column c's own (the bank neither adds nor takes)
float sample(const Grid &g, float x, float z, int c)
{
	const int x0 = int(std::floor(x)), z0 = int(std::floor(z));
	const float tx = x - x0, tz = z - z0;
	float sum = 0;
	for (int k = 0; k < 4; k++) {
		const int sx = x0 + (k & 1), sz = z0 + (k >> 1);
		const bool in = sx >= 0 && sx < g.cols && sz >= 0 && sz < g.cols;
		const int s = sz * g.cols + sx;
		const float eta = in && g.wet(s) ? g.eta[s] : g.eta[c];
		sum += ((k & 1) ? tx : 1 - tx) * ((k >> 1) ? tz : 1 - tz) * eta;
	}
	return sum;
}

} // namespace

} // namespace liquids
