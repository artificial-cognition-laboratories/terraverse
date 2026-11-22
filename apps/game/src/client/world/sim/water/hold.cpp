// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/hold.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace liquids {

namespace {
const float POOLS = 0.25;       // s between finding the pools again
const float HOLD = 3;           // per s: how fast a pool's water closes on the server's amount
const float HOLD_MOST = 0.25;   // nodes³/s a tile's pool may be filled or drained by, at most
const float SLACK = 1 / 128.0f; // nodes³ a tile's water may be off before it's pulled back
const float SEA_HOLD = 4;       // per s: how fast the sea's columns close on its level
const float RIVER_HOLD = 0.8f;  // ... a river's or lake's on theirs: slower, so a wake's seen carried away
const float STILL = 1e-5;       // node heights moved in a step: still
const float CREDIT_FADE = 1;    // per s: drops' water fades as the server's amounts take over
const float CURRENT = 0.8f;     // nodes/s: how fast a river carries what's on it, till the server says
const float FILM = 1 / 32.0f;   // node heights: thinner isn't a pool's water to add to
} // namespace

Hold::Hold(Opts o) : m_grid(o.grid) {}

void Hold::frame(float dtime)
{
	m_pooled -= dtime;
	if (m_pooled <= 0) {
		m_pooled = POOLS;
		pools();
		currents();
	}
	hold(dtime);
}

// The pools: tiles with water joined side by side, each held to the server as one
void Hold::pools()
{
	Grid &g = m_grid;
	std::vector<int> pool(g.tile.size(), 0);
	int next = 0;
	auto wet = [&](int t) {
		const Tile &k = g.tile[t];
		return k.known && !k.sea && (k.target > 0 || k.volume > 1e-5f || k.credit > 0);
	};
	for (int t = 0; t < int(g.tile.size()); t++) {
		if (pool[t] || !wet(t))
			continue;
		const int id = ++next;
		std::vector<int> open{ t };
		pool[t] = id;
		while (!open.empty()) {
			const int q = open.back();
			open.pop_back();
			const int qx = q % g.tiles, qz = q / g.tiles;
			for (int n : { qx + 1 < g.tiles ? q + 1 : -1, qx > 0 ? q - 1 : -1, qz + 1 < g.tiles ? q + g.tiles : -1,
						 qz > 0 ? q - g.tiles : -1 })
				if (n >= 0 && !pool[n] && wet(n)) {
					pool[n] = id;
					open.push_back(n);
				}
		}
	}
	m_coast.assign(next + 1, 0);
	for (int t = 0; t < int(g.tile.size()); t++) {
		g.tile[t].pool = pool[t];
		const int tx = t % g.tiles, tz = t / g.tiles;
		for (int s = 0; s < 4 && pool[t]; s++) {
			const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
			if (nx >= 0 && nx < g.tiles && nz >= 0 && nz < g.tiles && g.tile[nz * g.tiles + nx].sea)
				m_coast[pool[t]] = 1;
		}
	}
}

