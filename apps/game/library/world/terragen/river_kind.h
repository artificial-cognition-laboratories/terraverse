// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// What kind of river a run is, by what it runs over and how hard it works
// (planning/sprint-116-land-by-the-book.md, phase 6d), and what each kind
// looks like: a bag, read by World::shapeRuns and World::ground.
#pragma once

#include "library/world/terragen/fixed.h"
#include <array>
#include <algorithm>
#include <cstddef>

namespace terragen {
namespace river {

enum class Kind : uint8_t
{
	Bedrock,    // cut into hard rock: narrow, steep walls, no bars, holds its line
	Confined,   // in its valley, on a thin cover: wanders a little, a bar here and there
	Meandering, // on its own sediment, gentle: loops, a floodplain, a bar in every bend
	Braided,    // steep for its load of gravel: wide, shallow, among bars
	Count
};

struct Look
{
	fix turn;     // its meanders' sharpest turn (radians), as far as its grade and banks let it
	fix width;    // of the width its flow gives
	fix bars;     // the share of its bends with a point bar
	fix bank;     // how steeply its banks rise from the water (blocks a block); twice on a bend's outside
	int32_t lip;  // blocks its banks stand over its water at the edge
};

constexpr std::array<Look, (std::size_t)Kind::Count> LOOKS{ {
	{ ratio(25, 100), ratio(70, 100), 0, toFix(2), 2 },              // Bedrock: 14°
	{ ratio(80, 100), ONE, ratio(30, 100), ratio(80, 100), 1 },      // Confined: 46°
	{ ratio(145, 100), ONE, ONE, ratio(30, 100), 1 },               // Meandering: 83° (some loops to 112°)
	{ ratio(12, 100), toFix(3), 0, ratio(50, 100), 1 },              // Braided: 7°
} };

inline const Look &look(Kind k) { return LOOKS[(std::size_t)k]; }

// How much wider than life rivers are drawn, to be seen and sailed: BOLD, but
// never more than the land's heights are made taller (√(R⊕ / R), 1 / metre,
// the blocks a metre): on a planet Earth's size, as they are (World::riverWidth)
constexpr fix WIDTH = ratio(12, 10000); // metres for √(m²) of what runs off, at Earth's rain
constexpr fix BOLD = ratio(25, 10);
inline fix bold(fix metre) { return std::min<fix>(BOLD, div(ONE, std::max<fix>(1, metre))); }

// Its kind: braided where it's steep for what it carries (World::braids);
// otherwise by its bed: on hard rock with little sediment over it, steeper
// than a floodplain's, it's cut into the rock; on a deep cover of its own
// sediment, or nearly flat on soft ground, and free of the hills, it
// meanders; between, confined
// (alluvium: how deep the sediment over the rock is, 0..1; hard: the rock's
// hardness, 0..1; grade: its fall over its run)
constexpr fix BEDROCK_HARD = ratio(50, 100), BEDROCK_COVER = ratio(35, 100), BEDROCK_GRADE = ratio(2, 1000);
constexpr fix MEANDER_COVER = ratio(80, 100), MEANDER_GRADE = ratio(15, 10000);
// (confined: how the hills hold it, 0..1: it meanders only where it's free)
constexpr fix MEANDER_FREE = ratio(50, 100);
inline Kind kindOf(bool braids, fix alluvium, fix hard, fix grade, fix confined)
{
	if (braids)
		return Kind::Braided;
	if (hard >= BEDROCK_HARD && alluvium < BEDROCK_COVER && grade >= BEDROCK_GRADE)
		return Kind::Bedrock;
	if (confined < MEANDER_FREE && (alluvium >= MEANDER_COVER || (grade < MEANDER_GRADE && hard < BEDROCK_HARD)))
		return Kind::Meandering;
	return Kind::Confined;
}

} // namespace river
} // namespace terragen
