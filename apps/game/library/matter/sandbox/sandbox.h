// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/matter/matter/mixture.h"
#include "library/matter/sandbox/physics.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/*
	A sandbox: a tiny piece of the universe, a few blocks across, simulated
	with the game's own physics so the wiki can show what happens instead of
	saying it. Not a toy: every block is a matter::Mixture (the Reactor's,
	the furnace's), and between blocks:

	- heat conducts by each substance's conductivity in its state, and
	  leaves the outer faces to the room by convection and radiation;
	- gases mix, hot gas rises, gas made by a reaction pushes out and gas a
	  reaction uses is drawn in (fire breathing); the top is open to the
	  room's air;
	- liquids fall into space below and level out sideways;
	- matter carries its heat wherever it moves, so energy is conserved
	  except at the faces that touch the room.

	Solids stay where they are. Deterministic: the same fills and steps give
	the same result.
*/
namespace sandbox {

struct SandboxError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

struct At
{
	int32_t x, y, z; // y is up
};

// Substances by id with a share each (of a block's volume, or of a gas's moles).
using Parts = std::vector<std::pair<std::string, double>>;

struct Opts
{
	const matter::Matter &matter;
	int32_t x, y, z;   // blocks along each side, 1..16
	double block;      // edge of a block, m
	double ambient;    // K: the room around it
	double pressure;   // Pa
	Parts air;         // the room's gas by mole share (empty: a vacuum)
};

// What a block looks like, for drawing.
struct BlockView
{
	int32_t main;        // substance index filling most of it, −1 if only gas
	matter::State state; // of `main`
	double fill;         // share of the block's volume that's solid or liquid
	double temperature;  // K
	double activity;     // W: reaction heat released (+) or taken (−) in it
	int32_t vapour;      // substance index of the gas most unlike the room's, −1 if none
	double haze;         // 0..1: how much of its gas that is
};

class Sandbox
{
public:
	explicit Sandbox(const Opts &opts); // throws SandboxError on a bad size or an unknown gas

	// Fill a block: solids or liquids by share of its volume, at a temperature;
	// the rest is the room's gas, or `gas` by mole share (a furnace's air).
	// Shares sum to ≤ 1.
	void fill(At at, const Parts &by_volume, double temperature, const Parts &gas = {});
	// Gas only, by mole share, filling the block at the sandbox's pressure.
	void gas(At at, const Parts &by_moles, double temperature);
	// Heat poured into a block, watts, until set again (0 stops it).
	void heater(At at, double watts);
	// A block brought to a temperature now (a spark, a quench).
	void touch(At at, double temperature);

	void step(double seconds);

	std::vector<BlockView> view() const; // x fastest, then z, then y (bottom layer first)
	const matter::Mixture &block(At at) const;
	double time() const { return m_time; }
	// Joules held as heat by everything in it, counted from 0 K (for conservation checks).
	double energy() const;
	int32_t sizeX() const { return m_x; }
	int32_t sizeY() const { return m_y; }
	int32_t sizeZ() const { return m_z; }

private:
	size_t index(At at) const; // throws SandboxError outside
	// flow.cpp: between the blocks, by the shared physics
	void conduct(double dt);
	void breathe(double dt);
	void pour(double dt);
	std::vector<double> gasShares(const Parts &by_moles) const;

	const matter::Matter &m_matter;
	Physics m_physics;
	int32_t m_x, m_y, m_z;
	double m_ambient;
	std::vector<double> m_air;        // by substance index: the room's gas mole shares
	std::vector<matter::Mixture> m_blocks;
	std::vector<double> m_heaters;    // W
	std::vector<double> m_activity;   // W, last step
	double m_time = 0;
};

} // namespace sandbox
