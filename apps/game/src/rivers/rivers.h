// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/sphere.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace terragen { struct Grid; class Sphere; struct Ground; struct Place; }
namespace weather { class Weather; }

/*
	The planet's rivers as the weather fills them (planning/sprint-115-rivers.md).
	Rain falls where the weather says; each of the grid's cells sheds what its
	ground can't take (by how wet its soil is) and holds water as a small
	reservoir, letting it go into the cell it drains to over its run's travel
	time. So a storm upstream reaches a river's mouth days later, a wet season
	on wet ground floods, and a drought dries the small streams first.

	Runs everywhere, a weather step at a time (the Settled tier): a few
	numbers per cell. Conserves water: what's rained off the land is held in
	its cells or has reached the sea. Deterministic: the same weather gives the
	same rivers.
*/
namespace rivers {

class RiversError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

// What a cell's ground does with its rain (m/s of water over the cell): what
// runs straight off it, and what soaks in deep enough to feed its streams
// for months (the rest of what soaks in the plants and the sun take back);
// and on a lake, what the air takes off its water (5e: a lake losing more
// than comes in lets none go on: a closed basin)
struct Shed
{
	double quick = 0, soak = 0, lose = 0;
	Shed(double quick = 0, double soak = 0, double lose = 0) : quick(quick), soak(soak), lose(lose) {}
};

class Network
{
public:
	// Per cell: the cell it drains into (−1: the sea), its area (m²) and how
	// long water takes to cross it (s); `memory` (s) how long its running
	// means remember. Throws RiversError on a loop.
	Network(std::vector<int32_t> receiver, std::vector<double> area, std::vector<double> travel, double memory);

	// From a world's grid: its drainage, cells' areas on a planet of
	// `radius` (m), water crossing a cell at `speed` (m/s), means over `memory` s.
	static Network ofGrid(const terragen::Grid &grid, const terragen::Sphere &sphere, double radius, double speed,
			double memory);

	// Every cell as it would stand under `shed` for ever: what a world starts
	// with. The running means start there too.
	void settle(const std::vector<Shed> &shed);

	// `dt` s under `shed` (per cell, held through the step). Each cell's
	// ground water lets go over GROUND_TIME into its channel: a river runs
	// on through a dry season, and dries in a long drought.
	void step(double dt, const std::vector<Shed> &shed);
	static constexpr double GROUND_TIME = 60 * 86400.0; // s

	int32_t size() const { return (int32_t)m_receiver.size(); }
	double area(int32_t cell) const { return m_area[cell]; } // m²
	// m³/s leaving a cell now, into the one it drains to
	double discharge(int32_t cell) const { return m_out[cell]; }
	// ... and its running mean over about a year: what its channel was cut for
	double mean(int32_t cell) const { return m_mean[cell]; }
	// m/s of water its own ground gives its channels now (run off and let go
	// from under it), and its running mean: what a stream below the grid
	// answers to (it drains its cell's ground, not the river's)
	double shedding(int32_t cell) const { return m_own[cell]; }
	double shedding_mean(int32_t cell) const { return m_own_mean[cell]; }
	double stored(int32_t cell) const { return m_store[cell] + m_ground[cell]; } // m³, in its channels and ground
	double stored() const;                                      // m³, every cell's
	double toSea() const { return m_to_sea; }                   // m³ reached the sea, ever
	// s water leaving a cell takes to reach the sea, at its runs' speed
	double toSeaTime(int32_t cell) const { return m_to_sea_time[cell]; }
	double rained() const { return m_rained; }                  // m³ run off the land, ever
	double evaporated() const { return m_evaporated; }          // m³ off its lakes, ever

	std::string save() const;
	void load(const std::string &bytes); // throws RiversError if it isn't this network's

private:
	std::vector<int32_t> m_receiver;
	std::vector<double> m_area, m_travel;
	double m_memory;
	std::vector<int32_t> m_order; // upstream before downstream
	std::vector<double> m_to_sea_time;
	std::vector<double> m_store, m_ground, m_out, m_mean, m_own, m_own_mean;
	double m_to_sea = 0, m_rained = 0, m_evaporated = 0;
};

// What ground this wet (soil 0..1, the weather's bucket) does with `rain`
// (m/s): dry ground soaks most of it up, saturated ground sheds most.
Shed shed(double rain, double soil);

// Each of the grid's cells' shedding now, from the weather: its rain, scaled
// to the world's climate there for the season as a column's is. Sea: none.
// ... and on the year's mean rain, on ground half wet: what a network is
// settled on when it starts (whatever the weather is doing that day)
// (with a weather, its lakes' evaporation now as well: the losses it starts with)
std::vector<Shed> climate(const terragen::Grid &grid, const weather::Weather *weather = nullptr,
		const terragen::Sphere *sphere = nullptr);

std::vector<Shed> runoff(const weather::Weather &weather, const terragen::Grid &grid, const terragen::Sphere &sphere,
		double now);

// A storm or drought put over a region by hand (a test of the rivers, not
// weather: the weather goes on as it was underneath). A storm rains `rain`
// (m/s) on saturated ground; a drought (rain 0) sheds nothing at all.
struct Forcing
{
	terragen::Place centre;
	double radius = 0; // rad of the sphere
	double rain = 0;   // m/s
	double until = 0;  // the weather's clock, s
};
void force(std::vector<Shed> &shed, const terragen::Sphere &sphere, const Forcing &f);

// A river's water at one column, as its flow now makes it.
struct Flow
{
	double discharge = 0; // m³/s: the grid's river's, or a stream's share of what its cell's ground sheds, at its planet's scale
	double local = 0;     // m³/s in the blocks: its channel's width × its depth now × its speed (what a dam holds back)
	double ratio = 1;     // its flow now against what its channel was cut for
	double rise = 0;      // blocks its surface stands over (+) or under (−) the generated level
	double speed = 0;     // nodes/s it runs at
	double to_sea = 0;    // s its water takes to reach the sea
	double share = 1;     // a stream's: what it drains, in its cell's areas (a grid river: 1, its cell's run)
	bool stream = false;  // a stream below the grid, not one of its rivers
	double surge = 0;     // blocks over (−: under) the generated level its surface would stand, unbounded:
	                      // past `rise`'s bound it's over its banks, a flood (server/lakes.cpp spreads it)
};

// `ground` is terragen's column at `at` (its water a river's or a lake's).
// A grid river's flow is its run's cell's; a stream's, its own cell's
// runoff over the stream's catchment; a lake stands at its level. Depth
// goes as the ratio's 0.4th power, speed as its 0.3th (a channel's
// hydraulic geometry); the surface stays within the generated top block
// and the one over it (past that is a flood: basins').
Flow flowAt(const Network &net, const terragen::Sphere &sphere, const terragen::Ground &ground,
		const terragen::Place &at);

} // namespace rivers
