// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "climate.h"
#include "grid.h"
#include "plates.h"
#include "sun.h"
#include "world.h"
#include <algorithm>
#include <cmath>

namespace terragen {

// ---- circulation (shared with src/fluid) -----------------------------------

/*
	Held & Hou (1980): rising equatorial air keeps its spin as it moves
	poleward, speeding eastward until it can't; the Hadley cell ends there,
	at √(5/3 · g·H·Δ / (Ω²a²)). Rhines (1975): beyond, eddies organise into
	jets about π√(2U/β) apart. Calibrated so Earth gets ~30° and one jet.
*/
static const double CONTRAST = 0.3;    // equator-to-pole contrast, as a share of the mean
static const double SURFACE = 1.0 / 8; // surface wind as a share of the wind aloft
static const double LIFT = 0.02;       // m/s
static const double PI_D = 3.14159265358979323846;
static const double COS_45 = 0.70710678118654752440;

Circulation circulate(double radius, double gravity, double day, double depth)
{
	double omega = 2 * PI_D / day;
	double edge = std::sqrt(5.0 / 3.0 * gravity * depth * CONTRAST / (omega * omega * radius * radius));
	edge = std::min(edge, PI_D / 2);
	double upper = std::min(gravity * depth * CONTRAST / (omega * radius), 0.3 * std::sqrt(gravity * depth));
	int32_t jets = 0;
	double span = radius * (PI_D / 2 - edge);
	if (span > 0.05 * radius) {
		double beta = 2 * omega * COS_45 / radius;
		double spacing = PI_D * std::sqrt(2 * upper / beta);
		jets = std::max<int32_t>(1, (int32_t)(span / spacing + 0.5));
	}
	double surface = upper * SURFACE;
	return { edge, jets, 0.7 * surface, surface, LIFT };
}

Circulation circulation(const Planet &p)
{
	// at Earth's size, where the calibration holds (the flow is the same:
	// see resized); the weather layer is 1.6 scale heights of Earth's air (8.4 km)
	Planet e = resized(p, EARTH_RADIUS);
	return circulate(e.radius, e.gravity, e.day, 1.6 * 8400);
}

Flow flow(const Circulation &c, fix lat, fix itcz)
{
	fix hadley = (fix)(c.hadley * ONE);
	fix north = std::min(HALF_PI, itcz + hadley), south = std::max(-HALF_PI, itcz - hadley);
	if (lat >= south && lat <= north) {
		fix side = lat >= itcz ? ONE : -ONE;
		fix edge = side > 0 ? north : south;
		fix s = edge == itcz ? 0 : div(lat - itcz, edge - itcz);
		fix sn = sin(mul(PI, s));
		return { -mul(ratio(7, 10), sn), -mul(side, mul(ratio(28, 100), sn)), cos(mul(PI, s)) };
	}
	if (c.jets == 0) // the cells reach the poles: only the polar cap, sinking
		return { 0, 0, -ONE };
	// beyond: westerly bands (rising at their poleward front), then easterly ones
	fix pole = lat > 0 ? ONE : -ONE;
	fix edge = pole > 0 ? north : -south;
	fix x = div(fabsx(lat) - edge, HALF_PI - edge) * (2 * c.jets);
	int32_t band = std::min(floorInt(x), 2 * c.jets - 1);
	fix f = x - toFix(band);
	fix s = sin(mul(PI, f));
	if (band % 2 == 0)
		return { s, mul(pole, mul(ratio(2, 10), s)), -cos(mul(PI, f)) };
	return { -s / 2, -mul(pole, s / 10), cos(mul(PI, f)) };
}

// ---- the climate, on the sphere ------------------------------------------------

namespace {

const int SWEEPS = 40;          // passes of the moisture march (it settles in ~30)
const int AVERAGED = 8;         // the last passes, averaged
// The march's step: how far air moves a pass at unit wind (the old
// climate grid's cell, 1/128 of the way round, which the tuning below is for).
const fix STEP = TWO_PI / 128;
// tuned against the fluid layer on an ocean world (wet ITCZ, dry 30°, a storm track)
const fix MONSOON = toFix(12);  // wind per unit of heat difference a step across, westerly units
const fix THERMAL = toFix(5);   // rising per unit of warmth above the latitude's mean, lift units
const fix STABLE = toFix(8);    // sinking per unit of chill from a cold current, lift units
const fix SLOPE = toFix(18);    // rising per unit of elevation climbed a step, lift units
const fix RECYCLE = ratio(12, 10);   // land gives back up to this much of its rain (plants draw on the soil it filled)
const fix EVAPORATE = ratio(8, 100); // of the air's shortfall from saturation over water, made up per pass
const fix DRIZZLE = ratio(5, 1000);  // of the vapour, what falls per pass anyway
const fix CONVERGE = ratio(15, 100); // rising air draws in this share of its vapour per pass, per unit of lift
const fix RAINOUT = ratio(5, 10);   // of the vapour above what the air holds, what falls per pass

// The energy balance (Budyko 1969, Sellers 1969, North 1975), in W/m² and °C.
const int32_t BANDS = 90;              // equal-area latitude bands (even in sin latitude)
const fix SOLAR = toFix(433);          // the day's mean sunlight at the equator at equinox (S₀/π, Earth's)
const fix RADIATE_A = toFix(188);      // heat radiated at 0 °C …
const fix RADIATE_B = ratio(209, 100); // … and per °C more (water vapour and CO₂ included)
const fix CARRY = ratio(40, 100);      // heat carried poleward per °C of gradient (in sin latitude)
const fix SEA_ALBEDO = ratio(7, 100), LAND_ALBEDO = ratio(16, 100), ICE_ALBEDO = ratio(42, 100);
const fix PUSH = ratio(40, 100);        // the star's and the setting's forcing, against their old offset
const fix ICE_FROM = -toFix(5), ICE_FULL = -toFix(15); // ice spreads over this span of the yearly mean
// Clouds brighten the planet, more toward the poles: 0.24 + 0.05 · P₂(sin latitude).
fix cloud(fix x)
{
	return ratio(24, 100) + mul(ratio(5, 100), (3 * mul(x, x) - ONE) / 2);
}
fix iced(fix celsius)
{
	return clamp(div(ICE_FROM - celsius, ICE_FROM - ICE_FULL), 0, ONE);
}

// What each cell is, for the fields below.
struct Cells
{
	const Sphere &sphere = gridSphere();
	int32_t n = sphere.size();
	std::vector<fix> lat, v;
	std::vector<Dir> east, north;
	std::vector<int32_t> band;
	std::vector<std::vector<int32_t>> in_band;
	Cells()
	{
		lat.resize(n);
		v.resize(n);
		east.resize(n);
		north.resize(n);
		band.resize(n);
		in_band.resize(BANDS);
		for (int32_t i = 0; i < n; i++) {
			const Dir &d = sphere.centre(i);
			lat[i] = latitudeOf(d);
			v[i] = World::v(d);
			east[i] = d.x == 0 && d.z == 0 ? Dir{ 0, 0, ONE } : normalise(-(int64_t)d.z, 0, d.x);
			north[i] = plates::cross(east[i], d);
			band[i] = bandOf(d.y);
			in_band[band[i]].push_back(i);
		}
	}
	static int32_t bandOf(fix x) { return std::clamp<int32_t>((int32_t)(((int64_t)(x + ONE) * BANDS) >> (FRAC + 1)), 0, BANDS - 1); }
	// a band's middle, in sin latitude
	static fix middle(int32_t b) { return ratio(2 * b + 1, BANDS) - ONE; }

