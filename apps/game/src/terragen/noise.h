// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include "sphere.h"

namespace terragen {

/*
	Value noise on the sphere: a 3D lattice sampled at a direction, so it has
	no seam and no pole (planning/sprint-86-sphere.md). `frequency` is
	lattice cells round the equator, as the flat map's noise counted them
	across its width; each octave doubles it. Results in −1..1.
*/
fix noise(const Dir &d, uint32_t seed, int32_t frequency);
fix fbm(const Dir &d, uint32_t seed, int32_t frequency, int octaves, fix persistence);
// The same at a place to the block: for what varies column to column, which
// a Dir can't tell apart on a large planet.
fix noise(const Place &p, uint32_t seed, int32_t frequency);
fix fbm(const Place &p, uint32_t seed, int32_t frequency, int octaves, fix persistence);

/*
	Value noise in 3D block coordinates, for what varies with depth (rock
	folds, ore veins, caves), at a block's place in the planet
	(World::solid). `cell` is the lattice spacing in blocks. Results in −1..1.
*/
fix value3(int32_t x, int32_t y, int32_t z, uint32_t seed, int32_t cell);

} // namespace terragen
