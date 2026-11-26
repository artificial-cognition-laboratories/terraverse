// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <optional>
#include <string>

namespace planetarium::probe {

/*
	`terraverse --planetarium-probe <out.json>`: the Planetarium alone, off
	screen, driven by a script (script.h: open a world, zoom, pan, zoom
	out), every step timed: its frames, every thread's zones (the tile
	workers' stages too), the tiles taken, let go and taken again. Written as
	JSON for `bun perf:planetarium`, which runs it cold and warm and shows
	the last run beside it. Throws if the world won't open or the file
	can't be written.
*/
struct Opts
{
	std::string code;                 // the world
	std::string script = "zoom";
	std::string out;                  // the JSON
	std::optional<std::string> shots; // a picture at each step's end, in this folder
	std::optional<float> cell;        // pixels a tile's cell may span (none: the game's)
	unsigned width = 1920, height = 1080;
};

void run(const Opts &opts);

} // namespace planetarium::probe
