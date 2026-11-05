// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "finewater.h"
#include "client/shallow.h"
#include "finepowder.h"
#include <algorithm>
#include <cmath>

namespace shell {

// As liquids::Layer's fine grid.
static constexpr float STEP = 1 / 100.0f, L = 1 / 16.0f, AREA = L * L;
static constexpr int MOST_STEPS = 3; // in a frame; past it the water runs slow rather than the frame
static constexpr float FILM = 1 / 32.0f, HOLD = 3, HOLD_MOST = 0.25f, SLACK = 1 / 128.0f, POOLS = 0.25f, SPLASH = 0.03f, STILL = 1e-5f, SLEEP = 1, SPILL = 1 / 128.0f;

void FineWater::sync(int32_t x, int32_t y, int32_t z, const std::vector<sandbox::Stage::Cell> &cells, const std::vector<sandbox::StageBlock> &blocks)
{
	if (x != m_nx || z != m_nz) {
		m_nx = x;
		m_nz = z;
		m_w = x * 16;
		m_d = z * 16;
		const size_t n = (size_t)m_w * m_d;
		m_floor.assign(n, 0);
		m_shown.assign(n, NONE);
		m_pool.assign(n, 0);
		m_depth.assign(n, 0);
		m_out.assign(n, {});
		m_over.assign(n, {});
		m_targets.assign((size_t)x * z, 0);
		m_colour.assign(n, 0x3f76e4);
		m_began = false;
	}
	std::vector<int32_t> floors((size_t)x * z, 0);
	std::vector<uint32_t> colours((size_t)x * z, 0);
	// (a powder on its way down isn't ground)
	const auto restsOn = [&](int nx, int ny, int nz) {
		if (ny == 0)
			return true;
		const sandbox::Stage::Cell &under = cells[((size_t)(ny - 1) * z + nz) * x + nx];
		const sandbox::StageBlock *b = under.block >= 0 ? &blocks[under.block] : nullptr;
		if (b && FinePowder::isPowder(*b))
			return under.level >= 0.9;
		return (b && b->spec.matter != "air" && b->spec.state != matter::State::Liquid) || (under.state == matter::State::Solid && under.main >= 0 && under.filled >= 0.9);
	};
	for (int nz = 0; nz < z; nz++)
		for (int nx = 0; nx < x; nx++) {
			const size_t node = (size_t)nz * x + nx;
			float water = 0;
			int32_t floor = 0;
			for (int ny = y - 1; ny >= 0; ny--) {
				const sandbox::Stage::Cell &c = cells[((size_t)ny * z + nz) * x + nx];
				const sandbox::StageBlock *b = c.block >= 0 ? &blocks[c.block] : nullptr;
				if (b && b->spec.vessel)
					continue; // what's in a crucible is the crucible's to draw
				if (c.fill.substance >= 0 && c.fill.share > 0.005) {
					water += (float)c.fill.share;
					colours[node] = c.fill_colour;
				}
				if (b && b->spec.state == matter::State::Liquid && b->spec.matter != "air") {
					water += b->spec.leveled ? (float)c.level : 1;
					colours[node] = b->spec.matter == "water" ? 0x3f76e4 : c.fill_colour ? c.fill_colour : colours[node];
					continue;
				}
				if (b && FinePowder::isPowder(*b) && !restsOn(nx, ny, nz))
					continue; // on its way down
				if (b && b->spec.matter != "air") {
					floor = (int32_t)std::lround((ny + (b->spec.leveled ? c.level : 1.0)) * 16);
					break;
				}
				// something loose lying in the air: as a heap, what the rest stands on
				if (c.main >= 0 && c.filled > 0.005 && c.state == matter::State::Solid && restsOn(nx, ny, nz)) {
					floor = (int32_t)std::lround((ny + std::min(1.0, c.filled)) * 16);
					break;
				}
			}
			floors[node] = floor;
			if (std::fabs(water - m_targets[node]) > 1e-4f)
				m_quiet = false;
			m_targets[node] = water;
		}
	for (int cz = 0; cz < m_d; cz++)
		for (int cx = 0; cx < m_w; cx++) {
			const int c = cz * m_w + cx;
			const int32_t f = floors[(size_t)(cz / 16) * x + cx / 16];
			if (const uint32_t colour = colours[(size_t)(cz / 16) * x + cx / 16])
				m_colour[c] = colour;
			if (f != m_floor[c]) {
				m_floor[c] = f;
				m_quiet = false;
			}
			if (!m_began)
				m_depth[c] = m_targets[(size_t)(cz / 16) * x + cx / 16];
		}
	m_began = true;
}

void FineWater::pools()
{
	std::fill(m_pool.begin(), m_pool.end(), 0);
	std::vector<int> open;
	int next = 0;
	for (int first = 0; first < m_w * m_d; first++) {
		if (m_pool[first] || m_depth[first] <= FILM)
			continue;
		m_pool[first] = ++next;
		open.push_back(first);
		while (!open.empty()) {
			const int c = open.back();
			open.pop_back();
			const int cx = c % m_w, cz = c / m_w;
			const int around[4] = { cx + 1 < m_w ? c + 1 : -1, cx > 0 ? c - 1 : -1, cz + 1 < m_d ? c + m_w : -1, cz > 0 ? c - m_w : -1 };
			for (const int n : around)
				if (n >= 0 && !m_pool[n] && m_depth[n] > FILM) {
					m_pool[n] = next;
					open.push_back(n);
				}
		}
	}
}

// A pool is held to what the matter has in the nodes it's in, and moves freely within it.
void FineWater::hold(float dt, float pace)
{
	// a film left where the matter has none dries: it's too thin to belong to a pool, so nothing else would take it
	for (int c = 0; c < m_w * m_d; c++)
		if (m_depth[c] > 0 && m_depth[c] <= FILM && m_targets[nodeOf(c)] <= 0) {
			m_depth[c] = std::max(0.0f, m_depth[c] - FILM * dt * pace);
			m_quiet = false;
		}
	const float rate = std::min(1.0f, HOLD * dt * pace);
	int count = 0;
	for (const int p : m_pool)
		count = std::max(count, p);
	std::vector<float> volume(count + 1, 0), want(count + 1, 0);
	std::vector<int> nodes(count + 1, 0);
	for (int c = 0; c < m_w * m_d; c++)
		if (m_pool[c])
			volume[m_pool[c]] += m_depth[c] * AREA;
	for (int nz = 0; nz < m_nz; nz++)
		for (int nx = 0; nx < m_nx; nx++) {
			const float target = m_targets[(size_t)nz * m_nx + nx];
			int pool = 0;
			for (int cz = nz * 16; cz < (nz + 1) * 16 && !pool; cz++)
				for (int cx = nx * 16; cx < (nx + 1) * 16 && !pool; cx++)
					pool = m_pool[cz * m_w + cx];
			if (pool) {
				want[pool] += target;
				nodes[pool]++;
			} else if (target > 0) {
				// nothing here yet: it arrives on the ground
				const float add = std::min(target, HOLD_MOST * dt * pace);
				for (int cz = nz * 16; cz < (nz + 1) * 16; cz++)
					for (int cx = nx * 16; cx < (nx + 1) * 16; cx++)
						m_depth[cz * m_w + cx] += add;
				m_quiet = false;
			}
		}
	for (int p = 1; p <= count; p++) {
		const float gap = want[p] - volume[p], slack = SLACK * nodes[p];
		if (std::fabs(gap) <= slack) {
			want[p] = 0;
			continue;
		}
		const float most = HOLD_MOST * dt * pace * std::max(1, nodes[p]);
		want[p] = std::clamp((gap - std::copysign(slack, gap)) * rate, -most, most); // what this pool gains now
		m_quiet = false;
	}
	std::vector<int> wet(count + 1, 0);
	for (int c = 0; c < m_w * m_d; c++)
		wet[m_pool[c]]++;
	for (int c = 0; c < m_w * m_d; c++) {
		const int p = m_pool[c];
		if (!p || want[p] == 0)
			continue;
		if (want[p] > 0)
			m_depth[c] += want[p] / AREA / wet[p];
		else if (volume[p] > 0)
			m_depth[c] *= std::max(0.0f, 1 + want[p] / volume[p]);
	}
}

void FineWater::flow(float dt)
{
	float moved = 0;
	for (int c = 0; c < m_w * m_d; c++) {
		const float d = m_depth[c];
		if (d <= 0) {
			m_out[c] = {};
			continue;
		}
		const int cx = c % m_w, cz = c / m_w;
		const int around[4] = { c + 1, c - 1, c + m_w, c - m_w };
		bool open[4] = { cx + 1 < m_w, cx > 0, cz + 1 < m_d, cz > 0 };
		// (never up onto higher ground the matter has no water on: a full basin stays in its walls)
		for (int s = 0; s < 4; s++)
			if (open[s] && m_floor[around[s]] > m_floor[c] && m_targets[nodeOf(around[s])] <= 0)
				open[s] = false;
		float higher[4] = {};
		for (int s = 0; s < 4; s++)
			if (open[s]) {
				const float drop = (m_floor[c] - m_floor[around[s]]) / 16.0f;
				higher[s] = drop + (d - m_depth[around[s]]);
				// over an edge with nothing up to it: only what's at the lip pushes, not the height of the fall
				if (drop > 0 && m_depth[around[s]] < drop)
					higher[s] = d;
			}
		liquids::shallow::flowOut(m_out[c], d, dt, L, open, higher);
	}
	const int STEPS[4] = { 1, -1, m_w, -m_w };
	for (int c = 0; c < m_w * m_d; c++)
		for (int s = 0; s < 4; s++) {
			const float o = m_out[c][s];
			if (o <= 0)
				continue;
			const float amount = o * dt / L;
			if (m_depth[c + STEPS[s]] <= 0)
				m_colour[c + STEPS[s]] = m_colour[c]; // ahead of the matter: it's still what it was
			if (m_floor[c] - m_floor[c + STEPS[s]] > 1 && m_depth[c + STEPS[s]] * 16 < m_floor[c] - m_floor[c + STEPS[s]]) {
				m_over[c][s] += amount;
				if (m_over[c][s] > SPILL) {
					m_over[c][s] = 0;
					if (m_spills.size() < 256)
						m_spills.push_back({ c, s });
				}
			}
			m_depth[c] -= amount;
			m_depth[c + STEPS[s]] += amount;
			moved = std::max(moved, amount);
		}
	for (float &d : m_depth)
		d = std::max(0.0f, d);
	m_still = moved < STILL ? m_still + dt : 0;
	if (m_still > SLEEP)
		m_quiet = true;
}

void FineWater::step(float dt, float pace)
{
	if (!m_began)
		return;
	pace = std::max(1.0f, pace);
	m_pools_in -= dt;
	if (m_pools_in <= 0) {
		m_pools_in = POOLS;
		pools();
	}
	hold(dt, pace);
	if (!m_quiet) {
		m_behind = std::min(m_behind + dt, STEP * MOST_STEPS);
		for (; m_behind >= STEP; m_behind -= STEP)
			flow(STEP);
		for (int c = 0; c < m_w * m_d; c++)
			m_shown[c] = liquids::shallow::topShown(m_shown[c], m_floor[c], m_depth[c], NONE);
	}
}

void FineWater::splash(float x, float z, float speed)
{
	const int cx = (int)(x * 16), cz = (int)(z * 16);
	if (cx < 0 || cz < 0 || cx >= m_w || cz >= m_d)
		return;
	for (float &o : m_out[cz * m_w + cx])
		o += SPLASH * speed;
	m_quiet = false;
	m_still = 0;
}

std::vector<FineWater::Spill> FineWater::takeSpills()
{
	std::vector<Spill> out;
	out.swap(m_spills);
	return out;
}

float FineWater::volume() const
{
	float v = 0;
	for (const float d : m_depth)
		v += d * AREA;
	return v;
}

float FineWater::target() const
{
	float v = 0;
	for (const float t : m_targets)
		v += t;
	return v;
}

} // namespace shell
