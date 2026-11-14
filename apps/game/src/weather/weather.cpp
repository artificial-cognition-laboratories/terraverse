// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "weather.h"
#include "terragen/planet.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace weather {

using namespace terragen;

static const int32_t CELLS = 64;          // the layer's grid, per side
static const int32_t SPAN = 4;             // points across a layer cell where terragen's land is read
static const double STEP = 3 * 3600;       // s
static const double DAY = 86400;
static const double LAND_METRES = 4000;    // a unit of terragen elevation, in the air's metres
static const double MEDIAN_RAIN = 1.6 / 24; // mm/h: the typical land's year (Earth's, ~600 mm)
static const char MAGIC[8] = { 'T', 'F', 'W', 'S', 0, 0, 0, 1 };

double clock(uint32_t day_count, double time_of_day)
{
	return (day_count + time_of_day) * DAY;
}

static double toDouble(fix v)
{
	return (double)v / ONE;
}

// Earth's fluid layer for a terra world.
static fluid::LayerOpts earth(const World &world, const matter::Matter &m)
{
	const WorldCode &code = world.code();
	const Grid &grid = world.grid();
	fluid::LayerOpts o;
	// the layer is tuned at Earth's size: it runs on the home planet resized
	// there, whose air flows the same (a model day is one game day). Sunlight
	// stays Earth's: the star's warmth is already in terragen's climate,
	// which anchors the layer.
	terragen::Planet p = terragen::resized(terragen::home(), terragen::EARTH_RADIUS);
	if (p.day != DAY)
		throw std::logic_error("weather: the model day must be the planet's day at Earth's size");
	o.planet = { p.radius, p.gravity, p.day, toDouble(code.tilt) * M_PI / 180, code.year * p.day, p.sunlight };
	o.width = o.height = CELLS;
	o.seed = code.seed_hash;

	double weighted = 0, weights = 0;
	for (int32_t row = 0; row < CELLS; row++) {
		for (int32_t col = 0; col < CELLS; col++) {
			double height = 0, wet = 0;
			int sea = 0;
			for (int32_t dz = 0; dz < SPAN; dz++)
				for (int32_t dx = 0; dx < SPAN; dx++) {
					const fix pu = ratio(2 * (col * SPAN + dx) + 1, 2 * CELLS * SPAN), pv = ratio(2 * (row * SPAN + dz) + 1, 2 * CELLS * SPAN);
					const int32_t i = world.cell(World::place(pu, pv));
					height += std::max(toDouble(grid.eroded[i]), 0.0) * LAND_METRES;
					sea += grid.base[i] < 0 || grid.lake[i];
				}
			height /= SPAN * SPAN;
			bool water = sea * 2 > SPAN * SPAN;
			// the cell's centre
			const Place at = World::place(ratio(2 * col + 1, 2 * CELLS), ratio(2 * row + 1, 2 * CELLS));
			wet = toDouble(world.column(at, world.sunlight(at, std::nullopt)).wet);
			// terragen's seasons there, at the hour the daily swing crosses zero:
			// the annual mean and half the swing from northern winter to summer
			double seasons[4];
			for (int k = 0; k < 4; k++)
				seasons[k] = toDouble(world.temperature(at, ratio(k, 4), ratio(3, 8))) + 273.15;
			double anchor = (seasons[0] + seasons[1] + seasons[2] + seasons[3]) / 4;
			double season = (seasons[1] - seasons[3]) / 2;
			o.ground.push_back({ water ? -1.0 : height, water, std::clamp((wet + 1) / 2, 0.0, 1.0), anchor, season });
			double area = std::cos(M_PI / 2 - (row + 0.5) * M_PI / CELLS);
			weighted += anchor * area;
			weights += area;
		}
	}
	o.air = { { { m.substance("nitrogen"), 0.79 }, { m.substance("oxygen"), 0.21 } }, 101325, m.substance("water"),
		weighted / weights };
	for (const Volcano &vo : grid.volcanoes) {
		if (!vo.active)
			continue;
		int32_t col = std::clamp((int32_t)(((int64_t)World::u(vo.at) * CELLS) >> FRAC), 0, CELLS - 1);
		int32_t row = std::clamp((int32_t)(((int64_t)World::v(vo.at) * CELLS) >> FRAC), 0, CELLS - 1);
		o.vents.push_back({ row * CELLS + col, 0.02 });
	}
	return o;
}

