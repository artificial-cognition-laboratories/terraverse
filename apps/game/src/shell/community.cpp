// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "community.h"
#include "bridge.h"
#include "worlds.h"
#include "content/content.h"
#include "content/subgames.h"
#include "exceptions.h"
#include "filesys.h"
#include "porting.h"
#include "settings.h"
#include "util/string.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace shell {

static std::vector<std::string> ordered(const std::unordered_set<std::string> &values)
{
	std::vector<std::string> out(values.begin(), values.end());
	std::sort(out.begin(), out.end());
	return out;
}

static Json::Value jsonStrings(const std::vector<std::string> &values)
{
	Json::Value out(Json::arrayValue);
	for (const auto &value : values)
		out.append(value);
	return out;
}

static std::string withoutSlash(std::string path)
{
	while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
		path.pop_back();
	return path;
}

static std::vector<ContentSpec> textures()
{
	std::vector<ContentSpec> out;
	std::set<std::string> seen;
	for (const auto &root : { porting::path_user + DIR_DELIM "textures",
			porting::path_share + DIR_DELIM "textures" }) {
		for (const auto &entry : fs::GetDirListing(root)) {
			if (!entry.dir || entry.name.empty() || entry.name[0] == '.' || entry.name == "base")
				continue;
			ContentSpec pack;
			pack.path = fs::AbsolutePath(root + DIR_DELIM + entry.name);
			if (!seen.insert(pack.path).second)
				continue;
			pack.name = entry.name;
			pack.title = entry.name;
			parseContentInfo(pack);
			if (pack.title.empty())
				pack.title = entry.name;
			out.push_back(std::move(pack));
		}
	}
	std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
		return a.title < b.title;
	});
	return out;
}

static std::string modTitle(const ModSpec &mod, bool builtin)
{
	ContentSpec content;
	content.path = mod.path;
	parseContentInfo(content);
	if (!content.title.empty())
		return content.title;
	std::string title = builtin && mod.name.rfind("tf_", 0) == 0 ? mod.name.substr(3) : mod.name;
	bool start = true;
	for (char &c : title) {
		if (c == '_') {
			c = ' ';
			start = true;
		} else if (start) {
			c = (char)std::toupper((unsigned char)c);
			start = false;
		}
	}
	return title;
}

std::vector<ModSpec> Community::builtins() const
{
	SubgameSpec game = findSubgame(m_gameid);
	if (!game.isValid())
		throw BaseException("shell: game '" + m_gameid + "' is not installed");
	return flattenMods(getModsInPath(game.gamemods_path, "games/" + m_gameid + "/mods"), true);
}

std::vector<ModSpec> Community::addons() const
{
	SubgameSpec game = findSubgame(m_gameid);
	if (!game.isValid())
		throw BaseException("shell: game '" + m_gameid + "' is not installed");
	std::vector<ModSpec> out;
	for (const auto &source : game.addon_mods_paths) {
		auto found = flattenMods(getModsInPath(source.second, source.first), true);
		out.insert(out.end(), found.begin(), found.end());
	}
	std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
		return a.name == b.name ? a.virtual_path < b.virtual_path : a.name < b.name;
	});
	return out;
}

static void readWorldSettings(const std::string &path, Settings &settings)
{
	if (!settings.readConfigFile((path + DIR_DELIM "world.mt").c_str()))
		throw BaseException("shell: could not read world.mt for " + path);
}

static bool active(const ModSpec &mod, const Settings &settings)
{
	const std::string key = "load_mod_" + mod.name;
	if (!settings.exists(key))
		return false;
	const std::string value = settings.get(key);
	return value == mod.virtual_path || is_yes(value);
}