// The rivers' currents: held water above the sea (a river's, sprint 115) is
// a staircase of levels, and runs toward its nearest step down: each column's
// way is down the distance to one, found across the grid. A run of one level
// longer than the grid sees has none, and a lake with no step lies still.
void Hold::currents()
{
	Grid &g = m_grid;
	const int n = g.cols * g.cols;
	if (int(g.current.size()) != n)
		return;
	auto river = [&](int c) { return g.sea[c] != NONE && g.sea[c] > g.sea_level; };
	auto held = [&](int c) { return g.sea[c] != NONE; };
	auto inside = [&](int x, int z) { return x >= 0 && x < g.cols && z >= 0 && z < g.cols; };
	std::vector<int> dist(n, -1), open;
	for (int c = 0; c < n; c++) {
		g.current[c] = {};
		if (!river(c))
			continue;
		const int x = c % g.cols, z = c / g.cols;
		for (int s = 0; s < 4; s++) {
			const int nx = x + SIDES[s].X, nz = z + SIDES[s].Z;
			if (!inside(nx, nz))
				continue;
			const int m = nz * g.cols + nx;
			if (held(m) && g.sea[m] < g.sea[c]) {
				dist[c] = 0;
				open.push_back(c);
				break;
			}
		}
	}
	// outward from the steps across each level's water
	for (size_t i = 0; i < open.size(); i++) {
		const int c = open[i], x = c % g.cols, z = c / g.cols;
		for (int s = 0; s < 4; s++) {
			const int nx = x + SIDES[s].X, nz = z + SIDES[s].Z;
			if (!inside(nx, nz))
				continue;
			const int m = nz * g.cols + nx;
			if (dist[m] < 0 && river(m) && g.sea[m] == g.sea[c]) {
				dist[m] = dist[c] + 1;
				open.push_back(m);
			}
		}
	}
	for (int c : open) {
		const int x = c % g.cols, z = c / g.cols;
		// how far on to a step beside it: a lower level's is past one (−1), what's
		// not this water as far as here
		auto on = [&](int nx, int nz) {
			if (!inside(nx, nz))
				return float(dist[c]);
			const int m = nz * g.cols + nx;
			if (held(m) && g.sea[m] < g.sea[c])
				return -1.0f;
			return dist[m] >= 0 && g.sea[m] == g.sea[c] ? float(dist[m]) : float(dist[c]);
		};
		const float vx = on(x - 1, z) - on(x + 1, z), vz = on(x, z - 1) - on(x, z + 1);
		const float len = std::hypot(vx, vz);
		// as fast as the server says its river runs; till it's said, CURRENT
		const int node = (z / g.res) * g.nodes + x / g.res;
		const float speed = g.runs.size() == size_t(g.nodes) * g.nodes && g.runs[node] > 0 ? g.runs[node] : CURRENT;
		if (len > 0)
			g.current[c] = { vx / len * speed, vz / len * speed };
	}
	// where the server's said which way its river runs (down its run), that
	// way: a long level reach has no step to run toward, and still flows
	if (g.ways.size() != size_t(g.nodes) * g.nodes)
		return;
	for (int c = 0; c < n; c++) {
		if (!river(c))
			continue;
		const int x = c % g.cols, z = c / g.cols, node = (z / g.res) * g.nodes + x / g.res;
		const auto &w = g.ways[node];
		if (w[0] == 0 && w[1] == 0)
			continue;
		const float speed = g.runs[node] > 0 ? g.runs[node] : CURRENT;
		g.current[c] = { w[0] * speed, w[1] * speed };
	}
}

void Hold::carry(float dt)
{
	Grid &g = m_grid;
	if (g.current.size() != g.depth.size())
		return;
	const float L = 1.0f / g.res;
	std::vector<std::pair<int, float>> moves;
	for (int t = 0; t < int(g.tile.size()); t++) {
		if (!g.tile[t].awake || !g.tile[t].known)
			continue;
		const int tx = t % g.tiles, tz = t / g.tiles;
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int gx = tx * 16 + cx, gz = tz * 16 + cz, c = gz * g.cols + gx;
				const auto &u = g.current[c];
				if ((u[0] == 0 && u[1] == 0) || g.floor[c] == NONE)
					continue;
				const float extra = g.off(c);
				for (int a = 0; a < 2; a++) {
					if (u[a] == 0)
						continue;
					const int s = a * 2 + (u[a] < 0);
					const int nx = gx + SIDES[s].X, nz = gz + SIDES[s].Z;
					if (nx < 0 || nx >= g.cols || nz < 0 || nz >= g.cols)
						continue;
					const int m = nz * g.cols + nx;
					if (!g.held(m)) // (into held water only: down a river's slope too, never onto its bank)
						continue;
					float amount = extra * std::min(0.25f, std::fabs(u[a]) * dt / L);
					amount = std::clamp(amount, -g.depth[m], g.depth[c]);
					moves.push_back({ c, -amount });
					moves.push_back({ m, amount });
				}
			}
	}
	for (const auto &[c, by] : moves) {
		g.depth[c] = std::max(0.0f, g.depth[c] + by);
		const int t = g.tileOf(c);
		if (!g.tile[t].awake && std::fabs(by) > STILL)
			g.wake(t);
	}
}

