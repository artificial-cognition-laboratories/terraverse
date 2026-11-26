// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/*
	A scoop (sprint 124: the shovel): the inventory's unit of dug ground, a
	block across and a quarter deep, four to a block. Dug ground is counted to
	the sixteenth and given as a scoop for each CELLS of one soil.
*/
namespace scoop {

constexpr int CELLS = 16 * 16 * 4; // a scoop's sixteenths

} // namespace scoop
