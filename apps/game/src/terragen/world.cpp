// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"
#include "lid.h"
#include "climate.h"
#include "planet.h"
#include "plates.h"
#include "noise.h"
#include "sun.h"
#include "parallel.h"
#include "progress.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <mutex>
#include <stdexcept>

namespace terragen {

enum MareStream : uint32_t { WRINKLE = 239, RILLE = 241, DOME = 251, RAY = 257, LINEA = 263, CHAOS = 293, CAP = 307, FLOW = 311, CANYON = 313, PATERA = 317, FOLDS = 367 };

const char *const BIOME_NAMES[(int)Biome::Count] = {
	"deep", "ocean", "seaice", "beach", "desert", "savanna", "grassland",
	"forest", "jungle", "taiga", "tundra", "rock", "snow", "monsoon", "mediterranean", "regolith", "mare", "dust", "sulfur",
	"saltflat", "marsh", "floodplain",
};

// Real land's roughness: amplitude ∝ wavelength^0.8, so each octave (half
// the wavelength) 2^−0.8 of the last
static const fix DETAIL_SLOPE = ratio(8, 10);
static const fix DETAIL_PERSISTENCE = ratio(574, 1000);
// (an fbm's typical swing is a third of its range: this is its range)
static const int32_t DETAIL_M = 1500;

// Each octave of the detail is read through one more turn of this rotation
// (a radian about (1, 2, 3), at 2³⁰), so the octaves' lattices never line
// up: alone they share the axes, and their cells show as squares
static const int64_t TURN[3][3] = {
	{ 615402086, -653915903, 588723848 },
	{ 794943514, 721172795, -29935152 },
	{ -377182430, 453017987, 897457310 },
};

static Place turned(const Place &p)
{
	auto row = [&](int k) { return (int32_t)((TURN[k][0] * p.x + TURN[k][1] * p.y + TURN[k][2] * p.z) >> 30); };
	return Place(row(0), row(1), row(2));
}

// The detail's noise, −1..1: rolling (fbm) and ridged (each octave folded at
// its middle: sharp crests, V-shaped valleys), mixed `ridges` of the way
static fix detailNoise(Place d, uint32_t seed, int32_t frequency, int octaves, fix ridges)
{
	fix sum = 0, amp = ONE, norm = 0;
	for (int o = 0; o < octaves; o++) {
		const fix n = noise(d, seed + (uint32_t)o * 1013u, frequency);
		sum += mul(amp, ridges > 0 ? n + mul(ridges, ONE - 2 * std::abs(n) - n) : n);
		norm += amp;
		amp = mul(amp, DETAIL_PERSISTENCE);
		frequency *= 2;
		d = turned(d);
	}
	return div(sum, norm);
}

// Fold ridges: this many a cell, on worlds whose cells can hold them (blocks),
// and this high (metres), young and worn down. Thrust-faulted ridges are
// steep toward the belt's edge (Davis, Suppe & Dahlen 1983; papers only)
static const int32_t FOLD_WAVES = 4, FOLD_CELL_LEAST = 256;
static const int32_t FOLD_M = 650, FOLD_OLD_M = 250;
static const fix FOLD_STEEP = ratio(30, 100);

World::World(const WorldCode &code) :
	m_code(code), m_grid(gridFor(code)), m_radius(radiusOf(code)),
	m_cell_blocks(cellBlocksOf(code)),
	// rivers' width: by the planet's size against the default's (16 384 round), never narrower
	m_river_scale(std::max<int64_t>(ONE, ((int64_t)code.size << FRAC) / 16384)),
	// meanders: about a wave a cell, counted round the equator as noise is
	m_meander((int32_t)std::max<int64_t>(8, ((int64_t)TWO_PI << FRAC) / gridSphere().spacing() >> FRAC)),
	m_height(heightScale(code)),
	m_metre(blocksPerMetre(code)),
	m_craters(code, m_radius, blocksPerMetre(code))
{
	// The detail between cells: real land's roughness falls as wavelength^0.8,
	// DETAIL_M at 4 km in a rising range; here from a cell's wavelength (in the
	// body's metres) down to 16 blocks, in blocks
	const fix s = blocksPerMetre(code);
	const int64_t cell_m = (m_cell_blocks << FRAC) / s; // fixed, 64 bits
	const fix top = pow((fix)std::max<int64_t>(1, cell_m / 4000), DETAIL_SLOPE);
	m_detail = mul(mul(mul(toFix(DETAIL_M), top), s), code.processes.stiff);
	m_detail_octaves = 1;
	for (int64_t w = m_cell_blocks >> FRAC; w > 32; w /= 2)
		m_detail_octaves++;
	if (m_cell_blocks >> FRAC >= FOLD_CELL_LEAST)
		m_fold_waves = FOLD_WAVES;
	// the land from the drainage (sprint 108, TF19); TERRAGEN_LAND=noise, the
	// old land (noise with valleys cut in), for the region tools to compare
	const char *land = std::getenv("TERRAGEN_LAND");
	m_land = !(land && std::string(land) == "noise");
	if (code.processes.water) {
		const Grid &g = *m_grid;
		m_upstream.assign((size_t)g.size(), -1);
		for (int32_t c = 0; c < g.size(); c++) {
			const int32_t r = g.receiver[c];
			if (r >= 0 && (drawn(c) || (m_land && g.base[c] >= 0)) && (m_upstream[r] < 0 || g.drained[c] > g.drained[m_upstream[r]]))
				m_upstream[r] = c;
		}
		Step step(code.text, "rivers' runs");
		shapeRuns();
		deltas();
	}
	// (after the runs: its streams end on their lines)
	{
		Step step(code.text, "channels");
		m_channels.emplace(code, *m_grid, *this, m_radius, m_cell_blocks, m_land);
	}
	if (!m_channels->active())
		m_channels.reset();
	// (after the channels: the ground along the runs is cut by their valleys)
	if (code.processes.water)
		profileRuns();
	// rays fade within a billion years or so (space weathering darkens them):
	// of the craters made after the lava, the wide ones a third of the time
	for (size_t k = 0; k < m_grid->craters.size(); k++) {
		const Crater &c = m_grid->craters[k];
		if (c.young && c.complex && plates::mix((uint32_t)k, RAY, m_code.seed_hash) % 3 == 0)
			m_rayed.push_back(c);
	}
}

std::shared_ptr<const World> worldFor(const WorldCode &code)
{
	using Built = std::shared_future<std::shared_ptr<const World>>;
	static std::mutex lock;
	static std::vector<std::pair<std::string, Built>> recent; // by the code's text, latest first
	std::promise<std::shared_ptr<const World>> building;
	Built built;
	bool mine = false; // this call builds it; else whoever is, waited for outside the lock
	{
		std::lock_guard<std::mutex> guard(lock);
		for (auto &entry : recent)
			if (entry.first == code.text)
				built = entry.second;
		if (!built.valid()) {
			mine = true;
			built = building.get_future().share();
			recent.insert(recent.begin(), { code.text, built });
			if (recent.size() > 4)
				recent.pop_back();
		}
	}
	if (mine) {
		beginMaking(code.text);
		try {
			building.set_value(std::make_shared<const World>(code));
			endMaking(code.text);
		} catch (...) {
			endMaking(code.text);
			building.set_exception(std::current_exception());
			std::lock_guard<std::mutex> guard(lock);
			// asked again, tried again
			recent.erase(std::remove_if(recent.begin(), recent.end(), [&](const auto &entry) { return entry.first == code.text; }), recent.end());
		}
	}
	return built.get(); // (rethrows what building it threw)
}

fix blocksPerMetre(const WorldCode &code)
{
	// √ of R / R⊕, with 16 bits more than a fix holds (the smallest world's
	// ratio is 27 / 65 536 as a fix)
	const int64_t ratio = std::min<int64_t>((int64_t)ONE << FRAC, (World::radiusOf(code) << FRAC) / EARTH_RADIUS_M);
	return sqrtWide64(ratio) >> 8;
}

int64_t World::radiusOf(const WorldCode &code)
{
	return ((int64_t)code.size << (2 * FRAC)) / TWO_PI;
}

int64_t World::cellBlocksOf(const WorldCode &code)
{
	return blocksOf(gridSphere().spacing(), radiusOf(code));
}

Place World::place(fix u, fix v)
{
	return dirAt(mul(HALF - v, PI), mul(u - HALF, TWO_PI));
}

fix World::u(const Dir &d)
{
	fix u = div(longitudeOf(d), TWO_PI) + HALF;
	return u >= ONE ? u - ONE : u < 0 ? u + ONE : u;
}

fix World::v(const Dir &d)
{
	return HALF - div(latitudeOf(d), PI);
}

int32_t World::blocksBetween(const Place &a, const Place &b) const
{
	return (int32_t)(blocksFine(chord(a, b)) >> FRAC);
}

std::array<int32_t, 3> World::solid(const Place &d, int32_t y) const
{
	const int64_t r = m_radius + toFix(y); // blocks from the centre, fixed
	// rounded: (c · r) / 2^(30 + 16), half up
	auto at = [&](int32_t c) { return (int32_t)((mulShift(c, r, PLACE_FRAC + FRAC - 1) + 1) >> 1); };
	return { at(d.x), at(d.y), at(d.z) };
}

fix World::sunlight(const Place &d, std::optional<fix> date) const
{
	return terragen::sunlight(m_code, v(d.dir()), date);
}

/*
	Land biomes by climate, as data: the first niche a column fits. Heat
	(−1..1), wetness (−1..1, from the year's rain) and seasonality (1: the
	rain all falls in summer, −1: all in winter). Monsoon forest needs hot
	land whose rain comes in summer; Mediterranean scrub, mild land whose
	rain comes in winter. Hot land with a dry season and less rain is savanna.
*/
struct Niche
{
	fix heat_lo, heat_hi, wet_lo, wet_hi, season_lo, season_hi;
	Biome biome;
};

const fix ANY = toFix(9);

const Niche NICHES[] = {
	{ -ANY, -ratio(30, 100), 0, ANY, -ANY, ANY, Biome::Taiga },
	{ -ANY, -ratio(30, 100), -ANY, ANY, -ANY, ANY, Biome::Tundra },
	{ ratio(20, 100), ANY, ratio(5, 100), ANY, ratio(55, 100), ANY, Biome::Monsoon },
	{ ratio(35, 100), ANY, ratio(10, 100), ANY, -ANY, ratio(55, 100), Biome::Jungle },
	{ ratio(30, 100), ANY, -ratio(55, 100), ANY, ratio(25, 100), ANY, Biome::Savanna }, // a wet season, then a dry one
	{ ratio(19, 100), ANY, -ANY, -ratio(15, 100), -ANY, ANY, Biome::Desert }, // hot desert from an 18 °C year (Köppen's BWh)
	{ ratio(35, 100), ANY, -ANY, ANY, -ANY, ANY, Biome::Savanna },
	{ 0, ratio(35, 100), -ratio(60, 100), ratio(40, 100), -ANY, -ratio(30, 100), Biome::Mediterranean },
	{ -ANY, ANY, ratio(5, 100), ANY, -ANY, ANY, Biome::Forest },
	{ -ANY, ANY, -ANY, ANY, -ANY, ANY, Biome::Grassland },
};

// A lifeless body's one biome, by its bare ground (Surface; Soil never bare)
static const Biome BARE[] = { Biome::Rock, Biome::Rock, Biome::Desert, Biome::Snow, Biome::Regolith, Biome::Dust, Biome::Sulfur };
// …and on the maria's lava (dark regolith where it's ground to dust)
static const Biome FLOODED[] = { Biome::Rock, Biome::Rock, Biome::Desert, Biome::Snow, Biome::Mare, Biome::Dust, Biome::Sulfur };

static Biome classify(fix e, fix heat, fix wet, fix season)
{
	if (e < 0)
		return heat < -ratio(75, 100) ? Biome::SeaIce : e < -ratio(18, 100) ? Biome::Deep : Biome::Ocean;
	if (e < ratio(3, 100))
		return heat < -ratio(60, 100) ? Biome::Snow : Biome::Beach;
	if (heat < -ratio(60, 100))
		return Biome::Snow;
	if (e > ratio(50, 100))
		return heat < -ratio(10, 100) ? Biome::Snow : Biome::Rock;
	for (const Niche &n : NICHES)
		if (heat >= n.heat_lo && heat < n.heat_hi && wet >= n.wet_lo && wet < n.wet_hi && season >= n.season_lo && season < n.season_hi)
			return n.biome;
	return Biome::Grassland; // the last niche takes everything
}

// Coasts by their rock (sprint 97): how hard each province's rock is to the
// sea. Old shields and ranges' cores stand as cliffs; sediment basins, rifts
// and young volcanic ground give way to beaches and coves.
static const fix HARD[(int)Province::Count] = {
	ONE,               // shield
	ratio(85, 100),    // orogen
	ratio(60, 100),    // arc
	ratio(30, 100),    // rift
	0,                 // basin
	ratio(45, 100),    // volcanic
	0,                 // ocean floor
	ratio(70, 100),    // highland
	ratio(50, 100),    // mare
};

fix World::hardness(const Dir &d) const
{
	const Grid &g = *m_grid;
	const Sphere::Spot spot = gridSphere().spot(d);
	fix h = 0;
	for (int k = 0; k < 3; k++)
		h += mul(HARD[(int)g.province[spot.cells[k]]], spot.weights[k]);
	// sand and silt laid along it bury the rock
	return mul(h, ONE - alluvium(between(g.sediment, d)));
}

int32_t World::cell(const Dir &d) const
{
	return gridSphere().cell(d);
}

void World::volcanic(const Place &at, int32_t &raise, int32_t &lava, int32_t &floor) const
{
	const Grid &g = *m_grid;
	raise = 0;
	lava = INT32_MIN;
	floor = INT32_MIN;
	int32_t i = cell(at);
	for (uint32_t k = g.volcano_start[i]; k < g.volcano_start[i + 1]; k++) {
		const Volcano &vo = g.volcanoes[g.volcano_list[k]];
		int64_t far = blocksFine(chord(at, Place(vo.at))); // blocks, fixed
		// (a patera's rim is a broken outline, never a circle)
		if (vo.kind == VolcanoKind::Patera && far < 2 * (int64_t)toFix(vo.radius))
			far = mulShift(far, ONE + fbm(at, m_code.seed_hash + PATERA + g.volcano_list[k], m_meander / 2, 3, HALF) / 4, FRAC);
		if (far >= toFix(vo.radius))
			continue;
		const fix d = (fix)far; // within its slopes: fits
		// the cone: (1 − d/r)^1.5, steeper near the top
		fix t = ONE - d / vo.radius;
		int32_t cone = floorInt(mul(toFix(vo.height), mul(t, sqrt(t))));
		if (d < toFix(vo.crater)) {
			// the crater: a bowl sunk below the rim
			fix r = div(d, toFix(vo.crater));
			fix rim_t = ONE - div(toFix(vo.crater), toFix(vo.radius));
			int32_t rim = floorInt(mul(toFix(vo.height), mul(rim_t, sqrt(rim_t))));
			int32_t depth = vo.depth;
			// built from the vent's ground, so the rim is level and a lava lake sits flat in it
			floor = std::max(floor, vo.base + rim - floorInt(mul(toFix(depth), ONE - mul(r, r))));
			if (vo.active)
				lava = std::max(lava, vo.base + rim - depth / 2);
			continue;
		}
		raise = std::max(raise, cone);
	}
}

fix World::between(const std::vector<fix> &f, const Dir &d) const
{
	const Sphere::Spot spot = gridSphere().spot(d);
	int64_t sum = 0;
	for (int k = 0; k < 3; k++)
		sum += (int64_t)f[spot.cells[k]] * spot.weights[k];
	return (fix)(sum >> FRAC);
}

Biome World::bareBiome(const Dir &d, bool flooded) const
{
	const Processes &p = m_code.processes;
	// what's left of the seas' water, frozen at the poles (Mars's caps reach
	// some 70°–80°), its edge wandering
	if (p.ancient_water > 0 && fabsx(latitudeOf(d)) + fbm(d, m_code.seed_hash + CAP, 12, 3, HALF) / 8 > ratio(1309, 1000)) // 75°
		return Biome::Snow;
	// fresh dark lava showing through the sulfur that snows on it
	if (p.eruptions > 0 && fbm(d, m_code.seed_hash + FLOW, 24, 4, HALF) > ratio(15, 100))
		return Biome::Rock;
	return (flooded ? FLOODED : BARE)[(int)p.bare];
}

bool World::bySea(const Dir &d) const
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	for (int32_t i : sphere.spot(d).cells) {
		if (g.base[i] < 0)
			return true;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			if (g.base[*p] < 0)
				return true;
	}
	return false;
}

