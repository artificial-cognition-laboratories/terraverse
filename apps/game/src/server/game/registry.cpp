// Terra Firma: what the world's made of, registered natively (sprint 119). See registry.h.
// Was builtin/game/world.lua, matter.lua, planet.lua and sidecar.lua.
#include "src/server/game/registry.h"

#include "src/shared/base/config.h"
#if CHECK_CLIENT_BUILD()
#include "src/client/render/map/mesh/node_visuals.h" // ~NodeVisuals, as ContentFeatures moves
#endif

#include "src/server/game/defs.h"
#include "src/server/world/load/emerge.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/log.h"
#include "src/server/world/generate/luanti/mg_biome.h"
#include "src/server/world/run/rules/matter.h"
#include "library/matter/matter/matter.h"
#include "src/shared/platform/system/porting.h"
#include "src/server/app/server.h"
#include "src/server/app/mods/sidecar/sidecar.h"
#include "src/server/world/terra.h"
#include "library/world/terragen/src/base/fixed.h"
#include "library/world/terragen/src/world.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace {

double toDouble(terragen::fix v)
{
	return (double)v / terragen::ONE;
}

Json::Value read(const std::string &file)
{
	Json::Value root;
	Json::CharReaderBuilder reader;
	std::string errors;
	std::istringstream in(worldData(file));
	if (!Json::parseFromStream(reader, in, &root, &errors))
		throw BaseException("world: " + file + " isn't JSON: " + errors);
	return root;
}

void alias(Server *server, const std::string &from, const std::string &to)
{
	server->getWritableItemDefManager()->registerAlias(from, to);
}

Json::Value list(std::initializer_list<Json::Value> items)
{
	Json::Value out(Json::arrayValue);
	for (const Json::Value &v : items)
		out.append(v);
	return out;
}

// The world's own blocks, each as its definition (blocks.json). In order by name: the
// order doesn't matter, and a fixed one keeps content ids stable.
void worldBlocks(Server *server)
{
	const Json::Value blocks = read("blocks.json")["blocks"];
	if (!blocks.isObject())
		throw BaseException("world: blocks.json is { \"blocks\": { name: definition } }");
	for (const std::string &name : blocks.getMemberNames()) // (sorted)
		addBlock(server, name, blocks[name]);
}

// Soils (sprint 111): a block for each parent rock and stoniness terragen names, each
// point of its fine earth (its param2) a composition: the rock's stones, the fine earth's
// grains and clay, organic matter as wood (it burns, and peat burns best).
void soils(Server *server)
{
	const Json::Value soils = read("soils.json");
	for (int parent = 0; parent < (int)terragen::SoilParent::Count; parent++) {
		const std::string parentName = terragen::SOIL_PARENT_NAMES[parent];
		for (int stony = 0; stony < terragen::SOIL_STONY; stony++) {
			const std::string stonyName = terragen::SOIL_STONY_NAMES[stony];
			const Json::Value &p = soils["parents"][parentName], &s = soils["stony"][stonyName];
			if (!p.isObject() || !s.isObject())
				throw BaseException("world: soils.json has no soil for " + parentName + " " + stonyName);
			const double stones = toDouble(terragen::soilStonesOf(stony));
			Json::Value points(Json::arrayValue), holds(Json::arrayValue);
			for (int32_t i = 0; i < terragen::SOIL_POINTS; i++) {
				const terragen::Soil pt = terragen::soilPoint((uint8_t)i);
				const double sand = toDouble(pt.sand), silt = toDouble(pt.silt),
						clay = toDouble(pt.clay), organic = toDouble(pt.organic);
				// what its pores hold where the ground's wet: its fine earth's water at field
				// capacity (Saxton and Rawls, simplified), clay and organic matter holding most,
				// sand least, stones none, of the 45% of it that's pores
				const double fc = std::min(0.45, 0.08 + 0.40 * clay + 0.20 * silt + 0.5 * organic);
				holds.append(std::min(1.0, fc * (1 - stones) / 0.45));
				const double mineral = 1 - organic;
				const double rock = mineral * stones, fine = mineral * (1 - stones);
				Json::Value parts(Json::objectValue);
				const auto add = [&](const std::string &id, double share) {
					if (share > 0)
						parts[id] = parts.get(id, 0.0).asDouble() + share;
				};
				add(p["rock"].asString(), rock);
				for (const std::string &id : p["grains"].getMemberNames())
					add(id, fine * (sand + silt) * p["grains"][id].asDouble());
				for (const std::string &id : p["clay"].getMemberNames())
					add(id, fine * clay * p["clay"][id].asDouble());
				add("wood", organic);
				points.append(parts);
			}
			Json::Value groups = soils["groups"];
			for (const std::string &g : s["groups"].getMemberNames())
				groups[g] = s["groups"][g];
			std::string description = s.get("description", "").asString();
			if (description.empty()) {
				description = s["prefix"].asString() + "soil";
				description[0] = (char)std::toupper((unsigned char)description[0]);
			}
			const std::string id = "soil_" + parentName + (stonyName.empty() ? "" : "_" + stonyName);
			Json::Value def;
			def["description"] = description;
			def["tiles"] = list({ "tf_world_soil.png^[multiply:" + p["tint"].asString() });
			def["paramtype2"] = "color";
			def["palette"] = "tf_world_soil_palette.png";
			def["groups"] = groups;
			// grains with air (and water) between them
			def["_family"]["share"] = soils["share"];
			def["_family"]["points"] = points;
			def["_family"]["holds"] = holds;
			// its stones: the rock itself, in pebbles (as they are, not the palette's colour)
			if (s.isMember("pebbles")) {
				Json::Value overlay;
				overlay["name"] = "tf_goodvibes_" + p["rock"].asString() + ".png^[mask:tf_world_pebbles_" +
						std::to_string(s["pebbles"].asInt()) + ".png";
				overlay["color"] = "white";
				def["overlay_tiles"] = list({ overlay });
			}
			addBlock(server, "core:" + id, def);
			alias(server, "mapgen_" + id, "core:" + id);
			alias(server, "tf_world:" + id, "core:" + id); // worlds from before
		}
	}
	// the column's soil, as biomes name it: the generator puts the kind it is in its place
	alias(server, "core:soil", "core:soil_granitic");
	alias(server, "tf_world:soil", "core:soil_granitic");
	alias(server, "mapgen_soil", "core:soil_granitic");
}

