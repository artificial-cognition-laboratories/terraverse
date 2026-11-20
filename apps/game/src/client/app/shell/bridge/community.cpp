// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/community.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/app/shell/bridge/worlds.h"
#include "src/shared/content/content.h"
#include "src/shared/content/subgames.h"
#include "src/shared/core/exceptions.h"
#include "src/shared/platform/filesys.h"
#include "src/shared/platform/porting.h"
#include "src/shared/platform/settings.h"
#include "src/shared/util/string.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace shell {

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

Json::Value Community::list() const
{
	auto saved_worlds = m_worlds.list();

	Json::Value out;
	out["textures"] = Json::Value(Json::arrayValue);
	out["worlds"] = Json::Value(Json::arrayValue);
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