Column World::column(const Place &p, fix sun) const
{
	// the climate and the grid's fields: at cell scale
	const Dir d = p.dir();
	const WorldCode &c = m_code;
	const fix v = this->v(d);
	fix e = between(m_grid->base, d);
	if (e >= 0) // only the land wears down; the sea floor stays as made
		e = std::max<fix>(e + between(m_grid->worn, d), 0);
	// the year's heat here, the season's on top (by the sunlight above the
	// year's), cooler with height
	fix heat = between(m_grid->heat, d) + mul(sun - terragen::sunlight(c, v, std::nullopt), between(m_grid->heat_gain, d))
			- mul(maxx(0, e), LAPSE);
	// the climate's rain here, in each season; the hemisphere decides which is summer
	fix north = between(m_grid->rain_season[0], d), south = between(m_grid->rain_season[1], d);
	fix rain = (north + south) / 2;
	fix summer = v < HALF ? north : south, winter = v < HALF ? south : north;
	// (a dry body has no rain in either season, and no seasonality of it)
	fix season = summer + winter > 0 ? div(summer - winter, summer + winter) : 0;
	// as the grid reads it (grid.cpp): the seasons' fields are interpolated and
	// can dip to nothing in the driest places; below 1/100 it's as dry as dry
	// gets (−1) either way
	fix wet = clamp(log2(maxx(rain, ratio(1, 100))) / 2, -ONE, ONE);

	Biome biome = classify(e, heat, wet, season);
	// a beach only where sand gathers or the rock is soft; a hard coast is
	// cliffs (ground()), its top the land's biome
	if (biome == Biome::Beach && c.processes.water && hardness(d) > HALF)
		biome = classify(ratio(3, 100), heat, wet, season);
	// and only by the sea: low land inland (a worn shield, a plain) is the
	// land's own (low alone, a third of some worlds' land was sand)
	if (biome == Biome::Beach && c.processes.water && !bySea(d))
		biome = classify(ratio(3, 100), heat, wet, season);
	// a lifeless body: one bare biome over all of it, low ground and poles too
	if (!c.processes.life)
		biome = bareBiome(d, between(m_grid->mare, d) > 0);
	return { e, heat, wet, rain, season, biome };
}


// The daily swing either side of the day's mean, in °C: 6 where the air is
// average, 3 where soaked, 9 where arid; half that over water.
static fix dailySwing(const Column &c)
{
	fix swing = toFix(6) - c.wet * 3;
	return c.elevation < 0 ? swing / 2 : swing;
}

fix World::temperature(const Place &d, fix date, fix hour) const
{
	Column c = column(d, sunlight(d, date));
	// warmest at 15:00, coldest at 03:00
	// (none where the sun never moves)
	fix daily = m_code.processes.facing_star ? 0 : mul(dailySwing(c), cos(mul(TWO_PI, hour - ratio(5, 8))));
	return celsiusOf(c.heat) + daily;
}

// the finest lattice this thread's columns read (World::Coarse), place units
static thread_local int64_t t_least = 0;

World::Coarse::Coarse(const World &world, int64_t cell_blocks)
{
	t_least = cell_blocks > 1 ? (int64_t)(((__int128)cell_blocks * PLACE_ONE << FRAC) / world.m_radius) : 0;
}

World::Coarse::~Coarse() { t_least = 0; }

const World::ChannelsAt &World::channelsAt(const Place &at) const
{
	thread_local ChannelsAt last;
	const Channels *from = &*m_channels;
	if (last.from == from && last.least == t_least && last.at.x == at.x && last.at.y == at.y && last.at.z == at.z)
		return last;
	// (read in the rivers' own warp, nothing more: the streams end on their lines)
	std::array<int64_t, 3> w{ 0, 0, 0 };
	for (int k = 0; k < 3; k++) {
		// (the rivers' warp, ground()'s)
		const fix f = fbm(at, m_code.seed_hash + MEANDER + (uint32_t)k, m_meander / 2, 2, HALF) / 3
				+ fbm(at, m_code.seed_hash + MEANDER + 3 + (uint32_t)k, m_meander * 2, 2, HALF) / 16;
		w[k] += (int64_t)f * gridSphere().spacing() >> (2 * FRAC - PLACE_FRAC);
		last.warp[k] = f;
	}
	const fix belt = between(m_grid->tectonics.belt, at);
	const fix range = clamp(div(belt - ratio(18, 100), ratio(62, 100)), 0, ONE);
	last.read = m_channels->read(placeOf((int64_t)at.x + w[0], (int64_t)at.y + w[1], (int64_t)at.z + w[2]), range, t_least);
	last.from = from;
	last.least = t_least;
	last.at = at;
	return last;
}

World::Folds World::folds(const Place &at) const
{
	Folds out;
	if (m_fold_waves == 0)
		return out;
	const Tectonics &t = m_grid->tectonics;
	const fix weight = between(t.fold, at);
	if (weight <= 0)
		return out;
	const uint32_t seed = m_code.seed_hash + FOLDS;
	// contours of the way in from the belt's edge, wandering by a ridge or so,
	// finer too, so they bend, split and close (the Appalachians' zigzags, the
	// Zagros' whalebacks), not run parallel for a continent;
	// (the cell's fraction is what's kept: a cell's height fits a fix)
	const fix front = between(t.front, at) + fbm(at, seed, m_meander, 2, HALF) / 4 + fbm(at, seed + 2, m_meander * 4, 2, HALF) / 12;
	const fix u = (fix)((front * m_fold_waves) & (ONE - 1));
	const fix up = u < FOLD_STEEP ? div(u, FOLD_STEEP) : div(ONE - u, ONE - FOLD_STEEP);
	out.crest = smooth(up);
	// each ridge plunges and dies out along its length, and the next takes over
	// (in patches: about half of a belt has standing ridges, the rest is worn low)
	const fix live = smooth(clamp(ratio(15, 100) + fbm(at, seed + 1, m_meander, 3, HALF) * 3, 0, ONE));
	const fix metres = toFix(FOLD_M) + mul(toFix(FOLD_OLD_M - FOLD_M), between(t.oldness, at));
	// (no steeper than the ridges are wide can carry)
	const fix most = (fix)std::min<int64_t>((m_cell_blocks / m_fold_waves) / 3, toFix(4000));
	out.height = mul(mul(std::min(mul(metres, m_metre), most), weight), live);
	return out;
}

// Gullies: an erosion filter (after Rune Skovbo Johansen's, 2023), the
// valleys and spurs a slope is worn into below what the grid's erosion sees.
// Each octave is waves running down the slope (crests and troughs along the
// fall line), blended between lattice points by phase so they stay waves;
// each finer octave runs down the slope the coarser ones left, so the small
// gullies run off the big ones' spurs into their floors and the drainage
// branches. Wavelengths from a cell down, the first GULLY_HEIGHT of its
// tall at full strength, each finer GULLY_GAIN of the last (gentler than
// halving with the wave: the fine rills mustn't be cliffs).
static const int GULLY_OCTAVES = 6;
static const fix GULLY_HEIGHT = ratio(22, 100), GULLY_GAIN = ratio(42, 100);
// slopes gentler than this are cut less (blocks a block), to GULLY_FLAT of
// it on the level: a range's crest is dissected too, a plain's barely (its
// strength is the ruggedness: small there)
static const fix GULLY_SLOPE = ratio(25, 100), GULLY_FLAT = ratio(45, 100);