static double dateOf(double t, double year)
{
	double d = std::fmod(t / year, 1.0);
	return d < 0 ? d + 1 : d;
}

Weather::Weather(const Opts &opts) :
	m_world(opts.world), m_path(opts.path), m_each_step(opts.each_step), m_layer_opts(earth(opts.world, opts.matter))
{
}

std::unique_ptr<Weather> Weather::open(const Opts &opts, double now)
{
	std::unique_ptr<Weather> w(new Weather(opts));
	std::ifstream in(opts.path, std::ios::binary);
	if (opts.path.empty() || !in) {
		w->settle(now);
		return w;
	}
	std::stringstream bytes;
	bytes << in.rdbuf();
	std::string s = bytes.str();
	if (s.size() < sizeof MAGIC + sizeof(double) || s.compare(0, sizeof MAGIC, MAGIC, sizeof MAGIC) != 0)
		throw fluid::FluidError("weather: " + opts.path + " isn't a weather file");
	std::memcpy(&w->m_start, s.data() + sizeof MAGIC, sizeof(double));
	w->m_layer = std::make_unique<fluid::Layer>(w->m_layer_opts);
	w->m_layer->load(s.substr(sizeof MAGIC + sizeof(double)));
	w->advance(now);
	return w;
}

void Weather::settle(double now)
{
	double year = m_layer_opts.planet.year;
	m_layer = std::make_unique<fluid::Layer>(m_layer_opts);
	m_start = now - year;
	advance(now);
}

void Weather::advance(double target)
{
	double year = m_layer_opts.planet.year;
	if (target < now() - STEP || target > now() + year) {
		settle(target);
		return;
	}
	bool new_day = false;
	while (now() + STEP <= target) {
		double before = now();
		m_layer->step(STEP, dateOf(before, year));
		if (m_each_step)
			m_each_step(*this, STEP);
		new_day = new_day || std::floor(before / DAY) != std::floor(now() / DAY);
	}
	if (new_day)
		save();
}

double Weather::cellRain(const terragen::Grid &grid, const Dir &d, int32_t cell, double now, double &soil) const
{
	const double u = toDouble(World::u(d)), v = std::clamp(toDouble(World::v(d)), 0.0, 1.0 - 1e-9);
	const double date = dateOf(now, m_layer_opts.planet.year);
	const fluid::Sample s = m_layer->sample(u, v, 0, 0.5);
	soil = s.soil;
	// as sample() scales a column's: the grid's rain for each hemisphere's
	// summer, swung through the year by which one's having it
	const double north = std::sin(2 * M_PI * date); // 1 at the north's midsummer
	const double climate_rain = (toDouble(grid.rain_season[0][cell]) * (1 + north) +
			toDouble(grid.rain_season[1][cell]) * (1 - north)) / 2 * MEDIAN_RAIN;
	const double own = std::max(m_layer->climatology(u, v), 0.01);
	return s.precipitation * std::clamp(climate_rain / own, 0.1, 10.0);
}

double Weather::cellRainMean(const terragen::Grid &grid, int32_t cell)
{
	return toDouble(grid.rain(cell)) * MEDIAN_RAIN;
}

fluid::Sample Weather::sample(const Place &at, int32_t y, double now) const
{
	const double date = dateOf(now, m_layer_opts.planet.year);
	return sampleOver(at, y, now, m_world.ground(at, m_world.sunlight(at, (fix)(date * ONE))).terrain + 1);
}

