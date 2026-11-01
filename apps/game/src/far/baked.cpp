// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/baked.h"
#include "exceptions.h"
#include "filesys.h"
#include "porting.h"
#include <fstream>
#include "mapnode.h"
#include "terragen/geology.h"
#include "terragen/world.h"
#include <unordered_map>

namespace far {

namespace {

[[noreturn]] void fail(const std::string &what)
{
	throw BaseException("far palette: " + what);
}

const Json::Value &field(const Json::Value &v, const char *name)
{
	if (!v.isObject() || !v.isMember(name))
		fail(std::string("no field '") + name + "'");
	return v[name];
}

std::string text(const Json::Value &v, const char *name)
{
	const Json::Value &f = field(v, name);
	if (!f.isString())
		fail(std::string("'") + name + "' isn't text");
	return f.asString();
}

double number(const Json::Value &v, const char *name)
{
	const Json::Value &f = field(v, name);
	if (!f.isNumeric())
		fail(std::string("'") + name + "' isn't a number");
	return f.asDouble();
}

bool flag(const Json::Value &v, const char *name)
{
	const Json::Value &f = field(v, name);
	if (!f.isBool())
		fail(std::string("'") + name + "' isn't true or false");
	return f.asBool();
}

Json::Value colour(const video::SColorf &c)
{
	Json::Value out(Json::arrayValue);
	out.append(c.r);
	out.append(c.g);
	out.append(c.b);
	return out;
}

video::SColorf colourOf(const Json::Value &v)
{
	if (!v.isArray() || v.size() != 3 || !v[0].isNumeric() || !v[1].isNumeric() || !v[2].isNumeric())
		fail("a colour is [r, g, b]");
	return video::SColorf(v[0].asFloat(), v[1].asFloat(), v[2].asFloat(), 1);
}

// The ids a read file's blocks get: by name, in the file's order; air and
// ignore the engine's own.
struct Ids
{
	std::unordered_map<std::string, content_t> by_name{ { "air", CONTENT_AIR }, { "ignore", CONTENT_IGNORE } };
	std::vector<std::string> names;

