// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "finepowder.h"
#include <algorithm>
#include <cmath>

namespace shell {

static constexpr double FULL = 0.9;   // a block this full carries what's over it; less, and that's still falling
static constexpr float SETTLES = 24;  // sixteenths a second the surface moves

void FinePowder::sync(int32_t x, int32_t y, int32_t z, const std::vector<sandbox::Stage::Cell> &cells, const std::vector<sandbox::StageBlock> &blocks)
{
	if (x != m_x || y != m_y || z != m_z) {
		m_x = x;
		m_y = y;
		m_z = z;
		m_spans.assign((size_t)x * z, {});
	}
	m_closed.assign(cells.size(), false);
	m_falling.clear();
	for (size_t i = 0; i < cells.size(); i++) {
		const sandbox::StageBlock *b = cells[i].block >= 0 && (size_t)cells[i].block < blocks.size() ? &blocks[cells[i].block] : nullptr;
		m_closed[i] = b && b->spec.matter != "air" && b->spec.state != matter::State::Liquid && !isPowder(*b);
	}
	for (int nz = 0; nz < z; nz++)
		for (int nx = 0; nx < x; nx++) {
			auto &spans = m_spans[(size_t)nz * x + nx];
			for (auto &[base, span] : spans)
				span.target = 0;
			int32_t base = 0;
			bool carried = true; // what's here so far rests on something
			float rests = 0;
			for (int ny = 0; ny <= y; ny++) {
				const bool shut = ny == y || closed(nx, ny, nz);
				if (shut) {
					if (rests > 0) {
						Span &s = spans[base];
						s.target = rests;
					}
					base = (ny + 1) * 16;
					carried = true;
					rests = 0;
					continue;
				}
				const sandbox::Stage::Cell &c = cells[((size_t)ny * z + nz) * x + nx];
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
				Span &s = spans[base];
				s.substance = c.main;
				s.temperature = c.temperature;
				carried = full >= FULL;
			}
		}
}

// The top of the stack beside a surface at `level` (sixteenths). A wall, or the stage's edge: none, it stays level.
float FinePowder::surface(int nx, int nz, float level) const
{
	if (nx < 0 || nz < 0 || nx >= m_x || nz >= m_z)
		return level;
	int ny = std::clamp((int)std::floor((level - 0.01f) / 16), 0, m_y - 1);
	if (closed(nx, ny, nz))
		return level;
	while (ny > 0 && !closed(nx, ny - 1, nz))
		ny--;
	const int32_t base = ny * 16;
	const auto &spans = m_spans[(size_t)nz * m_x + nx];
	const auto found = spans.find(base);
	return found == spans.end() ? (float)base : base + found->second.shown;
}

bool FinePowder::step(float dt, float pace)
{
	bool changed = false;
	for (auto &spans : m_spans)
		for (auto it = spans.begin(); it != spans.end();) {
			Span &s = it->second;
			const float gap = s.target - s.shown, most = SETTLES * dt * std::max(1.f, pace);
			if (gap != 0) {
				s.shown += std::clamp(gap, -most, most);
				changed = true;
			}
			if (s.target <= 0 && s.shown <= 0)
				it = spans.erase(it);
			else
				++it;
		}
	if (!changed)
		return false;
	m_columns.clear();
	for (int nz = 0; nz < m_z; nz++)
		for (int nx = 0; nx < m_x; nx++)
			for (const auto &[base, s] : m_spans[(size_t)nz * m_x + nx]) {
				if (s.substance < 0)
					continue;
				const float own = base + s.shown;
				for (int cz = 0; cz < 16; cz++)
					for (int cx = 0; cx < 16; cx++) {
						// a slope between block centres: towards whichever neighbours this column's nearest
						const float fx = (cx + 0.5f) / 16 - 0.5f, fz = (cz + 0.5f) / 16 - 0.5f;
						const int sx = fx < 0 ? -1 : 1, sz = fz < 0 ? -1 : 1;
						const float ax = std::fabs(fx), az = std::fabs(fz);
						const float top = own * (1 - ax) * (1 - az) + surface(nx + sx, nz, own) * ax * (1 - az)
								+ surface(nx, nz + sz, own) * (1 - ax) * az + surface(nx + sx, nz + sz, own) * ax * az;
						const int32_t height = (int32_t)std::lround(top);
						if (height > base)
							m_columns.push_back({ nx * 16 + cx, nz * 16 + cz, base, height, s.substance, s.temperature });
					}
			}
	return true;
}

} // namespace shell
