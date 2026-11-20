// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/matter/matter/matter.h"
#include <vector>

namespace matter {

// What a mixture's surroundings give it.
struct Conditions
{
	double surroundings; // kelvin: what it exchanges heat with (a furnace, the air)
	double pressure;     // pascals
	double power;        // watts of electric current through it
	double light;        // watts of light on it
	double exchange = 50; // W/K with the surroundings (a crucible's; a cell of a sandbox exchanges with its neighbours instead: 0)
	// sprint 118: how far a flame must travel to cross it (m: a block's edge),
	// and how fast it's stirred (m/s: its wind). 0: no fronts, a premixed gas
	// burns as fast as its chemistry goes (a test tube, a lab's flask)
	double across = 0;
	double stir = 0;
	// a flame front is in it (it reached it from a neighbour, or a spark lit
	// it): a premixed gas burns at the front's pace whatever its average
	// temperature, the flame's thin zone being hot however cold the rest is
	bool lit = false;
};

/*
	A mixture: amounts of substances (moles), with a temperature, at the
	pressure its surroundings hold it at. Its properties come from what's in
	it. Stepping it runs every reaction whose conditions are met, at its rate,
	and balances energy: reaction heat, latent heat as substances change
	state, and heat exchanged with the surroundings. The same primitive at
	every scale — a crucible, a room's air, an ocean, an atmosphere.

	Changes of state take their time: a substance crossing its melting or
	boiling point is split between the two states and the mixture stays at
	that temperature until the heat to finish (or undo) the change has come
	(ice in a warm room sits at 0 °C until it has melted). Matter added in
	another state than what's there resolves the same way (steam meeting cold
	water condenses, warming it).
*/
class Mixture
{
public:
	explicit Mixture(const Matter &table);
	// Reactions per second a flame front allows a premixed gas (sprint 118):
	// the front crossing `across` at its speed here, by mixture, warmth and
	// stirring. 0 outside its limits: it can't carry a flame.
	double frontRate(const Reaction &r, const Conditions &c) const;
	// The speed of that front, m/s (0: it can't carry one here).
	double frontSpeed(const Reaction &r, const Conditions &c) const;

	std::vector<double> amounts; // moles, by substance index
	double temperature;          // kelvin
	double vessel = 1000;        // J/K: what holds it (a pot), so a few moles don't swing wildly

	// Each reaction's average rate over the run, in reactions per second.
	std::vector<double> run(double seconds, const Conditions &c);

	// The state most of a substance is in.
	State state(size_t substance, double pressure) const;
	// Moles of a substance in a state (two at once while it changes).
	double in(size_t substance, State state) const;
	// Add or take moles of a substance in a given state (moving matter
	// between mixtures). A mixture filled this way knows every state; one
	// filled through `amounts` learns them on its first run.
	void add(size_t substance, State state, double moles);
	void take(size_t substance, State state, double moles);
	// Joules per kelvin: everything in it, plus the vessel.
	double heatCapacity() const;
	// Whether its last run had nothing to do (sprint 79): see m_ran_at.
	bool still() const { return m_still; }
	// How it looks (sprint 79): its substances' colours by mass, each as
	// strong as its tint (0xRRGGBB); black if it's empty.
	uint32_t colour() const;

private:
	// How far a reaction could go on what's here (its limiting reactant).
	double room(const Reaction &r) const;
	bool met(const Reaction &r, double pressure) const;
	double rate(const Reaction &r, const Conditions &c) const;

	// Each substance's state; while one changes, `m_lagging` moles of it are
	// still in `m_from` (the rest in `m_states`).
	void track(double pressure);
	void settle(double pressure);

	// The reactions it could run (sprint 79): those whose every reactant
	// it holds, found again only when what it holds changes, so a block
	// pays for what's in it, not for the size of the table.
	const std::vector<size_t> &candidates();

	const Matter &m_table;
	std::vector<uint64_t> m_held;      // which substances it held when they were found, a bit each
	std::vector<size_t> m_candidates;
	// where it last ran (sprint 79): one that's still (nothing it could
	// run, nothing changing state, no warmer or cooler since and holding
	// the same things) isn't run again, so a warm rock costs nothing here
	double m_ran_at = -1;
	bool m_held_changed = true, m_still = false;
	std::vector<State> m_states, m_from;
	std::vector<double> m_lagging;
};

} // namespace matter
