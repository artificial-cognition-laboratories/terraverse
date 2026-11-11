// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "matter/matter.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

/*
	A fluid layer on a spinning sphere: weather. The atmosphere is the first
	instance; the physics is the same for any body, only its numbers change
	(see planning/sprint-12-weather.md). What it models, coarsely, on a
	longitude × latitude grid:

	- the circulation the body's spin allows (circulation.cpp): Hadley cells
	  as wide as the spin lets them reach, then alternating jets — one on
	  Earth, a dozen on a fast giant, none on a slow world;
	- heat: starlight in by latitude and season, radiation out, the surface
	  (slow sea, fast land) exchanging with the air;
	- the condensable (whichever substance sits near its phase line there —
	  water on Earth, methane on Titan): evaporated from wet surfaces,
	  carried by the wind, rained or snowed out wherever air rises and cools,
	  releasing its latent heat;
	- storms (storms.cpp): lows along the jets' fronts, tropical cyclones over
	  warm sea, drifting with the wind;
	- events (events.cpp): named thresholds on what results.

	Units are SI. Pure and deterministic given its state: stepping the same
	state the same way gives the same weather. Live state, saved with the
	world (save / load).
*/
namespace fluid {

struct FluidError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

// What the body gives its fluid layer.
struct Planet
{
	double radius;    // m
	double gravity;   // m/s²
	double day;       // rotation period, s (the sign of the spin doesn't matter here)
	double tilt;      // axial tilt, radians
	double year;      // s of model time per orbit (seasons)
	double sunlight;  // W/m² at the top of the air, facing the star
};

struct Gas
{
	matter::Substance substance;
	double share; // of the moles
};

// The layer's fluid: what it's made of, how much of it, what condenses.
struct Air
{
	std::vector<Gas> gases;
	double pressure;               // Pa at the datum (sea level)
	matter::Substance condensable; // see pickCondensable
	double mean;                   // the body's mean surface temperature, K (the body model's)
};

// The ground under one cell.
struct Ground
{
	double height; // m above the datum; ≤ 0 where `water`
	bool water;    // a sea or lake of the condensable
	double soil;   // land only: how wet the ground starts, 0..1
	double anchor; // annual mean surface temperature here, K; 0 = the layer's own
	double season; // with an anchor: half its swing from northern winter to summer, K (negative south)
};

// A volcano that can put ash in the air.
struct Vent
{
	int32_t cell;
	double strength; // 0..1
};

struct LayerOpts
{
	Planet planet;
	Air air;
	int32_t width, height;     // cells: longitude wraps, row 0 is the north pole's
	std::vector<Ground> ground; // width × height, row-major
	std::vector<Vent> vents;
	uint64_t seed;
};

// The large-scale flow the spin allows (circulation.cpp).
struct Circulation
{
	double hadley;   // the Hadley cells' poleward edge, radians
	int32_t jets;    // eddy-driven jets per hemisphere beyond them
	double trades;   // surface wind scale, m/s
	double westerly; // m/s
	double lift;     // vertical wind scale, m/s
};
Circulation circulate(const Planet &planet, const Air &air);

// The surface wind and rising air of the background flow at a latitude,
// with the rising branch's centre (the ITCZ) at `itcz`.
struct Flow
{
	double u, v, w; // east, north, up (m/s)
};
Flow flow(const Circulation &c, double latitude, double itcz);

enum class StormKind : uint8_t { Front, Tropical };

struct Storm
{
	StormKind kind;
	double lon, lat; // radians
	double radius;   // m
	double depth;    // Pa: how far the centre's pressure falls
	double age;      // s
};

// The named events (events.cpp); a sample carries the ones under way.
enum class Event : uint8_t { Thunderstorm, Blizzard, Fog, Hurricane, Drought, Ashfall, Count };
extern const char *const EVENT_NAMES[(int)Event::Count];

// The weather at one point.
struct Sample
{
	double temperature;   // K, at the given height and hour
	double humidity;      // relative, 0..1
	double cloud;         // 0..1
	double precipitation; // mm/h
	bool snow;            // falling as its solid
	double wind_u, wind_v; // m/s, east and north
	double pressure;      // Pa at the given height
	double ash;           // 0..1
	double soil;          // how wet the ground is, 0..1 (its bucket: rain in, evaporation out; 1 at sea)
	uint32_t events;      // bit (1 << Event)
};

class Layer
{
public:
	explicit Layer(LayerOpts opts); // throws FluidError on a malformed grid

