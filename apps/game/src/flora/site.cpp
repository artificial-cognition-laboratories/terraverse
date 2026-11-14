// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grow.h"
#include <algorithm>

namespace flora {

const char *const KIND_NAMES[(int)Kind::Count] = { "competitor", "tolerator", "pioneer" };
const char *const LIMIT_NAMES[(int)Limit::Count] = { "none", "cold", "drought", "hunger", "rooting", "shade", "wet" };
const char *const PART_NAMES[(int)Part::Count] = { "wood", "branch", "leaf", "flesh", "flower", "vein" };

namespace {

// 0 below lo, 1 above hi, a straight ramp between
fix ramp(fix v, fix lo, fix hi)
{
	return clamp(div(v - lo, hi - lo), 0, ONE);
}

uint32_t mix(uint32_t a, uint32_t b)
{
	uint32_t h = a ^ (b * 0x27d4eb2du);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

// a value in lo..hi, from a seed and which draw it is
fix draw(uint32_t seed, uint32_t which, fix lo, fix hi)
{
	return lo + mul(hi - lo, (fix)(mix(seed, which) >> 16));
}

uint32_t rgb(fix r, fix g, fix b)
{
	const auto byte = [](fix v) { return (uint32_t)clamp(roundInt(v * 255), 0, 255); };
	return (byte(r) << 16) | (byte(g) << 8) | byte(b);
}

void split(uint32_t c, fix &r, fix &g, fix &b)
{
	r = ratio((c >> 16) & 255, 255);
	g = ratio((c >> 8) & 255, 255);
	b = ratio(c & 255, 255);
}

uint32_t blend(uint32_t from, uint32_t to, fix t)
{
	fix r0, g0, b0, r1, g1, b1;
	split(from, r0, g0, b0);
	split(to, r1, g1, b1);
	return rgb(lerp(r0, r1, t), lerp(g0, g1, t), lerp(b0, b1, t));
}

// Growth stops where the year's too cold; past mild, more heat adds nothing.
const fix COLD_FROM = R(-80), COLD_TO = R(50);
// What rain leaves a plant, 0..1 of soaked, to how well watered it is.
const fix DRY_FROM = R(5), DRY_TO = R(55);
const fix SAND_DRAINS = R(45), CLAY_HOLDS = R(10), STONES_DRAIN = R(30); // of the water, by their share of the soil
const fix HEAT_DRIES = R(15);  // of soaked, at the hottest
const fix SUMMER_DROUGHT = R(12); // the same, where all the rain is winter's and the summer's hot
const fix BANK_WATER = R(85);  // a bank's roots reach the river
const int32_t SOIL_HOLDS = 4, SOIL_ROOTS = 3; // blocks: soil deeper than this holds no more water, cramps no more roots
const fix THIN_ROOTING = R(40); // on bare rock
const fix POOR_FOOD = R(60);    // on the poorest ground
const fix WIND_UPSETS = R(80);
const fix STEEP_FROM = R(50), STEEP_TO = R(150); // rise over run: ground that slides
const fix PIONEERS = R(60), COMPETITORS = R(45);

// A gene's range in the plant space, and its usual value
#define G(id, label, group, lo, hi, usual, whole) { id, label, group, lo, hi, usual, whole }
const fix PI_2 = HALF_PI;

} // namespace

const GeneInfo GENES[(int)Gene::Count] = {
	G("woody", "Woody", "Stems", 0, ONE, 0, false),
	G("fleshy", "Fleshy", "Stems", 0, ONE, 0, false),
	G("height", "Height (m)", "Stems", R(5), toFix(45), ONE, false),
	G("low", "Height where it struggles", "Stems", R(5), ONE, R(30), false),
	G("stems", "Stems from the foot", "Stems", ONE, toFix(40), ONE, true),
	G("splay", "Stems splay", "Stems", 0, R(140), R(20), false),
	G("girth", "Girth", "Stems", R(1), R(40), R(3), false),
	G("taper", "Taper", "Stems", 0, ONE, R(60), false),
	G("upright", "Upright … arching", "Stems", -ONE, ONE, R(60), false),
	G("wander", "Crooked", "Stems", 0, ONE, R(15), false),
	G("levels", "Branching levels", "Branches", 0, toFix(3), 0, true),
	G("angle", "Branch angle", "Branches", R(10), R(160), R(70), false),
	G("perNode", "Branches at a node", "Branches", ONE, toFix(6), ONE, true),
	G("spiral", "Turn between nodes", "Branches", 0, HALF, R(38), false),
	G("internode", "Node spacing", "Branches", R(2), HALF, R(12), false),
	G("reach", "Branch length", "Branches", R(10), R(120), HALF, false),
	G("crown", "Cone … umbrella", "Branches", -ONE, ONE, 0, false),
	G("bare", "Bare trunk", "Branches", 0, R(90), 0, false),
	G("rise", "Branches droop … rise", "Branches", -ONE, ONE, R(30), false),
	G("leafy", "Leafy share", "Leaves", R(2), ONE, ONE, false),
	G("leaves", "Leaves at a node", "Leaves", 0, toFix(12), ONE, true),
	G("size", "Leaf length (m)", "Leaves", R(2), toFix(3), R(30), false),
	G("stalk", "Leaf stalk", "Leaves", 0, toFix(2), R(20), false),
	G("width", "Leaf width", "Leaves", R(2), R(120), R(40), false),
	G("widest", "Widest at", "Leaves", R(10), R(90), R(40), false),
	G("point", "Sharp … blunt tip", "Leaves", R(30), toFix(4), ONE, false),
	G("heart", "Heart base", "Leaves", 0, ONE, 0, false),
	G("teeth", "Toothed edge", "Leaves", 0, ONE, 0, false),
	G("lobes", "Lobes", "Leaves", 0, toFix(8), 0, true),
	G("cut", "Lobe depth", "Leaves", 0, R(90), 0, false),
	G("splits", "Splits", "Leaves", 0, ONE, 0, false),
	G("holes", "Holes", "Leaves", 0, ONE, 0, false),
	G("compound", "Simple, feathered, fan", "Leaves", 0, toFix(2), 0, true),
	G("leaflets", "Leaflets", "Leaves", toFix(3), toFix(40), toFix(9), true),
	G("spread", "Leaf angle", "Leaves", 0, R(160), R(90), false),
	G("droop", "Leaf droop", "Leaves", 0, R(250), R(30), false),
	G("curl", "Rolled … cupped", "Leaves", -ONE, ONE, 0, false),
	G("thick", "Leaf thickness", "Leaves", ONE, toFix(4), ONE, true),
	G("variegate", "Patterned", "Leaves", 0, ONE, R(10), false),
	G("terminal", "Leaves crowning each tip", "Leaves", 0, toFix(30), 0, true),
	G("bloom", "Flowering", "Flowers", 0, ONE, 0, false),
	G("head", "Flower size (m)", "Flowers", R(3), ONE, R(12), false),
	G("raised", "Flower stalk (m)", "Flowers", 0, toFix(2), 0, false),
};
#undef G

namespace {

const fix NEVER = 2 * ONE; // (a `sodden` no water reaches)

struct Range
{
	Gene gene;
	fix lo, hi;
};
// A corner of the space and what it bears: each preset's lineages are drawn inside its ranges (mostly: see STRAYS).
struct Row
{
	Preset preset;
	std::vector<Range> genes;
	fix cold[2], dry[2], shade[2];
	fix sodden;
	bool stores, evergreen;
	uint32_t leaf[2];
	Cover cover = {};
};
constexpr fix D(double v) { return (fix)(v * 65536); } // (only ever a constant)
const std::vector<Row> ROWS = {
	// trees and bushes
	{ { "conifer", "Conifer", "Trees and bushes" }, { { Gene::Woody, ONE, ONE }, { Gene::Height, D(18), D(34) }, { Gene::Low, D(.12), D(.2) }, { Gene::Girth, D(.025), D(.035) },
		{ Gene::Taper, D(.8), D(.9) }, { Gene::Upright, ONE, ONE }, { Gene::Wander, 0, D(.15) }, { Gene::Levels, D(2), D(2) }, { Gene::Angle, D(1.2), D(1.45) }, { Gene::PerNode, D(3), D(5) },
		{ Gene::Spiral, D(.15), D(.3) }, { Gene::Internode, D(.06), D(.09) }, { Gene::Reach, D(.3), D(.45) }, { Gene::Crown, -ONE, D(-.7) }, { Gene::Bare, D(.05), D(.2) },
		{ Gene::Rise, D(-.3), D(.1) }, { Gene::Leafy, D(.7), D(.9) }, { Gene::Leaves, D(6), D(10) }, { Gene::Size, D(.08), D(.15) }, { Gene::Stalk, 0, 0 }, { Gene::Width, D(.1), D(.15) },
		{ Gene::Spread, D(.6), D(.9) }, { Gene::Droop, D(.1), D(.3) } },
		{ D(.04), D(.15) }, { D(.12), D(.25) }, { D(.2), D(.3) }, NEVER, false, true, { 0x2f5a44, 0x4a7a3a } },
	{ { "broadleaf", "Broadleaf", "Trees and bushes" }, { { Gene::Woody, ONE, ONE }, { Gene::Height, D(14), D(26) }, { Gene::Girth, D(.035), D(.05) }, { Gene::Upright, D(.7), D(.9) },
		{ Gene::Wander, D(.3), D(.5) }, { Gene::Levels, D(3), D(3) }, { Gene::Angle, D(.55), D(.9) }, { Gene::PerNode, D(1), D(2) }, { Gene::Spiral, D(.38), HALF }, { Gene::Internode, D(.1), D(.16) },
		{ Gene::Reach, D(.55), D(.7) }, { Gene::Crown, D(.1), D(.4) }, { Gene::Bare, D(.2), D(.35) }, { Gene::Rise, D(.2), D(.5) }, { Gene::Leafy, D(.45), D(.6) }, { Gene::Leaves, D(3), D(6) },
		{ Gene::Size, D(.25), D(.45) }, { Gene::Stalk, D(.1), D(.2) }, { Gene::Width, D(.4), D(.65) }, { Gene::Lobes, 0, D(4) }, { Gene::Cut, 0, D(.45) }, { Gene::Teeth, 0, D(.6) } },
		{ D(.25), D(.4) }, { D(.28), D(.4) }, { D(.25), D(.35) }, NEVER, false, false, { 0x4f8a35, 0x7ab04f } },
	{ { "flattop", "Flat-topped", "Trees and bushes" }, { { Gene::Woody, ONE, ONE }, { Gene::Height, D(7), D(13) }, { Gene::Girth, D(.035), D(.05) }, { Gene::Wander, D(.4), D(.7) },
		{ Gene::Levels, D(3), D(3) }, { Gene::Angle, D(.75), D(1.05) }, { Gene::PerNode, D(2), D(2) }, { Gene::Internode, D(.14), D(.2) }, { Gene::Reach, D(.6), D(.8) }, { Gene::Crown, D(.8), ONE },
		{ Gene::Bare, D(.45), D(.6) }, { Gene::Rise, D(.5), D(.8) }, { Gene::Leafy, D(.3), D(.45) }, { Gene::Leaves, D(4), D(7) }, { Gene::Size, D(.25), D(.4) }, { Gene::Compound, ONE, ONE },
		{ Gene::Leaflets, D(12), D(20) }, { Gene::Width, D(.3), D(.4) }, { Gene::Spread, D(1.2), D(1.5) } },
		{ D(.5), D(.65) }, { D(.1), D(.2) }, { D(.55), D(.7) }, NEVER, false, false, { 0x7a9a45, 0xa2ab4a } },
	{ { "column", "Columnar", "Trees and bushes" }, { { Gene::Woody, ONE, ONE }, { Gene::Height, D(16), D(28) }, { Gene::Girth, D(.025), D(.035) }, { Gene::Levels, D(2), D(2) },
		{ Gene::Angle, D(.2), D(.35) }, { Gene::PerNode, D(1), D(2) }, { Gene::Internode, D(.05), D(.08) }, { Gene::Reach, D(.2), D(.3) }, { Gene::Crown, D(-.2), D(.2) }, { Gene::Rise, D(.7), ONE },
		{ Gene::Bare, D(.05), D(.15) }, { Gene::Leafy, D(.6), D(.8) }, { Gene::Leaves, D(3), D(5) }, { Gene::Size, D(.15), D(.3) }, { Gene::Width, D(.3), D(.5) } },
		{ D(.35), D(.5) }, { D(.14), D(.26) }, { D(.45), D(.6) }, NEVER, false, false, { 0x3a6a48, 0x6f8f62 } },
	{ { "weeping", "Weeping", "Trees and bushes" }, { { Gene::Woody, ONE, ONE }, { Gene::Height, D(10), D(16) }, { Gene::Girth, D(.04), D(.05) }, { Gene::Wander, D(.3), D(.5) },
		{ Gene::Levels, D(3), D(3) }, { Gene::Angle, D(.5), D(.8) }, { Gene::PerNode, D(1), D(2) }, { Gene::Reach, D(.7), D(.9) }, { Gene::Rise, -ONE, D(-.6) }, { Gene::Bare, D(.2), D(.3) },
		{ Gene::Leafy, D(.7), D(.9) }, { Gene::Leaves, D(4), D(6) }, { Gene::Size, D(.2), D(.35) }, { Gene::Width, D(.12), D(.2) }, { Gene::Droop, D(.5), D(.9) } },
		{ D(.25), D(.4) }, { D(.55), D(.68) }, { D(.45), D(.6) }, NEVER, false, false, { 0x6a9a4a, 0x8ab860 } },
	{ { "giant", "Rainforest giant", "Trees and bushes" }, { { Gene::Woody, ONE, ONE }, { Gene::Height, D(30), D(45) }, { Gene::Girth, D(.03), D(.04) }, { Gene::Upright, ONE, ONE },
		{ Gene::Levels, D(3), D(3) }, { Gene::Angle, D(.9), D(1.2) }, { Gene::PerNode, D(2), D(3) }, { Gene::Internode, D(.08), D(.12) }, { Gene::Reach, D(.45), D(.6) }, { Gene::Crown, D(.6), D(.9) },
		{ Gene::Bare, D(.6), D(.75) }, { Gene::Rise, D(.2), D(.5) }, { Gene::Leafy, D(.35), D(.5) }, { Gene::Leaves, D(4), D(6) }, { Gene::Size, D(.4), D(.7) }, { Gene::Width, D(.35), D(.5) },
		{ Gene::Point, D(.5), D(.8) } },
		{ D(.6), D(.72) }, { D(.52), D(.64) }, { D(.15), D(.25) }, NEVER, false, true, { 0x2f7a2a, 0x4a9a2e } },
	{ { "shrub", "Shrub", "Trees and bushes" }, { { Gene::Woody, D(.9), ONE }, { Gene::Height, D(1), D(3) }, { Gene::Stems, D(3), D(7) }, { Gene::Splay, D(.4), D(.8) },
		{ Gene::Girth, D(.02), D(.03) }, { Gene::Wander, D(.4), D(.6) }, { Gene::Levels, D(2), D(2) }, { Gene::Angle, D(.6), D(.9) }, { Gene::Reach, D(.5), D(.7) }, { Gene::Leafy, D(.6), D(.8) },
		{ Gene::Leaves, D(2), D(4) }, { Gene::Size, D(.08), D(.25) }, { Gene::Width, D(.3), D(.6) }, { Gene::Bloom, 0, D(.6) } },
		{ D(.06), D(.3) }, { D(.05), D(.14) }, { D(.4), D(.6) }, NEVER, false, false, { 0x5a7f4a, 0x8a9a6a } },
	{ { "bamboo", "Cane grass", "Trees and bushes" }, { { Gene::Woody, D(.8), D(.8) }, { Gene::Height, D(8), D(15) }, { Gene::Stems, D(6), D(14) }, { Gene::Splay, D(.05), D(.15) },
		{ Gene::Girth, D(.006), D(.01) }, { Gene::Taper, D(.1), D(.2) }, { Gene::Upright, ONE, ONE }, { Gene::Levels, D(1), D(1) }, { Gene::Angle, D(.6), D(.8) }, { Gene::PerNode, D(2), D(3) },
		{ Gene::Internode, D(.06), D(.08) }, { Gene::Reach, D(.12), D(.2) }, { Gene::Bare, D(.4), D(.6) }, { Gene::Leafy, ONE, ONE }, { Gene::Leaves, D(3), D(5) }, { Gene::Size, D(.2), D(.3) },
		{ Gene::Width, D(.1), D(.14) }, { Gene::Droop, D(.4), D(.7) } },
		{ D(.45), D(.6) }, { D(.4), D(.55) }, { D(.4), D(.55) }, NEVER, false, true, { 0x5a9a3a, 0x8ab04a } },
	// palms and great leaves
	{ { "palm", "Feather palm", "Palms and great leaves" }, { { Gene::Woody, D(.6), D(.6) }, { Gene::Height, D(8), D(18) }, { Gene::Girth, D(.02), D(.03) }, { Gene::Taper, D(.1), D(.2) },
		{ Gene::Wander, D(.15), D(.3) }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Leaves, 0, 0 }, { Gene::Terminal, D(10), D(16) }, { Gene::Size, D(2), D(3) }, { Gene::Stalk, D(.15), D(.25) },
		{ Gene::Compound, ONE, ONE }, { Gene::Leaflets, D(24), D(36) }, { Gene::Width, D(.2), D(.3) }, { Gene::Spread, D(.7), D(1.1) }, { Gene::Droop, D(1), D(1.6) } },
		{ D(.68), D(.78) }, { D(.35), D(.5) }, { D(.45), D(.6) }, NEVER, false, true, { 0x4a9a3a, 0x7fae4a } },
	{ { "fanpalm", "Fan palm", "Palms and great leaves" }, { { Gene::Woody, D(.6), D(.6) }, { Gene::Height, D(5), D(12) }, { Gene::Girth, D(.025), D(.035) }, { Gene::Taper, 0, D(.1) },
		{ Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(12), D(20) }, { Gene::Size, D(.9), D(1.4) }, { Gene::Stalk, D(.8), D(1.2) }, { Gene::Compound, D(2), D(2) },
		{ Gene::Leaflets, D(18), D(30) }, { Gene::Width, D(.5), D(.75) }, { Gene::Spread, D(.6), D(1.2) }, { Gene::Droop, D(.3), D(.6) } },
		{ D(.6), D(.72) }, { D(.2), D(.4) }, { D(.5), D(.65) }, NEVER, false, true, { 0x4a8a5a, 0x7a9a6a } },
	{ { "banana", "Banana", "Palms and great leaves" }, { { Gene::Fleshy, D(.3), D(.3) }, { Gene::Height, D(2), D(4) }, { Gene::Girth, D(.05), D(.07) }, { Gene::Taper, D(.2), D(.3) },
		{ Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(6), D(10) }, { Gene::Size, D(1.8), D(2.6) }, { Gene::Stalk, D(.15), D(.25) }, { Gene::Width, D(.3), D(.4) },
		{ Gene::Widest, D(.45), D(.55) }, { Gene::Point, D(1.5), D(2.5) }, { Gene::Spread, D(.25), D(.6) }, { Gene::Droop, D(.7), D(1.2) }, { Gene::Splits, 0, D(.35) }, { Gene::Variegate, D(.05), D(.15) } },
		{ D(.6), D(.72) }, { D(.45), D(.6) }, { D(.3), D(.5) }, NEVER, false, true, { 0x4a9a3a, 0x7ab04a } },
	{ { "monstera", "Split-leaf", "Palms and great leaves" }, { { Gene::Woody, D(.1), D(.1) }, { Gene::Height, D(.5), D(1.5) }, { Gene::Stems, D(1), D(3) }, { Gene::Splay, D(.4), D(.7) },
		{ Gene::Girth, D(.03), D(.04) }, { Gene::Upright, 0, D(.3) }, { Gene::Wander, D(.3), D(.5) }, { Gene::Internode, D(.12), D(.18) }, { Gene::Leaves, D(1), D(1) }, { Gene::Size, D(.5), D(.9) },
		{ Gene::Stalk, D(1), D(1.5) }, { Gene::Width, D(.8), D(.95) }, { Gene::Widest, D(.3), D(.35) }, { Gene::Heart, D(.5), D(.8) }, { Gene::Splits, D(.5), D(.9) }, { Gene::Holes, D(.3), D(.7) },
		{ Gene::Spread, D(.5), D(.8) }, { Gene::Droop, D(.4), D(.8) } },
		{ D(.6), D(.72) }, { D(.5), D(.62) }, { D(.05), D(.15) }, NEVER, false, true, { 0x2f7a2a, 0x3f8a3a } },
	{ { "elephant", "Elephant ear", "Palms and great leaves" }, { { Gene::Height, D(.15), D(.3) }, { Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(3), D(6) },
		{ Gene::Size, D(.7), D(1.2) }, { Gene::Stalk, D(1), D(1.5) }, { Gene::Width, D(.7), D(.9) }, { Gene::Widest, D(.25), D(.3) }, { Gene::Heart, D(.8), ONE }, { Gene::Point, D(.6), D(.9) },
		{ Gene::Spread, D(.3), D(.6) }, { Gene::Droop, D(.3), D(.5) }, { Gene::Variegate, D(.1), D(.3) } },
		{ D(.6), D(.72) }, { D(.5), D(.62) }, { D(.08), D(.2) }, NEVER, false, true, { 0x3f8a3a, 0x5a9a4a } },
	{ { "prayer", "Patterned leaf", "Palms and great leaves" }, { { Gene::Height, D(.15), D(.3) }, { Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(6), D(12) },
		{ Gene::Size, D(.3), D(.55) }, { Gene::Stalk, D(.8), D(1.2) }, { Gene::Width, D(.4), D(.55) }, { Gene::Widest, D(.45), D(.55) }, { Gene::Variegate, D(.35), D(.6) }, { Gene::Spread, D(.3), D(.6) },
		{ Gene::Droop, D(.1), D(.3) }, { Gene::Curl, D(.1), D(.3) } },
		{ D(.6), D(.72) }, { D(.5), D(.62) }, { D(.04), D(.12) }, NEVER, false, true, { 0x2f6a3a, 0x4a7a3a } },
	// ferns, grasses, herbs
	{ { "fern", "Fern", "Ferns, grasses, herbs" }, { { Gene::Height, D(.08), D(.25) }, { Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(6), D(12) }, { Gene::Size, D(.6), D(1.4) },
		{ Gene::Stalk, D(.15), D(.35) }, { Gene::Compound, ONE, ONE }, { Gene::Leaflets, D(14), D(24) }, { Gene::Width, D(.25), D(.35) }, { Gene::Spread, D(.15), D(.5) }, { Gene::Droop, D(.6), ONE } },
		{ D(.15), D(.3) }, { D(.5), D(.62) }, { D(.05), D(.12) }, NEVER, false, true, { 0x3f8a3a, 0x5aa040 } },
	{ { "treefern", "Tree fern", "Ferns, grasses, herbs" }, { { Gene::Woody, D(.3), D(.3) }, { Gene::Height, D(2.5), D(5) }, { Gene::Girth, D(.04), D(.06) }, { Gene::Taper, 0, D(.1) },
		{ Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(10), D(16) }, { Gene::Size, D(1.2), D(2) }, { Gene::Stalk, D(.1), D(.2) }, { Gene::Compound, ONE, ONE },
		{ Gene::Leaflets, D(18), D(26) }, { Gene::Width, D(.3), D(.4) }, { Gene::Spread, D(.9), D(1.3) }, { Gene::Droop, D(.7), D(1) } },
		{ D(.35), D(.5) }, { D(.55), D(.65) }, { D(.1), D(.2) }, NEVER, false, true, { 0x3f8a3a, 0x5aa040 } },
	{ { "grass", "Tussock grass", "Ferns, grasses, herbs" }, { { Gene::Height, D(.05), D(.1) }, { Gene::Stems, D(14), D(30) }, { Gene::Splay, D(.2), D(.5) }, { Gene::Girth, D(.02), D(.03) },
		{ Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(1), D(1) }, { Gene::Size, D(.5), D(1.2) }, { Gene::Stalk, 0, 0 }, { Gene::Width, D(.03), D(.06) }, { Gene::Point, D(.4), D(.6) },
		{ Gene::Spread, D(.1), D(.3) }, { Gene::Droop, D(.6), D(1.2) }, { Gene::Bloom, D(.4), D(.8) }, { Gene::Head, D(.08), D(.15) }, { Gene::Raised, D(.5), D(1.2) } },
		{ D(.04), D(.2) }, { D(.12), D(.25) }, { D(.45), D(.6) }, NEVER, false, false, { 0x7a9a50, 0xa8b060 } },
	{ { "reed", "Reed", "Ferns, grasses, herbs" }, { { Gene::Height, D(1.5), D(3) }, { Gene::Stems, D(8), D(16) }, { Gene::Splay, D(.05), D(.15) }, { Gene::Girth, D(.004), D(.006) },
		{ Gene::Upright, ONE, ONE }, { Gene::Internode, D(.12), D(.18) }, { Gene::Leafy, D(.8), D(.9) }, { Gene::Leaves, D(1), D(1) }, { Gene::Spread, D(.4), D(.6) }, { Gene::Size, D(.4), D(.7) },
		{ Gene::Width, D(.05), D(.08) }, { Gene::Point, D(.4), D(.6) }, { Gene::Droop, D(.5), D(.8) }, { Gene::Bloom, D(.7), ONE }, { Gene::Head, D(.15), D(.3) } },
		{ D(.15), D(.3) }, { D(.78), D(.84) }, { D(.5), D(.6) }, NEVER, false, false, { 0x5a9a4a, 0x7fae4a } },
	{ { "herb", "Flowering herb", "Ferns, grasses, herbs" }, { { Gene::Height, D(.3), D(.8) }, { Gene::Stems, D(1), D(4) }, { Gene::Splay, D(.2), D(.5) }, { Gene::Girth, D(.01), D(.02) },
		{ Gene::Levels, D(1), D(1) }, { Gene::Angle, D(.3), D(.6) }, { Gene::Reach, D(.4), D(.6) }, { Gene::Leaves, D(1), D(2) }, { Gene::Size, D(.06), D(.18) }, { Gene::Width, D(.2), D(.5) },
		{ Gene::Lobes, 0, D(3) }, { Gene::Cut, D(.2), D(.5) }, { Gene::Bloom, ONE, ONE }, { Gene::Head, D(.08), D(.25) } },
		{ D(.08), D(.3) }, { D(.25), D(.45) }, { D(.2), D(.5) }, NEVER, false, false, { 0x4a9a3a, 0x7ab04a } },
	// what stores water
	{ { "agave", "Spiked rosette", "Succulents" }, { { Gene::Fleshy, D(.8), D(.8) }, { Gene::Height, D(.05), D(.1) }, { Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) },
		{ Gene::Terminal, D(14), D(24) }, { Gene::Size, D(.6), D(1.2) }, { Gene::Stalk, 0, 0 }, { Gene::Width, D(.12), D(.18) }, { Gene::Point, D(.4), D(.6) }, { Gene::Thick, D(2), D(3) },
		{ Gene::Teeth, D(.4), D(.8) }, { Gene::Spread, D(.3), D(.7) }, { Gene::Droop, 0, D(.2) } },
		{ D(.5), D(.62) }, { 0, 0 }, { D(.65), D(.8) }, D(.6), true, true, { 0x5a9a7a, 0x8ab09a } },
	{ { "stonecrop", "Fleshy rosette", "Succulents" }, { { Gene::Fleshy, D(.8), D(.8) }, { Gene::Height, D(.05), D(.08) }, { Gene::Stems, D(1), D(5) }, { Gene::Splay, D(.6), ONE },
		{ Gene::Leaves, 0, 0 }, { Gene::Leafy, D(.02), D(.02) }, { Gene::Terminal, D(12), D(20) }, { Gene::Size, D(.1), D(.18) }, { Gene::Stalk, 0, 0 }, { Gene::Width, D(.45), D(.6) },
		{ Gene::Thick, D(2), D(2) }, { Gene::Spread, D(.6), ONE }, { Gene::Curl, D(.3), D(.6) }, { Gene::Bloom, D(.4), D(.8) }, { Gene::Raised, D(.1), D(.2) } },
		{ D(.3), D(.5) }, { 0, 0 }, { D(.6), D(.75) }, D(.6), true, true, { 0x6a9a7a, 0x9ab08a } },
	{ { "cactus", "Columnar cactus", "Succulents" }, { { Gene::Fleshy, ONE, ONE }, { Gene::Height, D(3), D(7) }, { Gene::Girth, D(.06), D(.09) }, { Gene::Taper, D(.05), D(.1) },
		{ Gene::Upright, ONE, ONE }, { Gene::Levels, D(1), D(1) }, { Gene::Angle, D(1.4), D(1.57) }, { Gene::Internode, D(.2), D(.3) }, { Gene::Reach, D(.3), D(.5) }, { Gene::Rise, ONE, ONE },
		{ Gene::Bare, D(.3), D(.4) }, { Gene::Leaves, 0, 0 }, { Gene::Bloom, D(.5), D(.8) }, { Gene::Head, D(.1), D(.15) } },
		{ D(.5), D(.62) }, { 0, 0 }, { D(.65), D(.8) }, D(.5), true, true, { 0x4f8a4a, 0x7a9a5a } },
	{ { "barrel", "Barrel cactus", "Succulents" }, { { Gene::Fleshy, ONE, ONE }, { Gene::Height, D(.5), D(1) }, { Gene::Girth, D(.35), D(.45) }, { Gene::Taper, 0, 0 }, { Gene::Upright, ONE, ONE },
		{ Gene::Leaves, 0, 0 }, { Gene::Bloom, D(.6), D(.9) }, { Gene::Head, D(.08), D(.12) } },
		{ D(.5), D(.62) }, { 0, 0 }, { D(.65), D(.8) }, D(.5), true, true, { 0x4f8a4a, 0x7a9a5a } },
	// covers
	{ { "moss", "Moss", "Covers" }, { { Gene::Height, D(.06), D(.25) } }, { D(.02), D(.08) }, { D(.6), D(.7) }, { D(.03), D(.08) }, NEVER, false, true, { 0x4a8a2a, 0x7ab03a },
		{ true, 0, 0 } },
	{ { "turf", "Turf", "Covers" }, { { Gene::Height, D(.1), D(.9) }, { Gene::Bloom, 0, D(.12) } }, { D(.04), D(.12) }, { D(.15), D(.28) }, { D(.3), D(.45) }, NEVER, false, false,
		{ 0x5a9a3a, 0x8ab04a }, { true, D(.9), 0 } },
	{ { "lichen", "Lichen", "Covers" }, { { Gene::Height, D(.04), D(.07) } }, { 0, D(.04) }, { D(.04), D(.15) }, { D(.4), D(.6) }, NEVER, false, true, { 0x9aa890, 0xc8c8b0 },
		{ true, 0, 0 } },
};

// One lineage in this many has one gene strayed outside its preset's range: worlds have their odd ones.
const uint32_t STRAYS = 3;
const fix STRAY = R(30); // how far, of the gene's whole range

// Colours a lineage's are drawn between two of.
const uint32_t BARKS[] = { 0x4f4338, 0x7a7670, 0x7a4a30, 0xd8d4c4, 0x2f2a26, 0x6a7058, 0x9a8a6a, 0x9a5a38 };
const uint32_t BLOOMS[] = { 0xf4f0e8, 0xf0a0c0, 0xf0d040, 0xe04a3a, 0x9a6ad8, 0x6a8af0, 0xf09030, 0xd84a9a };
const uint32_t AUTUMNS[] = { 0xd8b030, 0xd87a28, 0xb83a28, 0x8a6a38, 0xe0c850 };
const uint32_t ODD_LEAVES[] = { 0x6a9a8a, 0x8a4a3a, 0x9ab040, 0x7a8a9a, 0x5a2a3a };
const uint32_t VEINS[] = { 0xe8f0d0, 0xd8a0b0, 0xf0f0f0, 0x9ac050, 0x3a2a30 }; // pale, pink, white, lime, near black
const uint32_t ODD_LEAF = 5;
template <typename T, size_t N> constexpr uint32_t countOf(T (&)[N]) { return (uint32_t)N; }

// A lineage's name: a world has its own few sounds, and its plants are called in them.
const char *const ONSETS[] = { "b", "d", "f", "g", "h", "k", "l", "m", "n", "p", "r", "s", "t", "v", "w", "y", "z", "sh", "th", "br", "dr", "kr", "tr", "st", "" };
const char *const VOWELS[] = { "a", "e", "i", "o", "u", "ai", "ei", "ou", "ea" };
const char *const CODAS[] = { "", "", "n", "r", "l", "s", "th", "m" };

std::string nameOf(uint32_t world, uint32_t own)
{
	// (a world keeps eight onsets, three vowels and three codas of them)
	std::string out;
	const uint32_t syllables = 2 + mix(own, 90) % 2;
	for (uint32_t i = 0; i < syllables; i++) {
		out += ONSETS[mix(world, 100 + mix(own, i * 3) % 8) % countOf(ONSETS)];
		out += VOWELS[mix(world, 200 + mix(own, i * 3 + 1) % 3) % countOf(VOWELS)];
		if (i + 1 == syllables)
			out += CODAS[mix(world, 300 + mix(own, i * 3 + 2) % 3) % countOf(CODAS)];
	}
	out[0] = (char)(out[0] - 'a' + 'A');
	return out;
}


} // namespace

