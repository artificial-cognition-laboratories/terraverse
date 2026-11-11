// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "matter/mixture.h"
#include <optional>
#include <vector>

/*
	The rules between blocks of matter, shared by every place that runs them:
	the wiki's sandboxes (a dense little grid) and the world's own blocks
	(sparse, awake only where something's happening). One substrate, so what
	the wiki shows is what the world does.

	A block is a matter::Mixture filling a cube of `volume` m³: its solids and
	liquids take their volume by density, gas fills the rest at the pressure.
*/
namespace sandbox {

class Physics
{
public:
	struct Opts
	{
		const matter::Matter &matter;
		double pressure; // Pa
		double block;    // edge of a block, m
	};
	explicit Physics(const Opts &opts);

	const matter::Matter &matter() const { return m_matter; }
	double volume() const { return m_block * m_block * m_block; }
	double area() const { return m_block * m_block; }
	double block() const { return m_block; }
	double pressure() const { return m_pressure; }

	// What's in a block.
	double condensed(const matter::Mixture &b) const;       // m³ of solid and liquid
	double gasMoles(const matter::Mixture &b) const;
	double room(const matter::Mixture &b) const;            // moles of gas its free space holds
	double conductivity(const matter::Mixture &b) const;    // W/(m·K), its parts side by side
	struct Main
	{
		// filling most of it (−1: only gas): of the state that takes more of
		// it, solid or liquid (wet sand is sand, its pores' water held; sprint 111)
		int32_t substance;
		matter::State state;
		double fill;          // share of the volume that's solid or liquid
	};
	Main main(const matter::Mixture &b) const;

	// Filling a block: solids or liquids by share of its volume at a
	// temperature, the rest a gas by mole share (air, say). The parts are in
	// `state` if given (ice at its melting point), else in their state there.
	// Throws on a substance that isn't solid or liquid, or shares over the volume.
	void fill(matter::Mixture &b, const std::vector<std::pair<size_t, double>> &by_volume, double temperature,
			const std::vector<double> &gas, std::optional<matter::State> state = std::nullopt) const;

	// Run a block's reactions (and changes of state) for dt; the heat they
	// gave off (+) or took (−), in watts; each reaction's rate into `rates` if given.
	double react(matter::Mixture &b, double dt, std::vector<double> *rates = nullptr) const;
	// Heat between two neighbouring blocks for dt (conductivities given, so
	// a caller computes each block's once per step).
	void conduct(matter::Mixture &a, matter::Mixture &b, double ka, double kb, double dt) const;
	// The same with each one's heat capacity given (J/K: a block's, worked out once a step).
	void conduct(matter::Mixture &a, matter::Mixture &b, double ka, double kb, double ca, double cb, double dt) const;
	// Heat radiated across the face two neighbours share, where at least one
	// is mostly gas (a flame shining on the wood beside it; between two
	// solids, conduction is the path).
	void radiate(matter::Mixture &a, matter::Mixture &b, double dt) const;
	// The same toward an asleep solid neighbour at `t`: the joules it receives.
	double radiateInto(matter::Mixture &a, double t, double dt) const;
	// Heat an outer face loses to the room: still air and radiation.
	void lose(matter::Mixture &b, int faces, double ambient, double dt) const;
	// Two neighbours' gases trade a share; faster where the lower one (a) is warmer.
	void mix(matter::Mixture &a, matter::Mixture &b, bool b_above, double dt) const;
	// A block's gas trades with the room's air (a reservoir at `ambient`).
	void breathe(matter::Mixture &b, const std::vector<double> &air, double ambient, bool rising, double dt) const;
	// Matter moving from one block to another, carrying its heat. Arriving
	// in another state than what's there, the next react() settles it.
	void move(matter::Mixture &from, matter::Mixture &to, size_t substance, matter::State state, double moles) const;
	// Gas a block can't hold leaves into `to`; gas it lacks comes from `to`.
	void balance(matter::Mixture &b, matter::Mixture &to) const;
	// The same against the room: excess vents, a shortfall draws in air.
	void balance(matter::Mixture &b, const std::vector<double> &air, double ambient) const;
	// The longest step heat can take without overshooting between these blocks.
	double stable(const matter::Mixture &b, double k) const;

private:
	void admit(matter::Mixture &b, const std::vector<double> &air, double moles, double ambient) const;

	const matter::Matter &m_matter;
	double m_pressure, m_block;
};

} // namespace sandbox
