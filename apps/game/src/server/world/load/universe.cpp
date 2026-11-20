#include "src/server/world/load/universe.h"

#include "src/shared/content/subgames.h"
#include "src/shared/core/exceptions.h"
#include "src/shared/platform/filesys.h"
#include "src/shared/platform/settings.h"
#include "library/world/terragen/atlas.h"
#include "library/world/terragen/code.h"
#include <algorithm>
#include <cmath>

namespace universe {

// The highest sky a new world gets (layers): what the map holds above the sea with room
// to spare (planet::Atlas: 30,912 up).
static constexpr int SKY_MOST = 28000;

void make(const std::string &path, const std::string &name, const SubgameSpec &game, const std::string &code)
{
	const terragen::WorldCode world_code = terragen::WorldCode::decode(code);
	loadGameConfAndInitWorld(path, name, game, true);
	// The map generator's settings: terragen, this code, the engine's own seed (ores,
	// decorations) tied to the code's seed, and the planet the world is: an atlas
	// (planet.paper.md) as many blocks round as the code's world (radius size / 2π), so
	// the world is laid on it block for block.
	Settings meta("[end_of_params]");
	meta.set("mg_name", "terra");
	meta.set("mgterra_code", code);
	// Its sky reaches a radius up (sprint 59): the air there is ten scale heights thin
	// (terragen::sphere's), clear for an orbit, so you can fly to one and build up to
	// it; as high as the map holds on the largest worlds.
	terragen::Atlas::Params atlas = terragen::Atlas::standard(world_code.size / (2 * M_PI));
	atlas.height = (int)std::min(atlas.radius, (double)SKY_MOST);
	meta.set("planet_atlas", terragen::Atlas(atlas).code());
	// chunks of 2³ map blocks: an 80-node chunk would be mostly seam and other tiles'
	// storage, generated for nothing; a single block is made inside a border 27 blocks'
	// worth and reaches for its neighbours' trees over 25 times its own columns (2 made
	// a place 12% sooner than 1)
	meta.set("chunksize", "2");
	if (g_settings->getBool("planet_atlas_debug"))
		meta.setBool("planet_atlas_debug", true);
	meta.set("seed", std::to_string(world_code.seed_hash));
	if (!meta.updateConfigFile((path + DIR_DELIM "map_meta.txt").c_str()))
		throw BaseException("universe: could not write the map settings for " + path);
}

std::string codeOf(const std::string &path)
{
	Settings meta("[end_of_params]");
	if (!meta.readConfigFile((path + DIR_DELIM "map_meta.txt").c_str()))
		return "";
	return meta.exists("mgterra_code") ? meta.get("mgterra_code") : "";
}

} // namespace universe
