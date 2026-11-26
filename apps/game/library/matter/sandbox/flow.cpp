// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/matter/sandbox/sandbox.h"
#include <algorithm>
#include <cmath>

/*
	How heat and matter move between a sandbox's blocks: the shared physics
	(physics.h) applied across its grid. The walls and floor are closed; the
	top is open to the room.
*/
namespace sandbox {

using matter::State;

static const double FALLING = 4;   // per s: how fast a liquid runs into space below
static const double LEVELLING = 1; // per s: how fast it evens out sideways

void Sandbox::conduct(double dt)
{
	std::vector<double> k(m_blocks.size());
	for (size_t i = 0; i < m_blocks.size(); i++)
		k[i] = m_physics.conductivity(m_blocks[i]);
	for (int32_t y = 0; y < m_y; y++)
		for (int32_t z = 0; z < m_z; z++)
			for (int32_t x = 0; x < m_x; x++) {
				size_t i = index({ x, y, z });
				for (At n : { At{ x + 1, y, z }, At{ x, y + 1, z }, At{ x, y, z + 1 } })
					if (n.x < m_x && n.y < m_y && n.z < m_z) {
						size_t j = index(n);
						m_physics.conduct(m_blocks[i], m_blocks[j], k[i], k[j], dt);
						m_physics.radiate(m_blocks[i], m_blocks[j], dt);
					}
				// faces on the outside lose heat to the room
				int faces = (x == 0) + (x == m_x - 1) + (y == 0) + (y == m_y - 1) + (z == 0) + (z == m_z - 1);
				m_physics.lose(m_blocks[i], faces, m_ambient, dt);
			}
}

void Sandbox::breathe(double dt)
{
	// gas a block can't hold goes up (and out of the top); space left draws gas down, and the top draws the room's air
	for (int32_t y = 0; y < m_y; y++)
		for (int32_t z = 0; z < m_z; z++)
			for (int32_t x = 0; x < m_x; x++) {
				matter::Mixture &b = m_blocks[index({ x, y, z })];
				if (y == m_y - 1)
					m_physics.balance(b, m_air, m_ambient);
				else
					m_physics.balance(b, m_blocks[index({ x, y + 1, z })]);
			}
	// mixing between neighbours, faster where warm gas sits under cool
	for (int32_t y = 0; y < m_y; y++)
		for (int32_t z = 0; z < m_z; z++)
			for (int32_t x = 0; x < m_x; x++) {
				size_t i = index({ x, y, z });
				if (x + 1 < m_x) m_physics.mix(m_blocks[i], m_blocks[index({ x + 1, y, z })], false, dt);
				if (z + 1 < m_z) m_physics.mix(m_blocks[i], m_blocks[index({ x, y, z + 1 })], false, dt);
				if (y + 1 < m_y) m_physics.mix(m_blocks[i], m_blocks[index({ x, y + 1, z })], true, dt);
			}
	// the top trades gas with the room the same way, rising too if hot
	for (int32_t z = 0; z < m_z; z++)
		for (int32_t x = 0; x < m_x; x++)
			m_physics.breathe(m_blocks[index({ x, m_y - 1, z })], m_air, m_ambient, true, dt);
}

void Sandbox::pour(double dt)
{
	size_t subs = m_matter.substances().size();
	double volume = m_physics.volume();
	auto litres = [&](const matter::Mixture &b, size_t s) { // m³ of a liquid in a block
		const matter::Substance &sub = m_matter.substances()[s];
		return b.in(s, State::Liquid) * sub.molar_mass / 1000.0 / sub.density(State::Liquid, b.temperature, m_physics.pressure());
	};
	for (int32_t y = 0; y < m_y; y++)
		for (int32_t z = 0; z < m_z; z++)
			for (int32_t x = 0; x < m_x; x++) {
				matter::Mixture &b = m_blocks[index({ x, y, z })];
				for (size_t s = 0; s < subs; s++) {
					if (b.in(s, State::Liquid) <= 0)
						continue;
					// down, into whatever space is free below
					if (y > 0) {
						matter::Mixture &below = m_blocks[index({ x, y - 1, z })];
						double space = std::max(0.0, volume - m_physics.condensed(below));
						double v = std::min(litres(b, s), space) * std::min(1.0, FALLING * dt);
						if (v > 0)
							m_physics.move(b, below, s, State::Liquid, b.in(s, State::Liquid) * v / litres(b, s));
					}
					if (b.in(s, State::Liquid) <= 0)
						continue;
					// sideways, toward neighbours holding less, if it rests on something
					bool resting = y == 0 || m_physics.condensed(m_blocks[index({ x, y - 1, z })]) > volume * 0.99;
					if (!resting)
						continue;
					for (At a : { At{ x - 1, y, z }, At{ x + 1, y, z }, At{ x, y, z - 1 }, At{ x, y, z + 1 } }) {
						if (a.x < 0 || a.z < 0 || a.x >= m_x || a.z >= m_z || b.in(s, State::Liquid) <= 0)
							continue;
						matter::Mixture &side = m_blocks[index(a)];
						double difference = m_physics.condensed(b) - m_physics.condensed(side);
						if (difference <= 0)
							continue;
						double v = std::min(litres(b, s), difference / 2) * std::min(1.0, LEVELLING * dt);
						m_physics.move(b, side, s, State::Liquid, b.in(s, State::Liquid) * v / litres(b, s));
					}
				}
			}
}

} // namespace sandbox
