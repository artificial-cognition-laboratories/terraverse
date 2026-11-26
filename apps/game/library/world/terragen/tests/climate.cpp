// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// What can grow follows warmth and water: warm wet ground grows more than
// cold or dry; beyond the treeline nothing; trees stand only where it can grow.
TEST_CASE("Cover")
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	double warm_wet = 0, cold = 0, dry = 0;
	int n_ww = 0, n_cold = 0, n_dry = 0, trees = 0, columns = 0;
	for (int32_t k = 0; k < 6000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		const Place at = flat(w, x, z);
		Ground g = w.ground(at, w.sunlight(at, std::nullopt));
		Cover c = w.cover(at, g);
		REQUIRE((c.density >= 0 && c.density <= ONE));
		if (g.kind != Water::None)
			continue;
		columns++;
		trees += c.tree;
		if (c.tree)
			REQUIRE(c.density > 0);
		if (g.column.heat < -ratio(45, 100))
			REQUIRE(c.density == 0); // beyond the treeline
		double d = c.density / 65536.0;
		if (g.column.heat > ratio(10, 100) && g.column.wet > ratio(20, 100)) { warm_wet += d; n_ww++; }
		else if (g.column.heat < -ratio(30, 100)) { cold += d; n_cold++; }
		else if (g.column.wet < -ratio(30, 100) && !c.bank) { dry += d; n_dry++; }
	}
	REQUIRE((trees > 0 && trees < columns / 10));
	if (n_ww && n_cold) REQUIRE(warm_wet / n_ww > cold / n_cold);
	if (n_ww && n_dry) REQUIRE(warm_wet / n_ww > dry / n_dry);
}

// The climate of a real world: the air's bands show through, every biome
// the climate allows turns up somewhere, and fertility follows the rock.
TEST_CASE("Climate")
{
	World w(WorldCode::decode("TF24.dGVycmE"));
	const Grid &g = w.grid();
	auto band = [&](double lo, double hi) {
		int64_t sum = 0, n = 0;
		for (int32_t i = 0; i < g.size(); i++) {
			double lat = std::fabs(latitudeOf(gridSphere().centre(i)) / 65536.0 * 180 / M_PI);
			if (lat < lo || lat >= hi)
				continue;
			sum += g.rain(i);
			n++;
		}
		return (double)sum / n / ONE;
	};
	note() << "climate: rain 0-10° " << band(0, 10) << ", 25-38° " << band(25, 38) << ", 50-65° " << band(50, 65) << std::endl;
	REQUIRE(band(0, 10) > 3 * band(25, 38)); // the ITCZ, and the dry belts under the sinking air
	REQUIRE(band(50, 65) > 2 * band(25, 38)); // the storm tracks
	// the same code, the same climate
	World again(WorldCode::decode("TF24.dGVycmE"));
	REQUIRE((again.grid().rain_season[0] == g.rain_season[0] && again.grid().rain_season[1] == g.rain_season[1]));

	int counts[(int)Biome::Count] = {};
	double fertile[(int)Province::Count] = {};
	int cells[(int)Province::Count] = {};
	for (int32_t i = 0; i < g.size(); i++) {
		const Dir &d = gridSphere().centre(i);
		counts[(int)w.column(d, w.sunlight(d, std::nullopt)).biome]++;
		if (g.eroded[i] >= 0) {
			fertile[(int)g.province[i]] += g.fertility[i] / 65536.0;
			cells[(int)g.province[i]]++;
		}
	}
	for (Biome b : { Biome::Desert, Biome::Savanna, Biome::Forest, Biome::Jungle, Biome::Monsoon, Biome::Mediterranean })
		REQUIRE(counts[(int)b] > 0);
	int volcanic = (int)Province::Volcanic, shield = (int)Province::Shield;
	if (cells[volcanic] && cells[shield])
		REQUIRE(fertile[volcanic] / cells[volcanic] > fertile[shield] / cells[shield]);
}

// The climate over land we draw: a range in the westerlies has a wet side
// and a rain shadow; a continent in the tropics gets its rain in summer.
TEST_CASE("ClimateFromLand")
{
	WorldCode code = WorldCode::decode("TF24.dGVycmE");
	const std::vector<fix> e = drawn([](double lat, double x) {
		if (lat >= 38 && lat <= 58 && x >= 50 && x <= 74) // a north–south range, lowland either side
			return x >= 60 && x <= 63 ? ratio(6, 10) : ratio(5, 100);
		if (lat >= 8 && lat <= 30 && x >= 10 && x <= 40) // a continent in the tropics
			return ratio(8, 100);
		return -ratio(3, 10);
	});
	Climate c = climate(code, e);
	std::vector<fix> year(e.size());
	for (size_t i = 0; i < e.size(); i++)
		year[i] = c.rain[0][i] + c.rain[1][i];
	const double windward = meanOver(year, [](double lat, double x) { return lat >= 42 && lat <= 55 && x >= 55 && x <= 59; });
	const double leeward = meanOver(year, [](double lat, double x) { return lat >= 42 && lat <= 55 && x >= 64 && x <= 68; });
	const auto tropical = [](double lat, double x) { return lat >= 12 && lat <= 24 && x >= 12 && x <= 38; };
	const double summer = meanOver(c.rain[0], tropical), winter = meanOver(c.rain[1], tropical);
	note() << "climate: windward " << windward << " leeward " << leeward
			<< "; tropical land summer " << summer << " winter " << winter << std::endl;
	REQUIRE(windward > 1.5 * leeward);
	REQUIRE(summer > 2 * winter);
	REQUIRE_THROWS_AS(climate(code, std::vector<fix>(10, 0)), std::invalid_argument);
}

