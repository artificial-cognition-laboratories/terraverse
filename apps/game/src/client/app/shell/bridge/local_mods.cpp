// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/local_mods.h"

#include "src/client/app/shell/bridge/bridge.h"
#include "src/server/app/mods/sidecar/sidecar.h"
#include "src/server/app/mods/sidecar/terra.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/util/hash/hashing.h"
#include "src/shared/util/text/hex.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>

namespace shell {

namespace {

std::string modsDir();
Json::Value describe(const std::string &path, const std::string &folder);
Json::Value parse(const std::string &text, const std::string &what);
std::string edited(const std::string &path);
std::string owner();
std::string title(const std::string &name);
std::string lastLines(const std::string &text, size_t count);

} // namespace

Json::Value LocalMods::list() const
{
	Json::Value out(Json::arrayValue);
	const std::string dir = modsDir();
	if (!fs::PathExists(dir))
		return out;
	for (const fs::DirListNode &n : fs::GetDirListing(dir)) {
		const std::string path = dir + DIR_DELIM + n.name;
		std::string text;
		if (!n.dir || n.name[0] == '.' || !fs::ReadFile(path + DIR_DELIM "package.json", text))
			continue; // not a mod: a mod is a folder with a package.json (terra mod init makes one)
		out.append(describe(path, n.name));
	}
	return out;
}

Json::Value LocalMods::contents(const std::string &path) const
{
	// only the profile's own mods: the page names one by the path list() gave it
	if (!fs::PathStartsWith(path, modsDir()) || fs::PathsEqual(path, modsDir()) || path.find("..") != std::string::npos)
		throw Refused("not one of your mods: " + path);
	const std::string id = hex_encode(hashing::sha1(path)).substr(0, 16);
	const std::string pictures = m_opts.dir + DIR_DELIM + id;
	sidecar::Terra::Ran ran;
	try {
		ran = sidecar::Terra::locate().run({ "art", "list", path, "--json", "--pictures", pictures }, path);
	} catch (const sidecar::Error &e) {
		throw Refused(std::string("can't run terra to read the mod: ") + e.what());
	}
	if (ran.code != 0)
		throw Refused(lastLines(ran.output, 8));
	// the JSON is terra's last line: anything before it is the mods' own logging
	const size_t at = ran.output.rfind("{\"entries\"");
	if (at == std::string::npos)
		throw Refused("terra said nothing it could show: " + lastLines(ran.output, 4));
	const Json::Value listed = parse(ran.output.substr(at), "terra art list");
	static std::atomic<unsigned> drawn{ 0 };
	const std::string base = "shell://contents/" + id + "/";
	const std::string bust = "?s=" + m_opts.session + "-" + std::to_string(drawn++);
	Json::Value out(Json::arrayValue);
	for (const Json::Value &e : listed["entries"]) {
		const Json::Value &p = e["pictures"];
		Json::Value item;
		item["item"] = e["name"];
		item["description"] = e["description"];
		item["icon"] = base + p["icon"].asString() + bust;
		item["mass"] = Json::nullValue;
		item["stackMax"] = 99;
		item["category"] = e["type"].asString() == "block" ? "building" : "items";
		if (p["faces"].isObject()) {
			item["faces"]["top"] = base + p["faces"]["top"].asString() + bust;
			item["faces"]["bottom"] = base + p["faces"]["bottom"].asString() + bust;
			item["faces"]["side"] = base + p["faces"]["side"].asString() + bust;
		} else {
			item["faces"] = Json::nullValue;
		}
		out.append(item);
	}
	return out;
}

namespace {

// A local mod as the page shows a registry one (bridge RegistryMod), and where it is: the
// same cards and page serve both. What only publishing gives (stars, versions) is empty.
Json::Value describe(const std::string &path, const std::string &folder)
{
	std::string text;
	fs::ReadFile(path + DIR_DELIM "package.json", text);
	const Json::Value pkg = parse(text, path + DIR_DELIM "package.json");
	Json::Value mod;
	mod["name"] = pkg["name"].isString() ? pkg["name"].asString() : folder;
	mod["title"] = title(mod["name"].asString());
	mod["description"] = pkg["description"].isString() ? pkg["description"].asString() : "";
	mod["owner"] = owner();
	mod["latestVersion"] = pkg["version"].isString() ? pkg["version"].asString() : "0.0.0";
	mod["stars"] = 0;
	mod["updatedAt"] = edited(path);
	mod["type"] = "mod";
	mod["categories"] = Json::Value(Json::arrayValue);
	mod["icon"] = Json::nullValue;
	mod["banner"] = Json::nullValue;
	mod["installs"] = Json::nullValue;
	mod["installed"] = true;
	mod["official"] = false;
	std::string readme;
	fs::ReadFile(path + DIR_DELIM "README.md", readme);
	mod["readme"] = readme;
	mod["deprecated"] = Json::nullValue;
	mod["versions"] = Json::Value(Json::arrayValue);
	mod["media"] = Json::nullValue;
	mod["files"] = Json::nullValue;
	mod["dependencies"] = pkg["dependencies"].isObject() ? pkg["dependencies"] : Json::Value(Json::objectValue);
	mod["path"] = path;
	return mod;
}

// When it was last saved: the newest file in it, outside what's built or installed. ISO 8601.
std::string edited(const std::string &path)
{
	namespace sfs = std::filesystem;
	sfs::file_time_type newest{};
	std::error_code ec;
	for (auto it = sfs::recursive_directory_iterator(path, ec); !ec && it != sfs::recursive_directory_iterator(); it.increment(ec)) {
		const std::string name = it->path().filename().string();
		if (it->is_directory() && (name == "node_modules" || name == ".terra" || name == ".git")) {
			it.disable_recursion_pending();
			continue;
		}
		if (it->is_regular_file())
			newest = std::max(newest, it->last_write_time());
	}
	const auto since = std::chrono::duration_cast<std::chrono::seconds>(newest - sfs::file_time_type::clock::now()) +
			std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
	const std::time_t t = since.count();
	char buf[32];
	std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
	return buf;
}

// Whose mods these are: the profile's handle, or "you" playing signed out.
std::string owner()
{
	const std::string profile = fs::GetFilenameFromPath(porting::path_user.c_str());
	return !profile.empty() && profile[0] == '@' ? profile.substr(1) : "you";
}

// The active profile's, read each time: a sign-in can change the profile under the menu.
std::string modsDir()
{
	return porting::path_user + DIR_DELIM "mods";
}

Json::Value parse(const std::string &text, const std::string &what)
{
	Json::Value v;
	Json::CharReaderBuilder reader;
	std::string errors;
	std::istringstream in(text);
	if (!Json::parseFromStream(reader, in, &v, &errors))
		throw Refused(what + " isn't JSON: " + errors);
	return v;
}

// "@you/stone-tools" → "Stone tools", as terra titles a mod with none of its own
std::string title(const std::string &name)
{
	std::string t = name.substr(name.find('/') == std::string::npos ? 0 : name.find('/') + 1);
	for (char &c : t)
		if (c == '-' || c == '_')
			c = ' ';
	if (!t.empty())
		t[0] = (char)toupper((unsigned char)t[0]);
	return t;
}

std::string lastLines(const std::string &text, size_t count)
{
	size_t end = text.find_last_not_of("\n ");
	if (end == std::string::npos)
		return "terra failed, and said nothing";
	size_t at = end + 1;
	for (size_t n = 0; n < count && at > 0; n++) {
		size_t nl = text.rfind('\n', at - 1);
		at = nl == std::string::npos ? 0 : nl;
	}
	return text.substr(at == 0 ? 0 : at + 1, end + 1 - (at == 0 ? 0 : at + 1));
}

} // namespace

} // namespace shell
