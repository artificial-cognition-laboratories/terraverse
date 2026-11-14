// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/fixed.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

/*
	Flora: plants grown from where they stand (planning/sprint-113-flora.md).
	A plant is lineage × site, and nothing about it is rolled freely:

	- a Site is what terragen knows of a column: its climate, its soil, its
	  slope and wind;
	- a Strategy is what the site comes to: how far warmth, water and food
	  are met (vigour), and how often the ground or the weather knocks a plant
	  back (disturbance). Ecology's triangle: competitors where both are kind,
	  tolerators where supply is short, pioneers where the ground is new;
	- a Lineage is what climate never changes: leaf type, how it branches,
	  its crown's profile, its colours, the cold and drought it can bear;
	- Traits are a lineage's answer to a site, all moved by the strategy
	  together, so the same conditions give the same look everywhere;
	- a Plant is its blocks.

	Pure and fixed-point, as terragen: a plant's blocks decide the world, so
	the same seed grows the same plant on every machine.
*/
namespace flora {

using terragen::fix;
using terragen::HALF;
using terragen::ONE;
using terragen::ratio;

struct Site
{
	// a column's climate, in terragen's units (Column)
	fix heat = 0;   // ~−1 frozen .. 1 scorching
	fix wet = 0;    // ~−1 arid .. 1 soaked
	fix season = 0; // when the rain falls: 1 all in summer .. −1 all in winter
	// its soil (Soil, Cover)
	fix fertility = HALF;
	fix sand = ratio(4, 10), clay = ratio(2, 10); // shares of the fine earth
	fix stones = 0;
	int32_t depth = 4; // blocks of soil over the rock
	// what knocks a plant back
	fix wind = 0;  // 0 calm .. 1 a gale most days
	fix slope = 0; // rise over run
	fix upset = 0; // 0..1: how lately the ground was made or moved (fresh lava, a dune, a river's bar)
	bool bank = false; // beside a river or lake: water whatever the rain
	fix light = ONE;   // 0..1 of the open sky's, under whatever stands over it
};

enum class Kind : uint8_t { Competitor, Tolerator, Pioneer, Count };
extern const char *const KIND_NAMES[(int)Kind::Count];

// What holds a plant back most.
enum class Limit : uint8_t { None, Cold, Drought, Hunger, Rooting, Shade, Wet, Count };
extern const char *const LIMIT_NAMES[(int)Limit::Count];

struct Strategy
{
	fix warmth, water, light, food, rooting; // 0..1 each: how far the site meets that need
	fix vigour;      // what can grow there: the climate's shorter supply, scaled by the ground's
	fix disturbance; // 0..1
	Kind kind;
	Limit limit;
};
Strategy strategy(const Site &site);

/*
	A plant's genes: every way one plant differs from another, as numbers.
	A tree is a tall woody one, a bush a short woody one, a grass a short
	green one with strap leaves from its foot; a monstera a green one with
	few great split leaves on long stalks. Nothing in the generator knows
	those names: they're corners of this space (the presets, site.cpp).
	Each gene has its range and its word (GENES), so the wiki lists them
	without knowing them either.
*/
enum class Gene : uint8_t
{
	// the stems
	Woody,      // 0 green and soft .. 1 wood
	Fleshy,     // 0 .. 1 swollen with water (a cactus's stem)
	Height,     // blocks, where it does best
	Low,        // and where it barely grows, of that
	Stems,      // from its foot
	Splay,      // how far they lean out from each other, radians
	Girth,      // a stem's radius at its foot, of the plant's height
	Taper,      // 0 as thick at its top .. 1 to a point
	Upright,    // −1 its stems arch over .. 1 they hold straight up
	Wander,     // how much they kink as they go, 0..1
	// how it branches
	Levels,     // of branching off the stems: 0 none
	Angle,      // a branch's off its parent, radians
	PerNode,    // branches at a node
	Spiral,     // the turn from one node to the next, of a full turn (0.5 alternate, 0.38 the golden spiral)
	Internode,  // the gap between nodes, of the axis
	Reach,      // a branch's length, of its parent's
	Crown,      // −1 its low branches longest (a cone) .. 1 its high ones (an umbrella)
	Bare,       // of the stem, from its foot, with nothing on it
	Rise,       // how branches turn up (or, under 0, down) as they go
	// leaves
	Leafy,      // of each axis, from its tip, that bears leaves
	Leaves,     // at a node
	Size,       // a leaf's length, blocks (stalk not counted)
	Stalk,      // its stalk's, of that
	Width,      // its blade's, of its length
	Widest,     // where along it that is, 0 its base .. 1 its tip
	Point,      // how it closes to its tip: under 1 sharp, over 1 blunt
	Heart,      // lobes back past its stalk, 0..1
	Teeth,      // its edge's, 0..1
	Lobes,      // how many on each side
	Cut,        // how deep they go, of its half-width
	Splits,     // slits in from its edge, 0..1 (a monstera's)
	Holes,      // windows in it, 0..1
	Compound,   // 0 one blade, 1 feathered (leaflets in rows), 2 a fan (leaflets from one point)
	Leaflets,   // how many
	Spread,     // a leaf's angle off its axis, radians
	Droop,      // how far it bends down along its length
	Curl,       // its sides cupped up (over 0) or rolled down
	Thick,      // voxels deep
	Variegate,  // of it in its second colour (a pale vein, stripes)
	Terminal,   // a crowning cluster of leaves at each tip, 0..1 of a node's
	// flowers
	Bloom,      // how freely it flowers, 0..1
	Head,       // a flower's size, blocks
	Raised,     // held up on a stalk over the leaves, blocks
	Count,
};
struct GeneInfo
{
	const char *id, *label, *group;
	fix lo, hi;
	fix usual; // where a preset says nothing
	bool whole; // counted, not measured
};
extern const GeneInfo GENES[(int)Gene::Count];
using Genome = std::array<fix, (int)Gene::Count>;

// No single plant: a cover on the ground (moss, turf, lichen). Its own small generator.
struct Cover
{
	bool on = false;
	fix tufted = 0;  // 0 a smooth cushion .. 1 separate blades
	fix flowers = 0; // of its blades, the share that flower
};

// A lineage's colours: what climate only shades.
struct Colours
{
	uint32_t bark, leaf, vein, bloom, autumn; // 0xRRGGBB
};

// The kinds a lineage starts from (it's then its own). See PRESETS in site.cpp.
struct Preset
{
	const char *id, *label, *group;
};
extern const std::vector<Preset> &presets();

struct Lineage
{
	uint32_t seed;
	std::string name; // its own, made of the world's syllables
	int preset;
	Genome genes;
	Cover cover;
	fix cold, dry;  // the least warmth and water it lives on (Strategy's)
	fix shade;      // the least light
	fix sodden;     // the most water: past it, it rots (over 1: never)
	bool stores;    // it keeps water: a drought doesn't stunt it
	bool evergreen; // it keeps its leaves whatever the year does
	Colours colours;
};
// A world's lineages: one of each preset first, so every site has a taker, then more of any.
std::vector<Lineage> lineages(uint32_t seed, int count);

// One plant's place in its own life and in the year.
struct Life
{
	fix age = ratio(60, 100); // 0 a seedling .. 1 as old as its kind gets
	fix season = HALF;        // 0 midwinter, a quarter spring, a half midsummer, three quarters autumn
};

struct Traits
{
	bool lives;  // (not: `why` says what it can't bear)
	Limit why;
	Genome genes; // its lineage's, as the site and its life bent them: Height is its own now
	Cover cover;
	int32_t grain;  // its voxels to a block's side (2, 4 or 16)
	bool evergreen;
	fix leafing;    // 0 bare .. 1 in full leaf
	fix flowering;  // 0..1 of its bloom, now
	fix lean;       // how far the wind bends it, 0..1
	fix age;
	fix patch;      // a cover's radius on the ground, blocks
	Colours colours; // its leaves' as the site and season shade them
	fix gene(Gene g) const { return genes[(int)g]; }
};
Traits traits(const Lineage &lineage, const Site &site, const Life &life = Life());

enum class Part : uint8_t { Wood, Branch, Leaf, Flesh, Flower, Vein, Count };
extern const char *const PART_NAMES[(int)Part::Count];

struct Voxel
{
	int16_t x, y, z; // from its foot, in its grain (y up; the wind blows toward +x)
	Part part;
	uint8_t size;    // a Branch's thickness in sixteenths of its voxel; else 16
};

struct Plant
{
	int32_t grain = 1;         // voxels to a block's side
	std::vector<Voxel> voxels; // sorted by y, z, x
	int32_t count[(int)Part::Count] = {};
	int32_t reach = 0, top = 0; // the furthest it gets from its foot sideways, and up (voxels)
	// each voxel's woody neighbours (Wood or Branch), a bit for each of the 26
	// round it: (dy + 1) · 9 + (dz + 1) · 3 + (dx + 1). What joins a thin
	// branch to what it grows from, found once, not each time it's placed
	std::vector<uint32_t> woody;
};
// The furthest any plant reaches from its foot sideways, in blocks: the
// generator grows a chunk's neighbours' trees out to here, and a small
// plant (under a tree's height) is drawn this far past the node it's kept
// in (TestFlora checks both).
constexpr int32_t MOST_REACH = 32, MOST_GROUND_REACH = 6;
// A plant of those traits; `seed` is the individual's (its place's roll).
Plant grow(const Traits &traits, uint32_t seed);

} // namespace flora
