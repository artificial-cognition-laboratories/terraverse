// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/wear.h"
#include "library/world/terragen/src/grid/climate.h"
#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/grid/erosion.h"
#include "library/world/terragen/src/grid/setting.h"
#include "library/world/terragen/src/grid/water.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/base/parallel.h"
#include <algorithm>

namespace terragen::wear {

namespace {

// Erosion: how hard running water bites, and how many rounds it gets: long
// enough for the rising belts to stand where uplift and erosion balance (a
// dry body's ancient seas, DRY_ROUNDS at most). Each round is STEP of the
// old ones: uplift, wear and creep all scale with it, so where they balance
// doesn't move, and the implicit stream power is stable at any step (creep,
// explicit, holds below a sixth)
const int STEP = 4;
const fix ERODIBILITY = ratio(6, 100) * STEP;
const int EROSION_ROUNDS = 300, DRY_ROUNDS = 12;
// The glacial buzzsaw (Brozović, Burbank & Meigs 1997; Egholm et al. 2009):
// above the snowline ice wears the land far faster than rivers, so ranges
// stand little over it however fast they rise. Erodibility grows to
// BUZZSAW more where the yearly mean is ICE_FULL under freezing
const fix BUZZSAW = toFix(8), ICE_FULL = toFix(12);
// The crust floats (erosion.h): land worn away rises back by this share,
// spread over this many rounds of neighbour averaging
const fix REBOUND = ratio(82, 100);
const int FLEX_PASSES = 3;
// What a stream lays down (the ξ–q model: Davy & Lague 2009; SPACE, Shobe
// et al. 2017, Landlab's SpaceLargeScaleEroder, MIT): crossing a cell it
// passes on Qs / (1 + G·a/q) of its load, a the cell's area, q the water
// through it (rain-weighted, in cells), and lays the rest. A small stream
// lays down most of what it carries, a big river little; where it lays more
// than it wears the bed builds up and steepens until it carries on.
const fix DEPOSIT = ratio(25, 100);
// Heavy grains (gold, tin) settle this many times as readily as the rest of
// a river's load: they gather where it first slows, below the mountains.
const int64_t HEAVY = 2;
// Each round, where the plates collide the land is pushed up by this much
// (times the belt, 0..1: the history's recent uplift), against which rivers
// wear it: a range stands where the two balance, not as the history left it.
const fix UPLIFT = ratio(15, 2000) * STEP;
// Hillslopes creep (erosion.h): each round a land cell trades this share of
// its height difference with each land neighbour, which fills the cells'
// sharpest notches. Soil creep is a hillside's process: across a cell's
// tens of kilometres it barely acts, so it reaches about a cell over the
// whole run (√(4κ·rounds)), whatever the rounds and their STEP.
const fix CREEP = ratio(1, 1250);
// (shallower, it's a hollow that sediment fills)
// a closed basin's salt pan: where its water stands in the wet years, when
// this many times its mean inflow comes in (the floor its lake spreads over
// Snow on ground whose yearly mean (°C, at its height) is under MELT_FROM
// melts less of it each summer, and none under MELT_NONE: an ice sheet's,
// Sinking basins (see build): this many cells in from the sea at the least,
// where a setting sags (its `sink`, setting.h), up to BASIN_DEPTH
const int32_t BASIN_INLAND = 2;
// How broad the lie of the land is that old ground was worn toward: rounds
// of averaging over neighbours (some √WORN_PASSES cells)
const int WORN_PASSES = 12;
const fix BASIN_DEPTH = ratio(30, 160), BASIN_SINK = ratio(5, 100);
// Glacial scour (see build): how deep, and below what yearly mean (°C).
const fix SCOUR = ratio(24, 160);
const fix SCOUR_BELOW = toFix(4);

} // namespace

void scour(const WorldCode &code, Grid &g)
{
	const Sphere &sphere = gridSphere();
	const size_t n = (size_t)sphere.size();
	// Glacial scour: ice ages' sheets ground hollows into the land that's
	// cold now (and colder then), which hold the lakes of the north (Canada's,
	// Finland's): up to SCOUR deep, in patches, where the yearly mean is
	// below SCOUR_BELOW
	for (size_t i = 0; i < n && code.processes.water; i++) {
		if (g.base[i] < ratio(2, 160))
			continue;
		const fix cold = heatOfCelsius(SCOUR_BELOW) - (g.heat[i] - mul(g.base[i], LAPSE));
		if (cold <= 0)
			continue;
		const fix patches = clamp(fbm(sphere.centre((int32_t)i), code.seed_hash + SCOURED, 30, 3, HALF) * 2, 0, ONE);
		// (on a worn-flat shield a hollow as deep would join its neighbours into one inland sea)
		const fix depth = mul(mul(mul(SCOUR, patches), std::min<fix>(ONE, cold * 4)), ONE - g.tectonics.stable[i] * 2 / 3);
		g.base[i] = std::max<fix>(g.base[i] - depth, ratio(1, 160));
	}
}

void erode(const WorldCode &code, Grid &g, const std::vector<fix> &rain, std::optional<Step> &step,
		const std::function<void(const ErosionRound &)> *watch)
{
	const Sphere &sphere = gridSphere();
	const size_t n = (size_t)sphere.size();
	// Erosion (erosion.h): rounds of uplift, stream power over the land's
	// drainage with its pits routed to the sea (drainage.h), and creep. Lakes
	// don't erode their beds; the sea stays put. What it wears comes off the
	// cell's loose sediment first, then its rock, and is carried down the same
	// receivers, laid by DEPOSIT where streams are small (floodplains, fans);
	// a lake takes all that reaches it till it's full, and the sea builds a
	// shelf at the coast. Nothing is made or lost but what goes out to the
	// deep sea (Grid::sediment, testSediment).
	std::vector<fix> h = g.base;
	g.sediment.assign(n, 0);
	// What its history made each cell (setting.h) says how it rises, sags and
	// wears, and how far it was levelled before: toward the broad lie of the
	// land round it (WORN_PASSES of averaging), not to one plain
	const std::vector<Setting> made = settings(code, g, g.base);
	if (code.processes.water) {
		std::vector<fix> lie = h;
		for (int pass = 0; pass < WORN_PASSES; pass++) {
			const std::vector<fix> was = lie;
			parallelFor((int32_t)n, [&](int32_t i) {
				int64_t sum = was[i];
				int32_t count = 1;
				for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
					if (was[*p] >= 0) {
						sum += was[*p];
						count++;
					}
				lie[i] = (fix)(sum / count);
			});
		}
		for (size_t i = 0; i < n; i++)
			if (h[i] > 0) {
				const fix was = h[i];
				h[i] = std::max<fix>(h[i] + mul(lie[i] - h[i], SETTINGS[(int)made[i]].worn), ratio(1, 160));
				g.uplifted += (int64_t)(h[i] - was) * sphere.area((int32_t)i); // (the sediment's balance holds)
			}
	}
	// What's rising now starts from the crust it stands on and rises under
	// rivers already running (antecedent rivers cut gorges through ranges
	// that rose across them): its height over that comes off, and uplift
	// builds the range back over the rounds (not a plateau's: thick crust
	// floats high, risen long since)
	if (code.processes.water)
		for (size_t i = 0; i < n; i++)
			if (h[i] > 0) {
				const fix range = mul(mul(h[i], clamp(g.tectonics.belt[i], 0, ONE)), ONE - g.tectonics.plateau[i]);
				h[i] -= range;
				g.uplifted -= (int64_t)range * sphere.area((int32_t)i); // (the sediment's balance holds)
			}
	// Sinking basins: away from the coasts and the rising ranges, some of a
	// continent's interior sags (the crust stretched or loaded beside a range:
	// the Great Basin, the Caspian's, the Tarim, Lake Eyre's), all the time
	// the rivers work, BASIN_SINK of BASIN_DEPTH a round. Where it sinks
	// faster than its rivers fill it or cut in from outside, it's a closed
	// hollow, and the water's balance says what it holds: an inland sea where
	// rain is plenty, a salt lake or a dry pan where it isn't; where it
	// doesn't, it's a plain of what they laid.
	std::vector<fix> sinks(n, 0);
	if (code.processes.water) {
		// rings of cells in from the sea
		std::vector<int32_t> inland(n, INT32_MAX);
		std::vector<int32_t> ring;
		for (size_t i = 0; i < n; i++)
			if (g.base[i] < 0) {
				inland[i] = 0;
				ring.push_back((int32_t)i);
			}
		for (size_t head = 0; head < ring.size(); head++) {
			const int32_t c = ring[head];
			for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++)
				if (inland[*p] == INT32_MAX) {
					inland[*p] = inland[c] + 1;
					ring.push_back(*p);
				}
		}
		for (size_t i = 0; i < n; i++) {
			if (g.base[i] <= 0 || inland[i] < BASIN_INLAND)
				continue;
			// (the crust's own sag: a plume gone, a range's weight beside it)
			const fix where = SETTINGS[(int)made[i]].sink;
			if (where <= 0)
				continue;
			// (none in a rising range; deepest well in from the coast)
			const fix away = ONE - clamp(std::max<fix>(g.tectonics.belt[i], ratio(18, 100)) * 2, 0, ONE);
			const fix in = clamp((fix)((int64_t)(inland[i] - BASIN_INLAND + 1) * ONE / BASIN_INLAND), 0, ONE);
			sinks[i] = mul(mul(mul(mul(where, away), in), BASIN_DEPTH), BASIN_SINK);
		}
	}
	// (a plateau's thick crust barely wears: a twentieth of the rest; and
	// what's frozen wears more, by its height each round: the buzzsaw)
	// (a dry body's ancient seas wore it a round at a time, not STEP: its
	// few rounds are the old ones)
	const fix pace = code.processes.water ? ONE : ONE / STEP;
	std::vector<fix> erodibility(n), hard(n);
	for (size_t i = 0; i < n; i++)
		hard[i] = mul(mul(mul(ERODIBILITY, pace), ONE - g.tectonics.plateau[i] * 19 / 20), SETTINGS[(int)made[i]].hard);
	std::vector<int64_t> heavy_laid(n, 0); // heavy grains in each cell's sediment, as height × area
	// the last round's, per cell: what it wore, and what it laid (and of the
	// heavy grains): where it wore at least as much, that's load in transit
	std::vector<fix> worn_last(n, 0), laid_last(n, 0);
	std::vector<int64_t> dropped_last(n, 0), ran(n, 0);
	std::vector<int64_t> crumbs(n, 0); // creep's remainders below a unit of height (× area), kept for the next round
	// (no water, no erosion: the land stays as the history left it. Seas long
	// gone wore it as long as they lasted, against their own level, with no
	// belts rising to renew it: the old valleys)
	const int rounds = code.processes.water ? EROSION_ROUNDS : std::min<int>(DRY_ROUNDS, floorInt(DRY_ROUNDS * code.processes.ancient_water) * 2 / 9);
	fix old_sea = 0;
	if (!code.processes.water && rounds > 0) {
		std::vector<fix> low = h;
		std::nth_element(low.begin(), low.begin() + (ptrdiff_t)(n * 3 / 10), low.end());
		old_sea = low[n * 3 / 10];
		for (fix &v : h)
			v -= old_sea;
	}
	if (watch) {
		const Routing first = route(h);
		(*watch)({ h, g.sediment, accumulate(first.receiver, first.order, rain) });
	}
	step.reset();
	step.emplace(code.text, "erosion", rounds);
	for (int round = 0; round < rounds; round++) {
		if (round > 0)
			step->advance();
		for (size_t i = 0; i < n && code.processes.water; i++)
			if (h[i] >= 0) {
				// (a plateau stands as high as thick crust floats: it has stopped rising)
				// (× the Mountains setting, 0.5..1.5: the history's belt is capped,
				// so how hard the plates push shows here, in how high ranges stand)
				// (and a setting's own: a swell's, a margin's shoulder; a craton's long stopped)
				const SettingRow &row = SETTINGS[(int)made[i]];
				const fix push = mul(g.tectonics.belt[i], row.still) + row.rise;
				const fix up = mul(mul(mul(UPLIFT, HALF + code.mountains), push), ONE - g.tectonics.plateau[i]);
				h[i] += up;
				g.uplifted += (int64_t)up * sphere.area((int32_t)i);
				// (sinking is the crust's doing, not the rivers': uplift's
				// opposite, so the sediment's balance still holds)
				if (sinks[i] > 0) {
					const fix was = h[i];
					h[i] = std::max<fix>(h[i] - sinks[i], std::min<fix>(was, ratio(1, 160)));
					g.uplifted -= (int64_t)(was - h[i]) * sphere.area((int32_t)i);
				}
			}
		for (size_t i = 0; i < n; i++) {
			const fix frozen = clamp(div(-celsiusOf(g.heat[i] - mul(std::max<fix>(0, h[i]), LAPSE)), ICE_FULL), 0, ONE);
			erodibility[i] = mul(hard[i], ONE + mul(BUZZSAW, frozen));
		}
		// (which basins spill: rain against what open water gives back, warmer
		// the lower it stands)
		std::vector<fix> gives(n);
		for (size_t i = 0; i < n; i++)
			gives[i] = water::evaporation(g.heat[i] - mul(std::max<fix>(0, h[i]), LAPSE));
		std::vector<fix> sheds(n);
		for (size_t i = 0; i < n; i++)
			sheds[i] = water::runoff(rain[i], g.heat[i] - mul(std::max<fix>(0, h[i]), LAPSE));
		const Balance balance{ sheds, gives };
		const Routing way = route(h, code.processes.water ? &balance : nullptr);
		const std::vector<int32_t> &down = way.receiver, &order = way.order;
		const std::vector<fix> &surface = way.surface;
		const std::vector<int64_t> area = accumulate(down, order, rain);
		ran = area;
		const std::vector<fix> before = h;
		const std::vector<fix> floated = h; // (what the crust answers: all this round takes off or lays on)
		streamPower(h, way, area, erodibility);
		// downstream, heads first: each cell's load is its donors' plus what it
		// wore, in height × area (fixed); none is raised to its lowest donor (a
		// river never dams itself), nor a lake past its surface (and the heavy
		// grains in it, worn out of the mountain belts' cores, where veins and
		// plutons are: gold, tin; they settle first)
		std::vector<int64_t> load(n, 0), heavy(n, 0);
		std::vector<fix> lowest_donor(n, INT32_MAX);
		for (size_t k = order.size(); k-- > 0;) {
			const int32_t c = order[k];
			const int64_t a = sphere.area(c);
			const fix worn = before[c] - h[c];
			worn_last[c] = worn;
			laid_last[c] = 0;
			dropped_last[c] = 0;
			if (worn > 0) {
				// the loose sediment first, and its heavy grains with it
				const fix loose = std::min(worn, g.sediment[c]);
				if (loose > 0) {
					const int64_t gone = heavy_laid[c] * loose / g.sediment[c];
					heavy[c] += gone;
					heavy_laid[c] -= gone;
					g.sediment[c] -= loose;
				}
				// (heavy grains are a small share of the rock: up to 1/64, deepest in the core)
				if (worn > loose && g.tectonics.belt[c] > VEIN_CORE)
					heavy[c] += ((int64_t)(worn - loose) * a >> FRAC) * div(g.tectonics.belt[c] - VEIN_CORE, ONE - VEIN_CORE) / 64;
				load[c] += (int64_t)worn * a;
			}
			const int32_t r = down[c];
			int64_t keep; // the most this cell may be raised, and of its load the share laid (fixed)
			int64_t share;
			if (h[c] < 0 && r < 0) {
				keep = -1 - h[c]; // the sea: a shelf up to just under its level
				share = ONE;
			} else if (r < 0) {
				keep = INT32_MAX; // a closed basin's lowest: all that reaches it stays (a salt flat's fill)
				share = ONE;
			} else if (surface[c] > before[c]) {
				keep = surface[c] - h[c]; // a lake (as the round began): all of it settles, till it's full
				share = ONE;
			} else {
				keep = (int64_t)lowest_donor[c] - 1 - h[c]; // (64 bits: with no donor that's INT32_MAX less a height)
				const int64_t settles = mulShift(DEPOSIT, a, FRAC);
				share = (settles << FRAC) / std::max<int64_t>(area[c] + settles, 1);
			}
			const fix laid = (fix)std::clamp<int64_t>(std::min(keep, mulShift(load[c], share, FRAC) / a), 0, INT32_MAX);
			if (laid > 0) {
				// heavy grains drop out HEAVY times as readily as the rest
				const int64_t part = std::min<int64_t>(ONE, ((int64_t)laid * a << FRAC) / std::max<int64_t>(load[c], 1) * HEAVY);
				const int64_t dropped = mulShift(heavy[c], part, FRAC);
				h[c] += laid;
				g.sediment[c] += laid;
				heavy_laid[c] += dropped;
				laid_last[c] = laid;
				dropped_last[c] = dropped;
				load[c] -= (int64_t)laid * a;
				heavy[c] -= dropped;
			}
			if (r >= 0) {
				load[r] += load[c];
				heavy[r] += heavy[c];
				lowest_donor[r] = std::min(lowest_donor[r], h[c]);
			} else {
				g.to_deep += load[c];
			}
		}
		// creep, between land cells (the coast stays where the rivers left it)
		creep(h, mul(CREEP, pace), crumbs);
		g.uplifted += rebound(h, floated, REBOUND, FLEX_PASSES);
		if (watch)
			(*watch)({ h, g.sediment, area });
	}
	for (size_t i = 0; i < n; i++)
		g.to_deep += crumbs[i]; // (under a unit of height a cell: counted as gone, so the balance is exact)
	for (fix &v : h)
		v += old_sea;
	g.eroded = h;
	step.reset();
	step.emplace(code.text, "lakes and rivers");
	g.worn.resize(n);
	for (size_t i = 0; i < n; i++)
		g.worn[i] = g.base[i] >= 0 ? g.eroded[i] - g.base[i] : 0;
	// placers: the share of what was laid that's heavy grains, ×12 (0..1: rich gravel is a few percent gold and tin)
	// What lies on the land: off the rivers, a gully that wears as fast as it
	// lays (steep, at its balance) only carries its load across: the bed it
	// lays one round it wears the next, so its last round's is in transit,
	// not alluvium. A river reworks its bed and floodplain the same way, but
	// always floors its valley with what it carries: that stays.
	const int64_t river = water::riverThreshold(code);
	for (size_t i = 0; i < n; i++)
		if (laid_last[i] > 0 && worn_last[i] >= laid_last[i] && ran[i] < river) {
			g.sediment[i] -= laid_last[i];
			heavy_laid[i] -= dropped_last[i];
		}
	g.placer.assign(n, 0); // (heavy_laid: what's in the sediment now)
	for (size_t i = 0; i < n; i++)
		// (below the source rock: within it, the grains are still in their veins)
		if (g.sediment[i] > 0 && g.tectonics.belt[i] <= VEIN_CORE)
			g.placer[i] = (fix)std::min<int64_t>(ONE, (heavy_laid[i] / sphere.area((int32_t)i) * 12 << FRAC) / g.sediment[i]);
}

} // namespace terragen::wear