std::string weight(double kg)
{
	char s[32];
	if (kg >= 1000)
		std::snprintf(s, sizeof s, "%.2f t", kg / 1000);
	else
		std::snprintf(s, sizeof s, "%d kg", (int)std::floor(kg + 0.5));
	return s;
}

std::string lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	return s;
}

// The matter's own blocks (sprint 119: were tf_matter's). The world's matter itself is
// native (server/matter.cpp): it registers every block that says what it's made of.
void matterBlocks(Server *server)
{
	// The flame: the light of air that's burning. Never placed: the engine sets and clears
	// it where the world's matter burns (never the form air takes).
	{
		Json::Value def;
		def["description"] = "Flame";
		def["drawtype"] = "airlike";
		def["paramtype"] = "light";
		def["light_source"] = 12;
		def["sunlight_propagates"] = true;
		def["walkable"] = false;
		def["pointable"] = false;
		def["diggable"] = false;
		def["buildable_to"] = true;
		def["floodable"] = true;
		def["drop"] = "";
		def["groups"]["not_in_creative_inventory"] = 1;
		def["_matter"]["matter"] = "air";
		addBlock(server, "core:flame", def);
		alias(server, "tf_matter:flame", "core:flame");
	}
	// A vessel (sprint 79): a crucible holds what's put in it, and its matter does what it
	// would. The block is air that keeps what's put into it, so the world's physics runs it
	// like any other. The wiki's chemistry scenes are crucibles; in the world, nothing
	// fills one yet.
	{
		Json::Value def;
		def["description"] = "Crucible";
		def["drawtype"] = "nodebox";
		def["paramtype"] = "light";
		def["tiles"] = list({ "matter_substance.png^[multiply:#b0703c" });
		def["node_box"]["type"] = "fixed";
		def["node_box"]["fixed"] = list({
			list({ -0.4, -0.5, -0.4, 0.4, -0.4, 0.4 }),  // floor
			list({ -0.4, -0.4, -0.4, 0.4, 0.3, -0.32 }), // walls
			list({ -0.4, -0.4, 0.32, 0.4, 0.3, 0.4 }),
			list({ -0.4, -0.4, -0.32, -0.32, 0.3, 0.32 }),
			list({ 0.32, -0.4, -0.32, 0.4, 0.3, 0.32 }),
		});
		def["groups"]["cracky"] = 3;
		def["_matter"]["matter"] = "air";
		def["_matter"]["vessel"] = true;
		addBlock(server, "core:crucible", def);
		alias(server, "tf_matter:crucible", "core:crucible");
	}

	// A block of every substance the engine knows, so anything can be held, placed, heated
	// and mixed: its solid, its powder, and its liquid where it has one at 1 atm. Each is
	// the form its matter takes in that state, after the world's own blocks (lava still
	// cools into basalt). The catalogue shows each as it is at room temperature; the rest
	// exist for the physics.
	const matter::Matter &table = serverMatter(server).table;
	const auto groups = [](bool shown) {
		Json::Value g;
		g["cracky"] = 3;
		g["creative_matter"] = 1;
		if (!shown)
			g["not_in_creative_inventory"] = 1;
		return g;
	};
	// a levelled block: param2 says how full it is, in sixty-fourths (sprint 80)
	Json::Value levelled;
	levelled["type"] = "leveled";
	levelled["fixed"] = list({ -0.5, -0.5, -0.5, 0.5, 0.5, 0.5 });
	for (const matter::Substance &s : table.substances()) {
		char tint[8];
		std::snprintf(tint, sizeof tint, "#%06x", s.colour);
		const matter::State room = s.state(293.15, matter::ATMOSPHERE);
		const double density = s.density(room, 293.15, matter::ATMOSPHERE);
		const double melts = s.meltingPoint(std::max(matter::ATMOSPHERE, s.triple.p));
		const std::optional<double> boils = s.boilingPoint(matter::ATMOSPHERE);
		const std::string base = "core:" + s.id;
		const std::string look = std::string("matter_substance.png^[multiply:") + tint;

		// the solid: every substance has one (frozen nitrogen included)
		Json::Value solid;
		solid["description"] = s.name + " (" + s.formula + ")\n1 m³ · " + weight(density);
		solid["tiles"] = list({ look });
		solid["groups"] = groups(room == matter::State::Solid);
		solid["_matter"]["matter"] = s.id;
		solid["_matter"]["form"] = true;
		solid["_matter_generated"] = true;
		addBlock(server, base, solid);

		// its powder: poured, it heaps and slides (sprint 80); levelled by how much is there
		Json::Value powder;
		powder["description"] = s.name + " powder";
		powder["drawtype"] = "nodebox";
		powder["node_box"] = levelled;
		powder["paramtype"] = "light";
		powder["paramtype2"] = "leveled";
		powder["leveled"] = 64;
		powder["tiles"] = list({ look });
		powder["groups"] = groups(false);
		powder["drop"] = "";
		powder["_matter"]["matter"] = s.id;
		powder["_matter"]["form"] = true;
		powder["_matter"]["loose"] = true;
		powder["_matter_generated"] = true;
		addBlock(server, base + "_powder", powder);

		// the liquid, where it has one at 1 atm: a full block, and a puddle levelled by how
		// much is there; the world's matter runs both (sprint 80)
		std::vector<std::string> forms = { "", "_powder" };
		if ((!boils || *boils > melts) && melts < 3500) {
			const bool hot = melts > 700;
			Json::Value liquid;
			liquid["description"] = "Liquid " + lower(s.name) + " (" + s.formula + ")\n1 m³ · " + weight(density);
			liquid["drawtype"] = "liquid";
			liquid["tiles"] = list({ std::string("matter_liquid.png^[multiply:") + tint });
			liquid["use_texture_alpha"] = "blend";
			liquid["paramtype"] = "light";
			liquid["light_source"] = hot ? 10 : 0;
			liquid["walkable"] = false;
			liquid["pointable"] = true;
			liquid["buildable_to"] = false;
			liquid["liquid_move_physics"] = true;
			liquid["drowning"] = 1;
			liquid["damage_per_second"] = hot ? 6 : 0;
			liquid["post_effect_color"]["a"] = 160;
			liquid["post_effect_color"]["r"] = 120;
			liquid["post_effect_color"]["g"] = 120;
			liquid["post_effect_color"]["b"] = 130;
			liquid["groups"] = groups(room == matter::State::Liquid);
			liquid["_matter"]["matter"] = s.id;
			liquid["_matter"]["state"] = "liquid";
			liquid["_matter"]["form"] = true;
			liquid["_matter_generated"] = true;
			Json::Value puddle = liquid;
			addBlock(server, base + "_liquid", liquid);
			forms.push_back("_liquid");

			puddle["description"] = "Liquid " + lower(s.name);
			puddle["drawtype"] = "nodebox";
			puddle["node_box"] = levelled;
			puddle["paramtype2"] = "leveled";
			puddle["leveled"] = 64;
			puddle["groups"] = groups(false);
			puddle["groups"]["matter_puddle"] = 1; // sprint 81: liquid is a fill now; these become one on load
			puddle["drop"] = "";
			puddle["liquid_move_physics"] = false; // waded through, not swum
			puddle["drowning"] = 0;
			puddle.removeMember("post_effect_color");
			// the world's own wins where it has one (its shallow water is drawn as water)
			if (!server->getItemDefManager()->isKnown(base + "_puddle"))
				addBlock(server, base + "_puddle", puddle);
			forms.push_back("_puddle");
		}
		// worlds saved while these were tf_matter's
		for (const std::string &form : forms)
			alias(server, "tf_matter:" + s.id + form, base + form);
	}
}