	content_t add(const std::string &name)
	{
		if (auto it = by_name.find(name); it != by_name.end())
			return it->second;
		content_t id = (content_t)names.size();
		while (id == CONTENT_AIR || id == CONTENT_IGNORE)
			id++;
		if (names.size() <= id)
			names.resize(id + 1);
		names[id] = name;
		by_name.emplace(name, id);
		return id;
	}
	content_t of(const std::string &name) const
	{
		auto it = by_name.find(name);
		if (it == by_name.end())
			fail("block '" + name + "' is used but has no look");
		return it->second;
	}
};

} // namespace

Json::Value Baked::write(const Content &content, const Palette &palette,
		const std::function<std::string(content_t)> &name)
{
	const terra::Blocks &b = content.blocks;
	Json::Value out(Json::objectValue);
	out["version"] = VERSION;

	Json::Value blocks(Json::objectValue);
	blocks["stone"] = name(b.stone);
	blocks["water"] = name(b.water);
	blocks["lava"] = name(b.lava);
	blocks["placer"] = name(b.placer);
	blocks["soil"] = name(b.soil);
	Json::Value biomes(Json::objectValue);
	for (int i = 0; i < (int)terragen::Biome::Count; i++) {
		const terra::BiomeBlocks &bb = b.biomes[i];
		Json::Value v(Json::objectValue);
		v["top"] = name(bb.top);
		v["filler"] = name(bb.filler);
		v["riverbed"] = name(bb.riverbed);
		v["water_top"] = name(bb.water_top);
		v["depth_top"] = bb.depth_top;
		v["depth_filler"] = bb.depth_filler;
		v["depth_riverbed"] = bb.depth_riverbed;
		v["depth_water_top"] = bb.depth_water_top;
		biomes[terragen::BIOME_NAMES[i]] = v;
	}
	blocks["biomes"] = biomes;
	Json::Value rock(Json::objectValue), ore(Json::objectValue);
	for (int i = 0; i < (int)terragen::Rock::Count; i++)
		rock[terragen::ROCK_NAMES[i]] = name(b.rock[i]);
	for (int i = 1; i < (int)terragen::Ore::Count; i++) // [None] is no block
		ore[terragen::ORE_NAMES[i]] = name(b.ore[i]);
	blocks["rock"] = rock;
	blocks["ore"] = ore;
	out["blocks"] = blocks;

	Json::Value species(Json::arrayValue);
	for (const terra::Species &s : content.species) {
		Json::Value v(Json::objectValue);
		v["name"] = s.name;
		const terra::Niche &n = s.niche;
		Json::Value niche(Json::objectValue);
		for (auto [key, range] : { std::pair{ "heat", n.heat }, std::pair{ "wet", n.wet }, std::pair{ "season", n.season } }) {
			Json::Value r(Json::arrayValue);
			r.append(range[0]);
			r.append(range[1]);
			niche[key] = r;
		}
		niche["seasonal"] = n.seasonal;
		niche["bank"] = n.bank;
		niche["shore"] = n.shore;
		v["niche"] = niche;
		Json::Value on(Json::arrayValue);
		for (content_t c : s.on)
			on.append(name(c));
		v["on"] = on;
		v["trunk"] = name(s.trunk);
		v["leaves"] = name(s.leaves);
		const terra::Shape &sh = s.shape;
		Json::Value shape(Json::objectValue);
		shape["axiom"] = sh.axiom;
		shape["rules_a"] = sh.rules_a;
		shape["rules_b"] = sh.rules_b;
		shape["rules_c"] = sh.rules_c;
		shape["rules_d"] = sh.rules_d;
		shape["trunk_type"] = sh.trunk_type;
		shape["angle"] = sh.angle;
		shape["iterations"] = sh.iterations;
		shape["random_level"] = sh.random_level;
		shape["thin_branches"] = sh.thin_branches;
		v["shape"] = shape;
		species.append(v);
	}
	out["species"] = species;

	Json::Value looks(Json::objectValue);
	for (const auto &[id, l] : palette.looks()) {
		Json::Value v(Json::objectValue);
		Json::Value face(Json::arrayValue), tinted(Json::arrayValue), tints(Json::arrayValue);
		for (int k = 0; k < 6; k++) {
			face.append(colour(l.face[k]));
			tinted.append(l.tinted[k]);
		}
		for (const video::SColorf &t : l.tints)
			tints.append(colour(t));
		v["face"] = face;
		v["tinted"] = tinted;
		v["tints"] = tints;
		v["over_black"] = colour(l.over_black);
		v["clarity"] = l.clarity;
		looks[name(id)] = v;
	}
	out["looks"] = looks;
	return out;
}

Baked Baked::read(const Json::Value &json)
{
	if (number(json, "version") != VERSION)
		fail("version " + std::to_string((int)number(json, "version")) + ", not " + std::to_string(VERSION));
	Baked out;
	Ids ids;

	// every block with a look gets an id first: the content refers to them
	const Json::Value &looks = field(json, "looks");
	if (!looks.isObject())
		fail("'looks' isn't an object");
	for (const std::string &name : looks.getMemberNames()) {
		const Json::Value &v = looks[name];
		Palette::Look l{};
		const Json::Value &face = field(v, "face"), &tinted = field(v, "tinted"), &tints = field(v, "tints");
		if (!face.isArray() || face.size() != 6 || !tinted.isArray() || tinted.size() != 6)
			fail("block '" + name + "' needs six faces and six tinted flags");
		for (int k = 0; k < 6; k++) {
			l.face[k] = colourOf(face[k]);
			if (!tinted[k].isBool())
				fail("block '" + name + "': tinted is true or false");
			l.tinted[k] = tinted[k].asBool();
		}
		if (!tints.isArray())
			fail("block '" + name + "': tints is a list");
		for (const Json::Value &t : tints)
			l.tints.push_back(colourOf(t));
		l.over_black = colourOf(field(v, "over_black"));
		l.clarity = (float)number(v, "clarity");
		out.palette.add(ids.add(name), std::move(l));
	}

	const Json::Value &blocks = field(json, "blocks");
	terra::Blocks &b = out.content.blocks;
	b.stone = ids.of(text(blocks, "stone"));
	b.water = ids.of(text(blocks, "water"));
	b.lava = ids.of(text(blocks, "lava"));
	b.placer = ids.of(text(blocks, "placer"));
	b.soil = ids.of(text(blocks, "soil"));
	const Json::Value &biomes = field(blocks, "biomes");
	for (int i = 0; i < (int)terragen::Biome::Count; i++) {
		const Json::Value &v = field(biomes, terragen::BIOME_NAMES[i]);
		terra::BiomeBlocks &bb = b.biomes[i];
		bb.top = ids.of(text(v, "top"));
		bb.filler = ids.of(text(v, "filler"));
		bb.riverbed = ids.of(text(v, "riverbed"));
		bb.water_top = ids.of(text(v, "water_top"));
		bb.depth_top = (s16)number(v, "depth_top");
		bb.depth_filler = (s16)number(v, "depth_filler");
		bb.depth_riverbed = (s16)number(v, "depth_riverbed");
		bb.depth_water_top = (s16)number(v, "depth_water_top");
	}
	const Json::Value &rock = field(blocks, "rock"), &ore = field(blocks, "ore");
	for (int i = 0; i < (int)terragen::Rock::Count; i++)
		b.rock[i] = ids.of(text(rock, terragen::ROCK_NAMES[i]));
	b.ore[0] = CONTENT_IGNORE;
	for (int i = 1; i < (int)terragen::Ore::Count; i++)
		b.ore[i] = ids.of(text(ore, terragen::ORE_NAMES[i]));

	const Json::Value &species = field(json, "species");
	if (!species.isArray())
		fail("'species' is a list");
	for (const Json::Value &v : species) {
		terra::Species s;
		s.name = text(v, "name");
		const Json::Value &niche = field(v, "niche");
		for (auto [key, range] : { std::pair{ "heat", s.niche.heat }, std::pair{ "wet", s.niche.wet },
					 std::pair{ "season", s.niche.season } }) {
			const Json::Value &r = field(niche, key);
			if (!r.isArray() || r.size() != 2 || !r[0].isNumeric() || !r[1].isNumeric())
				fail("species '" + s.name + "': " + key + " is [low, high]");
			range[0] = r[0].asDouble();
			range[1] = r[1].asDouble();
		}
		s.niche.seasonal = flag(niche, "seasonal");
		s.niche.bank = flag(niche, "bank");
		s.niche.shore = flag(niche, "shore");
		const Json::Value &on = field(v, "on");
		if (!on.isArray())
			fail("species '" + s.name + "': on is a list");
		for (const Json::Value &g : on) {
			if (!g.isString())
				fail("species '" + s.name + "': on is block names");
			s.on.push_back(ids.of(g.asString()));
		}
		s.trunk = ids.of(text(v, "trunk"));
		s.leaves = ids.of(text(v, "leaves"));
		const Json::Value &shape = field(v, "shape");
		terra::Shape &sh = s.shape;
		sh.axiom = text(shape, "axiom");
		sh.rules_a = text(shape, "rules_a");
		sh.rules_b = text(shape, "rules_b");
		sh.rules_c = text(shape, "rules_c");
		sh.rules_d = text(shape, "rules_d");
		sh.trunk_type = text(shape, "trunk_type");
		sh.angle = (s32)number(shape, "angle");
		sh.iterations = (s32)number(shape, "iterations");
		sh.random_level = (s32)number(shape, "random_level");
		sh.thin_branches = flag(shape, "thin_branches");
		out.content.species.push_back(std::move(s));
	}
	out.names = std::move(ids.names);
	return out;
}

Baked Baked::load(const std::string &game)
{
	const std::string path = porting::path_share + DIR_DELIM "games" DIR_DELIM + game + DIR_DELIM "palette.json";
	std::ifstream in(path);
	if (!in)
		fail("no baked palette at " + path + " (bun palette, bun stage)");
	Json::Value json;
	Json::CharReaderBuilder reader;
	std::string errors;
	if (!Json::parseFromStream(reader, in, &json, &errors))
		fail(path + " isn't JSON: " + errors);
	return read(json);
}

} // namespace far