	// A field's slope at each cell, east and north, per STEP of distance:
	// over the neighbours, (2/k) Σ (f_j − f_i) (c_j − c_i) / |c_j − c_i|².
	void slope(const std::vector<fix> &f, std::vector<fix> &east_slope, std::vector<fix> &north_slope) const
	{
		east_slope.resize(n);
		north_slope.resize(n);
		for (int32_t i = 0; i < n; i++) {
			const Dir &c = sphere.centre(i);
			int64_t gx = 0, gy = 0, gz = 0;
			const fix *gap = sphere.distancesBegin(i);
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, gap++) {
				const Dir &o = sphere.centre(*p);
				const int64_t g2 = (int64_t)*gap * *gap >> FRAC; // |Δ|², fixed
				const int64_t df = f[*p] - f[i];
				gx += df * (o.x - c.x) / g2;
				gy += df * (o.y - c.y) / g2;
				gz += df * (o.z - c.z) / g2;
			}
			const int32_t k = sphere.neighbourCount(i);
			const Dir g{ (fix)(2 * gx / k), (fix)(2 * gy / k), (fix)(2 * gz / k) }; // per radian
			east_slope[i] = mul((fix)(plates::dot(g, east[i]) >> FRAC), STEP);
			north_slope[i] = mul((fix)(plates::dot(g, north[i]) >> FRAC), STEP);
		}
	}
};

const Cells &cells()
{
	static const Cells c;
	return c;
}

/*
	A field spread over σ (radians), as air and water mix: on a coarser
	sphere (about 3° a cell), by passes of each cell with its neighbours,
	read back between the coarse cells. Smooth by design: only for what
	is smooth over hundreds of km.
*/
std::vector<fix> blur(const std::vector<fix> &f, fix sigma)
{
	static const Sphere coarse(20);
	static const std::vector<int32_t> down = [] {
		const Sphere &fine = gridSphere();
		std::vector<int32_t> out(fine.size());
		for (int32_t i = 0; i < fine.size(); i++)
			out[i] = coarse.cell(fine.centre(i));
		return out;
	}();
	const int32_t m = coarse.size();
	std::vector<int64_t> sum(m, 0), count(m, 0);
	for (size_t i = 0; i < f.size(); i++) {
		sum[down[i]] += f[i];
		count[down[i]]++;
	}
	std::vector<fix> c(m), t(m);
	for (int32_t k = 0; k < m; k++)
		c[k] = (fix)(sum[k] / std::max<int64_t>(count[k], 1));
	// each pass spreads by about 0.43 spacings² (a cell and its ~6 neighbours, evenly)
	const int64_t spacing = coarse.spacing();
	const int32_t passes = (int32_t)(((int64_t)sigma * sigma / (spacing * spacing * 43 / 100)) + 1);
	for (int32_t pass = 0; pass < passes; pass++) {
		for (int32_t k = 0; k < m; k++) {
			int64_t s = c[k];
			for (const int32_t *p = coarse.neighboursBegin(k); p != coarse.neighboursEnd(k); p++)
				s += c[*p];
			t[k] = (fix)(s / (coarse.neighbourCount(k) + 1));
		}
		c.swap(t);
	}
	std::vector<fix> out(f.size());
	const Sphere &fine = gridSphere();
	for (int32_t i = 0; i < fine.size(); i++) {
		const Sphere::Spot spot = coarse.spot(fine.centre(i));
		int64_t s = 0;
		for (int k = 0; k < 3; k++)
			s += (int64_t)c[spot.cells[k]] * spot.weights[k];
		out[i] = (fix)(s >> FRAC);
	}
	return out;
}

fix degrees(int32_t d)
{
	return (fix)((int64_t)PI * d / 180);
}

// How much vapour the air holds at a temperature (relative): doubles every half unit of heat.
fix capacity(fix heat)
{
	return exp2(heat * 2);
}

// 1 at `centre`, falling to 0 at ± `width` (radians)
fix bell(fix x, fix centre, fix width)
{
	fix d = div(x - centre, width);
	return maxx(0, ONE - mul(d, d));
}

/*
	Ocean currents, as the gyres winds drive. In each ocean the trades push
	water west and the westerlies push it east, so it circles: in the
	subtropics a narrow, fast, warm current runs poleward along the ocean's
	western edge (off east coasts: the Gulf Stream, the Kuroshio) and a
	broad, cold one runs toward the equator along its eastern edge (off west
	coasts: the Canary, Humboldt and Benguela currents, with upwelling). In
	the subpolar gyres beyond the westerlies it turns the other way round:
	warm water drifts to the eastern side (Norway's), cold runs down the
	western (Labrador's). What results is how much warmer or colder each
	stretch of sea is than its latitude, carried a little way onto the land.
	The distance to land along each parallel is read on a fine raster of
	the cells (an index for the scan, nothing more).
*/
const int32_t RASTER = 512;               // columns round the world (and half as many rows)
const int32_t COAST_REACH = 160;          // columns (112°): how far across an ocean a coast's current is felt
const fix CURRENT_HEAT = ratio(28, 100);  // heat a full-strength current carries (~10 °C)

std::vector<fix> currents(const std::vector<fix> &elevation)
{
	const Cells &g = cells();
	const int32_t rows = RASTER / 2;
	std::vector<uint8_t> land((size_t)RASTER * rows);
	for (int32_t y = 0; y < rows; y++)
		for (int32_t x = 0; x < RASTER; x++)
			land[(size_t)y * RASTER + x] = elevation[g.sphere.cell(World::place(ratio(2 * x + 1, 2 * RASTER), ratio(2 * y + 1, 2 * rows)).dir())] >= 0;
	std::vector<fix> sea(g.n, 0);
	for (int32_t i = 0; i < g.n; i++) {
		if (elevation[i] >= 0)
			continue;
		const Dir &d = g.sphere.centre(i);
		const int32_t x = std::clamp(floorInt(World::u(d) * RASTER), 0, RASTER - 1), y = std::clamp(floorInt(g.v[i] * rows), 0, rows - 1);
		const uint8_t *row = &land[(size_t)y * RASTER];
		int32_t west = COAST_REACH, east = COAST_REACH;
		for (int32_t k = 1; k < COAST_REACH && west == COAST_REACH; k++)
			if (row[((x - k) % RASTER + RASTER) % RASTER]) west = k;
		for (int32_t k = 1; k < COAST_REACH && east == COAST_REACH; k++)
			if (row[(x + k) % RASTER]) east = k;
		const fix away = fabsx(g.lat[i]);
		const fix subtropical = bell(away, degrees(28), degrees(20)), subpolar = bell(away, degrees(58), degrees(12));
		const fix tropical = bell(away, 0, degrees(12));
		// the western boundary current is narrow, the eastern broad; none without a coast
		const fix western = west < COAST_REACH ? exp2(-ratio(west, 12)) : 0;
		const fix eastern = east < COAST_REACH ? exp2(-ratio(east, 32)) : 0;
		const fix poleward = mul(subtropical, western - mul(eastern, ratio(7, 10)))
				+ mul(subpolar, mul(eastern, ratio(8, 10)) - mul(western, ratio(6, 10)));
		// and on the equator, cold water welling up off the eastern side
		const fix upwelling = mul(tropical, mul(eastern, ratio(4, 10)));
		sea[i] = mul(poleward - upwelling, CURRENT_HEAT);
	}
	// currents spread and mix, then carry onto the coasts, thinning inland
	const std::vector<fix> mixed = blur(sea, degrees(4));
	for (int32_t i = 0; i < g.n; i++)
		if (elevation[i] < 0)
			sea[i] = mixed[i];
	const std::vector<fix> spread = blur(sea, degrees(6));
	for (int32_t i = 0; i < g.n; i++)
		if (elevation[i] >= 0)
			sea[i] = mul(spread[i], ratio(7, 10));
	return sea;
}

/*
	The zonal energy balance: in each band, sunlight absorbed (less the
	band's albedo, brightened by ice where the band is below freezing) and
	any forcing = A + B·T radiated − what diffusion carries in from the
	bands either side (D (1 − x²) dT/dx, x = sin latitude). Tridiagonal,
	solved exactly (64-bit fixed); ice and temperature settle together.
	`sun` is each band's sunlight (relative to the equator at equinox),
	`surface` its albedo without ice, `ice` the share of it that can freeze.
*/
std::vector<fix> balance(const std::vector<fix> &sun, const std::vector<fix> &surface, fix forcing,
		const std::vector<fix> *ice_from = nullptr)
{
	using wide = int64_t; // fixed, FRAC bits, 64 wide
	const wide dx = ratio(2, BANDS);
	std::vector<wide> up(BANDS), dn(BANDS);
	for (int32_t b = 0; b < BANDS; b++) {
		auto flux = [&](fix edge) { return (wide)mul(CARRY, ONE - mul(edge, edge)) * ONE / dx * ONE / dx; };
		up[b] = b + 1 < BANDS ? flux(Cells::middle(b) + (fix)dx / 2) : 0;
		dn[b] = b > 0 ? flux(Cells::middle(b) - (fix)dx / 2) : 0;
	}
	std::vector<fix> t(BANDS, toFix(10));
	std::vector<wide> c(BANDS), d(BANDS);
	for (int round = 0; round < 40; round++) {
		std::vector<fix> was = t;
		for (int32_t b = 0; b < BANDS; b++) {
			// the band's ice from its own temperature, or (a season) from the year's
			const fix frozen = iced(ice_from ? (*ice_from)[b] : t[b]);
			const fix albedo = lerp(surface[b], ICE_ALBEDO, frozen) + cloud(Cells::middle(b));
			const wide rhs = (wide)mul(mul(SOLAR, sun[b]), ONE - albedo) - RADIATE_A + forcing;
			const wide diag = RADIATE_B + up[b] + dn[b];
			// Thomas: eliminate below
			const wide m = b == 0 ? diag : diag + mulShift(dn[b], c[b - 1], FRAC);
			c[b] = (up[b] << FRAC) / m * -1;
			d[b] = ((rhs + (b == 0 ? 0 : mulShift(dn[b], d[b - 1], FRAC))) << FRAC) / m;
		}
		for (int32_t b = BANDS - 1; b >= 0; b--)
			t[b] = (fix)(d[b] - (b + 1 < BANDS ? mulShift(c[b], t[b + 1], FRAC) : 0));
		// ice and temperature settle together: halfway each round
		for (int32_t b = 0; b < BANDS; b++)
			t[b] = (t[b] + was[b]) / 2;
	}
	return t;
}

// A band field read at a sin latitude, between band middles.
fix inBands(const std::vector<fix> &f, fix x)
{
	const fix at = mul(x + ONE, toFix(BANDS) / 2) - HALF;
	const int32_t b = std::clamp(floorInt(at), 0, BANDS - 2);
	return lerp(f[b], f[b + 1], clamp(at - toFix(b), 0, ONE));
}

} // namespace