// The planet's own blocks (sprint 119: were tf_planet's).
void planetBlocks(Server *server)
{
	// The seam: every node that is no cell, between the planet's regions. A wall nothing
	// passes, never drawn; the generator places it (mapgen_seam), cell by cell.
	Json::Value seam;
	seam["description"] = "Seam (between the planet's regions)";
	seam["drawtype"] = "airlike";
	seam["paramtype"] = "light";
	seam["sunlight_propagates"] = true; // no shadow along the joins
	seam["walkable"] = true;
	seam["pointable"] = false;
	seam["diggable"] = false;
	seam["buildable_to"] = false;
	seam["is_ground_content"] = false;
	seam["drop"] = "";
	seam["groups"]["not_in_creative_inventory"] = 1;
	addBlock(server, "core:seam", seam);
	alias(server, "mapgen_seam", "core:seam");
	alias(server, "tf_planet:seam", "core:seam"); // worlds saved before it was the engine's

	// TEMP(2026-10-27): the debug sphere's paints (planet_atlas_debug, sprint 30): a cell
	// coloured by what it is, its edges lined so the grid shows.
	const std::pair<const char *, const char *> paints[] = {
		{ "segment_a", "#8a9a8f" }, { "segment_b", "#a7b5aa" }, { "cap", "#5f86c4" },
		{ "core", "#7a5aa6" }, { "segment_seam", "#e0b050" }, { "cap_edge", "#f08a3c" },
		{ "cut", "#e8463a" }, { "core_edge", "#d36ca0" }, { "line", "#3b4a45" },
	};
	for (const auto &[name, colour] : paints) {
		Json::Value def;
		def["description"] = std::string("Atlas debug: ") + name;
		def["tiles"] = list({ std::string("[fill:16x16:") + colour + "^[fill:16x1:0,0:#00000050^[fill:1x16:0,0:#00000050" });
		def["groups"]["cracky"] = 3;
		def["groups"]["not_in_creative_inventory"] = 1;
		def["is_ground_content"] = false;
		addBlock(server, std::string("core:atlas_debug_") + name, def);
		alias(server, std::string("tf_planet:debug_") + name, std::string("core:atlas_debug_") + name);
	}
}

