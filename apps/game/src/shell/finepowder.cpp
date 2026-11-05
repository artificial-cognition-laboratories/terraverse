// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "finepowder.h"
#include <algorithm>
#include <cmath>

namespace shell {

static constexpr double FULL = 0.9;   // a block this full carries what's over it; less, and that's still falling
static constexpr float SETTLES = 24;  // sixteenths of a block a second that land or leave
static constexpr float REPOSE = 0.7f; // sixteenths of height a column: about 35°
static constexpr float DRAINS = 2 * 4096; // sixteenths³ (2/16 of a block): how far the picture runs ahead of the matter at a hole
static constexpr float STIRRED = 0.01f;
static constexpr int SLIDES = 6;      // passes a frame: a slide runs a column a pass
static constexpr int LANDS_ON = 2;    // columns round the top it lands on

void FinePowder::sync(int32_t x, int32_t y, int32_t z, const std::vector<sandbox::Stage::Cell> &cells, const std::vector<sandbox::StageBlock> &blocks)
{
	if (x != m_x || y != m_y || z != m_z) {
		m_x = x;
		m_y = y;
		m_z = z;
		m_layers.clear();
		m_wet.assign((size_t)x * z, false);
	}
	m_closed.assign(cells.size(), false);
	m_falling.clear();
	for (size_t i = 0; i < cells.size(); i++) {
		const sandbox::StageBlock *b = cells[i].block >= 0 && (size_t)cells[i].block < blocks.size() ? &blocks[cells[i].block] : nullptr;
		m_closed[i] = b && b->spec.matter != "air" && b->spec.state != matter::State::Liquid && !isPowder(*b);
		if (b && b->spec.vessel)
			m_closed[i] = true; // what's in a crucible is the crucible's to draw, within its walls
	}
	const auto closed = [&](int nx, int ny, int nz) { return (bool)m_closed[((size_t)ny * z + nz) * x + nx]; };
	const auto layerAt = [&](int32_t base) -> Layer & {
		Layer &l = m_layers[base];
		if (l.height.empty()) {
			l.height.assign((size_t)x * z * 256, 0);
			l.want.assign((size_t)x * z, 0);
			l.has.assign((size_t)x * z, 0);
			l.open.assign((size_t)x * z, false);
			l.hole.assign((size_t)x * z, false);
		}
		return l;
	};
	for (auto &[base, l] : m_layers)
		std::fill(l.want.begin(), l.want.end(), 0);
	for (int nz = 0; nz < z; nz++)
		for (int nx = 0; nx < x; nx++) {
			const size_t node = (size_t)nz * x + nx;
			int32_t base = 0;
			bool carried = true; // what's here so far rests on something
			float rests = 0;
			for (int ny = 0; ny <= y; ny++) {
				if (ny == y || closed(nx, ny, nz)) {
					if (rests > 0)
						layerAt(base).want[node] = rests;
					base = (ny + 1) * 16;
					carried = true;
					rests = 0;
					continue;
				}
				const sandbox::Stage::Cell &c = cells[((size_t)ny * z + nz) * x + nx];
				if ((c.fill.substance >= 0 && c.fill.share > 0.005) || (c.main >= 0 && c.state == matter::State::Liquid && c.filled > 0.005))
					m_wet[node] = true;
				const bool block = c.block >= 0 && (size_t)c.block < blocks.size() && isPowder(blocks[c.block]);
				const double full = block ? c.level : c.filled;
				if (c.main < 0 || c.state != matter::State::Solid || full <= 0.005) {
					carried = false;
					continue;
				}
				if (!carried) {
					m_falling.push_back({ { nx, ny, nz }, c.main });
					continue;
				}
				rests += (float)std::min(1.0, full) * 16;
				Layer &l = layerAt(base);
				l.substance = c.main;
				l.temperature = c.temperature;
				carried = full >= FULL;
			}
		}
	// where each floor has room
	for (auto &[base, l] : m_layers) {
		const int ny = base / 16;
		for (int nz = 0; nz < z; nz++)
			for (int nx = 0; nx < x; nx++)
			{
				l.open[(size_t)nz * x + nx] = ny < y && !closed(nx, ny, nz) && (ny == 0 || closed(nx, ny - 1, nz));
				l.hole[(size_t)nz * x + nx] = ny < y && ny > 0 && !closed(nx, ny, nz) && !closed(nx, ny - 1, nz);
			}
	}
}

// What's landed in a block goes on the heap's top, if there's a heap there or beside it; it slides from there.
void FinePowder::land(Layer &l, int nx, int nz, float sixteenths)
{
	const int w = m_x * 16, d = m_z * 16;
	const float volume = sixteenths * 256;
	if (m_wet[(size_t)nz * m_x + nx]) {
		// frozen where it lay: a sheet
		for (int cz = nz * 16; cz < (nz + 1) * 16; cz++)
			for (int cx = nx * 16; cx < (nx + 1) * 16; cx++)
				l.height[(size_t)cz * w + cx] += sixteenths;
		return;
	}
	int best = -1;
	float highest = 0;
	for (int cz = std::max(0, (nz - 1) * 16); cz < std::min(d, (nz + 2) * 16); cz++)
		for (int cx = std::max(0, (nx - 1) * 16); cx < std::min(w, (nx + 2) * 16); cx++) {
			const float h = l.height[(size_t)cz * w + cx];
			if (h > highest) {
				highest = h;
				best = cz * w + cx;
			}
		}
	const int cx = best >= 0 ? best % w : nx * 16 + 8, cz = best >= 0 ? best / w : nz * 16 + 8;
	int spots = 0;
	for (int k = 0; k < 2; k++)
		for (int az = cz - LANDS_ON; az <= cz + LANDS_ON; az++)
			for (int ax = cx - LANDS_ON; ax <= cx + LANDS_ON; ax++) {
				if (ax < 0 || az < 0 || ax >= w || az >= d || !l.open[(size_t)(az / 16) * m_x + ax / 16])
					continue;
				if (k == 0)
					spots++;
				else
					l.height[(size_t)az * w + ax] += volume / spots;
			}
}

// What's left a block comes off the top of what's there; if it's already slid away, off the rest.
void FinePowder::lift(Layer &l, int nx, int nz, float sixteenths)
{
	const int w = m_x * 16;
	float volume = sixteenths * 256, there = 0;
	// (what the picture's already let down a hole)
	const float owed = std::min(volume, l.drained);
	l.drained -= owed;
	volume -= owed;
	for (int cz = nz * 16; cz < (nz + 1) * 16; cz++)
		for (int cx = nx * 16; cx < (nx + 1) * 16; cx++)
			there += l.height[(size_t)cz * w + cx];
	const float here = std::min(volume, there);
	if (there > 0)
		for (int cz = nz * 16; cz < (nz + 1) * 16; cz++)
			for (int cx = nx * 16; cx < (nx + 1) * 16; cx++)
				l.height[(size_t)cz * w + cx] *= 1 - here / there;
	volume -= here;
	float all = 0;
	for (const float h : l.height)
		all += h;
	if (volume > 0 && all > 0)
		for (float &h : l.height)
			h *= 1 - std::min(1.f, volume / all);
}

// A pass of the slide: anything steeper than its angle moves down, each way a column has a neighbour,
// diagonals as well, so a heap comes out round and not a pyramid on the grid's axes.
bool FinePowder::slide(Layer &l) const
{
	const int w = m_x * 16, d = m_z * 16;
	bool moved = false;
	const auto open = [&](int cx, int cz) { return (bool)l.open[(size_t)(cz / 16) * m_x + cx / 16]; };
	int x0 = w, x1 = -1, z0 = d, z1 = -1;
	for (int cz = 0; cz < d; cz++)
		for (int cx = 0; cx < w; cx++)
			if (l.height[(size_t)cz * w + cx] > 0) {
				x0 = std::min(x0, cx), x1 = std::max(x1, cx), z0 = std::min(z0, cz), z1 = std::max(z1, cz);
			}
	static const int WAYS[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { -1, 1 } };
	for (int cz = std::max(0, z0 - 1); cz <= std::min(d - 1, z1 + 1); cz++)
		for (int cx = std::max(0, x0 - 1); cx <= std::min(w - 1, x1 + 1); cx++) {
			if (!open(cx, cz))
				continue;
			float &a = l.height[(size_t)cz * w + cx];
			// (each column's own say on its angle: grains aren't all alike, and it breaks up the grid's lines)
			const uint32_t grain = ((uint32_t)(cx * 73856093) ^ (uint32_t)(cz * 19349663)) >> 9;
			const float limit = REPOSE * (0.85f + (grain % 31) / 100.f);
			for (int s = 0; s < 4; s++) {
				const int ax = cx + WAYS[s][0], az = cz + WAYS[s][1];
				if (ax < 0 || az < 0 || ax >= w || az >= d || !open(ax, az))
					continue;
				float &b = l.height[(size_t)az * w + ax];
				const float gap = a - b, most = limit * (s < 2 ? 1 : 1.41f);
				if (std::fabs(gap) <= most)
					continue;
				const float move = std::copysign((std::fabs(gap) - most) * 0.2f, gap);
				a -= move;
				b += move;
				moved = moved || std::fabs(move) > STIRRED;
			}
		}
	// The edge of a hole has nothing to lean on: it runs in, as far ahead of the matter as DRAINS
	for (int cz = std::max(0, z0); cz <= std::min(d - 1, z1) && l.drained < DRAINS; cz++)
		for (int cx = std::max(0, x0); cx <= std::min(w - 1, x1); cx++) {
			float &a = l.height[(size_t)cz * w + cx];
			if (a <= REPOSE || !open(cx, cz))
				continue;
			bool edge = false;
			for (int dz = -1; dz <= 1 && !edge; dz++)
				for (int dx = -1; dx <= 1; dx++) {
					const int ax = cx + dx, az = cz + dz;
					if (ax >= 0 && az >= 0 && ax < w && az < d && l.hole[(size_t)(az / 16) * m_x + ax / 16])
						edge = true;
				}
			if (!edge)
				continue;
			const float move = (a - REPOSE) * 0.2f;
			a -= move;
			l.drained += move;
			moved = moved || move > STIRRED;
		}
	return moved;
}

bool FinePowder::step(float dt, float pace)
{
	if (dt <= 0)
		return false;
	bool changed = false;
	for (auto &[base, l] : m_layers) {
		for (int nz = 0; nz < m_z; nz++)
			for (int nx = 0; nx < m_x; nx++) {
				const size_t node = (size_t)nz * m_x + nx;
				const float most = SETTLES * dt * std::max(1.f, pace);
				const float by = std::clamp(l.want[node] - l.has[node], -most, most);
				if (by == 0)
					continue;
				if (by > 0)
					land(l, nx, nz, by);
				else
					lift(l, nx, nz, -by);
				l.has[node] += by;
				changed = true;
			}
		for (int k = 0; k < SLIDES; k++)
			if (slide(l))
				changed = true;
	}
	if (!changed)
		return false;
	m_columns.clear();
	const int w = m_x * 16, d = m_z * 16;
	for (const auto &[base, l] : m_layers)
		for (int cz = 0; cz < d; cz++)
			for (int cx = 0; cx < w; cx++) {
				const int32_t tall = (int32_t)std::lround(l.height[(size_t)cz * w + cx]);
				if (tall > 0)
					m_columns.push_back({ cx, cz, base, base + tall, l.substance, l.temperature });
			}
	return true;
}

} // namespace shell
