// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "sphere.h"

/*
	What a rock lid's interior does to its land (planning/sprint-99-lids.md),
	before the impacts: long-lived plumes split it into a low thin side and a
	high thick one, raise one broad rise and crack canyons out of it (Mars);
	eruptions bury the relief, leaving only blocks the crust thrust up (Io).
	Read from the card's Processes, never the body's name.
*/
namespace terragen {

struct Grid;

// A canyon: along the great circle from the rise through `way`, between two
// angles from the rise (radians); half its width as a sine; depth in elevation
struct Canyon
{
	Dir way, pole;
	fix from, to, half, depth;
};

// Needs Grid::thin. Changes Grid::base and (blocks' ridges) tectonics.belt;
// fills Grid::rise and canyons.
void shapeLid(const WorldCode &code, Grid &g);

// The rise itself, younger than the plains' lava it lifts (after flood()).
void raiseRise(const WorldCode &code, Grid &g);

} // namespace terragen