// An ocean between two continents: a warm current up its western side (off
// the first continent's east coast), a cold one down its eastern side (off
// the second's west coast), which leaves that coast drier.
TEST_CASE("OceanCurrents")
{
	WorldCode code = WorldCode::decode("TF24.dGVycmE");
	const std::vector<fix> e = drawn([](double lat, double x) {
		return lat >= 5 && lat <= 55 && ((x >= 20 && x <= 40) || (x >= 70 && x <= 90)) ? ratio(6, 100) : -ratio(3, 10);
	});
	Climate c = climate(code, e);
	std::vector<fix> year(e.size());
	for (size_t i = 0; i < e.size(); i++)
		year[i] = c.rain[0][i] + c.rain[1][i];
	const auto band = [](double lat) { return lat >= 20 && lat <= 35; };
	const double warm = meanOver(c.sea, [&](double lat, double x) { return band(lat) && x >= 41 && x <= 43; });
	const double cold = meanOver(c.sea, [&](double lat, double x) { return band(lat) && x >= 67 && x <= 69; });
	const double east_coast = meanOver(year, [&](double lat, double x) { return band(lat) && x >= 38 && x <= 40; });
	const double west_coast = meanOver(year, [&](double lat, double x) { return band(lat) && x >= 70 && x <= 72; });
	note() << "currents: off the east coast " << warm << ", off the west coast " << cold
			<< "; rain on the east coast " << east_coast << ", west coast " << west_coast << std::endl;
	REQUIRE((warm > 0 && cold < 0));
	REQUIRE(east_coast > west_coast);
}

// The energy balance: an Earth-like world is about Earth's temperature; a
// colder one has more ice than its forcing alone would give (ice brightens
// the world and cools it further); a continent's interior has fiercer
// seasons than its coast.
TEST_CASE("EnergyBalance")
{
	const std::vector<fix> e = drawn([](double lat, double x) {
		return lat >= 30 && lat <= 65 && x >= 20 && x <= 70 ? ratio(5, 100) : -ratio(3, 10);
	});
	const Sphere &s = gridSphere();
	auto mean = [&](const Climate &c) {
		int64_t sum = 0, area = 0;
		for (int32_t i = 0; i < s.size(); i++) {
			sum += (int64_t)celsiusOf(c.heat[i]) * s.area(i) >> FRAC;
			area += s.area(i);
		}
		return (double)sum / area;
	};
	const Climate earth = climate(WorldCode::decode("TF24.dGVycmE"), e);
	// the Temperature setting at −0.5 and −1
	const Climate cool = climate(WorldCode::decode("TF24.dGVycmE.AAKAqoCAAG1mgICAQICAgICCVQ"), e);
	const Climate cold = climate(WorldCode::decode("TF24.dGVycmE.AAKAqoCAAG1mgICAAICAgICCVQ"), e);
	const double t_earth = mean(earth), t_cool = mean(cool), t_cold = mean(cold);
	note() << "energy balance: " << t_earth << " °C, ice " << earth.ice / 655.36 << "%; cooler " << t_cool << " °C, ice "
			<< cool.ice / 655.36 << "%; colder " << t_cold << " °C, ice " << cold.ice / 655.36 << "%" << std::endl;
	REQUIRE((t_earth > 10 && t_earth < 25)); // (mostly sea: darker, so warmer than Earth)
	REQUIRE((cool.ice > earth.ice && cold.ice > cool.ice));
	// the second half of the cooling cools more than the first: the ice
	REQUIRE(t_cool - t_cold > t_earth - t_cool);
	const double coast = meanOver(earth.gain, [](double lat, double x) { return lat >= 45 && lat <= 50 && x >= 20 && x <= 22; });
	const double interior = meanOver(earth.gain, [](double lat, double x) { return lat >= 45 && lat <= 50 && x >= 44 && x <= 46; });
	note() << "seasons at 47° N: coast " << coast << ", interior " << interior << std::endl;
	REQUIRE(interior > 1.2 * coast);
}