// What the generator places: its biomes (what terragen's land is made of where) and tree
// species. Their blocks are resolved by name once every block's registered.
void generated(Server *server)
{
	EmergeManager *emerge = server->getEmergeManager();
	if (!emerge)
		return; // nothing's generated (the unit tests' mock server)
	BiomeManager *biomes = emerge->getWritableBiomeManager();
	const Json::Value biomeDefs = read("biomes.json")["biomes"];
	for (const Json::Value &d : biomeDefs) {
		auto b = std::unique_ptr<Biome>(static_cast<Biome *>(BiomeManager::create(BIOMETYPE_NORMAL)));
		const auto need = [&](const char *field) -> const Json::Value & {
			if (!d.isMember(field))
				throw BaseException("world: biomes.json: a biome has no " + std::string(field));
			return d[field];
		};
		b->name = need("name").asString();
		b->depth_top = need("depth_top").asInt();
		b->depth_filler = need("depth_filler").asInt();
		b->depth_riverbed = need("depth_riverbed").asInt();
		b->depth_water_top = d.get("depth_water_top", 0).asInt();
		b->heat_point = need("heat_point").asFloat();
		b->humidity_point = need("humidity_point").asFloat();
		b->min_pos.Y = need("y_min").asInt();
		b->max_pos.Y = need("y_max").asInt();
		std::vector<std::string> &nn = b->m_nodenames;
		nn = { need("node_top").asString(), need("node_filler").asString(), "",
			d.get("node_water_top", "").asString(), "", "", need("node_riverbed").asString(), "" };
		nn.push_back("ignore"); // no cave liquids
		b->m_nnlistsizes.push_back(1);
		nn.insert(nn.end(), { "", "", "" }); // no dungeons
		server->getNodeDefManager()->pendNodeResolve(b.get());
		if (biomes->add(b.get()) == OBJDEF_INVALID_HANDLE)
			throw BaseException("world: biomes.json: " + b->name + " couldn't be added");
		b.release(); // the manager's now
	}

	ServerTerra &t = server->terra();
	const Json::Value speciesDefs = read("species.json")["species"];
	for (const Json::Value &d : speciesDefs) {
		terra::SpeciesDef s{};
		s.name = d["name"].asString();
		const auto fail = [&](const std::string &what) {
			throw BaseException("world: species.json: " + s.name + ": " + what);
		};
		const auto range = [&](const char *field, double out[2]) {
			const Json::Value &r = d[field];
			if (!r.isArray() || r.size() != 2 || !r[0].isNumeric() || !r[1].isNumeric())
				fail(std::string(field) + " is [low, high]");
			out[0] = r[0].asDouble();
			out[1] = r[1].asDouble();
		};
		if (s.name.empty())
			fail("a species has a name");
		range("heat", s.niche.heat);
		range("wet", s.niche.wet);
		s.niche.seasonal = d.isMember("season");
		if (s.niche.seasonal)
			range("season", s.niche.season);
		s.niche.bank = d.get("bank", false).asBool();
		s.niche.shore = d.get("shore", false).asBool();
		s.trunk = d["trunk"].asString();
		s.leaves = d["leaves"].asString();
		if (s.trunk.empty() || s.leaves.empty())
			fail("trunk and leaves are blocks");
		for (const std::string &ground : d["on"].getMemberNames())
			if (d["on"][ground].asBool())
				s.on.push_back(ground);
		const Json::Value &shape = d["shape"];
		if (!shape.isObject() || shape["axiom"].asString().empty())
			fail("shape is an L-system with an axiom");
		s.shape.axiom = shape["axiom"].asString();
		s.shape.rules_a = shape.get("rules_a", "").asString();
		s.shape.rules_b = shape.get("rules_b", "").asString();
		s.shape.rules_c = shape.get("rules_c", "").asString();
		s.shape.rules_d = shape.get("rules_d", "").asString();
		s.shape.trunk_type = shape.get("trunk_type", "single").asString();
		s.shape.angle = shape.get("angle", 0).asInt();
		s.shape.iterations = shape.get("iterations", 0).asInt();
		s.shape.random_level = shape.get("random_level", 0).asInt();
		s.shape.thin_branches = shape.get("thin_branches", false).asBool();
		for (const terra::SpeciesDef &other : t.species)
			if (other.name == s.name)
				fail("is listed twice");
		t.species.push_back(std::move(s));
	}
}

} // namespace

