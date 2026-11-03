// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/terrain.h"
#include "mapgen/terra_species.h"
#include "matter/matter.h"
#include "terragen/system.h"
#include "terragen/world.h"
#include "weather/weather.h"
#include <memory>
#include <optional>
#include <vector>

/*
	What core.terra keeps for one server: the species the game registers, and
	what is built from the world on first use. The Server owns it, so a second
	world played in the same process starts from nothing, not from the first
	world's leftovers.

	Species are registered while mods load; the first use of them (resolved)
	closes registration, so nothing built from them can go stale.
*/
struct ServerTerra
{
	std::vector<terra::SpeciesDef> species; // registration order
	std::optional<std::vector<terra::Species>> resolved; // against the node definitions
	std::vector<std::unique_ptr<terragen::World>> worlds; // by body
	std::unique_ptr<matter::Matter> matter;
	std::unique_ptr<weather::Weather> weather; // after world and matter: it refers to both
	std::vector<std::unique_ptr<far::Generator>> surfaces; // by body
	std::unique_ptr<terragen::System> system; // the world's, for its bodies' clocks

	// The species against the node definitions. The first call closes registration.
	const std::vector<terra::Species> &resolve(const NodeDefManager *ndef)
	{
		if (!resolved)
			resolved = terra::resolve(species, ndef);
		return *resolved;
	}
};
