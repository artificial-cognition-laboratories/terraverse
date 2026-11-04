// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "mapnode.h"
#include "piece/machine.h"
#include <string>
#include <vector>

class IGameDef;

namespace workshop {

/*
	Small things (sprint 130): what lies about to be picked up, a machine of one
	part at SMALL_SCALE, so its sixteenths are 8 mm. A stone's a lump with
	flat breaks in it; a stick's a rod with a slight bend. `seed` makes each
	its own.
*/
constexpr f32 SMALL_SCALE = 1.f / 8;
// Half-widths in its cells each way. Throws std::invalid_argument on what's
// too small to be anything or too long for a part.
// Of these cells (sixteenths on its grid, where they are). Throws
// std::invalid_argument on one off its grid.
piece::Machine smallOfCells(IGameDef *gamedef, content_t material, const std::vector<v3s32> &cells);
piece::Machine stone(IGameDef *gamedef, content_t material, v3s16 half, u32 seed);
piece::Machine stick(IGameDef *gamedef, content_t material, s16 radius, s16 length, u32 seed);

} // namespace workshop