fix World::gullies(const Place &at, fix strength) const
{
	if (strength <= 0)
		return 0;
	const Dir up = at.dir();
	// the tangent plane: east and north (any pair at a pole)
	const int64_t flat = (int64_t)up.x * up.x + (int64_t)up.z * up.z;
	Dir east = flat > ((int64_t)ONE * ONE >> 12) ? Dir{ up.z, 0, -up.x } : Dir{ 0, up.z, -up.y };
	{
		const fix l = sqrt(mul(east.x, east.x) + mul(east.y, east.y) + mul(east.z, east.z));
		east = { div(east.x, l), div(east.y, l), div(east.z, l) };
	}
	const Dir north{ mul(up.y, east.z) - mul(up.z, east.y), mul(up.z, east.x) - mul(up.x, east.z), mul(up.x, east.y) - mul(up.y, east.x) };
	// the slope the grid left, blocks a block, across half a cell each way
	const int64_t radius = m_radius >> FRAC, cell_blocks = m_cell_blocks >> FRAC; // blocks
	const int64_t reach = std::max<int64_t>(1, cell_blocks / 2);
	const fix step = (fix)std::max<int64_t>(1, (reach << FRAC) / radius); // radians
	const auto land = [&](const Dir &t, int sign) {
		const Dir p{ up.x + sign * mul(t.x, step), up.y + sign * mul(t.y, step), up.z + sign * mul(t.z, step) };
		return std::max<fix>(between(m_grid->base, p) + between(m_grid->worn, p), 0);
	};
	const auto slopeAlong = [&](const Dir &t) {
		return (fix)(((int64_t)mul(land(t, 1) - land(t, -1), m_height) << FRAC) / (2 * reach) >> FRAC);
	};
	fix se = slopeAlong(east), sn = slopeAlong(north);
	strength = mul(strength, GULLY_FLAT + mul(ONE - GULLY_FLAT, smooth(clamp(div((fix)isqrt((uint64_t)((int64_t)se * se + (int64_t)sn * sn)), GULLY_SLOPE), 0, ONE))));
	if (strength <= 0)
		return 0;

	const int64_t least = (int64_t)(((__int128)t_least * radius) >> PLACE_FRAC); // blocks
	int64_t wave = std::max<int64_t>(16, cell_blocks); // blocks
	int64_t height = 0;                                     // blocks, fixed
	fix amp = (fix)std::min<int64_t>(mul(GULLY_HEIGHT, strength) * wave, INT32_MAX / 8); // blocks, fixed
	for (int o = 0; o < GULLY_OCTAVES && wave >= 8; o++, wave /= 2) {
		// across the slope (unit, in east and north): the waves' crests run down it
		// (in 64 bits: a slope of a few thousandths squared rounds to nothing in a
		// fix, and its way across snapped from the default to its own a block on)
		const fix s = (fix)isqrt((uint64_t)((int64_t)se * se + (int64_t)sn * sn));
		// (none on the level: where a crest or a valley floor turns the slope
		// over, its way across swings round in a step and the waves would ladder)
		const fix riding = smooth(clamp(div(s, GULLY_SLOPE), 0, ONE));
		const fix ae = s > 0 ? div(-sn, s) : ONE, an = s > 0 ? div(se, s) : 0;
		const Dir across{ mul(east.x, ae) + mul(north.x, an), mul(east.y, ae) + mul(north.y, an), mul(east.z, ae) + mul(north.z, an) };
		// the lattice, a wave apart (places: 2³⁰ the radius)
		const int64_t cell = std::max<int64_t>(1, (wave << PLACE_FRAC) / radius);
		const int64_t p[3] = { at.x, at.y, at.z };
		int64_t base[3];
		fix t[3];
		for (int k = 0; k < 3; k++) {
			base[k] = p[k] >= 0 ? p[k] / cell : -((-p[k] + cell - 1) / cell);
			t[k] = (fix)(((p[k] - base[k] * cell) << FRAC) / cell);
		}
		const fix w[3] = { smooth(t[0]), smooth(t[1]), smooth(t[2]) };
		int64_t c = 0, sn_ = 0; // the waves blended: Σ weight·(cos, sin)
		for (int corner = 0; corner < 8; corner++) {
			const int dx = corner & 1, dy = corner >> 1 & 1, dz = corner >> 2 & 1;
			const fix weight = mul(mul(dx ? w[0] : ONE - w[0], dy ? w[1] : ONE - w[1]), dz ? w[2] : ONE - w[2]);
			if (weight == 0)
				continue;
			// how far across from the corner, in waves; a phase of its own
			const fix off = mul(t[0] - dx * ONE, across.x) + mul(t[1] - dy * ONE, across.y) + mul(t[2] - dz * ONE, across.z);
			uint32_t h = (m_code.seed_hash + GULLIES + (uint32_t)o * 7919u) ^ (uint32_t)(base[0] + dx) * 0x27d4eb2du
					^ (uint32_t)(base[1] + dy) * 0x165667b1u ^ (uint32_t)(base[2] + dz) * 0x9e3779b1u;
			h = (h ^ (h >> 15)) * 0x2c1b3c6du;
			h = (h ^ (h >> 12)) * 0x297a2d39u;
			h ^= h >> 15;
			fix sv, cv;
			// (a phase of 0 or π: then a wave is the same read either way across,
			// and where the slope turns over, on a crest or in a valley, the
			// waves don't jump with it)
			sincos(mul(off, TWO_PI) + ((h >> 7) & 1 ? PI : 0), sv, cv);
			c += (int64_t)weight * cv;
			sn_ += (int64_t)weight * sv;
		}
		const fix cf = (fix)(c >> FRAC), sf = (fix)(sn_ >> FRAC);
		// normalised (a blend of waves is lower than a wave), but no more than
		// from half: where corners' waves cancel the sum turns over through
		// nothing, and normalised whole it flipped there, drawing the lattice
		const fix m = std::max<fix>(sqrt(mul(cf, cf) + mul(sf, sf)), HALF);
		// (waves under two of the cells read (World::Coarse) left out, fading in
		// to four: sampled a cell apart they'd beat against the atlas's grid in
		// stripes across a continent)
		const fix shown = least > 0 ? smooth(clamp((fix)(((wave - 2 * least) << FRAC) / (2 * least)), 0, ONE)) : ONE;
		if (shown == 0)
			break;
		// (the coarsest on gentler slopes too, so a range's crest is dissected,
		// but none on the level: at a summit the slope's way turns all round
		// and its waves would run in to a point)
		const fix crest = smooth(clamp(div(s, GULLY_SLOPE / 4), 0, ONE));
		const fix wave_amp = mul(o == 0 ? mul(amp, mul(crest, ratio(35, 100)) + mul(riding, ratio(65, 100))) : mul(amp, riding), shown);
		height += (int64_t)mul(wave_amp, div(cf, m));
		// and the slope it leaves, for the next octave: d/d(across) of amp·cos is −amp·sin·2π/wave
		const fix dv = (fix)(-(int64_t)mul(mul(wave_amp, div(sf, m)), TWO_PI) / wave);
		se += mul(dv, ae);
		sn += mul(dv, an);
		amp = mul(amp, GULLY_GAIN);
	}
	return (fix)height;
}

// Alpine crests: over ALPINE_FROM_M where the range is past ALPINE_RANGE of
// its rise, ridges up to ALPINE_M
static const int32_t ALPINE_FROM_M = 1500, ALPINE_M = 1200;
static const fix ALPINE_RANGE = ratio(40, 100);
// Glaciers ran where the yearly mean is now under GLACIAL_BELOW (°C), their
// valleys GLACIAL_DEEPER deeper than a river's, and however gently the land
// falls there, at least GLACIAL_TROUGH_M
static const fix GLACIAL_BELOW = toFix(1), GLACIAL_DEEPER = ratio(60, 100);
static const int32_t GLACIAL_TROUGH_M = 400;
// Snow lies on slopes gentler than this (35°: steeper, it sloughs off); above
// the trees, ground steeper than ALPINE_ROCK is bare rock (the prototype's
// land, by its own slope)
static const fix SNOW_STEEPEST = ratio(70, 100), ALPINE_ROCK = ratio(50, 100);
// How much smoother a shield's bumps are than a young plain's (Tectonics::stable)
static const fix SHIELD_SMOOTH = ratio(60, 100);
// how deep the channels' valleys are cut at their deepest (blocks), on a plain and in a range
static const int32_t VALLEY_PLAIN = 6, VALLEY_RANGE = 45;

// how far over and under the sea a coast is worn (blocks), and by how much at most;
static const int32_t COAST_REACH = 40, COAST_RELIEF = 20;
// how far a coast's height is read off, in cells, on the hardest rock
static const fix COAST_WARP = ratio(70, 100);

// Drowned valleys (sprint 108's prototype, P6): how far a valley's floor
// sinks at the coast, by ice, and how far inland the sinking reaches (by the
// land's height there), metres
static const int32_t DROWN_M = 60, FJORD_M = 300, DROWN_BAND_M = 600;

