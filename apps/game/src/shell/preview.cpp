// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "preview.h"
#include "bridge.h"
#include "worlds.h"
#include "terragen/preview.h"
#include "util/base64.h"
#include <optional>

namespace shell {

static std::string bytes(const void *data, size_t size)
{
	return base64_encode(std::string_view(static_cast<const char *>(data), size));
}

Json::Value previewJson(const Json::Value &intent, const std::optional<terragen::WorldCode> &world)
{
	if (!intent["code"].isString())
		throw Refused("worldgen.preview needs a world code");
	int resolution = intent["resolution"].isInt() ? intent["resolution"].asInt() : 0;
	if (resolution < 16 || resolution > 512)
		throw Refused("preview resolution must be 16..512");

	const terragen::WorldCode code = world ? *world : readWorldCode(intent["code"].asString());
	std::optional<terragen::fix> date;
	if (intent["date"].isNumeric()) {
		double d = intent["date"].asDouble();
		if (d < 0 || d >= 1)
			throw Refused("preview date must be in [0, 1)");
		date = (terragen::fix)(d * terragen::ONE);
	}

	const terragen::World built(code);
	terragen::Preview p = terragen::preview(built, date, resolution);

	Json::Value out;
	out["resolution"] = p.resolution;
	out["elevation"] = bytes(p.elevation.data(), p.elevation.size() * sizeof(int16_t));
	out["heat"] = bytes(p.heat.data(), p.heat.size());
	out["wet"] = bytes(p.wet.data(), p.wet.size());
	out["season"] = bytes(p.season.data(), p.season.size());
	out["fertility"] = bytes(p.fertility.data(), p.fertility.size());
	out["current"] = bytes(p.current.data(), p.current.size());
	out["biome"] = bytes(p.biome.data(), p.biome.size());
	out["lake"] = bytes(p.lake.data(), p.lake.size());
	out["rivers"] = bytes(p.rivers.data(), p.rivers.size());
	out["province"] = bytes(p.province.data(), p.province.size());
	out["plate"] = bytes(p.plate.data(), p.plate.size());
	out["boundary"] = bytes(p.boundary.data(), p.boundary.size());
	out["volcanoes"] = bytes(p.volcanoes.data(), p.volcanoes.size());
	// what happened to the plates, oldest first
	static const char *const HAPPENED[] = {
		"two continents collide and fold up a range",
		"a continent splits and an ocean opens",
		"ocean floor sinks under a plate: a chain of volcanoes rises",
	};
	Json::Value history(Json::arrayValue);
	for (const terragen::Preview::Moment &m : p.history) {
		Json::Value e;
		e["ago"] = m.ago;
		e["text"] = HAPPENED[(int)m.kind];
		history.append(e);
	}
	out["history"] = history;
	Json::Value names(Json::arrayValue);
	for (const char *name : terragen::BIOME_NAMES)
		names.append(name);
	out["biomes"] = names;
	return out;
}

} // namespace shell