void addBlock(Server *server, const std::string &name, const Json::Value &def)
{
	defs::Block b;
	try {
		b = defs::block(name, def);
	} catch (const std::invalid_argument &e) {
		throw BaseException(std::string("blocks: ") + e.what());
	}
	if (server->getItemDefManager()->isKnown(name))
		throw BaseException("blocks: " + name + " is registered twice");
	server->getWritableItemDefManager()->registerItem(b.item);
	const content_t id = server->getWritableNodeDefManager()->set(name, std::move(b.node));
	if (id == CONTENT_IGNORE)
		throw BaseException("blocks: there's no room for " + name + " (about 32 000 blocks at most)");
}

std::string worldData(const std::string &file)
{
	const std::string path = porting::path_share + DIR_DELIM "world" DIR_DELIM + file;
	std::string text;
	if (!fs::ReadFile(path, text))
		throw BaseException("world: " + path + " is missing");
	return text;
}

void registerContent(Server *server)
{
	// the hand: what digs when nothing's held
	try {
		server->getWritableItemDefManager()->registerItem(defs::item("", read("hand.json")["hand"]));
	} catch (const std::invalid_argument &e) {
		throw BaseException(std::string("world: hand.json: ") + e.what());
	}
	// cutting: its tools, and every member they cut, as items (sprint 123)
	try {
		server->terra().cuts = cut::Cuts::boot(server, read("cuts.json"));
	} catch (const std::invalid_argument &e) {
		throw BaseException(std::string("world: cuts.json: ") + e.what());
	}
	// digging by hand: the shovel, and the scoops it moves (sprint 124)
	try {
		server->terra().digs = dig::Digs::boot(server, read("digs.json"));
	} catch (const std::invalid_argument &e) {
		throw BaseException(std::string("world: digs.json: ") + e.what());
	}
	worldBlocks(server);
	soils(server);
	matterBlocks(server);
	planetBlocks(server);
	const Json::Value aliases = read("aliases.json")["aliases"];
	for (const std::string &from : aliases.getMemberNames())
		alias(server, from, aliases[from].asString());
	generated(server);
}