int32_t World::surface(fix e, const Place &d) const
{
	// The coast follows the land between cell centres, which alone would
	// draw it in the cells' straight edges: it wanders by noise about a cell
	// long (a few blocks of height, so flat coasts wander far, cliffs not).
	fix shore = fbm(d, m_code.seed_hash + SHORE, m_meander, 5, HALF);
	fix base = mul(e, m_height) + shore * 3;
	// Bays and headlands. A rocky coast is ragged (coves, points, drowned
	// valleys: Cornwall, Brittany, Maine), a sandy one smooth and straight
	// (the sand fills its bays): near the sea, the land's height is read a
	// way off, by noise from a cell down to a sixteenth of one, as far as the
	// rock is hard. Then the soft beds between harder ones are worn back a
	// little further (beds: harder and softer rock come and go along a coast).
	// (over the sea, only its shallows: the deep floor is no coast)
	const fix inshore = base < 0 ? div(-base, toFix(COAST_REACH)) : div(base, toFix(COAST_REACH * 3));
	if (m_code.processes.water && inshore < ONE) {
		const Sphere &sphere = gridSphere();
		const Sphere::Spot spot = sphere.spot(d.dir());
		// how much of the cells' neighbourhood is sea, between the cells: a low
		// plain far inland isn't a coast
		fix coast = 0;
		for (int k = 0; k < 3; k++) {
			const int32_t c = spot.cells[k];
			int sea = m_grid->base[c] < 0, all = 1;
			for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++, all++)
				sea += m_grid->base[*p] < 0;
			coast += mul(spot.weights[k], (fix)std::min<int64_t>(ONE, (int64_t)sea * 2 * ONE / all));
		}
		const fix hard = hardness(d.dir());
		const fix weight = mul(coast, ONE - smooth(inshore));
		if (weight > 0) {
			const fix reach = mul(mul(COAST_WARP, sphere.spacing()), ratio(25, 100) + mul(hard, ratio(75, 100)));
			const Dir at = d.dir();
			Dir w{};
			fix *const parts[3] = { &w.x, &w.y, &w.z };
			const fix along[3] = { at.x, at.y, at.z };
			for (int k = 0; k < 3; k++)
				*parts[k] = along[k] + mul(reach, clamp(fbm(d, m_code.seed_hash + SHORE + 3 + (uint32_t)k, m_meander, 4, ratio(55, 100)) * 3, -ONE, ONE)); // (an fbm swings a third of its range)
			fix ew = between(m_grid->base, w);
			if (ew >= 0)
				ew = std::max<fix>(ew + between(m_grid->worn, w), 0);
			base = lerp(base, mul(ew, m_height) + shore * 3, weight);
		}
		const fix beds = hard + fbm(d, m_code.seed_hash + SHORE + 2, m_meander * 2, 3, HALF) - HALF;
		const fix near = mul(ONE - smooth(clamp(div(std::abs(base), toFix(COAST_REACH)), 0, ONE)), coast);
		base += mul(mul(clamp(beds * 2, -ONE, ONE), toFix(COAST_RELIEF)), near);
	}
	// The fine detail fades to nothing at the waterline (never more than half
	// the height over or under it), so it never flips land and sea column by
	// column: the coastline is the base's, a smooth line.
	// (rougher where ranges are rising and with the Roughness setting;
	// gentler under the sea)
	// plains rolling (a tenth of a range's), rising ranges ridged
	const fix belt = between(m_grid->tectonics.belt, d);
	const fix range = clamp(div(belt - ratio(18, 100), ratio(62, 100)), 0, ONE);
	fix detail;
	if (m_channels && base >= 0) {
		// the valleys and what's between them; below the finest, noise
		const int levels = m_channels->levels();
		const fix fine = detailNoise(d, m_code.seed_hash + DETAIL, m_meander << (levels + 1), std::max(1, m_detail_octaves - levels - 1), range);
		detail = channelsAt(d).read.relief + fine / 8;
	} else if (base < 0) {
		// the sea floor's, read through a warp half its coarsest lattice deep:
		// value noise alone shows its lattice as squares where nothing else
		// (valleys, gullies) breaks it up
		const int64_t by = PLACE_ONE / std::max(1, m_meander) / 2;
		const Place w = placeOf((int64_t)d.x + mulShift(fbm(d, m_code.seed_hash + DETAIL + 1, m_meander * 2, 2, HALF), by, FRAC),
				(int64_t)d.y + mulShift(fbm(d, m_code.seed_hash + DETAIL + 2, m_meander * 2, 2, HALF), by, FRAC),
				(int64_t)d.z + mulShift(fbm(d, m_code.seed_hash + DETAIL + 3, m_meander * 2, 2, HALF), by, FRAC));
		detail = detailNoise(w, m_code.seed_hash + DETAIL, m_meander, m_detail_octaves, range);
	} else {
		detail = detailNoise(d, m_code.seed_hash + DETAIL, m_meander, m_detail_octaves, range);
	}
	fix rugged = mul(ratio(1, 10) + mul(range, ratio(9, 10)), HALF + m_code.roughness);
	// a shield's worn smooth: gently rolling, not a plain's hummocks
	rugged = mul(rugged, ONE - mul(between(m_grid->tectonics.stable, d), SHIELD_SMOOTH));
	// (a dry body has no waterline: all of it as gentle as the low ground)
	if (base < 0 || !m_code.processes.water)
		rugged = rugged * 2 / 5;
	// lava set level: the maria are smooth plains (deeper than 125 m, a tenth)
	const fix lava = clamp(div(between(m_grid->mare, d), ratio(1, 40)), 0, ONE);
	rugged = mul(rugged, ONE - mul(lava, ratio(9, 10)));
	// a high plain's a plain; between fold ridges the hills are small
	const Tectonics &t = m_grid->tectonics;
	const fix plain = between(t.plateau, d);
	const Folds ridges = base > 0 ? folds(d) : Folds{};
	// (its own bumps mostly; its edges and top are still cut by gullies below,
	// as high ground is: a dissected plateau, not a table)
	rugged = mul(rugged, ONE - mul(plain, ratio(85, 100)));
	const fix high_plain = plain;
	// (what wears the slopes reads it before this: the ridges start sharp at a
	// fold belt's edge, and gullies cut by it there drew the edge as a step)
	const fix worn_by = rugged;
	if (ridges.height > 0)
		rugged = mul(rugged, ONE - between(t.fold, d) / 2);
	fix amplitude = mul(m_detail, rugged);
	// where slopes are worn into gullies (below), the bumps are mostly worn away
	if (m_code.processes.water && base > 0)
		amplitude = mul(amplitude, ONE - mul(range, ratio(7, 10)));
	if (m_code.processes.water)
		amplitude = std::min<fix>(amplitude, std::abs(base) / 2);
	fix ridge = 0;
	if (ridges.height > 0) {
		// centred on the land; a large river keeps its way through (a water gap),
		// and a stream cuts a notch where it crosses
		ridge = mul(ridges.height, ridges.crest - HALF);
		if (m_channels) {
			const Channels::Read &read = channelsAt(d).read;
			ridge = mul(ridge, smooth(clamp(read.trunk * 4, 0, ONE)));
			if (read.stream_from > 0)
				ridge = mul(ridge, clamp((fix)std::min<int64_t>(read.stream_gap / 12, ONE), 0, ONE));
		}
		// (never flipping land to sea)
		ridge = std::max<fix>(ridge, -base / 2);
	}
	// worn into the slopes: as rugged as the land, never flipping it to sea
	fix gully = 0;
	if (m_code.processes.water && base > 0) {
		// (as the ruggedness to the 1.5: a range's slopes cut deep; and none
		// away from rising ground, or a plain's long gentle fall is ribbed)
		const fix cut = std::max(mul(clamp(div(worn_by, HALF + m_code.roughness), 0, ONE), smooth(clamp(range * 3, 0, ONE))),
				mul(high_plain, ratio(70, 100)));
		gully = gullies(d, mul(cut, sqrt(cut)));
		gully = clamp(gully, -base / 2, base / 2);
	}
	// Alpine crests: a rising range's high core is cut into knife ridges
	// (arêtes) between the ice-carved hollows, not rounded: the channels'
	// own divides (so the crests branch as the drainage does), raised and
	// sharpened (t²: a flat floor, a crest that comes to an edge), where the
	// range rises fastest and stands over ALPINE_FROM_M
	fix crests = 0;
	if (m_channels && base > 0 && range > ALPINE_RANGE) {
		const fix from = mul(toFix(ALPINE_FROM_M), m_metre);
		const fix alpine = mul(smooth(clamp(div(range - ALPINE_RANGE, ONE - ALPINE_RANGE) * 2, 0, ONE)),
				smooth(clamp(div(base - from, from), 0, ONE)));
		if (alpine > 0) {
			const fix t = clamp((channelsAt(d).read.relief + ONE) / 2, 0, ONE);
			crests = mul(mul(alpine, mul(toFix(ALPINE_M), m_metre)), mul(t, t) - ratio(33, 100));
		}
	}
	// valleys cut by the channels' flow: deep in a range, a few blocks on a
	// plain (VALLEY_PLAIN to VALLEY_RANGE at the floor of the deepest), never
	// to the sea
	fix valley = 0;
	if (m_channels && m_code.processes.water && base > 0) {
		// (by how much the ground's rising, a plateau's edge too: not the bumps' ruggedness, which plateaus and folds keep low)
		const fix rising = std::max(smooth(clamp(range * 2, 0, ONE)), mul(high_plain, ratio(70, 100)));
		fix depth = toFix(VALLEY_PLAIN) + mul(mul(rising, HALF + m_code.roughness), toFix(VALLEY_RANGE - VALLEY_PLAIN));
		fix cut = channelsAt(d).read.cut;
		// Glacial valleys: where the ice ages' glaciers ran (cold enough
		// now, a few degrees colder then), the ice widened the river's V into
		// a U, a flat floor under steep walls, deeper
		// (not fjords yet: a valley cut under the sea here is narrower than
		// the far view reads, which would draw it land; they're the grid's to cut)
		const fix cold = heatOfCelsius(GLACIAL_BELOW) - (between(m_grid->heat, d) - mul(std::max<fix>(0, e), LAPSE));
		const fix glacial = smooth(clamp(cold * 4, 0, ONE));
		if (glacial > 0) {
			cut = lerp(cut, smooth(clamp(cut * 2, 0, ONE)), glacial);
			depth = std::max(depth + mul(depth, mul(glacial, GLACIAL_DEEPER)), mul(glacial, mul(toFix(GLACIAL_TROUGH_M), m_metre)));
		}
		valley = std::min<fix>(mul(cut, depth), (base + mul(detail, amplitude) + ridge + gully + crests) / 2); // (never to the sea)
		valley = std::max<fix>(valley, 0);
	}
	// Sprint 108's prototype: the land from the drainage, the channels'
	// heights and their sides (never flipping land to sea)
	if (m_land && m_channels && base > 0) {
		const Channels::Read &read = channelsAt(d).read;
		if (read.land != INT64_MAX) {
			// valleys carried under the sea (P6): the sea stood lower through
			// the ice ages, and its valleys, cut to that, drowned as it rose;
			// near the coast a valley's floor sinks by DROWN_M (FJORD_M where
			// ice cut it), less up its walls, so the sea runs up it between
			// headlands: coves and rias, fjords
			// (a cell or two in from the sea, by cells by it: by height alone a
			// low plain's valleys drowned 50 km inland)
			const Sphere::Spot spot = gridSphere().spot(d.dir());
			fix seaward = 0;
			for (int k = 0; k < 3; k++) {
				bool by = m_grid->base[spot.cells[k]] < 0;
				for (const int32_t *q = gridSphere().neighboursBegin(spot.cells[k]); !by && q != gridSphere().neighboursEnd(spot.cells[k]); q++)
					by = m_grid->base[*q] < 0;
				if (by)
					seaward += spot.weights[k];
			}
			const fix coast = mul(smooth(seaward), ONE - smooth(clamp(div(base, mul(toFix(DROWN_BAND_M), m_metre)), 0, ONE)));
			// (a trunk's valley: a finer one's inlet is narrower than a far
			// tile's cells, which would draw it land where the sea's come in)
			if (m_code.processes.water && coast > 0 && read.bed != INT64_MAX && read.bed_level == 0) {
				const int64_t deep = mul(toFix(DROWN_M) + mul(toFix(FJORD_M - DROWN_M), read.ice), m_metre);
				const fix valley = ONE - clamp((fix)(((read.land - read.bed) << FRAC) / std::max<int64_t>(1, deep)), 0, ONE);
				const int64_t drop = mulShift(mulShift(deep, coast, FRAC), valley, FRAC);
				if (drop > 0 && read.land - drop < toFix(1))
					return (int32_t)((read.land - drop) >> FRAC);
			}
			return std::max(1, (int32_t)(read.land >> FRAC));
		}
	}
	return floorInt(base + mul(detail, amplitude) + ridge + gully + crests - valley);
}


// Rivers: segments from each river cell's centre to the centre of the cell it
// drains into. These say how wide, and how high the banks stand over the water.
static const int32_t BANK = 6;   // blocks beside the channel kept above the water
// Landforms: a floodplain beside a river this many times a river's least
// flow, up to this many blocks over its water and widths out; a marsh up to
// MARSH_TOP blocks over the sea, wetter than MARSH_WET, warmer than
// MARSH_COLD (frozen, it's tundra's bog), where at least MARSH_SILT of the
// ground is laid silt
static const int64_t FLOODPLAIN_FLOW = toFix(2);
static const int32_t FLOODPLAIN_RISE = 4, FLOODPLAIN_WIDTHS = 8, MARSH_TOP = 5;
static const fix MARSH_WET = 0, MARSH_COLD = ratio(-20, 100), MARSH_SILT = ratio(15, 100);
static const uint32_t MARSH_POOLS = 401;
// how far over a pan's level its floor may stand at its edge (blocks)
static const int32_t PAN_EDGE = 12;
// What grows, so can be a landform's: not sand, rock, ice or bare ground
static bool grows(Biome b)
{
	switch (b) {
	case Biome::Savanna: case Biome::Grassland: case Biome::Forest: case Biome::Jungle: case Biome::Taiga:
	case Biome::Monsoon: case Biome::Mediterranean: case Biome::Desert: return true;
	default: return false;
	}
}

// a run's fall per block of it under which a river meanders freely, and over which it barely does
static const fix MEANDER_FLAT = ratio(1, 1000), MEANDER_STEEP = ratio(12, 1000);
static const fix MAX_WIDTH = toFix(24);
// and never wider than a cell over this: wider, a run between two centres is
// a slab with straight banks, its ends not meeting the next run's
static const int64_t WIDEST = 6;

fix World::craters(const Place &at, Age age) const
{
	const Grid &g = *m_grid;
	const int32_t i = cell(at);
	fix sum = m_craters.height(at, g.crater_from, age, t_least);
	for (uint32_t k = g.crater_start[i]; k < g.crater_start[i + 1]; k++) {
		const Crater &c = g.craters[g.crater_list[k]];
		if (c.young == (age == Age::Young))
			sum += Craters::shape(c, at);
	}
	return sum;
}

// The lava's own relief (sprint 96; Watters & Johnson 2010, Head & Gifford
// 1980; papers only), each a share of the mares' noise: wrinkle ridges where
// the lava, cooling and sinking, buckled (sinuous, a few hundred metres
// high, a cell or so long); rilles, channels its flows cut (narrow, winding,
// a few tens of metres deep); domes, low shields over late vents.
static const int32_t CLIFF_M = 80, CLIFF_MIN = 6;
static const int32_t BEACH_TOP = 3; // blocks over the sea a beach reaches // a sea cliff's height: metres, and blocks at the least
static const int32_t WRINKLE_M = 300, RILLE_M = 250, DOME_M = 200, RIDGE_M = 200, CHAOS_M = 150;

int32_t World::mareRelief(const Place &at) const
{
	const uint32_t seed = m_code.seed_hash;
	fix out = 0;
	// a ridge along the zero line of a noise, where a slower one lets it
	const fix w = fbm(at, seed + WRINKLE, m_meander / 8, 4, HALF);
	if (noise(at, seed + WRINKLE + 1, m_meander / 16) > 0) {
		const fix x = std::max<fix>(0, ONE - div(std::abs(w), ratio(4, 100)));
		out += mul(toFix(WRINKLE_M), mul(x, x));
	}
	// a rille: narrower, longer, rarer, cut down
	const fix r = fbm(at, seed + RILLE, m_meander / 8, 4, HALF);
	if (noise(at, seed + RILLE + 1, m_meander / 16) > ratio(35, 100)) {
		const fix x = std::max<fix>(0, ONE - div(std::abs(r), ratio(1, 100)));
		out -= mul(toFix(RILLE_M), smooth(x));
	}
	// a dome where a fine noise peaks, in a few fields of them
	const fix d = noise(at, seed + DOME, m_meander / 2);
	if (d > ratio(70, 100) && noise(at, seed + DOME + 1, m_meander / 8) > ratio(20, 100)) {
		const fix x = div(d - ratio(70, 100), ratio(30, 100));
		out += mul(toFix(DOME_M), smooth(x));
	}
	return floorInt(mul(out, m_metre));
}

bool World::rayed(const Place &at) const
{
	for (size_t k = 0; k < m_rayed.size(); k++) {
		const Crater &c = m_rayed[k];
		const int64_t d = chord(at, c.at);
		if (d >= 10 * c.radius || d == 0)
			continue;
		if (d < c.radius * 3 / 2)
			return true; // its bright ejecta, all round
		// streaks: a noise of the way out from it, so a streak runs straight
		// out; fewer the further, out to ten radii
		const Place way = placeOf((int64_t)at.x - c.at.x, (int64_t)at.y - c.at.y, (int64_t)at.z - c.at.z);
		const fix r = (fix)((d << FRAC) / c.radius);
		const fix streak = fbm(way, m_code.seed_hash + RAY + (uint32_t)k, 40, 3, HALF);
		if (streak > ratio(30, 100) + div(r - ratio(15, 10), toFix(30)))
			return true;
	}
	return false;
}

