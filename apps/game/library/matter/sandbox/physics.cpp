// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/matter/sandbox/physics.h"
#include "library/matter/sandbox/sandbox.h"
#include <algorithm>
#include <cmath>

namespace sandbox {

using matter::State;

static const double CONVECTION = 10;  // W/(m²·K): still air on an outer face
static const double EMISSIVITY = 0.9;
static const double STEFAN = 5.670374e-8;
static const double MIXING = 2e-3;    // m²/s: gases stirring in still air (well above molecular diffusion)
static const double RISING = 0.02;    // per s per K: how fast warm gas trades places with cooler gas above

Physics::Physics(const Opts &opts) : m_matter(opts.matter), m_pressure(opts.pressure), m_block(opts.block)
{
	if (m_block <= 0 || m_pressure < 0)
		throw SandboxError("physics needs a positive block size and a pressure");
}

double Physics::condensed(const matter::Mixture &b) const
{
	double v = 0;
	for (size_t s = 0; s < b.amounts.size(); s++) {
		if (b.amounts[s] <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		for (State st : { State::Solid, State::Liquid })
			v += b.in(s, st) * sub.molar_mass / 1000.0 / sub.density(st, b.temperature, m_pressure);
	}
	return v;
}

double Physics::gasMoles(const matter::Mixture &b) const
{
	double n = 0;
	for (size_t s = 0; s < b.amounts.size(); s++)
		if (b.amounts[s] > 0)
			n += b.in(s, State::Gas); // `in` counts supercritical as gas
	return n;
}

double Physics::room(const matter::Mixture &b) const
{
	double free = std::max(0.0, volume() - condensed(b));
	return m_pressure * free / (matter::GAS_CONSTANT * b.temperature);
}

double Physics::conductivity(const matter::Mixture &b) const
{
	double k = 0, gas = 0, gas_k = 0;
	for (size_t s = 0; s < b.amounts.size(); s++) {
		if (b.amounts[s] <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		for (State st : { State::Solid, State::Liquid })
			k += b.in(s, st) * sub.molar_mass / 1000.0 / sub.density(st, b.temperature, m_pressure) / volume() * sub.conducts(st);
		double g = b.in(s, State::Gas);
		gas += g;
		gas_k += g * sub.conducts(State::Gas);
	}
	double free = std::max(0.0, 1 - condensed(b) / volume());
	return k + (gas > 0 ? free * gas_k / gas : 0);
}

Physics::Main Physics::main(const matter::Mixture &b) const
{
	Main out{ -1, State::Gas, condensed(b) / volume() };
	// which state takes more room, then the substance taking most of it
	double by_state[2] = { 0, 0 }, most[2] = { 0, 0 };
	int32_t which[2] = { -1, -1 };
	for (size_t s = 0; s < b.amounts.size(); s++) {
		if (b.amounts[s] <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		for (int k = 0; k < 2; k++) {
			const State st = k ? State::Liquid : State::Solid;
			double taken = b.in(s, st) * sub.molar_mass / 1000.0 / sub.density(st, b.temperature, m_pressure);
			by_state[k] += taken;
			if (taken > most[k]) {
				most[k] = taken;
				which[k] = (int32_t)s;
			}
		}
	}
	const int k = by_state[1] > by_state[0] ? 1 : 0;
	if (which[k] >= 0) {
		out.substance = which[k];
		out.state = k ? State::Liquid : State::Solid;
	}
	return out;
}

void Physics::fill(matter::Mixture &b, const std::vector<std::pair<size_t, double>> &by_volume, double temperature,
		const std::vector<double> &gas, std::optional<State> state) const
{
	if (temperature <= 0)
		throw SandboxError("a temperature must be above 0 K");
	for (size_t s = 0; s < b.amounts.size(); s++)
		for (State st : { State::Solid, State::Liquid, State::Gas })
			b.take(s, st, b.amounts[s]);
	b.temperature = temperature;
	double taken = 0;
	for (const auto &[s, share] : by_volume) {
		const matter::Substance &sub = m_matter.substances()[s];
		State st = state.value_or(sub.state(temperature, std::max(m_pressure, 1.0)));
		if (st != State::Solid && st != State::Liquid)
			throw SandboxError(sub.id + " isn't solid or liquid there: fill it as gas");
		if (share < 0)
			throw SandboxError("a share can't be negative");
		b.add(s, st, share * volume() * sub.density(st, temperature, m_pressure) / (sub.molar_mass / 1000.0));
		taken += share;
	}
	if (taken > 1 + 1e-9)
		throw SandboxError("a block's shares sum to more than its volume");
	double moles = room(b), shares = 0;
	for (size_t s = 0; s < gas.size(); s++) {
		b.add(s, State::Gas, gas[s] * moles);
		shares += gas[s];
	}
	if (shares > 0 && std::fabs(shares - 1) > 1e-6)
		throw SandboxError("a gas's shares must sum to 1");
}

double Physics::frontSpeed(const matter::Mixture &b, double stir) const
{
	matter::Conditions c{ b.temperature, std::max(m_pressure, 1.0), 0, 0, 0 };
	c.across = m_block;
	c.stir = stir;
	double fastest = 0;
	for (const matter::Reaction &r : m_matter.reactions())
		if (r.front)
			fastest = std::max(fastest, b.frontSpeed(r, c));
	return fastest;
}

double Physics::react(matter::Mixture &b, double dt, std::vector<double> *out, double stir, bool lit) const
{
	const auto &reactions = m_matter.reactions();
	matter::Conditions c{ b.temperature, std::max(m_pressure, 1.0), 0, 0, 0 };
	c.across = m_block;
	c.stir = stir;
	c.lit = lit;
	std::vector<double> rates = b.run(dt, c);
	if (out)
		*out = rates;
	double released = 0;
	for (size_t j = 0; j < reactions.size(); j++)
		if (reactions[j].energy == matter::Energy::None)
			released += rates[j] * reactions[j].heat * 1000.0;
	return released;
}

void Physics::conduct(matter::Mixture &a, matter::Mixture &b, double ka, double kb, double ca, double cb, double dt) const
{
	double k = ka + kb > 0 ? 2 * ka * kb / (ka + kb) : 0;
	double q = k * area() / m_block * (b.temperature - a.temperature) * dt;
	a.temperature = std::max(1.0, a.temperature + q / ca);
	b.temperature = std::max(1.0, b.temperature - q / cb);
}

void Physics::conduct(matter::Mixture &a, matter::Mixture &b, double ka, double kb, double dt) const
{
	// two halves in series: the harmonic mean
	double k = ka + kb > 0 ? 2 * ka * kb / (ka + kb) : 0;
	double q = k * area() / m_block * (b.temperature - a.temperature) * dt;
	a.temperature = std::max(1.0, a.temperature + q / a.heatCapacity());
	b.temperature = std::max(1.0, b.temperature - q / b.heatCapacity());
}

static const double SURFACE = 0.01; // share of a block solid or liquid that gives light something to land on
static const double FLAME_EMISSIVITY = 0.5; // a gas shining on a surface: between a clear and a sooty flame

void Physics::radiate(matter::Mixture &a, matter::Mixture &b, double dt) const
{
	if (condensed(a) > volume() / 2 && condensed(b) > volume() / 2)
		return;
	// clear gas barely takes in light (sprint 117): a flame shines on the
	// wood and stone round it, not on the air beside it
	if (condensed(a) < SURFACE * volume() && condensed(b) < SURFACE * volume())
		return;
	double ta = a.temperature, tb = b.temperature;
	double q = FLAME_EMISSIVITY * STEFAN * area() * (ta * ta * ta * ta - tb * tb * tb * tb) * dt;
	// never more than evens them out
	double ca = a.heatCapacity(), cb = b.heatCapacity();
	q = std::clamp(q, -(tb - ta) * ca * cb / (ca + cb) * (tb > ta ? 1 : 0), (ta - tb) * ca * cb / (ca + cb) * (ta > tb ? 1 : 0));
	a.temperature -= q / ca;
	b.temperature += q / cb;
}

double Physics::radiateInto(matter::Mixture &a, double t, double dt) const
{
	if (condensed(a) > volume() / 2)
		return 0;
	double ta = a.temperature;
	double q = FLAME_EMISSIVITY * STEFAN * area() * (ta * ta * ta * ta - t * t * t * t) * dt;
	q = std::min(q, (ta - t) * a.heatCapacity() * (ta > t ? 1 : 0));
	if (ta < t)
		q = std::max(q, (ta - t) * a.heatCapacity());
	a.temperature -= q / a.heatCapacity();
	return q;
}

void Physics::lose(matter::Mixture &b, int faces, double ambient, double dt) const
{
	double t = b.temperature;
	double q = faces * area() * (CONVECTION * (t - ambient) + EMISSIVITY * STEFAN * (t * t * t * t - ambient * ambient * ambient * ambient)) * dt;
	b.temperature = std::max(1.0, t - q / b.heatCapacity());
}

void Physics::mix(matter::Mixture &a, matter::Mixture &b, bool b_above, double dt) const
{
	// rock doesn't breathe through rock (sprint 79: gas stirred between
	// two solid blocks' pores was most of a warm slab's cost, for nothing)
	if (condensed(a) > volume() / 2 && condensed(b) > volume() / 2)
		return;
	if (gasMoles(a) <= 0 || gasMoles(b) <= 0)
		return;
	double rate = MIXING * area() / m_block / volume();
	if (b_above)
		rate += RISING * std::max(0.0, a.temperature - b.temperature);
	double f = std::min(0.25, rate * dt);
	for (size_t s = 0; s < a.amounts.size(); s++) {
		double from_a = a.in(s, State::Gas) * f, from_b = b.in(s, State::Gas) * f;
		move(a, b, s, State::Gas, from_a);
		move(b, a, s, State::Gas, from_b);
	}
}

void Physics::admit(matter::Mixture &b, const std::vector<double> &air, double moles, double ambient, double *kept) const
{
	double per_kelvin = 0;
	const double capacity = kept ? *kept : b.heatCapacity();
	for (size_t s = 0; s < air.size(); s++) {
		if (air[s] <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		per_kelvin += air[s] * moles * sub.molar_mass / 1000.0 * sub.heat_capacity;
		b.add(s, State::Gas, air[s] * moles);
	}
	if (capacity + per_kelvin > 0)
		b.temperature = (capacity * b.temperature + per_kelvin * ambient) / (capacity + per_kelvin);
	if (kept)
		*kept = capacity + per_kelvin;
}

void Physics::breathe(matter::Mixture &b, const std::vector<double> &air, double ambient, bool rising, double dt) const
{
	if (gasMoles(b) <= 0)
		return;
	double rate = MIXING * area() / m_block / volume() + (rising ? RISING * std::max(0.0, b.temperature - ambient) : 0);
	double f = std::min(0.25, rate * dt), out = 0;
	for (size_t s = 0; s < b.amounts.size(); s++) {
		double g = b.in(s, State::Gas) * f;
		out += g;
		b.take(s, State::Gas, g);
	}
	admit(b, air, out, ambient); // as much of the room's air comes back in
}

void Physics::move(matter::Mixture &from, matter::Mixture &to, size_t s, State state, double moles) const
{
	moles = std::min(moles, from.in(s, state));
	if (moles <= 0)
		return;
	const matter::Substance &sub = m_matter.substances()[s];
	double per_kelvin = moles * sub.molar_mass / 1000.0 * sub.heat_capacity;
	double capacity = to.heatCapacity();
	to.temperature = (capacity * to.temperature + per_kelvin * from.temperature) / (capacity + per_kelvin);
	from.take(s, state, moles);
	to.add(s, state, moles);
}

void Physics::carry(matter::Mixture &from, matter::Mixture *to, const std::vector<double> &gas, double share,
		double *from_capacity, double *to_capacity) const
{
	double per_kelvin = 0;
	const double capacity = to ? (to_capacity ? *to_capacity : to->heatCapacity()) : 0;
	for (size_t s = 0; s < gas.size(); s++) {
		if (gas[s] <= 0)
			continue;
		const double moles = std::min(gas[s] * share, from.in(s, State::Gas));
		if (moles <= 0)
			continue;
		from.take(s, State::Gas, moles);
		const matter::Substance &sub = m_matter.substances()[s];
		per_kelvin += moles * sub.molar_mass / 1000.0 * sub.heat_capacity;
		if (to)
			to->add(s, State::Gas, moles);
	}
	if (from_capacity)
		*from_capacity -= per_kelvin;
	if (to && per_kelvin > 0) {
		to->temperature = (capacity * to->temperature + per_kelvin * from.temperature) / (capacity + per_kelvin);
		if (to_capacity)
			*to_capacity = capacity + per_kelvin;
	}
}

void Physics::balance(matter::Mixture &b, matter::Mixture &to) const
{
	double have = gasMoles(b), holds = room(b);
	if (have > holds * 1.0001) {
		double share = (have - holds) / have;
		for (size_t s = 0; s < b.amounts.size(); s++)
			move(b, to, s, State::Gas, b.in(s, State::Gas) * share);
	} else if (have < holds * 0.9999) {
		double there = gasMoles(to);
		if (there <= 0)
			return;
		double share = std::min(1.0, (holds - have) / there);
		for (size_t s = 0; s < to.amounts.size(); s++)
			move(to, b, s, State::Gas, to.in(s, State::Gas) * share);
	}
}

void Physics::balance(matter::Mixture &b, const std::vector<double> &air, double ambient) const
{
	double have = gasMoles(b), holds = room(b);
	if (have > holds * 1.0001) {
		double share = (have - holds) / have;
		for (size_t s = 0; s < b.amounts.size(); s++)
			b.take(s, State::Gas, b.in(s, State::Gas) * share); // vented
	} else if (have < holds * 0.9999) {
		admit(b, air, holds - have, ambient);
	}
}

static const double SOUND = 10;  // m/s: how fast pressure evens out between blocks (see drive)
static const double DRAG = 0.5;  // per s: still air's hold on a wind
static const double EDDIES = 1.0; // per m: the drag that grows with speed (a plume rises at metres a second, no faster)

double Physics::excess(const matter::Mixture &b) const
{
	const double holds = room(b);
	return holds > 0 ? gasMoles(b) / holds - 1 : 0;
}

double Physics::gasDensity(const matter::Mixture &b) const
{
	const double free = volume() - condensed(b);
	if (free <= 0)
		return 0;
	double kg = 0;
	for (size_t s = 0; s < b.amounts.size(); s++)
		if (b.amounts[s] > 0)
			kg += b.in(s, State::Gas) * m_matter.substances()[s].molar_mass / 1000.0;
	return kg / free;
}

double Physics::airDensity(const std::vector<double> &air, double t) const
{
	double molar = 0;
	for (size_t s = 0; s < air.size(); s++)
		molar += air[s] * m_matter.substances()[s].molar_mass / 1000.0;
	return m_pressure * molar / (matter::GAS_CONSTANT * t);
}

double Physics::runniness(const matter::Mixture &b) const
{
	double n = 0, log_mu = 0, kg = 0, m3 = 0;
	for (size_t s = 0; s < b.amounts.size(); s++) {
		if (b.amounts[s] <= 0)
			continue;
		const double l = b.in(s, State::Liquid);
		if (l <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		n += l;
		log_mu += l * std::log(sub.viscosity(b.temperature));
		kg += l * sub.molar_mass / 1000.0;
		m3 += l * sub.molar_mass / 1000.0 / sub.density(State::Liquid, b.temperature, m_pressure);
	}
	if (n <= 0)
		return 0;
	return std::exp(log_mu / n) / (kg / m3);
}

double Physics::drive(double u, double ea, double eb, double ra, double rb, int up, double gravity, double dt) const
{
	double accel = SOUND * SOUND * (ea - eb) / m_block;
	if (up && ra + rb > 0)
		accel += up * gravity * (rb - ra) / ((ra + rb) / 2);
	// semi-implicit: the drag can't turn a wind round, however long the step
	return (u + dt * accel) / (1 + dt * (DRAG + EDDIES * std::fabs(u)));
}

double Physics::stable(const matter::Mixture &b, double k) const
{
	return b.heatCapacity() / (6 * std::max(k, 1e-6) * area() / m_block + 6 * 15 * area());
}

} // namespace sandbox
