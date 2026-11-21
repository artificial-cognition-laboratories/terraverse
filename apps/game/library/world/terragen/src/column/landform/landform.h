// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/base/fixed.h"
#include "library/world/terragen/src/base/sphere.h"
#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/grid/setting.h"
#include <cstdint>

/*
	The landforms the column builds (sprint 120, phase 6,
	.agents/planning/sprint-120-land-with-a-history.md). Each named landform
	is a row of LANDFORMS: the settings it forms in (what the cell's history
	made it, setting.h), the rock and climate it needs, and its shape. One
	builder reads every row, so a new landform is a new row.

	A landform is the size a player reads it, in blocks, as the dunes are:
	a planet is scaled down far more across (R / R⊕) than up (√ of that), so
	at a body's own metres an inselberg would be a block wide.
*/
namespace terragen {

struct Grid;

// The profiles a landform's built from (each a value function of the place: landform.cpp)
enum class Shape : uint8_t {
	Dome,   // round, steep-sided hills on a plain (bornhardts, drumlins)
	Cone,   // straight-sided cones (cinder cones)
	Tower,  // near-vertical towers with rounded tops (tower karst)
	Table,  // flat tops behind cliffs (mesas and buttes)
	Scarp,  // long ridges, gentle up the dip, steep down the scarp (cuestas)
	Ridged, // sharp ridges and gullies packed close (badlands)
	Count
};

// A range a landform forms in, eased in and out over an eighth of its width
struct Band
{
	fix from, to;
};

struct LandformRow
{
	const char *name;
	uint32_t settings; // the settings it forms in, as bits (`in`)
	bool limestone;    // only on limestone beds (the karst's: World::karst)
	Band hard;         // rock (World::hardness, 0..1)
	Band rain;         // the year's rain, of the planet's mean
	Band celsius;      // the yearly mean at its height
	Shape shape;
	int32_t wave;      // blocks from one to the next
	int32_t high;      // blocks, at the most
	fix share;         // of the ground where it forms, roughly, that it covers
};

enum class Landform : uint8_t {
	Inselbergs,
	Mesas,
	Cuestas,
	TowerKarst,
	Badlands,
	Cones,
	Drumlins,
	Count
};
extern const LandformRow LANDFORMS[(int)Landform::Count];

// The settings given, as LandformRow::settings' bits
template <typename... S>
constexpr uint32_t in(S... s)
{
	return ((1u << (int)s) | ...);
}

class Landforms
{
public:
	struct Opts
	{
		const Grid &grid;
		const WorldCode &code;
		int64_t radius;  // blocks
		int32_t meander; // noise waves round the planet a cell long (World's)
	};
	explicit Landforms(Opts o);

	// What's read at a column
	struct Site
	{
		Place at;
		fix elevation; // the grid's, at the place
		fix hard;      // World::hardness
		int64_t tile;  // the far view's tile, blocks (fixed; 0: a chunk's columns)
	};
	// The landforms' height over the land at a site, blocks (fixed)
	int64_t height(const Site &s) const;
	// How strongly a landform stands at a site (0..1: its setting, rock,
	// climate and patch together), for probes and the wiki
	fix strength(Landform form, const Site &s) const;

private:
	const Grid &m_grid;
	uint32_t m_seed;
	int64_t m_radius;
	int32_t m_meander;
};

} // namespace terragen