int32_t World::flexed(const Place &at) const
{
	const fix flexing = m_code.processes.flexing;
	if (flexing <= 0)
		return 0;
	const uint32_t seed = m_code.seed_hash;
	const fix amount = std::min<fix>(flexing, ONE);
	// Cracks: long double ridges along noises' zero lines, in several sets
	// that cross, each only where a slower noise lets it: harder flexing, more
	fix ridges = 0;
	static const int WAVES[] = { 16, 12, 8, 6 };
	const fix allowed = ratio(60, 100) - mul(amount, ratio(80, 100));
	for (int k = 0; k < 4; k++) {
		// (dying out over a stretch, not stopped dead at the gate's edge)
		const fix open = clamp(div(noise(at, seed + LINEA + k * 2 + 1, m_meander / 32) - allowed, ratio(10, 100)), 0, ONE);
		if (open <= 0)
			continue;
		const fix x = div(std::abs(fbm(at, seed + LINEA + k * 2, m_meander / WAVES[k], 3, HALF)), ratio(2, 100));
		if (x < ONE) // two crests with a trough down the crack
			ridges = std::max(ridges, mul(open, mul(toFix(4), mul(x, ONE - x))));
	}
	fix out = mul(toFix(RIDGE_M), ridges);
	// Chaos: under the hardest flexing, warm ice rose and broke the shell into
	// rafts tilted in a lower, jumbled matrix
	const fix strong = clamp((amount - HALF) * 2, 0, ONE);
	if (strong > 0) {
		const fix patch = mul(strong, clamp(div(fbm(at, seed + CHAOS, m_meander / 16, 3, HALF) - ratio(25, 100), ratio(5, 100)), 0, ONE));
		if (patch > 0) {
			const fix raft = fbm(at, seed + CHAOS + 1, m_meander * 2, 3, HALF);
			const fix jumble = raft > 0 ? mul(toFix(CHAOS_M), std::min<fix>(ONE, raft * 4)) : mul(toFix(CHAOS_M), raft) - toFix(CHAOS_M) / 2;
			out = mul(out, ONE - patch) + mul(jumble, patch);
		}
	}
	return floorInt(mul(out, m_metre));
}

static fix dotOf(const Dir &a, const Dir &b) { return mul(a.x, b.x) + mul(a.y, b.y) + mul(a.z, b.z); }

int32_t World::canyoned(const Place &at) const
{
	const Dir d = at.dir();
	fix deepest = 0;
	for (size_t k = 0; k < m_grid->canyons.size(); k++) {
		const Canyon &c = m_grid->canyons[k];
		const fix along = atan2(dotOf(d, c.way), dotOf(d, m_grid->rise));
		if (along <= c.from || along >= c.to)
			continue;
		const uint32_t seed = m_code.seed_hash + CANYON + (uint32_t)k;
		// wandering, and wide and narrow by turns; each end pinches out
		const fix off = std::abs(dotOf(d, c.pole) + fbm(d, seed, m_meander / 8, 3, HALF) / 40);
		const fix ends = std::min<fix>(ONE, div(std::min(along - c.from, c.to - along), ratio(15, 100)));
		const fix wide = mul(mul(c.half, ends), ratio(70, 100) + fbm(d, seed + 1, m_meander / 4, 3, HALF) / 2);
		if (wide <= 0 || off >= wide)
			continue;
		// steep walls round a flat floor
		deepest = std::max(deepest, mul(mul(c.depth, ends), smooth(std::min<fix>(ONE, div(wide - off, wide) * 3))));
	}
	return -floorInt(mul(deepest, m_height));
}

int32_t World::terrain(const Place &at, fix elevation, int32_t &mare) const
{
	const Grid &g = *m_grid;
	mare = 0;
	if (g.crater_stamp.empty())
		return surface(elevation, at);
	// the pool's level, flat, from the flooded cells round the place (as a
	// lake's): read between cells it would slope and its edge would be the
	// cells' triangles
	const Sphere::Spot spot = gridSphere().spot(at.dir());
	fix level = INT32_MIN;
	for (int32_t i : spot.cells)
		if (g.mare[i] > 0)
			level = std::max(level, g.mare_level[i]);
	// the land as it was before the lava (the grid's cells hold it raised to
	// the level), without the young craters the grid holds, and the old
	// exactly, less what its cells say of them
	const fix lava = level == INT32_MIN ? 0 : between(g.mare, at);
	const fix young = between(g.crater_young, at);
	int32_t t = surface(elevation - lava - young, at) + floorInt(craters(at, Age::Old) - mul(between(g.crater_stamp, at), m_height));
	// the lava filled what was lower than its level, so its shore follows the land
	if (level != INT32_MIN) {
		const int32_t top = floorInt(mul(level, m_height));
		if (top > t) {
			mare = top - t;
			t = top;
			// and its own relief, fading in from the shore (rilles don't cut
			// through to the ground under it)
			const int32_t relief = std::max(mareRelief(at) * std::min(mare, 8) / 8, 1 - mare);
			mare += relief;
			t += relief;
		}
	}
	return t + floorInt(craters(at, Age::Young)) + flexed(at) + canyoned(at);
}

// Deltas. A river carrying at least DELTA_FLOW times a river's least, over
// low land (under DELTA_LOW of elevation) into the sea, builds a fan of silt
// out into it: DELTA_WIDTHS of its widths (up to DELTA_MOST cells), as
// wide as it's long (DELTA_SPREAD across per along), a block or two over
// the water, marsh, and split into distributaries that wander out over it.
static const int64_t DELTA_FLOW = toFix(10);
static const int64_t DELTA_WIDTHS = 40;
static const fix DELTA_LOW = ratio(4, 100), DELTA_MOST = toFix(3), DELTA_SPREAD = ratio(110, 100);
static const uint32_t DELTAS = 409;

// A creek: a run drawn as water below a river's flow, from this share of it
// (the streams below the grid end on it, so it carries them on to a river)
static const int64_t CREEK_SHARE = 4;
// The narrowest a run's meanders are sized for (blocks)
static const int32_t LOOP_WIDTH_LEAST = 8;
// A valley with no river drawn in it (the drainage land's trunk) swings
// round its spurs in one to three broad bends, up to VALLEY_SWING of its run
// either side: straight from centre to centre, the grid's runs lay their
// valleys, and the ridges between, in its three directions across a range
static const fix VALLEY_SWING = ratio(14, 100);
// Far tiles: a run's drawn once it drains this many cells of rain per (tile
// in cells)², and a channel below the grid once its runs are this many tiles long
static const fix SHOWN = toFix(16);
static const int64_t STREAM_TILES = 24;
// How steeply the ground rises from a run's bank where it's cut down to it (blocks a block)
static const fix VALLEY_WALL = ratio(1, 2);
// Falls: a run's drop between profile points gathers into one step where
// it's at least FALL_LEAST blocks, no gentler than 1 in FALL_GRADE, over rock
// at least FALL_HARD hard (HARD)
static const int32_t FALL_LEAST = 4;
static const int64_t FALL_GRADE = 25;
static const fix FALL_HARD = ratio(45, 100);

bool World::drawn(int32_t c) const
{
	const Grid &g = *m_grid;
	return g.base[c] >= 0 && g.river_threshold != INT64_MAX && g.drained[c] * CREEK_SHARE >= g.river_threshold;
}

void World::RunShape::line(fix t, int64_t &side, int64_t &off, int64_t &rate) const
{
	const int64_t t2 = mul(t, t), t3 = mul((fix)t2, t);
	const fix phase = (fix)((int64_t)PI * loops * t >> FRAC);
	side = mulShift(swing, sin(phase), FRAC);
	off = side + mulShift(run, mul(m0, (fix)(t - 2 * t2 + t3)) + mul(m1, (fix)(t3 - t2)), FRAC);
	rate = mulShift(mulShift(swing, cos(phase), FRAC), (int64_t)PI * loops, FRAC)
			+ mulShift(run, mul(m0, (fix)(ONE - 4 * (int64_t)t + 3 * t2)) + mul(m1, (fix)(3 * t2 - 2 * (int64_t)t)), FRAC);
}

void World::shapeRuns()
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	constexpr int NEAR = 8; // fractional bits of a position (ground()'s)
	m_runs.assign((size_t)g.size(), RunShape{});
	for (int32_t i = 0; i < g.size(); i++) {
		const int32_t r = g.receiver[i];
		// (the prototype bows every run on land: the drainage's trunk valleys follow them)
		if (r < 0 || !(drawn(i) || (m_land && g.base[i] >= 0)))
			continue;
		RunShape &shape = m_runs[i];
		// A river's full width grows as √ of the flow (3 + 2√flow blocks on
		// the default planet); a larger planet's rivers drain more (its cells
		// are larger), so with the planet's size (in 64 bits; never wider
		// than a sixth of a cell, the land it drains between).
		// A creek (below a river's flow) is one to five blocks across, on any
		// planet: what drains a hillside, not a cell. A river grows from
		// there to its full width over its first few times the threshold,
		// so the channel doesn't jump where one becomes the other.
		const int64_t flow = (g.drained[i] << FRAC) / g.river_threshold; // 1 = a river
		int64_t width = toFix(1) + 4 * std::min<int64_t>(flow, ONE);
		if (g.river(i)) {
			const int64_t full = std::min({ mulShift(toFix(3) + 2 * sqrtWide64(flow), m_river_scale, FRAC),
					mulShift(MAX_WIDTH, m_river_scale, FRAC), m_cell_blocks / WIDEST });
			width += mulShift(std::max<int64_t>(0, full - width), std::min<int64_t>(ONE, (flow - ONE) / 3), FRAC);
		}
		shape.width = width;
		// positions from the cell's centre, blocks to 1/256
		const Place from(sphere.centre(i));
		const auto offset = [&](int32_t c) {
			const Place p(sphere.centre(c));
			return std::array<int64_t, 3>{ blocksFine((int64_t)p.x - from.x) >> (FRAC - NEAR), blocksFine((int64_t)p.y - from.y) >> (FRAC - NEAR),
				blocksFine((int64_t)p.z - from.z) >> (FRAC - NEAR) };
		};
		const std::array<int64_t, 3> b = offset(r);
		const int64_t sx = b[0], sy = b[1], sz = b[2];
		const int64_t len2 = (sx * sx + sy * sy + sz * sz) >> NEAR;
		if (len2 == 0)
			continue;
		// Meanders: the channel swings to one side and back, in loops
		// about eleven widths long (as rivers do), a whole number of them
		// to the run so it's on the line at both ends and runs join.
		const int64_t len = std::max<int64_t>(1, (int64_t)isqrt((uint64_t)(len2 << NEAR))); // 1/256 blocks
		// (a creek's loops as a few-block stream's: shorter, it's a sawtooth)
		shape.loops = std::max<int64_t>(1, (len << (FRAC - NEAR)) / (11 * std::max<int64_t>(width, toFix(LOOP_WIDTH_LEAST))));
		// How far it swings: a river on the flat wanders (up to three widths
		// either side, a floodplain's loops), one falling steeply barely does;
		// by the run's fall over its length, from MEANDER_FLAT to MEANDER_STEEP
		// (a fraction of the run too, so the bends never fold)
		const int64_t fall = std::max<int64_t>(0, (int64_t)waterBlocks(g.filled[i]) - (g.base[r] < 0 ? 0 : waterBlocks(g.filled[r]))) << FRAC;
		shape.run = std::max<int64_t>(ONE, len << (FRAC - NEAR));
		const fix grade = (fix)std::min<int64_t>(ONE, (fall << FRAC) / shape.run);
		const fix wander_by = ONE - smooth(clamp(div(grade - MEANDER_FLAT, MEANDER_STEEP - MEANDER_FLAT), 0, ONE));
		shape.reach_side = std::min<int64_t>(mulShift(width, ratio(1, 4) + mul(wander_by, ratio(11, 4)), FRAC), shape.run / (4 * shape.loops));
		uint32_t hash = ((uint32_t)i * 0x9e3779b1u) ^ m_code.seed_hash;
		hash ^= hash >> 15;
		hash *= 0x85ebca77u;
		hash ^= hash >> 13;
		if (m_land && !drawn(i)) {
			shape.loops = 1 + (int64_t)((hash >> 17) % 3);
			shape.reach_side = mulShift(shape.run, VALLEY_SWING, FRAC) / shape.loops;
			// (as little as a third of it: bends all one size swung in step)
			shape.swing = (shape.reach_side * (int64_t)(22000 + (hash & 0xFFFF) * 43536 / 65536) >> 16) * ((hash >> 16) & 1 ? 1 : -1);
		} else
			shape.swing = (shape.reach_side * (int64_t)(32768 + (hash & 32767)) >> 16) * ((hash >> 16) & 1 ? 1 : -1);
		// The run bowed so it leaves and arrives along the way the river
		// runs through each end (halfway between the run before and this,
		// this and the next): runs share their direction where they meet,
		// and a river turns there in a curve, not a corner (a cubic
		// across the run, Hermite's, its end slopes those ways)
		const int64_t ux = from.x >> 16, uy = from.y >> 16, uz = from.z >> 16;
		const int64_t nx = (sy * uz - sz * uy) >> 14, ny = (sz * ux - sx * uz) >> 14, nz = (sx * uy - sy * ux) >> 14;
		const int64_t nl = std::max<int64_t>(1, (int64_t)isqrt((uint64_t)(((nx * nx + ny * ny + nz * nz) >> NEAR))) << (NEAR / 2));
		const auto slopeAt = [&](int32_t a, int32_t c) -> fix {
			if (a < 0 || c < 0)
				return 0;
			const std::array<int64_t, 3> f = offset(a), e = offset(c);
			const int64_t wx = e[0] - f[0], wy = e[1] - f[1], wz = e[2] - f[2];
			const int64_t wl = (int64_t)isqrt((uint64_t)((wx * wx + wy * wy + wz * wz) >> NEAR)) << (NEAR / 2);
			if (wl == 0)
				return 0;
			// the way through the end: this run's way and the other's, each a unit (fixed)
			const int64_t cl = (int64_t)isqrt((uint64_t)len2) << (NEAR / 2);
			const int64_t tx = (sx << FRAC) / cl + (wx << FRAC) / wl, ty = (sy << FRAC) / cl + (wy << FRAC) / wl, tz = (sz << FRAC) / cl + (wz << FRAC) / wl;
			const int64_t on = (tx * sx + ty * sy + tz * sz) / cl, off = (tx * nx + ty * ny + tz * nz) / nl;
			return on <= 0 ? 0 : (fix)std::clamp<int64_t>((off << FRAC) / on, -ONE, ONE);
		};
		shape.m0 = slopeAt(m_upstream[i], i);
		shape.m1 = slopeAt(r, g.base[r] < 0 ? -1 : g.receiver[r]);
	}
}

