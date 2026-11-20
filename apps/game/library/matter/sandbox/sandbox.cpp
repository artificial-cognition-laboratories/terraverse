// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/matter/sandbox/sandbox.h"
#include <algorithm>
#include <cmath>

namespace sandbox {

using matter::State;

static const double LONGEST_STEP = 0.5; // s
static const double SAFETY = 0.25;      // of the fastest block's heat relaxation time
static const int MOST_STEPS = 20000;    // per call to step()
static const double EMPTY = 1e-3;       // J/K: a block holds nothing (a vacuum) without dividing by zero

Sandbox::Sandbox(const Opts &opts) :
	m_matter(opts.matter), m_physics({ opts.matter, opts.pressure, opts.block }),
	m_x(opts.x), m_y(opts.y), m_z(opts.z), m_ambient(opts.ambient)
{
	for (int32_t n : { m_x, m_y, m_z })
		if (n < 1 || n > 16)
			throw SandboxError("a sandbox is 1..16 blocks along each side");
	if (m_ambient <= 0)
		throw SandboxError("a sandbox needs a room temperature above 0 K");
	m_air = opts.air.empty() ? std::vector<double>(m_matter.substances().size(), 0) : gasShares(opts.air);
	size_t n = (size_t)m_x * m_y * m_z;
	matter::Mixture blank(m_matter);
	blank.vessel = EMPTY;
	m_blocks.reserve(n); // Mixture holds a reference: copied in, never assigned
	for (size_t i = 0; i < n; i++)
		m_blocks.push_back(blank);
	m_heaters.assign(n, 0);
	m_activity.assign(n, 0);
	for (matter::Mixture &b : m_blocks)
		m_physics.fill(b, {}, m_ambient, m_air);
}

std::vector<double> Sandbox::gasShares(const Parts &by_moles) const
{
	std::vector<double> out(m_matter.substances().size(), 0);
	double shares = 0;
	for (const auto &[id, share] : by_moles) {
		out[m_matter.index(id)] += share; // throws on an unknown substance
		shares += share;
	}
	if (std::fabs(shares - 1) > 1e-6)
		throw SandboxError("a gas's shares must sum to 1");
	return out;
}

size_t Sandbox::index(At a) const
{
	if (a.x < 0 || a.y < 0 || a.z < 0 || a.x >= m_x || a.y >= m_y || a.z >= m_z)
		throw SandboxError("a block outside the sandbox");
	return ((size_t)a.y * m_z + a.z) * m_x + a.x;
}

void Sandbox::fill(At at, const Parts &by_volume, double temperature, const Parts &gas)
{
	std::vector<std::pair<size_t, double>> parts;
	for (const auto &[id, share] : by_volume)
		parts.push_back({ m_matter.index(id), share });
	m_physics.fill(m_blocks[index(at)], parts, temperature, gas.empty() ? m_air : gasShares(gas));
}

void Sandbox::gas(At at, const Parts &by_moles, double temperature)
{
	if (by_moles.empty())
		throw SandboxError("a gas needs at least one substance");
	fill(at, {}, temperature, by_moles);
}

void Sandbox::heater(At at, double watts)
{
	m_heaters[index(at)] = watts;
}

void Sandbox::touch(At at, double temperature)
{
	if (temperature <= 0)
		throw SandboxError("a temperature must be above 0 K");
	m_blocks[index(at)].temperature = temperature;
}

void Sandbox::step(double seconds)
{
	if (seconds <= 0)
		throw SandboxError("steps must move forward");
	double left = seconds;
	int steps = 0;
	while (left > 1e-9) {
		if (++steps > MOST_STEPS)
			throw SandboxError("a sandbox needed more than 20000 steps for " + std::to_string(seconds) + " s");
		// a step short enough that heat can't overshoot between the most eager neighbours
		double fastest = INFINITY;
		for (const matter::Mixture &b : m_blocks)
			fastest = std::min(fastest, m_physics.stable(b, m_physics.conductivity(b)));
		double dt = std::min({ left, LONGEST_STEP, std::max(SAFETY * fastest, 1e-4) });

		for (size_t i = 0; i < m_blocks.size(); i++) {
			matter::Mixture &b = m_blocks[i];
			b.temperature += m_heaters[i] * dt / b.heatCapacity();
			m_activity[i] = m_physics.react(b, dt);
		}
		conduct(dt);
		breathe(dt);
		pour(dt);
		left -= dt;
		m_time += dt;
	}
}

std::vector<BlockView> Sandbox::view() const
{
	std::vector<BlockView> out;
	for (size_t i = 0; i < m_blocks.size(); i++) {
		const matter::Mixture &b = m_blocks[i];
		Physics::Main m = m_physics.main(b);
		BlockView v{ m.substance, m.state, m.fill, b.temperature, m_activity[i], -1, 0 };
		double gas = m_physics.gasMoles(b), unlike = 0;
		if (gas > 0)
			for (size_t s = 0; s < b.amounts.size(); s++) {
				double excess = b.in(s, State::Gas) / gas - m_air[s];
				if (b.amounts[s] > 0 && excess > unlike) {
					unlike = excess;
					v.vapour = (int32_t)s;
					v.haze = std::min(1.0, excess);
				}
			}
		out.push_back(v);
	}
	return out;
}

const matter::Mixture &Sandbox::block(At at) const
{
	return m_blocks[index(at)];
}

double Sandbox::energy() const
{
	double e = 0;
	for (const matter::Mixture &b : m_blocks)
		e += (b.heatCapacity() - b.vessel) * b.temperature;
	return e;
}

} // namespace sandbox
