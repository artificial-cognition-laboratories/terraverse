// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/tiling.h"
#include "terragen/world.h"
#include <SColor.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/*
	The overlays, painted on the land as it's built (planning/sprint-83-
	planetarium.md): each tile's vertices coloured from the world model's
	fields where they stand, so an overlay is as sharp as the tiles under
	it, a body's coarsest bake and the ground close up alike. The page's
	painter (ui/src/worldgen/fields.ts `paint`) paints the flat map the
	same way; its ramps are these.
*/
namespace planetarium {

// The overlays, as the page names them (OVERLAYS, less terrain and the weather).
enum class Overlay : uint8_t { Biomes, Elevation, Temperature, Moisture, Seasons, Fertility, Soil, Currents, Plates, Geology };
// By the page's name; none for a name that isn't one.
std::optional<Overlay> overlayNamed(const std::string &name);

class Painter
{
public:
	struct Opts
	{
		std::shared_ptr<const terragen::World> world;
		Overlay overlay;
		// or an image the page painted (the weather: a year simulated there),
		// equirectangular as the world model's preview is (row 0 north,
		// column 0 longitude −π), RGBA; then `world` isn't needed
		struct Image
		{
			u32 width, height;
			std::vector<u8> rgba;
		};
		std::shared_ptr<const Image> image;
		// that day's climate (0..1 through the year); none: the yearly mean
		std::optional<double> date;
		// what's been explored (survival): a grid × grid bit per cell, row 0
		// north, as tf_player keeps it; none: all of it
		std::optional<std::vector<u8>> explored;
		u32 grid = 0;
	};
	explicit Painter(Opts opts);

	// A vertex's colour: `own` the land's (its shade kept, so relief shows;
	// alpha 0 a liquid's top), at `at` round the body (any radius).
	video::SColor colour(video::SColor own, const far::Tiling::Vec &at);

private:
	struct Sample
	{
		terragen::Column column;
		terragen::fix fertility, current;
		terragen::fix sand, silt, clay, organic, stones; // the regional soil (World::regionalSoil), on land
		int32_t cell;
	};
	const Sample &sample(terragen::fix u, terragen::fix v);

	const Opts m_opts;
	// samples by place (a fine lattice): a tile's vertices share their corners
	std::unordered_map<u64, Sample> m_samples;
};

} // namespace planetarium
