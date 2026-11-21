// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/grid/geology.h"
#include <vector>

/*
	Each cell's tectonic setting: what its history made it (sprint 120,
	.agents/planning/sprint-120-land-with-a-history.md). A setting is finer
	than a province: a province says what rock lies under it, a setting says
	why the land stands as it does, which is what its relief will follow.
	Each setting's province is in its row of SETTINGS.
*/
namespace terragen {

struct Grid;

enum class Setting : uint8_t {
	Craton,       // old basement, nothing happening to it: worn flat (the Canadian Shield)
	Platform,     // a craton under a thin cover of flat beds (the Russian Platform)
	Sag,          // a broad basin sinking since a plume left it (the Congo, Michigan)
	Swell,        // held up over a plume now (Ahaggar, Tibesti, East Africa)
	FloodBasalt,  // a plume's head's lava, layer on layer (the Deccan Traps)
	Rift,         // being pulled apart now (the East African Rift)
	FailedRift,   // pulled apart once, then given up, filled with sediment (the Benue Trough)
	Margin,       // a continent's edge where an ocean opened: a coastal plain and its scarp (the Atlantic's)
	YoungBelt,    // a range rising now or lately (the Himalaya, the Alps)
	OldBelt,      // a range worn down to its roots (the Appalachians, the Urals)
	Foreland,     // the trough in front of a range, filled with what it sheds (the Ganges plain)
	Suture,       // the seam where two continents were sewn together, long ago
	Arc,          // over a sinking plate: volcanoes and their range (the Andes, Japan)
	Volcanic,     // a volcano's own field, or a world of eruptions
	Ridge,        // the sea floor opening
	OceanFloor,
	Highland,     // a stagnant lid's (planning/sprint-95-maria.md)
	Mare,
	Count
};

/*
	How a setting's land is made (sprint 120, phase 2), what the erosion
	reads: `still`, the share of the slow rise all young crust gets that it
	gets (a craton's has long stopped); `rise`, a rise of its own, of a
	range's (a plume's swell, a margin's upwarped shoulder); `sink`, how fast
	it sags, of a basin's (a sag, a foreland under its range's weight);
	`hard`, how readily its rock wears, of the usual (granite and basalt
	slowly; a basin's soft beds fast); `worn`, how far it was levelled before
	the history's last steps (a shield is a plain cut across old mountains'
	roots, worn for longer than the history runs).
*/
struct SettingRow
{
	const char *name;
	Province province;
	fix still, rise, sink, hard, worn;
};
extern const SettingRow SETTINGS[(int)Setting::Count];

// (land is where `height` is at or over the sea: the grid's base before
// the erosion, its eroded height after)
std::vector<Setting> settings(const WorldCode &code, const Grid &g, const std::vector<fix> &height);

} // namespace terragen
