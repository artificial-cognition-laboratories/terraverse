// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fluid/fluid.h"
#include "matter/matter.h"
#include "terragen/world.h"
#include <memory>
#include <string>
#include <vector>

/*
	A terra world's weather: Earth's fluid layer (src/fluid), its ground
	built from terragen's terrain (sea, land heights, how wet the soil starts)
	and anchored to terragen's climate (so seasons swing about the biomes'
	temperatures, while rain, wind and storms are the layer's own). Live
	state: stepped to the game clock, saved beside the world. Deterministic:
	the saved state and the clock give the same weather, so whatever happened
	after the last save is stepped again exactly.

	Time is model seconds: one game day is one model day (86400 s).
*/
namespace weather {

// Model seconds for the engine's clock.
double clock(uint32_t day_count, double time_of_day);

// Metres of air per block of height (the world's heights are exaggerated
// ~25× against its width; the air sees mountains at their real height).
constexpr double BLOCK_METRES = 25;

class Weather
{
public:
	struct Opts
	{
		const terragen::World &world;
		const matter::Matter &matter;
		std::string path; // the saved state; empty: never saved
	};

	// The saved state at `path` if there is one (throws fluid::FluidError if
	// it isn't this world's); otherwise a settled layer: a year of weather
	// run up to `now`.
	static std::unique_ptr<Weather> open(const Opts &opts, double now);

	// Step to `now`, 3 hours at a time, saving at each new day. A clock set
	// back, or more than a year ahead, starts the weather over (settled).
	void advance(double now);

	// The weather at height y over a place at `now` (which advance() has reached).
	fluid::Sample sample(const terragen::Place &at, int32_t y, double now) const;

	const fluid::Layer &layer() const { return *m_layer; }
	double now() const { return m_start + m_layer->time(); }

	// The menu's preview: a settled year from the world's start, sampled at
	// `snapshots` evenly spaced dates.
	static std::vector<fluid::Layer::Fields> year(const terragen::World &world, const matter::Matter &matter,
			int snapshots);

private:
	Weather(const Opts &opts);
	void settle(double now);
	void save() const;

	const terragen::World &m_world;
	std::string m_path;
	fluid::LayerOpts m_layer_opts;
	std::unique_ptr<fluid::Layer> m_layer;
	double m_start = 0; // model time at the layer's time 0
};

} // namespace weather