// The server's amounts. A pool that isn't the sea closes on the server's
// amount for it (what its tiles hold, and drops landed it hasn't sent back
// yet): water moves freely within it, and what's added goes in where its
// water is, evenly, so it spreads as it would.
void Hold::hold(float dtime)
{
	Grid &g = m_grid;
	const float rate = std::min(1.0f, HOLD * dtime);
	const float area = 1.0f / (g.res * g.res);
	struct Sum
	{
		float target = 0, volume = 0;
		int tiles = 0;
	};
	std::unordered_map<int, Sum> sums;
	for (int t = 0; t < int(g.tile.size()); t++) {
		Tile &k = g.tile[t];
		if (!k.known)
			continue;
		if (k.sea && k.awake)
			holdSea(t, std::min(1.0f, SEA_HOLD * dtime), std::min(1.0f, RIVER_HOLD * dtime));
		k.credit *= std::max(0.0f, 1 - CREDIT_FADE * dtime);
		// beside the sea, water under its level is the sea's (it ran in from it): only what's above is held
		const bool coast = k.pool && k.pool < int(m_coast.size()) && m_coast[k.pool] && g.sea_level != INT32_MIN;
		if (k.awake || k.volume > 0 || k.target > 0) {
			const int first = g.firstOf(t);
			float volume = 0;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int c = first + cz * g.cols + cx;
					volume += coast ? std::max(0.0f, g.depth[c] - under(c)) : g.depth[c];
				}
			k.volume = volume * area;
		}
		if (!k.pool)
			continue;
		Sum &sum = sums[k.pool];
		sum.target += (coast ? k.above : k.target) + k.credit;
		sum.volume += k.volume;
		sum.tiles++;
	}
	for (int t = 0; t < int(g.tile.size()); t++) {
		Tile &k = g.tile[t];
		if (!k.known || !k.pool)
			continue;
		const Sum &sum = sums.at(k.pool);
		const float gap = sum.target - sum.volume, slack = SLACK * sum.tiles;
		if (std::fabs(gap) <= slack)
			continue;
		// never fast: a pool short by a lake fills over seconds, not a frame
		const float by = std::clamp((gap - std::copysign(slack, gap)) * rate, -HOLD_MOST * dtime * sum.tiles,
				HOLD_MOST * dtime * sum.tiles);
		if (by > 0)
			add(t, by * (sum.volume > 1e-6f ? k.volume / sum.volume : 1.0f / sum.tiles));
		else if (sum.volume > 0)
			drain(t, std::max(0.0f, (sum.volume + by) / sum.volume),
					k.pool < int(m_coast.size()) && m_coast[k.pool] && g.sea_level != INT32_MIN);
	}
}

// A column's depth under the sea's level, the sea's: only where the server
// has water (sprint 84: a rut dug under the sea's level on a beach isn't the
// sea's; held as the sea's, it flooded and never drained). Beside the sea only.
float Hold::under(int c) const
{
	const Grid &g = m_grid;
	return g.want[c] == NONE ? 0.0f : std::max(0.0f, (g.sea_level - g.floor[c]) / 16.0f);
}

// The sea's tile: its columns at its level, up or down; the land in it drains
// what washed over it above the sea's level (and the server's water there),
// and under it, it's the sea's (it ran in from it)
void Hold::holdSea(int t, float rate, float river_rate)
{
	Grid &g = m_grid;
	Tile &k = g.tile[t];
	const int first = g.firstOf(t);
	float changed = 0;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			if (g.floor[c] == NONE)
				continue;
			float by;
			if (g.sea[c] != NONE) {
				by = -g.off(c) * (g.sea[c] > g.sea_level ? river_rate : rate);
			} else {
				const s32 cap = g.want[c] == NONE ? g.floor[c] : std::max(g.sea_level, g.want[c]);
				const float most = std::max(0.0f, (cap - g.floor[c]) / 16.0f);
				by = g.depth[c] > most ? (most - g.depth[c]) * rate : 0;
			}
			g.depth[c] += by;
			changed = std::max(changed, std::fabs(by));
		}
	if (changed > STILL)
		k.quiet = 0;
}

// Into a tile's water; dry, into its lowest ground. Woken only by what it changed.
void Hold::add(int t, float add)
{
	Grid &g = m_grid;
	if (add <= 0)
		return;
	const float area = 1.0f / (g.res * g.res);
	const int first = g.firstOf(t);
	int wet = 0;
	s32 lowest = INT_MAX;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			if (g.floor[c] == NONE)
				continue;
			wet += g.depth[c] > FILM;
			lowest = std::min(lowest, g.floor[c]);
		}
	if (lowest == INT_MAX)
		return;
	int into = wet;
	if (!wet)
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++)
				into += g.floor[first + cz * g.cols + cx] == lowest;
	const float each = add / area / into;
	float changed = 0;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			if (g.floor[c] != NONE && (wet ? g.depth[c] > FILM : g.floor[c] == lowest)) {
				g.depth[c] += each;
				changed += each;
			}
		}
	if (changed > STILL)
		g.wake(t);
}

// A tile's water kept by `keep`; beside the sea, only what's above its level
// drains (the rest is the sea's). Woken only by what it changed.
void Hold::drain(int t, float keep, bool coast)
{
	Grid &g = m_grid;
	const float area = 1.0f / (g.res * g.res);
	const int first = g.firstOf(t);
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			float &d = g.depth[c];
			const float below = coast ? std::min(d, under(c)) : 0;
			d = below + (d - below) * keep;
		}
	if (g.tile[t].volume * (1 - keep) / area > STILL)
		g.wake(t);
}

} // namespace liquids