int32_t World::landAt(const Place &at) const
{
	const Dir d = at.dir();
	fix e = between(m_grid->base, d);
	if (e >= 0)
		e = std::max<fix>(e + between(m_grid->worn, d), 0);
	int32_t mare;
	return terrain(at, e, mare);
}

Place World::unwarped(const Place &p) const
{
	// (the rivers' warp, ground()'s, read at the warped place: close enough
	// at its scale, a cell or two long)
	int64_t w[3];
	for (int k = 0; k < 3; k++) {
		const fix f = fbm(p, m_code.seed_hash + MEANDER + (uint32_t)k, m_meander / 2, 2, HALF) / 3
				+ fbm(p, m_code.seed_hash + MEANDER + 3 + (uint32_t)k, m_meander * 2, 2, HALF) / 16;
		w[k] = (int64_t)f * gridSphere().spacing() >> (2 * FRAC - PLACE_FRAC);
	}
	return placeOf((int64_t)p.x - w[0], (int64_t)p.y - w[1], (int64_t)p.z - w[2]);
}

void World::profileRuns()
{
	const Grid &g = *m_grid;
	const int32_t n = g.size();
	constexpr int P = PROFILE;
	m_profile.assign((size_t)n * (P + 1), 0);
	m_profiled = std::make_unique<std::once_flag[]>(n);
	// Each drawn run's level is a block under the network's own bed at its
	// start (linkHeight: the grid's heights, which the land is carved to: the
	// river cuts its way, ground()), never the finished land read there: that
	// was a full read of the ground at every run, most of making a world, and
	// matched the bed within a block or two at nine runs in ten (sprint 110)
	Step step(m_code.text, "river levels", n);
	std::vector<int32_t> bed(n, INT32_MAX);
	for (int32_t c = 0; c < n; c++)
		if (m_runs[c].run != 0 && drawn(c))
			bed[c] = (int32_t)(linkHeight(c, 0) >> FRAC);
	step.advance(n);
	// each node's level: a block under its bed, no higher than any drawn
	// run into it (upstream first: more drains through a cell than into it
	// from any one donor); a lake's is its water, the sea's 0
	const auto fixed = [&](int32_t c) -> std::optional<int32_t> {
		if (g.base[c] < 0)
			return 0;
		if (g.lake[c])
			return waterBlocks(g.filled[c]) - 1;
		return std::nullopt;
	};
	std::vector<int32_t> level(n, INT32_MAX);
	std::vector<int32_t> order;
	for (int32_t c = 0; c < n; c++)
		if (m_runs[c].run != 0 && drawn(c))
			order.push_back(c);
	std::sort(order.begin(), order.end(), [&](int32_t a, int32_t b) { return g.drained[a] != g.drained[b] ? g.drained[a] < g.drained[b] : a < b; });
	for (int32_t c : order) {
		const std::optional<int32_t> f = fixed(c);
		level[c] = f ? *f : std::max(0, std::min(level[c], bed[c] - 1)); // (never under the sea's level)
		const int32_t r = g.receiver[c];
		if (!fixed(r))
			level[r] = std::min(level[r], level[c]);
	}
	// and none lower than what it runs into: a lake or the sea standing higher
	// backs the water up the river to its level (downstream first)
	for (auto it = order.rbegin(); it != order.rend(); ++it) {
		const int32_t c = *it, r = g.receiver[c];
		const std::optional<int32_t> fr = fixed(r);
		level[c] = std::max(level[c], fr ? *fr : level[r]);
	}
	// each run's ends; the points between, profileRun
	for (int32_t c : order) {
		const int32_t r = g.receiver[c];
		const std::optional<int32_t> fr = fixed(r);
		const int32_t end = fr ? *fr : level[r];
		int32_t *out = &m_profile[(size_t)c * (P + 1)];
		out[0] = std::max(level[c], end);
		out[P] = end;
	}
}

// Along a run: falling, a block under the ground, never below where it ends
void World::profileRun(int32_t c) const
{
	constexpr int P = PROFILE;
	int32_t *out = &m_profile[(size_t)c * (P + 1)];
	// at full detail, whoever asks (a far tile's thread leaves the fine channels
	// out: its profile would differ from a block's)
	const int64_t least = t_least;
	t_least = 0;
	for (int j = 1; j < P; j++)
		out[j] = std::max(out[P], std::min(out[j - 1], landAt(unwarped(onRun(c, ratio(j, P)))) - 1));
	t_least = least;
}

const int32_t *World::profile(int32_t c) const
{
	if (m_profile.empty() || m_runs[c].run == 0 || !drawn(c))
		return nullptr;
	std::call_once(m_profiled[c], [&] { profileRun(c); });
	return &m_profile[(size_t)c * (PROFILE + 1)];
}

int64_t World::linkHeight(int32_t c, fix t) const
{
	const Grid &g = *m_grid;
	const int32_t r = g.receiver[c];
	// (a lake's cells at its bed, not its surface: the valleys running into
	// it drown, so its shore is the land's, bays up them and spurs between)
	const auto height = [&](int32_t i) -> int64_t {
		return g.base[i] < 0 ? 0 : (int64_t)std::max<fix>(0, g.lake[i] ? std::min(g.eroded[i], g.filled[i]) : g.filled[i]) * m_height >> FRAC;
	};
	const int64_t a = height(c), b = r < 0 ? a : height(r);
	return a + mulShift(b - a, t, FRAC);
}

int64_t World::gridHeight(const Place &p) const
{
	return (int64_t)std::max<fix>(0, between(m_grid->filled, p)) * m_height >> FRAC;
}

fix World::glacial(int32_t c, int64_t height) const
{
	const fix e = (fix)std::clamp<int64_t>((height << FRAC) / std::max<int64_t>(1, m_height), 0, toFix(8));
	const fix cold = heatOfCelsius(GLACIAL_BELOW) - (m_grid->heat[c] - mul(e, LAPSE));
	return smooth(clamp(cold * 4, 0, ONE));
}

Place World::onRun(int32_t c, fix t) const
{
	const Sphere &sphere = gridSphere();
	const Place a(sphere.centre(c)), b(sphere.centre(m_grid->receiver[c]));
	const int64_t sx = (int64_t)b.x - a.x, sy = (int64_t)b.y - a.y, sz = (int64_t)b.z - a.z;
	const Place on = placeOf((int64_t)a.x + mulShift(sx, t, FRAC), (int64_t)a.y + mulShift(sy, t, FRAC), (int64_t)a.z + mulShift(sz, t, FRAC));
	const RunShape &shape = m_runs[c];
	if (shape.run == 0)
		return on;
	int64_t side, off, rate;
	shape.line(t, side, off, rate);
	// across the run (run × up, as ground() reads it), the offset in place units
	const int64_t ux = on.x >> 16, uy = on.y >> 16, uz = on.z >> 16;
	const Dir n = normalise(((sy >> 8) * uz - (sz >> 8) * uy), ((sz >> 8) * ux - (sx >> 8) * uz), ((sx >> 8) * uy - (sy >> 8) * ux));
	const int64_t by = (int64_t)(((__int128)off << PLACE_FRAC) / m_radius);
	return placeOf((int64_t)on.x + mulShift(by, n.x, FRAC), (int64_t)on.y + mulShift(by, n.y, FRAC), (int64_t)on.z + mulShift(by, n.z, FRAC));
}

void World::deltas()
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	m_delta_at.assign((size_t)g.size(), -1);
	std::vector<int64_t> flow_at((size_t)g.size(), 0);
	for (int32_t i = 0; i < g.size(); i++) {
		const int32_t r = g.receiver[i];
		if (!g.river(i) || g.lake[i] || r < 0 || g.base[r] >= 0 || g.lake[r] || g.base[i] > DELTA_LOW)
			continue;
		const int64_t flow = (g.drained[i] << FRAC) / g.river_threshold;
		if (flow < DELTA_FLOW || m_deltas.size() >= INT16_MAX)
			continue;
		Delta d;
		d.apex = Place(sphere.centre(i));
		d.hash = plates::mix((uint32_t)i, DELTAS, m_code.seed_hash);
		// seaward: toward the cell it runs into, in the tangent plane
		const Dir up = sphere.centre(i), to = sphere.centre(r);
		fix v[3] = { to.x - up.x, to.y - up.y, to.z - up.z };
		const fix in = mul(v[0], up.x) + mul(v[1], up.y) + mul(v[2], up.z);
		v[0] -= mul(in, up.x); v[1] -= mul(in, up.y); v[2] -= mul(in, up.z);
		const fix l = sqrt(mul(v[0], v[0]) + mul(v[1], v[1]) + mul(v[2], v[2]));
		if (l <= 0)
			continue;
		for (int k = 0; k < 3; k++)
			d.sea[k] = div(v[k], l);
		d.side[0] = mul(up.y, d.sea[2]) - mul(up.z, d.sea[1]);
		d.side[1] = mul(up.z, d.sea[0]) - mul(up.x, d.sea[2]);
		d.side[2] = mul(up.x, d.sea[1]) - mul(up.y, d.sea[0]);
		const int64_t root = sqrtWide64(flow);
		d.width = std::min({ mulShift(toFix(3) + 2 * root, m_river_scale, FRAC), mulShift(MAX_WIDTH, m_river_scale, FRAC), m_cell_blocks / WIDEST });
		d.radius = std::min(d.width * DELTA_WIDTHS, mulShift(m_cell_blocks, DELTA_MOST, FRAC));
		d.arms = 3 + (int)std::min<int64_t>(3, root >> FRAC >> 1);
		const int16_t k = (int16_t)m_deltas.size();
		m_deltas.push_back(d);
		// the cells it reaches (the bigger river's where two would)
		const int64_t reach = d.radius + 2 * m_cell_blocks;
		for (int32_t c : sphere.around(i, [&](int32_t c) { return blocksFine(chord(Place(sphere.centre(c)), d.apex)) <= reach; }))
			if (flow > flow_at[c]) {
				flow_at[c] = flow;
				m_delta_at[c] = k;
			}
	}
}