fluid::Sample Weather::sampleCell(const Place &at, double now) const
{
	const int32_t surface = (int32_t)(m_world.gridHeight(at) >> terragen::FRAC) + 1;
	return sampleOver(at, surface, now, surface);
}

fluid::Sample Weather::sampleOver(const Place &at, int32_t y, double now, int32_t surface) const
{
	double u = toDouble(World::u(at)), v = std::clamp(toDouble(World::v(at)), 0.0, 1.0 - 1e-9);
	double hour = std::fmod(now / DAY, 1.0), date = dateOf(now, m_layer_opts.planet.year);
	fluid::Sample s = m_layer->sample(u, v, y * BLOCK_METRES, hour);
	// the temperature: terragen's climate for this very column, day and hour
	// (what the biomes were made from), plus the layer's departure from its
	// seasons, cooling with height above the ground (`surface`)
	double climate = toDouble(m_world.temperature(at, (fix)(date * ONE), (fix)(hour * ONE))) + 273.15;
	double t = m_layer->aloft(climate + m_layer->departure(u, v, date), std::max(0, y - surface) * BLOCK_METRES);
	// the same vapour at the new temperature
	s.humidity = std::min(1.0, s.humidity * m_layer->saturation(s.temperature, s.pressure) / m_layer->saturation(t, s.pressure));
	s.temperature = t;
	s.snow = m_layer->frozen(t, s.pressure);
	// the rain: the layer's weather, scaled so its long-run mean here is
	// terragen's climate for this season (the biomes' rain), not its own
	Column c = m_world.column(at, m_world.sunlight(at, std::nullopt));
	double summer = std::sin(2 * M_PI * date) * (at.dir().y > 0 ? 1 : -1); // 1 at the local midsummer
	double climate_rain = toDouble(c.rain) * (1 + toDouble(c.season) * summer) * MEDIAN_RAIN;
	double own = std::max(m_layer->climatology(u, v), 0.01);
	s.precipitation *= std::clamp(climate_rain / own, 0.1, 10.0);
	s.events = m_layer->events(s, u, v);
	return s;
}

void Weather::save() const
{
	if (m_path.empty())
		return;
	std::string tmp = m_path + ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		out.write(MAGIC, sizeof MAGIC);
		out.write(reinterpret_cast<const char *>(&m_start), sizeof m_start);
		std::string state = m_layer->save();
		out.write(state.data(), state.size());
		if (!out)
			throw fluid::FluidError("weather: couldn't write " + tmp);
	}
	if (std::rename(tmp.c_str(), m_path.c_str()) != 0)
		throw fluid::FluidError("weather: couldn't replace " + m_path);
}

std::vector<fluid::Layer::Fields> Weather::year(const World &world, const matter::Matter &matter, int snapshots)
{
	Weather w(Opts{ world, matter, "" });
	double year = w.m_layer_opts.planet.year;
	w.settle(0);
	std::vector<fluid::Layer::Fields> out;
	for (int k = 0; k < snapshots; k++) {
		w.advance(k * year / snapshots);
		out.push_back(w.m_layer->fields());
	}
	return out;
}

double openWaterEvaporation(const fluid::Sample &air)
{
	const double c = air.temperature - 273.15;
	const double saturated = 6.112 * std::exp(17.62 * c / (c + 243.12)); // hPa (Magnus)
	const double want = saturated * (1 - std::clamp(air.humidity, 0.0, 1.0));
	const double wind = std::hypot(air.wind_u, air.wind_v);
	// (Penman's wind function, halved: Dalton alone has no sun to pay for the
	// heat it takes, and gave 24 mm a day on a hot dry windy day where real
	// desert lakes lose 6 to 12)
	const double per_day = 0.13 * (1 + 0.54 * wind) * want; // mm/day
	return std::max(0.0, per_day) / 24;
}

} // namespace weather
