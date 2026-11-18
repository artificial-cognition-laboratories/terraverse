// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mixture.h"
#include <algorithm>
#include <cmath>

namespace matter {

static constexpr double LONGEST_STEP = 10.0;   // seconds
static constexpr int MOST_STEPS = 400000;      // a run that needs more is stiff beyond reason
static constexpr double FAST = 1e-3;           // s: a reaction that would finish within this goes all the way each step

Mixture::Mixture(const Matter &table) :
	amounts(table.substances().size(), 0.0), temperature(293.15), m_table(table)
{
}

static bool gaseous(State s)
{
	return s == State::Gas || s == State::Supercritical;
}

static bool same(State a, State b)
{
	return a == b || (gaseous(a) && gaseous(b));
}

void Mixture::track(double pressure)
{
	const auto &subs = m_table.substances();
	while (m_states.size() < amounts.size()) {
		State s = subs[m_states.size()].state(temperature, pressure);
		m_states.push_back(s);
		m_from.push_back(s);
		m_lagging.push_back(0);
	}
}

State Mixture::state(size_t i, double pressure) const
{
	if (i >= m_states.size())
		return m_table.substances()[i].state(temperature, pressure);
	return m_lagging[i] * 2 > amounts[i] ? m_from[i] : m_states[i];
}

double Mixture::in(size_t i, State s) const
{
	if (i >= m_states.size())
		throw MatterError("Mixture::in before the mixture has run or been added to: its states aren't known");
	double lag = std::min(m_lagging[i], amounts[i]);
	return (same(m_from[i], s) && lag > 0 ? lag : 0) + (same(m_states[i], s) ? amounts[i] - lag : 0);
}

// Joules per mole to bring a substance from one state to another (negative gives heat back).
static double latent(const Substance &s, State from, State to)
{
	auto level = [&](State st) {
		return st == State::Solid ? 0.0 : st == State::Liquid ? s.fusion * 1000.0 : (s.fusion + s.vaporization) * 1000.0;
	};
	return level(to) - level(from);
}

void Mixture::add(size_t i, State s, double moles)
{
	if (moles <= 0)
		return;
	track(ATMOSPHERE);
	if (amounts[i] <= 0) {
		m_states[i] = m_from[i] = s;
		m_lagging[i] = 0;
	} else if (!same(s, m_states[i])) {
		if (m_lagging[i] > 0 && !same(s, m_from[i])) {
			// a third state: charge its change into the bulk's state now
			temperature -= moles * latent(m_table.substances()[i], s, m_states[i]) / heatCapacity();
		} else {
			m_from[i] = s; // it arrives as the lagging part; settle() resolves it
			m_lagging[i] += moles;
		}
	}
	amounts[i] += moles;
}

void Mixture::take(size_t i, State s, double moles)
{
	track(ATMOSPHERE);
	moles = std::min(moles, in(i, s));
	if (moles <= 0)
		return;
	if (m_lagging[i] > 0 && same(m_from[i], s) && !same(m_states[i], s))
		m_lagging[i] -= moles;
	amounts[i] = std::max(0.0, amounts[i] - moles);
	m_lagging[i] = std::min(m_lagging[i], amounts[i]);
}

// The temperature where a substance passes between two states at a pressure.
static double boundary(const Substance &s, State a, State b, double p)
{
	if (gaseous(a)) std::swap(a, b);
	if (a == State::Solid && b == State::Liquid) return s.meltingPoint(std::max(p, s.triple.p));
	if (a == State::Liquid && gaseous(b)) return s.boilingPoint(p).value_or(s.critical.t);
	// solid and gas: where the sublimation curve reaches p
	double lo = 1, hi = s.triple.t;
	for (int k = 0; k < 60; k++) {
		double mid = (lo + hi) / 2;
		(s.sublimationPressure(mid) < p ? lo : hi) = mid;
	}
	return (lo + hi) / 2;
}

void Mixture::settle(double pressure)
{
	track(pressure);
	const auto &subs = m_table.substances();
	for (size_t i = 0; i < amounts.size(); i++) {
		if (amounts[i] <= 0) {
			m_lagging[i] = 0;
			m_states[i] = m_from[i] = subs[i].state(temperature, pressure);
			continue;
		}
		const Substance &s = subs[i];
		// held in its solvent (sprint 79): as much as the liquid there holds
		// is liquid, the rest as it'd be; going in or out is quick, its heat
		// of solution given or taken at once
		if (s.dissolves) {
			const double solvent = in(s.dissolves->solvent, State::Liquid);
			const double held = std::min(amounts[i], s.dissolves->most * solvent);
			const State free = s.state(temperature, pressure);
			const double was_held = std::min(in(i, State::Liquid), amounts[i]);
			if (free != State::Liquid && std::fabs(held - was_held) > amounts[i] * 1e-9)
				temperature += (held - was_held) * s.dissolves->heat * 1000.0 / heatCapacity();
			m_states[i] = held > 0 ? State::Liquid : free;
			m_from[i] = free;
			m_lagging[i] = held > 0 ? amounts[i] - held : 0; // the rest, still `free`
			continue;
		}
		if (m_lagging[i] <= 0) {
			State now = s.state(temperature, pressure);
			if (same(now, m_states[i])) {
				m_states[i] = now;
				continue;
			}
			// begin a change: all of it in the old state, heading for the next one
			// toward `now` (solid melts before it boils, where there can be liquid)
			State old = m_states[i];
			State next = now;
			if (pressure > s.triple.p && ((old == State::Solid && gaseous(now)) || (gaseous(old) && now == State::Solid)))
				next = State::Liquid;
			m_from[i] = old;
			m_states[i] = next;
			m_lagging[i] = amounts[i];
		}
		// carry the change on as far as the heat allows, holding the temperature at the boundary
		State a = m_from[i], b = m_states[i];
		double heat = latent(s, a, b); // per mole, a → b
		double lag = std::min(m_lagging[i], amounts[i]);
		if (std::fabs(heat) < 1e-9) {
			m_lagging[i] = 0;
			continue;
		}
		double at = boundary(s, a, b, pressure), capacity = heatCapacity();
		double surplus = (heat > 0 ? temperature - at : at - temperature) * capacity;
		if (surplus >= 0) {
			double x = std::min(lag, surplus / std::fabs(heat));
			lag -= x;
			temperature -= x * heat / capacity;
		} else {
			double y = std::min(amounts[i] - lag, -surplus / std::fabs(heat));
			lag += y;
			temperature += y * heat / capacity;
		}
		if (lag <= amounts[i] * 1e-12) {
			m_lagging[i] = 0; // done: all of it in b
		} else if (lag >= amounts[i] * (1 - 1e-12)) {
			m_states[i] = a; // undone: all of it back in a
			m_lagging[i] = 0;
		} else {
			m_lagging[i] = lag;
		}
	}
}

double Mixture::heatCapacity() const
{
	double c = vessel;
	const auto &subs = m_table.substances();
	for (size_t i = 0; i < amounts.size(); i++)
		c += amounts[i] * subs[i].molar_mass / 1000.0 * subs[i].heat_capacity;
	return c;
}

double Mixture::room(const Reaction &r) const
{
	double most = INFINITY;
	for (const Reaction::Term &t : r.reactants)
		most = std::min(most, amounts[t.substance] / t.count);
	return most;
}

bool Mixture::met(const Reaction &r, double pressure) const
{
	for (const Reaction::Requirement &q : r.needs)
		if (in(q.substance, q.state) <= 0)
			return false;
	return true;
}

double Mixture::rate(const Reaction &r, const Conditions &c) const
{
	double limit = room(r);
	if (limit <= 1e-12 || !met(r, c.pressure))
		return 0;
	switch (r.energy) {
	case Energy::None:
		return r.factor * std::exp(-r.activation * 1000.0 / (GAS_CONSTANT * temperature)) * limit;
	case Energy::Electricity:
		return c.power / (-r.heat * 1000.0);
	case Energy::Light:
		return c.light / (-r.heat * 1000.0);
	}
	return 0;
}

static constexpr double PREHEAT = 600;   // K: the warmest unburnt gas ahead of a front counts as (past it, a front's own heat)
static constexpr double HELD = 0.1;      // of its flame speed: a mixture outside its limits, held hot, burns at
static constexpr double STIRRING = 0.2;  // of a wind's speed: what its eddies add to a flame's (turbulent flame speed)

double Mixture::frontSpeed(const Reaction &r, const Conditions &c) const
{
	const Reaction::Front &f = *r.front;
	const double fuel = in(f.fuel, State::Gas), oxidiser = in(f.oxidiser, State::Gas);
	if (fuel <= 0 || oxidiser <= 0)
		return 0;
	// how rich: fuel to oxidiser against the equation's
	const double phi = fuel / oxidiser / (f.fuel_count / f.oxidiser_count);
	if (phi <= f.lean || phi >= f.rich) {
		// it can't carry a flame, but held hot by something else it burns where
		// it meets the air, as a torch's flame over a spirit does: slowly
		if (temperature < f.ignites)
			return 0;
		return HELD * f.speed + STIRRING * c.stir;
	}
	// fastest at its peak, falling to nothing at either limit
	const double edge = phi < f.peak ? f.lean : f.rich, off = (phi - f.peak) / (edge - f.peak);
	double speed = f.speed * (1 - off * off);
	// a warmer mixture burns faster (laminar speed ∝ T^1.75), up to the
	// warmth of the unburnt gas ahead of a front: a burning block's own heat
	// is behind its front, not ahead of it
	speed *= std::pow(std::clamp(temperature, 200.0, PREHEAT) / 298.0, 1.75);
	return speed + STIRRING * c.stir;
}

double Mixture::frontRate(const Reaction &r, const Conditions &c) const
{
	// the front crosses it in across/speed: its fuel burns at that pace
	const double speed = frontSpeed(r, c);
	return speed > 0 ? in(r.front->fuel, State::Gas) / r.front->fuel_count * speed / c.across : 0;
}

uint32_t Mixture::colour() const
{
	// mixed as light is: in linear terms, not sRGB's
	auto linear = [](uint32_t c, int shift) { return std::pow(((c >> shift) & 0xff) / 255.0, 2.2); };
	double rgb[3] = { 0, 0, 0 }, weight = 0;
	const auto &subs = m_table.substances();
	for (size_t i = 0; i < amounts.size(); i++) {
		if (amounts[i] <= 0)
			continue;
		const double w = amounts[i] * subs[i].molar_mass * subs[i].tint;
		for (int k = 0; k < 3; k++)
			rgb[k] += w * linear(subs[i].colour, 16 - 8 * k);
		weight += w;
	}
	if (weight <= 0)
		return 0;
	uint32_t out = 0;
	for (int k = 0; k < 3; k++)
		out |= uint32_t(std::lround(std::pow(rgb[k] / weight, 1 / 2.2) * 255)) << (16 - 8 * k);
	return out;
}

const std::vector<size_t> &Mixture::candidates()
{
	std::vector<uint64_t> held((amounts.size() + 63) / 64, 0);
	for (size_t i = 0; i < amounts.size(); i++)
		if (amounts[i] > 0)
			held[i / 64] |= uint64_t(1) << (i % 64);
	if (held == m_held && !m_held.empty())
		return m_candidates;
	m_held = std::move(held);
	m_held_changed = true;
	m_candidates.clear();
	const auto &reactions = m_table.reactions();
	for (size_t j = 0; j < reactions.size(); j++)
		if (std::all_of(reactions[j].reactants.begin(), reactions[j].reactants.end(),
					[&](const Reaction::Term &t) { return amounts[t.substance] > 0; }))
			m_candidates.push_back(j);
	return m_candidates;
}

std::vector<double> Mixture::run(double seconds, const Conditions &c)
{
	const auto &reactions = m_table.reactions();
	std::vector<double> done(reactions.size(), 0.0);
	// still: nothing to do, and nothing given to it from outside (sprint 79)
	constexpr double STILL = 0.25; // K
	const bool alone = c.power == 0 && c.light == 0 && (c.exchange == 0 || c.surroundings == temperature);
	if (alone && candidates().empty() && !m_held_changed && std::fabs(temperature - m_ran_at) < STILL &&
			std::all_of(m_lagging.begin(), m_lagging.end(), [](double l) { return l <= 0; })) {
		m_still = true;
		return done;
	}
	m_still = false;
	m_held_changed = false;
	settle(c.pressure); // whatever arrived since the last run

	double left = seconds;
	int steps = 0;
	std::vector<double> rates(reactions.size());
	std::vector<bool> fast(reactions.size());
	while (left > 1e-9) {
		if (++steps > MOST_STEPS)
			throw MatterError("a mixture needed more than " + std::to_string(MOST_STEPS) + " steps for " +
					std::to_string(seconds) + " s");
		// how fast everything goes now, and how long a step that keeps it honest is
		// a reaction that would use up what it needs within FAST completes within
		// the step (a flame), and doesn't hold every other one to its pace
		double dt = std::min(left, LONGEST_STEP), heat = c.exchange * (c.surroundings - temperature);
		std::fill(rates.begin(), rates.end(), 0.0);
		std::fill(fast.begin(), fast.end(), false);
		for (size_t j : candidates()) {
			rates[j] = rate(reactions[j], c);
			if (reactions[j].front && c.across > 0)
				rates[j] = c.lit ? frontRate(reactions[j], c) : std::min(rates[j], frontRate(reactions[j], c));
			fast[j] = rates[j] > 0 && room(reactions[j]) / rates[j] < FAST;
			if (rates[j] > 0 && !fast[j]) {
				dt = std::min(dt, 0.1 * room(reactions[j]) / rates[j] + 1e-6);
				if (reactions[j].energy == Energy::None)
					heat += rates[j] * reactions[j].heat * 1000.0;
			}
		}
		double capacity = heatCapacity();
		if (heat != 0)
			dt = std::min(dt, 20.0 / (std::fabs(heat) / capacity));
		dt = std::max(dt, 1e-6);

		// react, never past what's there
		double released = c.exchange * (c.surroundings - temperature) * dt;
		for (size_t j = 0; j < reactions.size(); j++) {
			if (rates[j] <= 0)
				continue;
			const Reaction &r = reactions[j];
			double extent = fast[j] ? room(r) : std::min(rates[j] * dt, room(r));
			// its heat is between standard states (sprint 79): what's used
			// up in another state gives that state's heat back (steam
			// burnt holds its boiling), and what's made in another pays
			// for it (water out of a flame is steam: methane's 890 kJ is
			// 802 so)
			const auto &subs = m_table.substances();
			for (const Reaction::Term &t : r.reactants) {
				released += t.count * extent * latent(subs[t.substance], subs[t.substance].standard, state(t.substance, c.pressure));
				amounts[t.substance] = std::max(0.0, amounts[t.substance] - t.count * extent);
			}
			for (const Reaction::Term &t : r.products) {
				released -= t.count * extent * latent(subs[t.substance], subs[t.substance].standard, state(t.substance, c.pressure));
				amounts[t.substance] += t.count * extent;
			}
			if (r.energy == Energy::None) // driven reactions take their heat from the power, not the pot
				released += extent * r.heat * 1000.0;
			done[j] += extent;
		}
		temperature = std::max(1.0, temperature + released / heatCapacity());

		// changes of state, at their own pace
		settle(c.pressure);
		left -= dt;
	}
	for (double &d : done)
		d /= seconds;
	m_ran_at = temperature;
	return done;
}

} // namespace matter
