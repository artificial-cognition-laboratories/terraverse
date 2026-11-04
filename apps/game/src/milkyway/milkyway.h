// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <vector>

/*
	The sky seen from inside a galaxy, worked out from one: a disc of old
	stars, a bulge, spiral arms of young ones, and a thin layer of clumped
	dust, traced along each way out from a star in the disc. The band, the
	dark lanes down it, the reddening at their edges and where the nebulae
	lie all come from that, not from painting. Pure (no engine): render it
	alone with scripts/milkyway-check/render.cpp. Visual only: floats, and
	not part of any world's determinism.
*/
namespace milkyway {

struct Way
{
	float x, y, z;
};

struct Opts
{
	uint32_t width = 2048, height = 1024; // longitude across, latitude down
	float exposure = 1;
	// the galaxy's north, and the way to its middle (square to it), in the picture's axes
	Way pole{ 0.32f, 0.84f, 0.44f };
	Way centre{ 0.7f, -0.0f, -0.51f };
};

struct Picture
{
	uint32_t width, height;
	std::vector<uint8_t> rgb; // 3 bytes a pixel, top row first
};

// Takes seconds, on every core: run it off the main thread.
Picture bake(const Opts &opts);

} // namespace milkyway