Strategy strategy(const Site &s)
{
	Strategy out{};
	out.warmth = ramp(s.heat, COLD_FROM, COLD_TO);

	// water: the rain, what the soil keeps of it, less what the heat takes back
	const fix hot = maxx(0, s.heat);
	const fix holds = clamp(ONE - mul(SAND_DRAINS, s.sand) + mul(CLAY_HOLDS, s.clay) - mul(STONES_DRAIN, s.stones), R(20), ONE);
	const fix deep = lerp(HALF, ONE, ramp(toFix(s.depth), 0, toFix(SOIL_HOLDS)));
	fix left = mul(mul((s.wet + ONE) / 2, holds), deep) - mul(HEAT_DRIES, hot);
	left -= mul(mul(SUMMER_DROUGHT, maxx(0, -s.season)), hot);
	out.water = ramp(left, DRY_FROM, DRY_TO);
	if (s.bank)
		out.water = maxx(out.water, BANK_WATER);

	out.light = clamp(s.light, 0, ONE);
	out.food = lerp(POOR_FOOD, ONE, clamp(s.fertility, 0, ONE));
	out.rooting = lerp(THIN_ROOTING, ONE, ramp(toFix(s.depth), 0, toFix(SOIL_ROOTS)));
	// the climate's shorter supply sets the pace (Liebig's law); the ground scales it
	out.vigour = mul(std::min(out.warmth, out.water), mul(out.food, out.rooting));
	out.disturbance = std::max({ mul(WIND_UPSETS, clamp(s.wind, 0, ONE)), ramp(s.slope, STEEP_FROM, STEEP_TO), clamp(s.upset, 0, ONE) });

	out.kind = out.disturbance > PIONEERS ? Kind::Pioneer : out.vigour > COMPETITORS ? Kind::Competitor : Kind::Tolerator;
	// the shortest supply (food and rooting never fall to nothing: a tie goes to the climate)
	out.limit = Limit::None;
	fix least = R(90);
	const std::pair<fix, Limit> needs[] = { { out.warmth, Limit::Cold }, { out.water, Limit::Drought },
		{ out.food, Limit::Hunger }, { out.rooting, Limit::Rooting } };
	for (const auto &[met, limit] : needs)
		if (met < least) {
			least = met;
			out.limit = limit;
		}
	return out;
}

