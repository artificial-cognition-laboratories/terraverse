// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <optional>
#include <string>

namespace planetarium {

/*
	`luanti --planetarium [code]`: the Planetarium alone in a window, on the
	planet a world code makes (the default world's if none), its land from
	the game's baked palette (far/baked.h): no world, no server, nothing
	saved. For looking at what the generator makes. Escape closes it.
	Throws if the code doesn't decode or the palette won't load.
*/
// `bench`: run that many seconds on a script (a few to settle, then a layer
// switched each second, through synthetic layers and back to none) and print
// the worst frame overall and the worst just after a switch, then return.
// `shot`: render the main menu's picture (planetarium/shot.h) to that PNG, once
// its land is built, and return. For tuning its looks without the game.
// `ladder`: the home shot's side from a small planet down to the ground, as
// PNGs in that directory (scripts/map-shots.sh scores them), and return.
void runStandalone(const std::string &code, std::optional<double> bench = std::nullopt,
		std::optional<std::string> shot = std::nullopt, std::optional<std::string> ladder = std::nullopt);

} // namespace planetarium