void World::delta(const Place &at, Ground &out, bool arms) const
{
	// (a river or a lake already here keeps its water: the fan's built round them)
	if (m_delta_at.empty() || out.kind == Water::Lake || out.kind == Water::River)
		return;
	const int16_t k = m_delta_at[cell(at)];
	if (k < 0)
		return;
	const Delta &d = m_deltas[k];
	// where it is from the apex: along the fan and across it, blocks (fixed)
	const int64_t v[3] = { blocksFine((int64_t)at.x - d.apex.x), blocksFine((int64_t)at.y - d.apex.y), blocksFine((int64_t)at.z - d.apex.z) };
	const int64_t along = (v[0] * d.sea[0] + v[1] * d.sea[1] + v[2] * d.sea[2]) >> FRAC;
	const int64_t across = (v[0] * d.side[0] + v[1] * d.side[1] + v[2] * d.side[2]) >> FRAC;
	// the fan: a lobe round a point part way out, its edge wandering by noise
	// (a bird's foot where the arms push out, a cuspate front between)
	const int64_t mid = d.radius / 4;
	const int64_t ox = (along - mid) >> 8, oy = across >> 8;
	const int64_t dist = (int64_t)isqrt((uint64_t)(ox * ox + oy * oy)) << 8;
	const fix lobe = fbm(at, m_code.seed_hash + DELTAS, m_meander * 2, 3, HALF);
	const int64_t reach = d.radius * 3 / 4 + mulShift(d.radius, lobe / 2, FRAC);
	if (dist >= reach)
		return;
	// (how far out, 0 in the middle, 1 at the fan's edge; and from the apex)
	const fix rim = (fix)std::min<int64_t>(ONE, (dist << FRAC) / std::max<int64_t>(1, reach));
	const fix out_by = (fix)std::clamp<int64_t>((along << FRAC) / std::max<int64_t>(1, d.radius), 0, ONE);
	// its top: a block or two over the sea, shoaling under it at the rim
	const int32_t top = rim < ratio(90, 100) ? 1 + (rim < HALF) : roundInt(mul(toFix(-3), div(rim - ratio(90, 100), ratio(10, 100))));
	if (out.terrain >= top && out.terrain > 2)
		return; // (higher ground, as the land was)
	if (!arms) {
		if (top > out.terrain) {
			out.terrain = top;
			if (out.kind == Water::Sea && out.terrain >= out.water)
				out.kind = Water::None;
		}
		return;
	}
	if (out.kind != Water::None && out.kind != Water::Sea)
		return;
	// the distributaries: from the apex out at their own angles, swinging as they go
	for (int a = 0; a < d.arms; a++) {
		const uint32_t h = plates::mix((uint32_t)a, d.hash, DELTAS);
		// (an arm's angle across the fan, as across per along, to the fan's own spread)
		const fix slope = mul(DELTA_SPREAD, ratio(85, 100)) * (2 * a + 1 - d.arms) / d.arms + (fix)((h & 0x3fff) - 0x2000);
		const fix swing = fbm(at, h, m_meander * 3, 2, HALF);
		const int64_t line = mulShift(std::max<int64_t>(0, along), slope + swing / 3, FRAC);
		// (narrower than the river by how many it's split into, and narrowing out)
		const int64_t half = std::max<int64_t>(toFix(1), mulShift(d.width / 6, ONE - out_by / 2, FRAC));
		// (from the apex, round where they start)
		const int64_t off = std::abs(across - line), behind = std::max<int64_t>(0, -along);
		if ((behind >> 8) * (behind >> 8) + (off >> 8) * (off >> 8) < (half >> 8) * (half >> 8)) {
			out.terrain = std::min(out.terrain, -1 - (out_by < HALF));
			out.water = 0;
			out.kind = Water::River;
			out.column.biome = Biome::Marsh;
			return;
		}
	}
	if (out.kind == Water::None && m_code.processes.life && out.column.heat > MARSH_COLD && out.terrain <= 2) {
		// between them, reeds and standing pools
		out.column.biome = Biome::Marsh;
		if (fbm(at, m_code.seed_hash + MARSH_POOLS, m_meander * 16, 3, HALF) > ratio(32, 100)) {
			out.water = out.terrain;
			out.terrain--;
			out.kind = Water::Lake;
		}
	}
}

