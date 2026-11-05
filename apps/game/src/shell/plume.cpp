// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "plume.h"
#include <algorithm>
#include <cmath>

namespace shell {

static constexpr float PUFFS = 10;       // in a block of nothing but a gas you can see
static constexpr size_t MOST_GAS = 2500, MOST_FLAMES = 1500;
static constexpr float CLEAR = 0.3f;     // how much of a colourless gas shows
static constexpr float FLAME_WATTS = 2000; // a block's flame is at its fullest
static constexpr float CLEARANCE = 0.3f; // a flame's biggest cube, half across
static constexpr u32 STEAM = 0xe8eef2;

float Plume::Puff::size() const
{
	const float t = age / life;
	return burning ? start * (1 - 0.75f * t) : start * (1 + 3 * t);
}

video::SColor Plume::Puff::colour() const
{
	const float t = std::clamp(age / life, 0.f, 1.f);
	const float r = (tint >> 16) & 255, g = (tint >> 8) & 255, b = tint & 255;
	if (!burning) {
		const float in = std::min(1.f, t * 6), out = std::min(1.f, (1 - t) * 2.5f);
		return video::SColor((u32)(255 * opacity * in * out), (u32)r, (u32)g, (u32)b);
	}
	// white at the root, its own colour, then a dull tip
	const float hot = std::max(0.f, 1 - t * 4), dull = std::max(0.f, t - 0.5f) * 2;
	const bool warm = r > b;
	const auto at = [&](float own, float keep) {
		return (u32)std::clamp((own + (255 - own) * hot * 0.8f) * (1 - dull * (1 - keep)), 0.f, 255.f);
	};
	const u32 alpha = (u32)(235 * std::min(1.f, (1 - t) * 2));
	return video::SColor(alpha, at(r, warm ? 0.85f : 0.55f), at(g, warm ? 0.4f : 0.55f), at(b, warm ? 0.3f : 0.7f));
}

float Plume::random()
{
	m_seed = m_seed * 1664525u + 1013904223u;
	return (m_seed >> 8) / 16777216.0f;
}

int Plume::cellAt(v3f at) const
{
	const int x = (int)std::floor(at.X), y = (int)std::floor(at.Y), z = (int)std::floor(at.Z);
	if (x < 0 || y < 0 || z < 0 || x >= m_x || y >= m_y || z >= m_z)
		return -1;
	return ((y * m_z + z) * m_x) + x;
}

void Plume::sync(int32_t x, int32_t y, int32_t z, const std::vector<sandbox::Stage::Cell> &cells, const std::vector<sandbox::StageBlock> &blocks,
		const std::vector<video::SColor> &substances, const std::vector<bool> &water)
{
	m_x = x;
	m_y = y;
	m_z = z;
	m_gas.clear();
	m_flames.clear();
	m_closed.assign(cells.size(), false);
	m_walled.assign(cells.size(), false);
	m_rise.assign(cells.size(), 0);
	m_wanted.assign(cells.size(), 0);
	size_t i = 0;
	for (int ny = 0; ny < y; ny++)
		for (int nz = 0; nz < z; nz++)
			for (int nx = 0; nx < x; nx++, i++) {
				const sandbox::Stage::Cell &c = cells[i];
				const sandbox::StageBlock *b = c.block >= 0 && (size_t)c.block < blocks.size() ? &blocks[c.block] : nullptr;
				const bool air = !b || b->spec.matter == "air";
				const bool vessel = b && b->spec.vessel;
				m_closed[i] = !air && !vessel && b->spec.state != matter::State::Liquid && b->boxes.empty();
				m_walled[i] = m_closed[i] || vessel || (b && !b->boxes.empty());
				const v3f corner((float)nx, (float)ny, (float)nz);
				const float warmth = std::clamp((float)(c.temperature - 293) / 900, 0.f, 1.f);
				m_rise[i] = 0.1f + 0.6f * warmth;
				if (c.vapour >= 0 && c.haze > 0.02 && (size_t)c.vapour < substances.size() && !m_closed[i]) {
					const video::SColor own = substances[c.vapour];
					const bool steam = (size_t)c.vapour < water.size() && water[c.vapour];
					// the gases you can really see have a colour; the rest barely show
					const int spread = std::max({ own.getRed(), own.getGreen(), own.getBlue() }) - std::min({ own.getRed(), own.getGreen(), own.getBlue() });
					// steam proper is clear: it's mist where it's cooled that shows, thin close over the boil, thick higher up
					const float shows = steam ? std::clamp((float)(430 - c.temperature) / 60, 0.45f, 1.f) : spread > 40 ? 1 : CLEAR;
					const float want = (float)c.haze * shows * PUFFS;
					if (want >= 0.5f) {
						m_wanted[i] = want;
						{
							// off the surface, if it's coming off a liquid
							const bool wet = c.fill.substance >= 0 && c.fill.share > 0.005;
							const float surface = wet ? (float)std::min(1.0, c.fill.share) : 0;
							Source g{ corner, v3f(0, surface, 0), v3f(1, wet ? std::min(1.f, surface + 0.2f) : 1.f, 1), want, m_rise[i], steam ? STEAM : own.color & 0xffffff, steam ? 0.05f + 0.1f * shows : 0.14f + 0.14f * shows };
							g.biggest = steam ? 0.45f : 1;
							m_gas.push_back(g);
						}
					}
				}
			}
	// (once every block's closed-or-not is known)
	i = 0;
	for (int ny = 0; ny < y; ny++)
		for (int nz = 0; nz < z; nz++)
			for (int nx = 0; nx < x; nx++, i++) {
				const sandbox::Stage::Cell &c = cells[i];
				if (!c.flame || c.activity <= 0.5)
					continue;
				const sandbox::StageBlock *b = c.block >= 0 && (size_t)c.block < blocks.size() ? &blocks[c.block] : nullptr;
				const bool vessel = b && b->spec.vessel;
				// a whole block burns off its top; else off what's lying there; else through the gas
				const bool lying = c.main >= 0 && c.filled > 0.005;
				const float floor = vessel ? 0.1f : 0, top = m_closed[i] ? 1.f : lying ? floor + std::min(1.f, (float)c.filled) * (vessel ? 0.8f : 1.f) : 0.f;
				const float inset = vessel ? 0.3f : m_closed[i] ? 0.2f : 0.05f;
				v3f lo(inset, top, inset), hi(1 - inset, lying || m_closed[i] ? top : 0.7f, 1 - inset);
				// and a flame's cube never starts inside what's beside it
				const auto shut = [&](int dx, int dz) {
					const int ax = nx + dx, az = nz + dz;
					return ax >= 0 && az >= 0 && ax < x && az < z && m_walled[((size_t)ny * z + az) * x + ax];
				};
				if (shut(-1, 0)) lo.X = std::max(lo.X, CLEARANCE);
				if (shut(1, 0)) hi.X = std::min(hi.X, 1 - CLEARANCE);
				if (shut(0, -1)) lo.Z = std::max(lo.Z, CLEARANCE);
				if (shut(0, 1)) hi.Z = std::min(hi.Z, 1 - CLEARANCE);
				const float strength = std::min(1.f, (float)(c.activity / FLAME_WATTS));
				m_flames.push_back({ v3f((float)nx, (float)ny, (float)nz), lo, hi, 25 + strength * 90, 1.1f + 0.7f * strength, c.flame, 1, vessel ? 0.4f : 1.f });
			}
}

void Plume::step(float dt)
{
	if (dt <= 0 || !m_x)
		return;
	m_counts.assign(m_closed.size(), 0);
	for (const Puff &p : m_puffs)
		if (!p.burning)
			if (const int c = cellAt(p.at); c >= 0)
				m_counts[c]++;
	for (Puff &p : m_puffs) {
		p.age += dt;
		const float swing = std::sin(p.wander + p.age * 2.3f);
		if (p.burning) {
			// a tongue: quick, narrowing to the middle of its block, flickering
			const float cx = std::floor(p.at.X) + 0.5f, cz = std::floor(p.at.Z) + 0.5f;
			p.at.X += ((cx - p.at.X) * 1.6f + swing * 0.35f) * dt;
			p.at.Z += ((cz - p.at.Z) * 1.6f + std::cos(p.wander * 3 + p.age * 3) * 0.35f) * dt;
			p.at.Y += p.start * 12 * dt;
			continue;
		}
		const int c = cellAt(p.at);
		// what a block has too much of thins out, and what's left the stage soon goes
		if (c < 0 || m_counts[c] > m_wanted[c] + 2)
			p.age += dt;
		p.at.X += swing * 0.12f * dt;
		p.at.Z += std::cos(p.wander * 2 + p.age * 1.7f) * 0.12f * dt;
		p.at.Y += (c >= 0 ? m_rise[c] : 0.2f) * dt;
	}
	m_puffs.erase(std::remove_if(m_puffs.begin(), m_puffs.end(), [&](const Puff &p) {
		const int c = cellAt(p.at);
		return p.age >= p.life || (c >= 0 && m_closed[c]);
	}), m_puffs.end());
	// a few a frame, so a block fills as a cloud does and not at a stroke
	size_t gas = 0;
	for (const Puff &p : m_puffs)
		gas += !p.burning;
	size_t flames = m_puffs.size() - gas;
	for (const Source &s : m_gas) {
		const int c = cellAt(s.corner);
		if (m_counts[c] >= s.want || gas >= MOST_GAS || random() > dt * 8)
			continue;
		const v3f at = s.corner + s.lo + v3f(random() * (s.hi.X - s.lo.X), random() * (s.hi.Y - s.lo.Y), random() * (s.hi.Z - s.lo.Z));
		m_puffs.push_back({ at, 0, 2.5f + random() * 2.5f, (2 + random() * 2) / 16 * s.biggest, random() * 6.28f, s.opacity, s.tint, false });
		gas++;
	}
	for (const Source &s : m_flames) {
		float due = s.want * dt + random();
		for (; due >= 1 && flames < MOST_FLAMES; due -= 1, flames++) {
			const v3f at = s.corner + s.lo + v3f(random() * (s.hi.X - s.lo.X), random() * (s.hi.Y - s.lo.Y), random() * (s.hi.Z - s.lo.Z));
			m_puffs.push_back({ at, 0, 0.3f + random() * 0.4f, (3 + random() * 3) / 16 * (0.6f + s.rise * 0.4f) * s.biggest, random() * 6.28f, 1, s.tint, true });
		}
	}
	// gas first, flames over it, and in each the oldest under the youngest
	std::stable_sort(m_puffs.begin(), m_puffs.end(), [](const Puff &a, const Puff &b) {
		if (a.burning != b.burning)
			return b.burning;
		return a.burning && a.age / a.life > b.age / b.life;
	});
}

} // namespace shell