const std::vector<Preset> &presets()
{
	static const std::vector<Preset> out = [] {
		std::vector<Preset> list;
		for (const Row &r : ROWS)
			list.push_back(r.preset);
		return list;
	}();
	return out;
}

std::vector<Lineage> lineages(uint32_t seed, int count)
{
	std::vector<Lineage> out;
	const int presets = (int)ROWS.size();
	for (int i = 0; i < count; i++) {
		const uint32_t own = mix(seed, 0x464c4f52u + (uint32_t)i); // "FLOR"
		const int which = i < presets ? i : (int)(mix(own, 77) % (uint32_t)presets);
		const Row &row = ROWS[which];
		Lineage l{};
		l.seed = own;
		l.name = nameOf(seed, own);
		l.preset = which;
		l.cover = row.cover;
		l.sodden = row.sodden;
		l.stores = row.stores;
		l.evergreen = row.evergreen;
		uint32_t n = 0;
		const auto in = [&](fix lo, fix hi) { return draw(own, n++, lo, hi); };
		const auto roll = [&]() { return mix(own, 1000 + n++); };
		const auto among = [&](const uint32_t *palette, uint32_t size) {
			const uint32_t one = palette[roll() % size], other = palette[roll() % size];
			return blend(one, other, in(0, ONE));
		};
		for (int g = 0; g < (int)Gene::Count; g++)
			l.genes[g] = GENES[g].usual;
		for (const Range &r : row.genes)
			l.genes[(int)r.gene] = in(r.lo, r.hi);
		// the odd one: a gene pushed past its kind's range
		if (roll() % STRAYS == 0) {
			const int g = (int)row.genes[roll() % row.genes.size()].gene;
			const GeneInfo &info = GENES[g];
			l.genes[g] = clamp(l.genes[g] + mul(mul(info.hi - info.lo, STRAY), in(-ONE, ONE)), info.lo, info.hi);
		}
		for (int g = 0; g < (int)Gene::Count; g++)
			if (GENES[g].whole)
				l.genes[g] = toFix(roundInt(l.genes[g]));
		l.cold = in(row.cold[0], row.cold[1]);
		l.dry = in(row.dry[0], row.dry[1]);
		l.shade = in(row.shade[0], row.shade[1]);
		if (row.cover.on)
			l.cover.tufted = row.cover.tufted;
		l.colours.bark = among(BARKS, countOf(BARKS));
		l.colours.leaf = blend(row.leaf[0], row.leaf[1], in(0, ONE));
		if (roll() % ODD_LEAF == 0)
			l.colours.leaf = blend(l.colours.leaf, ODD_LEAVES[roll() % countOf(ODD_LEAVES)], R(65));
		l.colours.vein = blend(l.colours.leaf, VEINS[roll() % countOf(VEINS)], in(R(45), R(85)));
		l.colours.bloom = BLOOMS[roll() % countOf(BLOOMS)];
		l.colours.autumn = among(AUTUMNS, countOf(AUTUMNS));
		out.push_back(l);
	}
	return out;
}

