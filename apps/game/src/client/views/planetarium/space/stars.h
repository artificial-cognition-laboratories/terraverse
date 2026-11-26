// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_ptr.h"
#include "src/shared/base/irrlichttypes_bloated.h"
#include "library/space/milkyway/milkyway.h"
#include <CMeshBuffer.h>
#include <vector>

/*
	The galaxy's stars as the sky draws them (Galaxy), made off the frame's
	thread: on it, the first frame took 95 ms to make them. Pure CPU: what's
	made goes to the GPU when it's first drawn.
*/
namespace planetarium::stars {

constexpr u32 GRAIN = 1024; // the unresolved stars' texture, a side

// The stars bright enough to draw, as points (Galaxy's star shader places
// them); their buffers' material is the drawer's to set
std::vector<irr_ptr<scene::SMeshBuffer>> of(const milkyway::Picture &sky);
// The stars too many to draw, as a texture's pixels (GRAIN a side, A8R8G8B8)
std::vector<u32> grain();

} // namespace planetarium::stars