Ground World::ground(const Place &at, fix sun) const
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	Column c = column(at, sun);
	int32_t mare;
	Ground out{ terrain(at, c.elevation, mare), 0, Water::None, c, 0 };
	out.mare = mare;
	// a lifeless body's dust is dark where lava lies (the column's own guess is the cells')
	if (!m_code.processes.life)
		out.column.biome = bareBiome(at.dir(), mare > 0 && !rayed(at));
	// volcanoes stand on the land (or rise from the sea floor to islands)
	int32_t raise, lava, floor;
	volcanic(at, raise, lava, floor);
	if (raise > 0)
		out.terrain += raise;
	if (floor != INT32_MIN) {
		out.terrain = std::max(out.terrain, floor);
		if (lava != INT32_MIN && lava > out.terrain && lava > 1) {
			out.water = lava; // an active crater: a lava lake
			out.kind = Water::Lava;
			return out;
		}
	}
	// the prototype's: what covers the ground by its own height and slope,
	// not its cell's (the air cooling as it rises): snow only where it lies,
	// bare rock where it doesn't and above the trees where hillsides are
	// steep (a range's critical slopes are scree and crags, its valleys turf)
	if (m_land && m_channels && m_code.processes.life && out.terrain > BEACH_TOP) {
		const Channels::Read &read = channelsAt(at).read;
		if (read.land != INT64_MAX) {
			const fix real = (fix)std::clamp<int64_t>(((int64_t)out.terrain << (2 * FRAC)) / std::max<int64_t>(1, m_height), 0, toFix(8));
			const fix heat = out.column.heat + mul(std::max<fix>(0, out.column.elevation) - real, LAPSE);
			Biome b = classify(std::max(real, ratio(3, 100)), heat, out.column.wet, out.column.season);
			if ((b == Biome::Snow && read.slope >= SNOW_STEEPEST) || (b == Biome::Tundra && read.slope >= ALPINE_ROCK))
				b = Biome::Rock;
			else if (b == Biome::Rock && read.slope < ALPINE_ROCK)
				b = Biome::Tundra; // high, gentle: turf
			else if (read.wash && b != Biome::Snow && b != Biome::Rock && b != Biome::Tundra)
				b = Biome::Desert; // a wash's floor: sand the floods spread
			out.column.biome = b;
			out.column.heat = heat;
		}
	}
	if (!m_code.processes.water)
		return out; // no water: its basins stand dry
	// a hard coast rises as a cliff: the land's first metres stood up three
	// times as steep, to CLIFF_M over the sea (so inland it's only lifted by
	// that, and the lift fades where the rock softens: no steps)
	if (out.terrain >= 1) {
		const int32_t cliff = std::max(CLIFF_MIN, floorInt(mul(toFix(CLIFF_M), m_metre)));
		fix hard = clamp(div(hardness(at.dir()) - ratio(40, 100), ratio(40, 100)), 0, ONE);
		// (the prototype's: lifted only as the land rises a cliff over its
		// channel's bed, which isn't lifted: the water's set on the bed after
		// this, and every stream in hard rock ran in a slot a cliff deep;
		// sprint 109 P4)
		if (m_land && m_channels && hard > 0) {
			const Channels::Read &read = channelsAt(at).read;
			if (read.land != INT64_MAX && read.clear != INT64_MAX)
				hard = mul(hard, smooth((fix)std::clamp<int64_t>(read.clear / cliff, 0, ONE)));
		}
		out.terrain += floorInt(hard * std::min(out.terrain * 3, cliff));
	}
	// the beach is the strand: a coastal plain past a few blocks over the
	// sea is the land's biome (rivers' and lakes' strands come below)
	if (out.column.biome == Biome::Beach && out.terrain > BEACH_TOP && m_code.processes.life)
		out.column.biome = classify(ratio(3, 100), out.column.heat, out.column.wet, out.column.season);
	if (out.terrain < 0) {
		out.kind = Water::Sea;
		out.water = 0;
	}
	delta(at, out, false);

	// Lakes: the grid's lake mask, between cell centres. Inside the outline the
	// floor drops under the lake's level; on the rim the ground holds it in.
	// Lakes and rivers are read at a place a little way off, by smooth noise
	// (no steeper than this: steeper, the warp folds and a wide river breaks
	// into pools): alone, lakes' outlines would
	// be the cells' triangles and rivers the straight runs between centres.
	// Both use the same warp, so rivers still run into their lakes.
	constexpr int NEAR = 8; // fractional bits of a river position here (below)
	std::array<int64_t, 3> wander; // the warp in blocks, to 1/256
	Place warped = at;
	{
		std::array<int64_t, 3> w;
		for (int k = 0; k < 3; k++) {
			// a third of a cell in waves two cells long, a sixteenth in waves half a cell long
			const fix f = m_channels ? channelsAt(at).warp[k] : fbm(at, m_code.seed_hash + MEANDER + (uint32_t)k, m_meander / 2, 2, HALF) / 3
					+ fbm(at, m_code.seed_hash + MEANDER + 3 + (uint32_t)k, m_meander * 2, 2, HALF) / 16;
			w[k] = (int64_t)f * sphere.spacing();
			wander[k] = mulShift(f, m_cell_blocks, FRAC) >> (FRAC - NEAR);
		}
		warped = placeOf((int64_t)at.x + w[0], (int64_t)at.y + w[1], (int64_t)at.z + w[2]);
	}
	const Sphere::Spot spot = sphere.spot(warped.dir());
	fix mask = 0;
	for (int k = 0; k < 3; k++)
		mask += g.lake[spot.cells[k]] ? spot.weights[k] : 0;
	if (mask > 0)
		mask += fbm(at, m_code.seed_hash + SHORE + 1, m_meander * 4, 3, HALF) / 4;
	// a lake's level, from its cells here or beside them: the water may reach
	// a cell past its outline, as far as the ground under its level does (the
	// fill holds the cells round a lake above it, but where it spills)
	int32_t level = INT32_MIN;
	for (int32_t i : spot.cells) {
		if (g.lake[i])
			level = std::max(level, waterBlocks(g.lake_level[i]));
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			if (g.lake[*p])
				level = std::max(level, waterBlocks(g.lake_level[*p]));
	}
	if (level != INT32_MIN) {
		// The shore is where the land itself dips under the level: its bays,
		// points and islands are the ground's, not the cells' triangles. The
		// core is water whatever (a lake's there by the grid), deepening from
		// the shore.
		// (the sea beside a lake stays the sea)
		const bool core = mask >= ratio(3, 4), below = out.terrain < level && out.kind != Water::Sea;
		if (core || below) {
			const int32_t depth = 1 + roundInt(mul(clamp(mask - HALF, 0, HALF), toFix(12)));
			out.terrain = std::min(out.terrain, level - depth);
			out.water = level;
			out.kind = Water::Lake;
		} else {
			// a soft shore is a strand of sand round the water (sprint 97)
			if (m_code.processes.life && out.terrain <= level + 1 && hardness(at.dir()) < HALF)
				out.column.biome = Biome::Beach;
		}
	}

	// Rivers: the nearest river segment within reach of this column, from
	// the cells within three spacings of it. Positions are blocks from this
	// column, straight through the planet (this near, the same as along it),
	// to 1/256 of a block: three spacings on a large planet are tens of
	// thousands of blocks, whose squares wouldn't fit 64 bits at 1/65 536.
	const int64_t reach = 3 * m_cell_blocks + toFix(4);
	const auto offset = [&](const Dir &d) {
		const Place c(d);
		const auto along = [&](int64_t u) { return blocksFine(u) >> (FRAC - NEAR); };
		return std::array<int64_t, 3>{ along((int64_t)c.x - at.x) - wander[0], along((int64_t)c.y - at.y) - wander[1],
			along((int64_t)c.z - at.z) - wander[2] };
	};
	int64_t best_gap = INT64_MAX; // blocks, fixed (64 bits: a far segment's gap doesn't fit 32)
	int64_t next_gap = INT64_MAX; // the next nearest run's: where two meet, the bend's inside is rounded
	int32_t best_level = 0;
	int64_t best_half = 0;
	int64_t best_dist = 0;
	int64_t best_bar = 0; // how wide the point bar is here (blocks, fixed)
	int64_t best_flow = 0; // the nearest run's flow (1 a river's least)
	int32_t best_pool = 0; // how much deeper the bed is in a fall's plunge pool
	int best_bend = 0; // the column's side of the bend: 1 inside (a bar), −1 outside (a cut bank), 0 on a straight
	// (in a lake or the sea, the river is the water there: no channel, no banks)
	const bool open = out.kind != Water::Lake && out.kind != Water::Sea;
	// a far tile's width (blocks, fixed; 0 for a block), and the least a run
	// drains to be drawn on it: SHOWN cells of rain per tile's area in cells
	const int64_t tile = t_least > 0 ? blocksFine(t_least) : 0;
	const int64_t cells = tile > 0 ? (tile << FRAC) / std::max<int64_t>(m_cell_blocks, 1) : 0; // tile in cells, fixed
	// (never a creek: on a smooth slope the grid's runs lie parallel, a herringbone a map never shows)
	const int64_t shown = std::max<int64_t>(g.river_threshold, mulShift(mulShift(cells, cells, FRAC), SHOWN, FRAC));
	for (int32_t i : sphere.around(cell(at), [&](int32_t k) {
			return open && blocksFine(chord(Place(sphere.centre(k)), at)) <= reach; })) {
		int32_t r = g.receiver[i];
		// (a run from one lake cell to another is under the lake: none)
		if (!open || !drawn(i) || r < 0 || (g.lake[i] && g.lake[r]) || m_runs[i].run == 0)
			continue;
		const std::array<int64_t, 3> a = offset(sphere.centre(i)), b = offset(sphere.centre(r));
		const int64_t sx = b[0] - a[0], sy = b[1] - a[1], sz = b[2] - a[2];
		const int64_t len2 = (sx * sx + sy * sy + sz * sz) >> NEAR;
		if (len2 == 0)
			continue;
		// closest point on the segment to the origin (this column)
		const int64_t along = -((a[0] * sx + a[1] * sy + a[2] * sz) >> NEAR);
		fix t = (fix)std::clamp<int64_t>((along << FRAC) / len2, 0, ONE);
		const int64_t px = a[0] + ((sx * t) >> FRAC), py = a[1] + ((sy * t) >> FRAC), pz = a[2] + ((sz * t) >> FRAC);
		const RunShape &shape = m_runs[i];
		const int64_t flow = (g.drained[i] << FRAC) / g.river_threshold; // 1 = a river
		// Far tiles (World::Coarse) draw the network as a map does: a run that
		// drains enough for the tile's size, at least a tile wide
		// so every tile it crosses shows it (alone, a river narrower than a
		// tile was missed by most of the tiles it crossed: few rivers, dashed)
		if (tile > 0 && g.drained[i] < shown)
			continue;
		const int64_t width = shape.width, half = std::max(width / 2, tile * 40 / 100);
		int bend = 0;
		int64_t bar = 0;
		int64_t dist = (int64_t)isqrt((uint64_t)(px * px + py * py + pz * pz)) << (FRAC - NEAR);
		{
			const int64_t len = std::max<int64_t>(1, (int64_t)isqrt((uint64_t)(len2 << NEAR))); // 1/256 blocks
			const int64_t run = shape.run;
			// across: the column against the run's side (run × up, the column's own up)
			const int64_t ux = at.x >> 16, uy = at.y >> 16, uz = at.z >> 16;
			const int64_t nx = (sy * uz - sz * uy) >> 14, ny = (sz * ux - sx * uz) >> 14, nz = (sx * uy - sy * ux) >> 14;
			const int64_t across = ((a[0] * nx + a[1] * ny + a[2] * nz) / len) << (FRAC - NEAR);
			// and along it from its start (blocks, fixed; past either end too)
			const int64_t x = (along << FRAC) / len;
			// The nearest point on the line: from the run's, a few steps
			// (Gauss–Newton) along it. Straight across the run is further
			// than square to the line where it runs steeply across, so the
			// river would swell there, pinch where it runs along, and end its
			// runs in points
			fix u = t;
			int64_t side = 0, off = 0, rate = 0;
			for (int step = 0; step < 4; step++) {
				shape.line(u, side, off, rate);
				const __int128 on = (__int128)run * (mulShift(run, u, FRAC) - x) + (__int128)(across + off) * rate;
				const __int128 norm = (__int128)run * run + (__int128)rate * rate;
				u = (fix)std::clamp<__int128>(u - (on << FRAC) / std::max<__int128>(norm, 1), 0, ONE);
			}
			shape.line(u, side, off, rate);
			t = u;
			const int64_t ga = (mulShift(run, u, FRAC) - x) >> (FRAC - NEAR), gb = (across + off) >> (FRAC - NEAR);
			dist = (int64_t)isqrt((uint64_t)(ga * ga + gb * gb)) << (FRAC - NEAR);
			// the line swung toward the outside of its bend: there the water
			// cuts the bank, on the inside it slows and lays its load down
			const int64_t rel = across + off;
			bend = rel == 0 || side == 0 ? 0 : (rel > 0) == (side > 0) ? 1 : -1;
			// a bar is a crescent: widest at the bend's apex, nothing where the line crosses back
			bar = shape.reach_side > 0 ? width * std::abs(side) / shape.reach_side : 0;
		}
		if (dist - half >= best_gap) {
			next_gap = std::min(next_gap, dist - half);
		} else {
			next_gap = best_gap;
			best_gap = dist - half;
			best_half = half;
			best_dist = dist;
			best_bar = bar;
			best_bend = bend;
			// the run's profile (profileRuns), between its points
			const int32_t *profile = World::profile(i);
			const int64_t along_points = (int64_t)t * PROFILE;
			const int j = (int)std::min<int64_t>(PROFILE - 1, along_points >> FRAC);
			const fix within = (fix)(along_points - ((int64_t)j << FRAC));
			// Falls: where the run drops steeply over hard rock, the drop isn't a
			// ramp but a step at a lip of the harder bed: level above it, a
			// plunge pool below (the walls cut back to the lower water make the
			// gorge). On soft rock or a gentler drop, a ramp: rapids.
			const int32_t drop = profile[j] - profile[j + 1];
			const int64_t piece = std::max<int64_t>(ONE, shape.run / PROFILE) >> FRAC; // blocks
			const fix hard = mul(HARD[(int)g.province[i]], ONE - alluvium(g.sediment[i]));
			if (drop >= FALL_LEAST && hard >= FALL_HARD && (int64_t)drop * FALL_GRADE >= piece) {
				uint32_t h = ((uint32_t)i * 0x9e3779b1u) ^ ((uint32_t)j * 0x85ebca77u) ^ m_code.seed_hash;
				h ^= h >> 15;
				const fix lip = ratio(25, 100) + (fix)(h % (HALF + 1));
				best_level = std::max<int32_t>(0, within < lip ? profile[j] : profile[j + 1]);
				// (the pool: a few blocks past the foot, as deep as a third of the fall)
				const int64_t past = (int64_t)(within - lip) * piece >> FRAC; // blocks
				best_pool = within >= lip && past < (int64_t)drop * 2 ? drop / 3 : 0;
			} else {
				best_level = std::max<int32_t>(0, floorInt(lerp(toFix(profile[j]), toFix(profile[j + 1]), within)));
				best_pool = 0;
			}
			best_flow = flow;
		}
	}
	// Where runs meet at an angle, each alone a band with round ends, the
	// bend's outside is round and its inside a notch: the two distances'
	// smooth minimum fills the inside with a curve about a channel wide
	if (next_gap != INT64_MAX && best_gap != INT64_MAX) {
		const int64_t k = std::max<int64_t>(toFix(2), 2 * best_half);
		const int64_t h = std::max<int64_t>(0, k - (next_gap - best_gap));
		const int64_t in = (h * h / k) / 4;
		best_gap -= in;
		best_dist = std::max<int64_t>(0, best_dist - in);
	}
	// a point bar: inside a bend, as wide as the river, at the water's level
	// (gravel and sand: a beach of the river's)
	if (best_bend > 0 && best_dist >= best_half && best_dist < best_half + best_bar && out.kind == Water::None &&
			out.terrain > best_level) {
		out.terrain = best_level;
		if (m_code.processes.life)
			out.column.biome = Biome::Beach;
	} else if (best_gap < toFix(BANK)) {
		if (best_dist < best_half) {
			// the channel: a bed under the water, deepest mid-stream
			int32_t depth = 1 + (int32_t)std::min<int64_t>(3, (((best_half - best_dist) >> 1) + HALF) >> FRAC);
			out.terrain = std::min(out.terrain, best_level - depth - best_pool);
			if (out.kind == Water::None || best_level > out.water) {
				out.water = best_level;
				out.kind = Water::River;
			}
		} else if (out.terrain < best_level + 1 + (best_bend < 0 ? 2 : 0)) {
			// the bank holds the river in; outside a bend it's cut steep
			out.terrain = best_level + 1 + (best_bend < 0 ? 2 : 0);
		}
	}
	// The ground beside a run slopes down to it: where the land stands over
	// its water, it's cut back to walls rising VALLEY_WALL a block out from
	// the bank (the river's cut into the land; the land never holds it up)
	// (not the prototype's: its land already holds the river in its valley,
	// and in a range walls 1 in 2 were all there was, a planar band each side)
	if (best_gap != INT64_MAX && best_dist >= best_half && out.kind == Water::None && !m_land) {
		const int64_t out_from = std::min<int64_t>(best_dist - best_half, toFix(1 << 14));
		const int32_t wall = best_level + 1 + (best_bend < 0 ? 2 : 0) + floorInt((fix)mulShift(out_from, VALLEY_WALL, FRAC));
		out.terrain = std::min(out.terrain, wall);
	}
	// Streams: on the floors of the valleys the wetter channels cut (above),
	// as wide as their level's, a block into the ground there so no level of
	// their own can disagree with the land (none through a cone: it stands on
	// what they drained)
	if (m_channels && raise <= 0 && floor == INT32_MIN && out.kind == Water::None && out.terrain > 1 && best_gap > toFix(BANK)) {
		const Channels::Read &stream = channelsAt(at).read;
		// the prototype's: the water of the channel whose valley this is, at its own level
		if (m_land) {
			// (on a far tile, as the noise land's: only a level whose runs are long
			// next to the tile is widened to it; finer streams, each a band a tile
			// wide, would cover a far view of a wet country in water. A pond's
			// drawn where it is.)
			const bool long_enough = stream.pond ||
					(stream.water_from > 0 && blocksFine(m_channels->spacing(stream.water_from)) >= tile * STREAM_TILES);
			const int64_t within = tile == 0 ? stream.water_width / 2 : long_enough ? std::max(stream.water_width / 2, tile * 40 / 100) : 0;
			if (stream.water != INT64_MIN && stream.water_gap < within && (tile == 0 || stream.water_width >= toFix(4))) {
				out.water = std::max(1, (int32_t)(stream.water >> FRAC));
				out.terrain = std::min(out.terrain, out.water - 1);
				out.kind = stream.pond ? Water::Lake : Water::River;
			}
		} else {
		// (on a far tile, only a level whose runs are long next to the tile, widened as the rivers are)
		const bool long_enough = stream.stream_from > 0 && blocksFine(m_channels->spacing(stream.stream_from)) >= tile * STREAM_TILES;
		const int64_t within = tile == 0 ? stream.stream_width / 2 : long_enough ? std::max(stream.stream_width / 2, tile * 40 / 100) : 0;
		if (stream.stream_from > 0 && stream.stream_gap < within) {
			out.water = out.terrain;
			out.terrain--;
			out.kind = Water::River;
		}
		}
	}
	// Landforms: where water has left the ground. Marsh and floodplain only
	// over what grows (the biome's climate kept beneath: a cold marsh is
	// still not a jungle's); a pan whatever (it's low, often by the sea's level).
	if (m_code.processes.life && out.kind == Water::None) {
		const bool grown = grows(out.column.biome);
		// a pan's floor: the ground under where its water stood, so its edge
		// runs round the basin's own hollows and spurs, not the cells'
		// triangles (as a lake's shore, a cell past its outline at most)
		bool pan = false;
		if (!g.pan.empty()) {
			const Sphere::Spot spot = sphere.spot(at.dir());
			int32_t stood = INT32_MIN;
			fix mask = 0; // how much of a pan's this is
			for (int k = 0; k < 3; k++)
				mask += g.pan[spot.cells[k]] ? spot.weights[k] : 0;
			for (int32_t i : spot.cells) {
				if (g.pan[i])
					stood = std::max(stood, waterBlocks(g.lake_level[i]));
				for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
					if (g.pan[*p])
						stood = std::max(stood, waterBlocks(g.lake_level[*p]));
			}
			if (stood != INT32_MIN) {
				// its floor's flat: what stood over the water's level is worn
				// down to it, less toward the pan's edge, where the land rises
				out.terrain = std::min(out.terrain, stood + roundInt(mul(ONE - smooth(mask), toFix(PAN_EDGE))));
				pan = out.terrain <= stood + roundInt(fbm(at, m_code.seed_hash + MARSH_POOLS + 2, m_meander * 8, 3, HALF) * 2);
			}
		}
		const int32_t over_water = best_gap == INT64_MAX ? INT32_MAX : out.terrain - best_level;
		const bool by_river = best_flow >= FLOODPLAIN_FLOW && best_gap < best_half * FLOODPLAIN_WIDTHS;
		// marsh: low, wet ground built of silt (a delta, a coast's flats), or
		// the lowest of a floodplain, its backswamps, in patches
		const bool lowland = out.terrain <= MARSH_TOP && alluvium(between(g.sediment, at.dir())) > MARSH_SILT + fbm(at, m_code.seed_hash + MARSH_POOLS + 3, m_meander * 4, 3, HALF) / 6;
		const bool backswamp = by_river && over_water <= 1 && fbm(at, m_code.seed_hash + MARSH_POOLS + 1, m_meander * 4, 3, HALF) > 0;
		if (pan) {
			// a closed basin's floor, where its water stood and dried: salt
			out.column.biome = Biome::SaltFlat;
		} else if (grown && (lowland || backswamp) && out.column.wet > MARSH_WET && out.column.heat > MARSH_COLD) {
			// standing water in pools between the reeds
			out.column.biome = Biome::Marsh;
			if (fbm(at, m_code.seed_hash + MARSH_POOLS, m_meander * 16, 3, HALF) > ratio(8, 100)) {
				out.water = out.terrain;
				out.terrain--;
				out.kind = Water::Lake;
			}
		} else if (grown && by_river && over_water <= FLOODPLAIN_RISE) {
			// the low ground a big river spreads over in flood, and leaves silt on
			out.column.biome = Biome::Floodplain;
		}
	}

	delta(at, out, true);

	// a bank or a lake's rim raised over the sea's level: dry ground, not sea
	if (out.kind == Water::Sea && out.terrain >= out.water)
		out.kind = Water::None;
	return out;
}

} // namespace terragen