namespace {

const fix STORED = R(65);          // the water a plant that stores it always has
const fix SHADED = R(70);          // growth at the least light a lineage bears, of its growth in all it can use
const fix LIGHT_ENOUGH = R(50);
const fix EDGE = R(12), STUNTED = R(45); // at the edge of what it bears, it's this short
const fix SEEDLING = R(12);
const int32_t ROCK_HOLDS = 6, DEPTH_HOLDS = 7; // roots hold a tree this tall on bare rock, and this much more per block of soil
const fix WIND_STUNTS = R(45);
const fix FINE_UNDER = R(250), QUARTER_UNDER = toFix(10); // a plant that spans less than this grows in sixteenths, then quarters, then halves
const fix SHADE_GROWS = R(60), DRY_SHRINKS = R(55); // a leaf's length: in deep shade, and in drought
const fix SHADE_WIDENS = R(30), DRY_NARROWS = R(30);
const fix FEWER = R(55);           // leaves at a node, where it barely grows
const fix FLAT = R(70), SHADE_CLEARS = R(55); // fire country's push toward a flat crown, and shade's bare trunk
const fix COLD_CONES = R(50);
const fix SHADE_LEVEL = R(40);     // leaves turn level to catch the light
const fix WINTER = R(75), DRY_SEASON = R(45);
const fix BARE = R(10), LEAFED = R(22), TURNING = R(30), BLOOM_FROM = R(14), BLOOM_TO = R(30), SPRING = R(25);
const uint32_t DRY_LEAF = 0x9a9c72, COLD_LEAF = 0x2f4a3a, LUSH_LEAF = 0x2f8a2a, STRAW = 0xc8b878;
const fix PATCH_SMALL = R(70), PATCH_LARGE = R(220);

fix after(fix from, fix to) { return (to - from) & (ONE - 1); }

} // namespace

