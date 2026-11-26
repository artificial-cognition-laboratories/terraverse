// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/grid/coast.h"
#include "library/world/terragen/src/grid/crust.h"
#include "library/world/terragen/src/grid/fertility.h"
#include "library/world/terragen/src/grid/water.h"
#include "library/world/terragen/src/grid/wear.h"
#include "library/world/terragen/src/surface/craters.h"
#include "library/world/terragen/src/surface/maria.h"
#include "library/world/terragen/src/surface/lid.h"
#include "library/world/terragen/src/grid/climate.h"
#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/grid/erosion.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/world.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <tuple>

#include "library/world/terragen/src/base/parallel.h"
#include "library/world/terragen/src/base/progress.h"

namespace terragen {

static int octavesFor(const WorldCode &c) { return 3 + roundInt(c.roughness * 3); }

const Sphere &gridSphere()
{
	static const Sphere sphere(GRID_FREQUENCY);
	return sphere;
}

std::vector<int32_t> lowestFirst(const std::vector<fix> &height)
{
	// each a height over its index in one number: sorted without looking anything up
	std::vector<uint64_t> keys(height.size());
	for (size_t i = 0; i < height.size(); i++)
		keys[i] = (uint64_t)((int64_t)height[i] - INT32_MIN) << 32 | (uint32_t)i;
	std::sort(keys.begin(), keys.end());
	std::vector<int32_t> by(height.size());
	for (size_t i = 0; i < height.size(); i++)
		by[i] = (int32_t)(uint32_t)keys[i];
	return by;
}

fix landmass(const WorldCode &c, const Dir &d)
{
	fix persistence = ratio(35, 100) + mul(c.roughness, ratio(25, 100));
	// continents, with the coastline broken up by finer noise
	return fbm(d, c.seed_hash + CONTINENT, c.continents, octavesFor(c), persistence)
			+ mul(fbm(d, c.seed_hash + COAST, c.continents * 7, 3, HALF), mul(c.coast, ratio(22, 100)));
}

namespace {

// Snow on ground whose yearly mean (°C, at its height) is under MELT_FROM
// melts less of it each summer, and none under MELT_NONE: an ice sheet's,
// which flows away as ice, not in rivers (Greenland's, Antarctica's)
const fix MELT_FROM = -toFix(5), MELT_NONE = -toFix(15);

// The last few results of a stage and what they were made from. Tweaking a
// setting on the create screen builds world after world that differ only
// downstream of it: the plates needn't move again for a change of rivers.
template <typename Key, typename Value>
class Kept
{
public:
	template <typename Make>
	std::shared_ptr<const Value> get(const Key &key, Make &&make)
	{
		{
			std::lock_guard<std::mutex> hold(m_lock);
			for (const auto &e : m_recent)
				if (e.first == key)
					return e.second;
		}
		auto made = std::make_shared<const Value>(make());
		std::lock_guard<std::mutex> hold(m_lock);
		m_recent.insert(m_recent.begin(), { key, made });
		if (m_recent.size() > 3)
			m_recent.pop_back();
		return made;
	}

private:
	std::mutex m_lock;
	std::vector<std::pair<Key, std::shared_ptr<const Value>>> m_recent;
};

// (exactly what history() reads of a code, and landmass() for it)
std::shared_ptr<const History> historyKept(const WorldCode &c)
{
	static Kept<std::vector<int64_t>, History> kept;
	return kept.get({ c.seed_hash, c.continents, c.land, c.coast, c.roughness, c.mountains,
			(int64_t)c.processes.regime, c.processes.water }, [&] { return history(c); });
}

// (exactly what climate() reads: the land, and the sky's settings)
std::shared_ptr<const Climate> climateKept(const WorldCode &c, const std::vector<fix> &land)
{
	static Kept<std::pair<std::vector<int64_t>, std::vector<fix>>, Climate> kept;
	return kept.get({ { c.processes.warming, c.temperature, c.humidity, c.tilt }, land }, [&] { return climate(c, land); });
}

std::shared_ptr<Grid> build(const WorldCode &code, const std::function<void(const ErosionRound &)> *watch = nullptr)
{
	const Sphere &sphere = gridSphere();
	auto g = std::make_shared<Grid>();
	size_t n = (size_t)sphere.size();
	std::vector<fix> rain(n);

	// The plates' history makes the land (history.cpp); the coasts are
	// broken up by finer noise (the Coast setting), the sea placed by
	// quantile so exactly the Land share of the planet's area is above it
	// (each part timed and counted for whoever's waiting: progress.h)
	std::optional<Step> step;
	step.emplace(code.text, "plates");
	const std::shared_ptr<const History> kept_past = historyKept(code);
	const History &past = *kept_past;
	g->tectonics = tectonics(code, past);
	g->history = past.events;
	step.reset();
	step.emplace(code.text, "crust and climate");
	crust::lay(code, *g, past);
	thinCrust(code, *g);
	shapeLid(code, *g);
	crust::strike(code, *g);

	// The climate over the land as made (climate.h): where rain falls, which
	// wears the land and feeds the rivers (a little everywhere, so every slope runs)
	{
		const std::shared_ptr<const Climate> kept_climate = climateKept(code, g->base);
		const Climate &cl = *kept_climate;
		g->rain_season[0] = cl.rain[0];
		g->rain_season[1] = cl.rain[1];
		g->sea_heat = cl.sea;
		for (int k = 0; k < 3; k++)
			g->wind[k] = cl.wind[k];
		g->heat = cl.heat;
		g->heat_gain = cl.gain;
		g->ice = cl.ice;
		if (!code.processes.water) // nothing to rain
			for (std::vector<fix> &season : g->rain_season)
				std::fill(season.begin(), season.end(), 0);
		for (size_t i = 0; i < n; i++)
			rain[i] = clamp(g->rain((int32_t)i), ratio(15, 100), toFix(3));
	}

	// The coasts the waves cut (coast.h), on the land the climate was made
	// over: only coastal cells go, so its winds and rain still hold
	g->base = coast::cut(code, *g);
	wear::scour(code, *g);
	wear::erode(code, *g, rain, step, watch);

	// The final water: lakes, closed basins' pans, the rivers' ways
	// what runs off as water: the rain, less the snow that never melts (an
	// ice sheet's, which feeds no river and fills no lake; Grid::runoff)
	std::vector<fix> melted(n);
	for (size_t i = 0; i < n; i++) {
		const fix celsius = celsiusOf(g->heat[i] - mul(std::max<fix>(0, g->eroded[i]), LAPSE));
		melted[i] = mul(rain[i], clamp(div(celsius - MELT_NONE, MELT_FROM - MELT_NONE), 0, ONE));
	}
	if (code.processes.water)
		water::stand(*g, rain, melted);
	else {
		// (basins stand dry: water, if any, ends in each)
		g->filled = g->eroded;
		g->lake.assign(n, 0);
		g->pan.assign(n, 0);
		g->lake_level.assign(n, 0);
		g->receiver = receivers(g->filled);
		const std::vector<int32_t> order = upstreamOrder(g->receiver);
		g->drained = accumulate(g->receiver, order, rain);
		g->runoff = accumulate(g->receiver, order, melted);
		std::vector<fix> sheds(n);
		for (size_t i = 0; i < n; i++)
			sheds[i] = water::runoff(melted[i], g->heat[i] - mul(std::max<fix>(0, g->eroded[i]), LAPSE));
		g->discharge = accumulate(g->receiver, order, sheds);
	}

	g->river_threshold = g->stream_threshold = water::riverThreshold(code);

	// geology on the finished land: volcanoes, which cells their slopes
	// reach, then provinces (lava fields need the volcanoes)
	step.reset();
	step.emplace(code.text, "geology");
	g->volcanoes = volcanoes(code, *g);
	{
		const int64_t radius = World::radiusOf(code), cell = World::cellBlocksOf(code);
		std::vector<std::vector<uint16_t>> reach(n);
		for (size_t k = 0; k < g->volcanoes.size(); k++) {
			const Volcano &v = g->volcanoes[k];
			// the cells within its slopes and two cells more, out from its vent's
			const int64_t within = (v.kind == VolcanoKind::Patera ? (int64_t)v.radius * ONE * 3 / 2 : floods(v) ? lavaReach(v) * ONE : (int64_t)v.radius * ONE) + 2 * cell; // (a patera's outline wanders out; a young shield's plains run far)
			for (int32_t c : sphere.around(sphere.cell(v.at), [&](int32_t c) {
					return World::blocksOf(chord(sphere.centre(c), v.at), radius) <= within; }))
				reach[c].push_back((uint16_t)k);
		}
		g->volcano_start.assign(n + 1, 0);
		for (size_t i = 0; i < n; i++) {
			g->volcano_start[i + 1] = g->volcano_start[i] + (uint32_t)reach[i].size();
			g->volcano_list.insert(g->volcano_list.end(), reach[i].begin(), reach[i].end());
		}
	}
	g->surf = coast::surf(code, *g);
	g->setting = settings(code, *g, g->eroded);
	g->province = provinces(code, *g);
	g->fertility = fertility(code, *g);
	return g;
}

} // namespace

// (grid_file.cpp) none: not kept yet
std::shared_ptr<const Grid> loadGrid(const std::string &code);
void saveGrid(const std::string &code, const Grid &grid);

std::shared_ptr<const Grid> watchErosion(const WorldCode &code, const std::function<void(const ErosionRound &)> &watch)
{
	return build(code, &watch);
}

std::shared_ptr<const Grid> gridFor(const WorldCode &code)
{
	// A few recent worlds: the map generator's threads share one, and the
	// preview reuses it while only the date changes.
	static std::mutex lock;
	static std::vector<std::pair<std::string, std::shared_ptr<const Grid>>> recent;
	{
		std::lock_guard<std::mutex> guard(lock);
		for (auto &entry : recent)
			if (entry.first == code.grid_key)
				return entry.second;
	}
	std::shared_ptr<const Grid> built;
	{
		Step step(code.text, "grid kept");
		built = loadGrid(code.grid_key);
		if (built)
			step.kept();
	}
	if (!built) {
		built = build(code);
		saveGrid(code.grid_key, *built);
	}
	std::lock_guard<std::mutex> guard(lock);
	for (auto &entry : recent)
		if (entry.first == code.grid_key)
			return entry.second; // another thread finished first: use theirs
	recent.insert(recent.begin(), { code.grid_key, built });
	if (recent.size() > 4)
		recent.pop_back();
	return built;
}

} // namespace terragen