	// Advance by `dt` seconds of model time; `date` is the year's fraction
	// (0 = northern spring equinox) at the step's start.
	void step(double dt, double date);

	// The weather at (u, v) ∈ [0,1)² — u east, v north (0) to south (1) — at
	// `height` m above the datum and `hour` (0..1) of the local day.
	Sample sample(double u, double v, double height, double hour) const;

	const LayerOpts &opts() const { return m_opts; }
	// How far the air at (u, v) is from where the surface's seasons alone
	// would put it on `date` (K): the seasons' lag and the weather. A world
	// that knows its own climate better adds this to it.
	double departure(double u, double v, double date) const;
	// The layer's own long-run precipitation at (u, v), mm/h (a year's running mean).
	double climatology(double u, double v) const;
	// The events under way for a sample taken at (u, v) — again, for a sample
	// a world has adjusted.
	uint32_t events(const Sample &s, double u, double v) const;
	// Whether the condensable falls as its solid at t and p.
	bool frozen(double t, double p) const;
	double lapse() const; // K/m: how the air cools with height
	// Air at t, `rise` metres higher: cooler by the lapse up through the
	// troposphere, holding its temperature above it.
	double aloft(double t, double rise) const;

	int32_t width() const { return m_opts.width; }
	int32_t height() const { return m_opts.height; }
	const Circulation &circulation() const { return m_circulation; }
	const std::vector<Storm> &storms() const { return m_storms; }
	double time() const { return m_time; } // s stepped since the layer began

	// Per cell, for maps and tests.
	struct Fields
	{
		std::vector<double> t, q, cloud, rain, ts, soil, u, v, pressure, ash;
		std::vector<double> recent, normal; // precipitation, running means (drought)
	};
	const Fields &fields() const { return m_f; }

	std::string save() const;
	void load(const std::string &bytes); // throws FluidError if it isn't this layer's

	// Saturation mixing ratio of the condensable, kg/kg, at temperature t and pressure p.
	double saturation(double t, double p) const;

private:
	double latitude(int32_t row) const;
	double insolation(double latitude, double date) const; // daily mean, W/m²
	double sunlit(int32_t row, double date) const;        // the layer's own: where sunlight alone takes the surface, K
	double radiative(int32_t cell, double date) const;    // where the surface heads, K
	void wind(double date);
	void advect(std::vector<double> &field, double dt);
	void spread(std::vector<double> &field, double k, double dt);
	void moisture(double dt);
	void heat(double dt, double date);
	void ashes(double dt);
	// storms.cpp: spawn, steer, deepen and fill
	void storm(double dt, double date);
	void stormWind(int32_t cell, double &u, double &v, double &w, double &dp) const;
	// events.cpp
	uint32_t events(const Sample &s, int32_t cell) const;
	int32_t nearest(double u, double v) const;

	LayerOpts m_opts;
	Circulation m_circulation;
	double m_cp, m_molar, m_scale; // air: J/(kg·K), kg/mol, scale height m
	double m_latent;                // the condensable's, J/kg
	double m_feedback;              // W/(m²·K): how much more a warmer surface radiates
	std::vector<double> m_annual, m_swing; // per row: the layer's own annual mean and half its seasonal swing
	std::vector<double> m_lift;     // per cell, last step: rising air, m/s
	Fields m_f;
	std::vector<Storm> m_storms;
	double m_time = 0;
	uint64_t m_steps = 0;
};

// Which of `candidates` condenses in this air: the one whose vapour
// pressure at the body's mean temperature is the largest share of the air's
// pressure below 1 (it can saturate without being the air itself). Throws
// FluidError if none can.
const matter::Substance &pickCondensable(const std::vector<const matter::Substance *> &candidates,
		double temperature, double pressure);

} // namespace fluid