Traits traits(const Lineage &l, const Site &site, const Life &life)
{
	const Strategy s = strategy(site);
	Traits t{};
	t.genes = l.genes;
	t.cover = l.cover;
	t.why = Limit::None;
	if (s.warmth < l.cold)
		t.why = Limit::Cold;
	else if (s.water < l.dry)
		t.why = Limit::Drought;
	else if (s.water > l.sodden)
		t.why = Limit::Wet;
	else if (s.light < l.shade)
		t.why = Limit::Shade;
	t.lives = t.why == Limit::None;
	const auto gene = [&](Gene g) -> fix & { return t.genes[(int)g]; };

	const fix water = l.stores ? maxx(s.water, STORED) : s.water;
	const fix lit = ramp(s.light, l.shade, std::min(ONE, l.shade + LIGHT_ENOUGH)), dim = ONE - lit;
	const fix vigour = mul(std::min({ s.warmth, water, lerp(SHADED, ONE, lit) }), mul(s.food, s.rooting));
	const fix margin = std::min(ramp(s.warmth, l.cold, l.cold + EDGE), l.stores ? ONE : ramp(s.water, l.dry, l.dry + EDGE));
	const fix wind = clamp(site.wind, 0, ONE), hot = maxx(0, site.heat);
	const fix age = clamp(life.age, 0, ONE), young = ONE - age, grown = ONE - mul(young, young);
	const bool woody = gene(Gene::Woody) >= HALF;
	t.age = age;
	t.lean = wind;

	// height: what the site grows, what roots hold, what wind and the edge leave, and how far it's got
	fix tall = mul(gene(Gene::Height), lerp(gene(Gene::Low), ONE, vigour));
	if (woody)
		tall = std::min(tall, toFix(ROCK_HOLDS + DEPTH_HOLDS * site.depth));
	tall = mul(mul(mul(tall, ONE - mul(WIND_STUNTS, wind)), lerp(STUNTED, ONE, margin)), lerp(SEEDLING, ONE, grown));
	gene(Gene::Height) = maxx(tall, R(3));

	// leaves: bigger and wider in shade, smaller and narrower in drought, fewer where it barely grows, level to catch what light there is
	gene(Gene::Size) = mul(mul(gene(Gene::Size), ONE + mul(dim, SHADE_GROWS)), lerp(DRY_SHRINKS, ONE, water));
	gene(Gene::Width) = mul(mul(gene(Gene::Width), ONE + mul(dim, SHADE_WIDENS)), lerp(ONE - DRY_NARROWS, ONE, water));
	for (const Gene g : { Gene::Leaves, Gene::Terminal })
		if (gene(g) > 0)
			gene(g) = toFix(std::max(1, roundInt(mul(gene(g), lerp(FEWER, ONE, vigour)))));
	gene(Gene::Spread) = lerp(gene(Gene::Spread), HALF_PI, mul(dim, SHADE_LEVEL));
	if (water < R(30) && !l.stores)
		gene(Gene::Thick) = std::min(gene(Gene::Thick) + ONE, toFix(4));
	// crowns: fire country (heat and a dry season) and drought flatten and lift them; cold makes cones; shade lifts them
	if (woody) {
		const fix flat = mul(FLAT, maxx(mul(hot, fabsx(site.season)), mul(ONE - water, HALF)));
		gene(Gene::Crown) = clamp(gene(Gene::Crown) + mul(flat, ONE - gene(Gene::Crown)) - mul(COLD_CONES, ONE - s.warmth), -ONE, ONE);
		gene(Gene::Bare) = std::max({ gene(Gene::Bare), mul(dim, SHADE_CLEARS), mul(flat, R(40)) });
	}
	// crooked: thin soil, wind, the edge of its range, great age
	const fix thin_soil = div(ONE - s.rooting, ONE - THIN_ROOTING);
	gene(Gene::Wander) = clamp(gene(Gene::Wander) + std::max({ mul(thin_soil, R(40)), mul(wind, R(40)), mul(ONE - margin, R(50)), age > R(80) ? (age - R(80)) * 2 : 0 }), 0, ONE);
	// the young haven't branched as far
	gene(Gene::Levels) = toFix(roundInt(mul(gene(Gene::Levels), grown)));

	// the grain: what it spans
	const fix span = std::max(gene(Gene::Height), mul(gene(Gene::Size), ONE + gene(Gene::Stalk)) + gene(Gene::Raised));
	t.grain = l.cover.on || span < FINE_UNDER ? 16 : span < QUARTER_UNDER ? 4 : 2;

	// the year
	const bool winters = s.warmth < WINTER, dries = fabsx(site.season) > DRY_SEASON && s.water < HALF;
	t.evergreen = l.evergreen || l.stores || !(winters || dries);
	const fix dead = !winters && dries && site.season < 0 ? HALF : 0;
	const fix since = after(dead, life.season), until = after(life.season, dead);
	fix turned = 0;
	t.leafing = ONE;
	if (!t.evergreen) {
		t.leafing = ramp(std::min(since, until), BARE, LEAFED);
		turned = until < TURNING ? ONE - ramp(until, BARE, TURNING) : 0;
		t.flowering = since > BLOOM_FROM && since < BLOOM_TO ? ONE : 0;
	} else {
		const fix from_spring = std::min(after(SPRING, life.season), after(life.season, SPRING));
		t.flowering = from_spring < (BLOOM_TO - BLOOM_FROM) / 2 ? ONE : 0;
	}
	if (age < R(25))
		t.flowering = 0;

	t.colours = l.colours;
	uint32_t leaf = blend(l.colours.leaf, DRY_LEAF, mul(R(60), ONE - water));
	leaf = blend(leaf, COLD_LEAF, mul(R(45), ONE - s.warmth));
	leaf = blend(leaf, LUSH_LEAF, mul(R(30), mul(s.warmth, s.water)));
	if (woody)
		leaf = blend(leaf, l.colours.autumn, mul(turned, R(90)));
	else if (!t.evergreen) {
		leaf = blend(leaf, STRAW, mul(ONE - t.leafing, R(85))); // (what isn't woody dies back to straw, and stands)
		t.leafing = ONE;
	}
	t.colours.leaf = leaf;
	t.colours.vein = blend(l.colours.vein, leaf, R(30));
	t.patch = lerp(PATCH_SMALL, PATCH_LARGE, vigour);
	if (l.cover.on)
		t.cover.flowers = t.flowering > 0 ? gene(Gene::Bloom) : 0;
	return t;
}