Json::Value Community::list() const
{
	auto game_mods = builtins();
	auto user_mods = addons();
	auto saved_worlds = m_worlds.list();
	std::set<std::string> builtin_names;
	std::map<std::string, int> addon_counts;
	for (const auto &mod : game_mods)
		builtin_names.insert(mod.name);
	for (const auto &mod : user_mods)
		++addon_counts[mod.name];
	std::map<std::string, std::vector<std::string>> enabled_worlds;
	for (const auto &world : saved_worlds) {
		Settings settings;
		readWorldSettings(world.path, settings);
		for (const auto &mod : user_mods) {
			if (active(mod, settings))
				enabled_worlds[mod.virtual_path].push_back(world.path);
		}
	}

	Json::Value out;
	out["mods"] = Json::Value(Json::arrayValue);
	out["textures"] = Json::Value(Json::arrayValue);
	out["worlds"] = Json::Value(Json::arrayValue);
	for (const auto &mod : game_mods) {
		Json::Value row;
		row["id"] = mod.virtual_path;
		row["name"] = mod.name;
		row["title"] = modTitle(mod, true);
		row["description"] = mod.desc;
		row["author"] = mod.author;
		row["source"] = "builtIn";
		row["depends"] = jsonStrings(ordered(mod.depends));
		row["enabledWorlds"] = Json::Value(Json::arrayValue);
		row["configurable"] = false;
		out["mods"].append(row);
	}
	for (const auto &mod : user_mods) {
		Json::Value row;
		row["id"] = mod.virtual_path;
		row["name"] = mod.name;
		row["title"] = modTitle(mod, false);
		row["description"] = mod.desc;
		row["author"] = mod.author;
		row["source"] = "installed";
		row["depends"] = jsonStrings(ordered(mod.depends));
		row["enabledWorlds"] = jsonStrings(enabled_worlds[mod.virtual_path]);
		row["configurable"] = builtin_names.count(mod.name) == 0 && addon_counts[mod.name] == 1;
		if (builtin_names.count(mod.name))
			row["issue"] = "This mod has the same name as a Terraverse built-in.";
		else if (addon_counts[mod.name] > 1)
			row["issue"] = "Multiple installed copies share this mod name.";
		out["mods"].append(row);
	}

	std::set<std::string> active_textures;
	for (const auto &path : str_split(g_settings->get("texture_path"), PATH_DELIM[0]))
		active_textures.insert(withoutSlash(path));
	for (const auto &pack : textures()) {
		Json::Value row;
		row["id"] = pack.path;
		row["name"] = pack.title;
		row["description"] = pack.desc;
		row["author"] = pack.author;
		row["active"] = active_textures.count(withoutSlash(pack.path)) != 0;
		out["textures"].append(row);
	}
	for (const auto &world : saved_worlds) {
		Json::Value row;
		row["name"] = world.name;
		row["path"] = world.path;
		Settings meta("[end_of_params]");
		const std::string meta_path = world.path + DIR_DELIM "map_meta.txt";
		if (fs::PathExists(meta_path) && !meta.readConfigFile(meta_path.c_str()))
			throw BaseException("shell: could not read " + meta_path);
		row["code"] = meta.exists("mgterra_code") ? Json::Value(meta.get("mgterra_code")) : Json::Value();
		out["worlds"].append(row);
	}
	return out;
}

void Community::setMod(const std::string &id, const std::string &world_path, bool enabled) const
{
	WorldSpec world = m_worlds.find(world_path); // never trust a path from the page
	auto game_mods = builtins();
	auto user_mods = addons();
	auto world_mods = flattenMods(getModsInPath(world.path + DIR_DELIM "worldmods", "worldmods"), true);
	std::set<std::string> builtin_names;
	for (const auto &mod : game_mods)
		builtin_names.insert(mod.name);
	for (const auto &mod : world_mods)
		builtin_names.insert(mod.name); // worldmods always load for this world
	const auto found = std::find_if(user_mods.begin(), user_mods.end(),
			[&](const ModSpec &mod) { return mod.virtual_path == id; });
	if (found == user_mods.end())
		throw Refused("That installed mod is no longer available.");
	if (builtin_names.count(found->name))
		throw Refused("An installed mod cannot replace a built-in or a world mod with the same name.");
	if (std::count_if(user_mods.begin(), user_mods.end(), [&](const ModSpec &mod) {
			return mod.name == found->name;
		}) > 1)
		throw Refused("Multiple installed copies share the name " + found->name + ". Resolve them before enabling either.");

	Settings settings;
	readWorldSettings(world.path, settings);
	if (enabled) {
		const std::string key = "load_mod_" + found->name;
		if (settings.exists(key) && settings.get(key) != "false" &&
				settings.get(key) != found->virtual_path && !is_yes(settings.get(key)))
			throw Refused("Another copy of " + found->name + " is enabled in this world. Disable it first.");
		for (const auto &dep : found->depends) {
			if (builtin_names.count(dep))
				continue;
			auto installed = std::find_if(user_mods.begin(), user_mods.end(),
					[&](const ModSpec &mod) { return mod.name == dep && active(mod, settings); });
			if (installed == user_mods.end())
				throw Refused("Enable " + dep + " in this world before " + found->name + ".");
		}
	} else {
		for (const auto &mod : user_mods) {
			if (mod.name != found->name && active(mod, settings) && mod.depends.count(found->name))
				throw Refused("Disable " + mod.name + " before " + found->name + ".");
		}
		for (const auto &mod : world_mods) {
			if (mod.depends.count(found->name))
				throw Refused("The world mod " + mod.name + " requires " + found->name + ".");
		}
	}
	settings.set("load_mod_" + found->name, enabled ? found->virtual_path : "false");
	if (!settings.updateConfigFile((world.path + DIR_DELIM "world.mt").c_str()))
		throw BaseException("shell: could not save mod selection for " + world.path);
}

void Community::setTexture(const std::string &id) const
{
	if (!id.empty()) {
		auto found = textures();
		if (std::none_of(found.begin(), found.end(), [&](const ContentSpec &pack) { return pack.path == id; }))
			throw Refused("That texture pack is no longer installed.");
	}
	g_settings->set("texture_path", id.empty() ? "" : id + DIR_DELIM);
	if (!g_settings_path.empty() && !g_settings->updateConfigFile(g_settings_path.c_str()))
		throw BaseException("shell: could not save the texture pack setting");
}

} // namespace shell