fix sunlight(const WorldCode &code, fix v, std::optional<fix> date)
{
	fix latitude = mul(HALF - v, PI);
	fix yearly = annualInsolation(latitude, code.tilt);
	if (!date)
		return yearly;
	// that day's sun, plus a share of the year's (land and sea store heat)
	fix today = insolation(latitude, declination(code.tilt, *date));
	return mul(today, ratio(6, 10)) + mul(yearly, ratio(4, 10));
}

Climate climate(const WorldCode &code, const std::vector<fix> &elevation)
{
	const Cells &g = cells();
	if ((int32_t)elevation.size() != g.n)
		throw std::invalid_argument("climate: the elevation must be per grid cell");
	Climate out;
	out.sea = currents(elevation);

	// --- temperature: the bands' balance, then each cell about its band ---
	// the land's share of each band, and how continental each cell is (how
	// far the sea's damping of the seasons reaches inland)
	std::vector<fix> land(g.n);
	for (int32_t i = 0; i < g.n; i++)
		land[i] = elevation[i] >= 0 ? ONE : 0;
	const std::vector<fix> inland = blur(land, degrees(6));
	std::vector<fix> surface(BANDS), sun_year(BANDS), sun_season[2] = { std::vector<fix>(BANDS), std::vector<fix>(BANDS) };
	for (int32_t b = 0; b < BANDS; b++) {
		int64_t share = 0;
		for (int32_t i : g.in_band[b])
			share += land[i];
		const fix f = g.in_band[b].empty() ? 0 : (fix)(share / (int64_t)g.in_band[b].size());
		surface[b] = lerp(SEA_ALBEDO, LAND_ALBEDO, f);
		const fix lat = HALF_PI - acos(Cells::middle(b)), v = HALF - div(lat, PI);
		sun_year[b] = sunlight(code, v, std::nullopt);
		sun_season[0][b] = sunlight(code, v, ratio(1, 4));
		sun_season[1][b] = sunlight(code, v, ratio(3, 4));
	}
	// the star and the Temperature setting push as forcings: what would warm
	// the world by PUSH × their old heat (in degrees) without ice, which
	// takes them further (a world near its star's habitable zone, nudged)
	const fix forcing = mul(mul(RADIATE_B, (code.star->warmth + mul(code.temperature, ratio(6, 10))) * 37), PUSH);
	const std::vector<fix> year = balance(sun_year, surface, forcing);
	const std::vector<fix> season[2] = { balance(sun_season[0], surface, forcing, &year), balance(sun_season[1], surface, forcing, &year) };

	out.heat.resize(g.n);
	out.gain.resize(g.n);
	std::vector<fix> swing[2] = { std::vector<fix>(g.n), std::vector<fix>(g.n) };
	int64_t frozen = 0;
	for (int32_t i = 0; i < g.n; i++) {
		const fix x = g.sphere.centre(i).y;
		const fix band_year = inBands(year, x);
		// its own surface against the band's: land brighter than sea, ice
		// brightest. Ice is the cell's own: where it's cold for itself (high
		// ground, cold currents), not where its latitude is, so an ice sheet
		// follows the land and the sea ice the water
		const fix own_cold = band_year + out.sea[i] * 37 - mul(maxx(0, elevation[i]), LAPSE) * 37;
		// (an ice sheet on land lasts only where it's colder still: summers melt it back)
		const fix own = elevation[i] >= 0 ? lerp(LAND_ALBEDO, ICE_ALBEDO, iced(own_cold + toFix(6))) : lerp(SEA_ALBEDO, ICE_ALBEDO, iced(own_cold));
		const fix mean = lerp(inBands(surface, x), ICE_ALBEDO, iced(band_year));
		const fix sun = sunlight(code, g.v[i], std::nullopt);
		// (a local difference is mostly undone by the air mixing it away:
		// land and sea at one latitude differ by a few degrees a year, not
		// ten; ice, a sheet over a whole region, keeps more of its chill)
		const fix bright = mul(mul(SOLAR, sun), mean - own);
		const fix local = div(bright, (own > mean ? 3 : 6) * RADIATE_B);
		// where it's cold, air off a continent is colder still (its winters):
		// interiors below their latitude, and sea ice reaching out along cold
		// coasts while open ocean stays clear
		const fix continental = mul(std::min<fix>(band_year, 0), inland[i]) / 2;
		const fix celsius = band_year + local + continental + out.sea[i] * 37;
		out.heat[i] = heatOfCelsius(celsius);
		if (celsius < ICE_FROM)
			frozen += g.sphere.area(i);
		// the seasons: the band's swing (each season balanced as if it lasted:
		// too much), damped by the sea's heat: half at sea, 1¼ in the middle of a continent
		const fix damp = HALF + mul(inland[i], ratio(3, 4));
		for (int s = 0; s < 2; s++)
			swing[s][i] = mul(inBands(season[s], x) - band_year, damp) / 37;
		// per unit of sunlight above the year's, from the cell's own summer
		const int s = g.lat[i] >= 0 ? 0 : 1;
		const fix more = sunlight(code, g.v[i], s == 0 ? ratio(1, 4) : ratio(3, 4)) - sun;
		out.gain[i] = more > ratio(2, 100) ? div(swing[s][i], more) : 0;
	}
	out.ice = (fix)(frozen / g.n);

	// --- rain: each season's winds carry the sea's vapour over the land ---
	const Circulation circ = circulation(home());
	for (int s = 0; s < 2; s++) {
		const fix date = s == 0 ? ratio(1, 4) : ratio(3, 4);
		// the rising branch follows the sun about halfway (thermal lows carry it further over land)
		const fix itcz = mul(declination(code.tilt, date) * 45, ratio(1, 100));
		std::vector<fix> heat(g.n), potential(g.n);
		for (int32_t i = 0; i < g.n; i++) {
			const fix e = maxx(0, elevation[i]);
			potential[i] = out.heat[i] + swing[s][i];
			heat[i] = potential[i] - mul(e, LAPSE);
		}
		// the air's temperature over hundreds of km; and the same brought down
		// to sea level (potential temperature), which decides thermal lows and
		// highs: land warmer than its latitude draws air in and lifts it, and a
		// high plateau is warm air, not cold (a summer heat low, like Tibet's)
		const std::vector<fix> air = blur(heat, degrees(14)), warmth = blur(potential, degrees(14));
		std::vector<fix> zonal(BANDS);
		for (int32_t b = 0; b < BANDS; b++) {
			int64_t sum = 0;
			for (int32_t i : g.in_band[b])
				sum += warmth[i];
			zonal[b] = g.in_band[b].empty() ? 0 : (fix)(sum / (int64_t)g.in_band[b].size());
		}
		std::vector<fix> warm_east, warm_north, climb_east, climb_north;
		g.slope(warmth, warm_east, warm_north);
		{
			// the land as the air feels it: smoothed to about a step (SLOPE is
			// tuned for that; a cell's own slopes are several times steeper)
			std::vector<fix> ground(g.n);
			for (int32_t i = 0; i < g.n; i++)
				ground[i] = maxx(0, elevation[i]);
			g.slope(blur(ground, degrees(2)), climb_east, climb_north);
		}

		// where each cell's air was a pass ago, and how fast it rises
		std::vector<Sphere::Spot> from(g.n);
		std::vector<fix> lift(g.n), surface_holds(g.n), holds(g.n);
		for (int32_t i = 0; i < g.n; i++) {
			const Flow f = flow(circ, g.lat[i], itcz);
			// the mean wind, bent toward warmer air (sea breezes at the scale of continents)
			const fix u = f.u + mul(warm_east[i], MONSOON), v = f.v + mul(warm_north[i], MONSOON);
			const Dir &c = g.sphere.centre(i), &e = g.east[i], &n = g.north[i];
			const fix du = mul(u, STEP), dv = mul(v, STEP);
			from[i] = g.sphere.spot(normalise((int64_t)c.x - mul(du, e.x) - mul(dv, n.x),
					(int64_t)c.y - mul(du, e.y) - mul(dv, n.y), (int64_t)c.z - mul(du, e.z) - mul(dv, n.z)));
			// wind blowing up the land is lifted by it; air over a cold current
			// is chilled from below and won't rise (the coastal deserts)
			const fix climb = mul(u, climb_east[i]) + mul(v, climb_north[i]);
			lift[i] = f.w + mul(warmth[i] - zonal[g.band[i]], THERMAL) + mul(climb, SLOPE) + mul(std::min<fix>(out.sea[i], 0), STABLE);
			// what the air can hold: at the surface (what evaporation fills it
			// to), and once lifted (rising air holds less, sinking air more).
			// Its temperature is set over hundreds of km, not by the patch under
			// it; a quarter of the local heat keeps mountains cooling what crosses
			surface_holds[i] = capacity((3 * air[i] + heat[i]) / 4);
			holds[i] = mul(surface_holds[i], exp2(-mul(lift[i], ratio(12, 10))));
		}

		// the moisture march: vapour carried along the wind, made up toward
		// saturation over water (and from wet land), rained out where the air,
		// lifted, holds less than it carries
		std::vector<fix> q(g.n, 0), rain(g.n, 0), total(g.n, 0), next(g.n);
		for (int pass = 0; pass < SWEEPS; pass++) {
			for (int32_t i = 0; i < g.n; i++) {
				const Sphere::Spot &sp = from[i];
				const fix was = (fix)(((int64_t)q[sp.cells[0]] * sp.weights[0] + (int64_t)q[sp.cells[1]] * sp.weights[1]
						+ (int64_t)q[sp.cells[2]] * sp.weights[2]) >> FRAC);
				const fix shortfall = mul(maxx(surface_holds[i] - was, 0), EVAPORATE);
				const fix fed = elevation[i] < 0 ? shortfall : std::min(shortfall, mul(rain[i], RECYCLE));
				// rising air is fed by low air converging from all around (the
				// one mean wind can't carry that), never past saturation at the surface
				const fix drawn = lift[i] > 0 ? std::min(mul(was, mul(lift[i], CONVERGE)), maxx(surface_holds[i] - was - fed, 0)) : 0;
				const fix carried = was + fed + drawn;
				const fix fall = mul(maxx(carried - holds[i], 0), RAINOUT) + mul(carried, DRIZZLE);
				next[i] = carried - fall;
				rain[i] = fall;
			}
			// a little mixing with the cells round: the eddies the mean wind leaves out
			for (int32_t i = 0; i < g.n; i++) {
				int64_t round = 0;
				for (const int32_t *p = g.sphere.neighboursBegin(i); p != g.sphere.neighboursEnd(i); p++)
					round += next[*p];
				q[i] = mul(next[i], ratio(8, 10)) + mul((fix)(round / g.sphere.neighbourCount(i)), ratio(2, 10));
			}
			if (pass >= SWEEPS - AVERAGED)
				for (int32_t i = 0; i < g.n; i++)
					total[i] += rain[i] / AVERAGED;
		}
		out.rain[s] = total;
	}

	// relative to the median year's rain over land (half the land is wetter
	// than 1; every cell on a world of sea), then the Humidity setting (×½ .. ×2)
	std::vector<fix> years;
	for (bool all : { false, true }) {
		for (int32_t i = 0; i < g.n; i++)
			if (all || elevation[i] >= 0)
				years.push_back((out.rain[0][i] + out.rain[1][i]) / 2);
		if (!years.empty())
			break;
	}
	std::nth_element(years.begin(), years.begin() + years.size() / 2, years.end());
	const fix median = std::max<fix>(years[years.size() / 2], 1);
	const fix wetter = exp2(code.humidity);
	for (int k = 0; k < 2; k++)
		for (fix &r : out.rain[k])
			r = maxx(mul(div(r, median), wetter), ratio(2, 100));
	return out;
}

} // namespace terragen