Plant grow(const Traits &t, uint32_t seed)
{
	if (!t.lives)
		return Plant();
	Plant plant = t.cover.on ? growMat(t, seed) : growPlant(t, seed);
	// (its wood as bits in a box round it, a voxel's margin each way: 26 looks a voxel, no hashing)
	int32_t lo[3] = { INT16_MAX, INT16_MAX, INT16_MAX }, hi[3] = { INT16_MIN, INT16_MIN, INT16_MIN };
	for (const Voxel &v : plant.voxels) {
		const int32_t c[3] = { v.x, v.y, v.z };
		for (int k = 0; k < 3; k++) {
			lo[k] = std::min(lo[k], c[k] - 1);
			hi[k] = std::max(hi[k], c[k] + 1);
		}
	}
	const int32_t nx = hi[0] - lo[0] + 1, ny = hi[1] - lo[1] + 1, nz = hi[2] - lo[2] + 1;
	const auto cellOf = [&](int32_t x, int32_t y, int32_t z) { return ((size_t)(y - lo[1]) * nz + (z - lo[2])) * nx + (x - lo[0]); };
	std::vector<bool> wood(plant.voxels.empty() ? 0 : (size_t)nx * ny * nz);
	for (const Voxel &v : plant.voxels)
		if (v.part == Part::Wood || v.part == Part::Branch)
			wood[cellOf(v.x, v.y, v.z)] = true;
	plant.woody.resize(plant.voxels.size());
	for (size_t i = 0; i < plant.voxels.size(); i++) {
		const Voxel &v = plant.voxels[i];
		uint32_t mask = 0;
		for (int32_t dy = -1; dy <= 1; dy++)
			for (int32_t dz = -1; dz <= 1; dz++)
				for (int32_t dx = -1; dx <= 1; dx++)
					if ((dx || dy || dz) && wood[cellOf(v.x + dx, v.y + dy, v.z + dz)])
						mask |= 1u << ((dy + 1) * 9 + (dz + 1) * 3 + (dx + 1));
		plant.woody[i] = mask;
	}
	return plant;
}

} // namespace flora
