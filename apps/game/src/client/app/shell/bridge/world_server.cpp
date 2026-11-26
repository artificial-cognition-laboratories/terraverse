// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world_server.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/server/app/mods/sidecar/ledger.h"
#include "src/server/app/mods/sidecar/sidecar.h"
#include "src/server/app/mods/sidecar/terra.h"
#include "src/shared/platform/system/filesys.h"
#include <regex>
#include <set>
#include <sstream>

namespace shell {

namespace {
Json::Value readJson(const std::string &path, const std::string &what);
void writeJson(const std::string &path, const Json::Value &value);
Json::Value pinned(const Json::Value &manifest);
std::set<std::string> names(const Json::Value &manifest);
std::string reason(const std::string &output);
const std::string MANIFEST = DIR_DELIM ".terra" DIR_DELIM "manifest.json";
const std::string PACKAGE = DIR_DELIM "package.json";
}

WorldServer::WorldServer(Opts o) : m_game(std::move(o.game)) {}

void WorldServer::make(const std::string &world, const std::vector<std::string> &mods) const
{
	if (!fs::PathExists(m_game + MANIFEST))
		throw Refused("The game's own mods aren't here to start a world from (bun run stage).");
	const std::string dir = world + DIR_DELIM "server";
	if (!fs::CreateAllDirs(dir) || !fs::CopyDir(m_game + DIR_DELIM ".terra", dir + DIR_DELIM ".terra") ||
			!fs::CopyFileContents(m_game + DIR_DELIM "server.config.ts", dir + DIR_DELIM "server.config.ts"))
		throw Refused("Couldn't write the world's server folder at " + dir + ".");

	Json::Value pkg(Json::objectValue);
	pkg["name"] = "world";
	pkg["version"] = "0.0.0";
	pkg["private"] = true;
	pkg["type"] = "module";
	// the official mods as this build ships them: the same versions are on the
	// registry, so a later install resolves them there
	pkg["dependencies"] = pinned(readJson(m_game + MANIFEST, "the game's own mods"));
	writeJson(dir + PACKAGE, pkg);
	if (!fs::safeWriteToFile(dir + DIR_DELIM "tsconfig.json", "{\n    \"extends\": \"./.terra/types/root.json\"\n}\n"))
		throw Refused("Couldn't write the world's server folder at " + dir + ".");
	if (mods.empty())
		return; // the game's mods, already prepared: nothing to fetch, so it works offline
	std::vector<std::string> args{ "install" };
	args.insert(args.end(), mods.begin(), mods.end());
	args.insert(args.end(), { "--yes", "--json" });
	terra(dir, args, "fetch mods");
}

Json::Value WorldServer::mods(const std::string &world) const
{
	const std::string own = folder(world);
	const Json::Value game = readJson(m_game + MANIFEST, "the game's own mods");
	const std::set<std::string> official = names(game);
	const Json::Value manifest = own.empty() ? game : readJson(own + MANIFEST, "this world's mods");
	sidecar::Ledger::Blocks blocks;
	try {
		blocks = sidecar::Ledger({ .world = world }).read();
	} catch (const sidecar::Error &e) {
		throw Refused(e.what());
	}

	Json::Value out(Json::objectValue);
	out["own"] = !own.empty();
	out["mods"] = Json::Value(Json::arrayValue);
	for (const Json::Value &m : manifest["mods"]) {
		Json::Value mod(Json::objectValue);
		const std::string name = m["name"].asString();
		mod["name"] = name;
		mod["version"] = m["version"].asString();
		mod["official"] = official.count(name) > 0;
		mod["blocks"] = Json::Value(Json::arrayValue);
		if (auto put = blocks.find(m["prefix"].asString()); put != blocks.end())
			for (const std::string &b : put->second)
				mod["blocks"].append(b);
		out["mods"].append(mod);
	}
	return out;
}

Json::Value WorldServer::add(const std::string &world, const std::vector<std::string> &mods) const
{
	if (mods.empty())
		throw Refused("Name a mod to add.");
	if (folder(world).empty()) {
		make(world, mods); // a world from before its own folder: it gets one now
		return this->mods(world);
	}
	std::vector<std::string> args{ "install" };
	args.insert(args.end(), mods.begin(), mods.end());
	// the page asked before a new major version: terra's own question is answered
	args.insert(args.end(), { "--yes", "--json" });
	terra(folder(world), args, "fetch mods");
	return this->mods(world);
}

Json::Value WorldServer::remove(const std::string &world, const std::string &name) const
{
	const Json::Value now = mods(world);
	bool found = false;
	for (const Json::Value &m : now["mods"]) {
		if (m["name"].asString() != name)
			continue;
		found = true;
		if (!m["blocks"].empty())
			throw Refused(name + " has put " + std::to_string(m["blocks"].size()) +
					" kinds of block in this world: taking it out would leave them as unknown blocks.");
	}
	if (!found)
		throw Refused(name + " isn't in this world.");
	if (folder(world).empty())
		make(world, {});
	const std::string dir = folder(world);
	Json::Value pkg = readJson(dir + PACKAGE, "this world's package.json");
	pkg["dependencies"].removeMember(name);
	writeJson(dir + PACKAGE, pkg);
	terra(dir, { "prepare", "." }, "take the mod out"); // installs again, so it's gone from bun.lock too
	return mods(world);
}

int WorldServer::added(const std::string &world) const
{
	const std::string own = folder(world);
	if (own.empty())
		return 0;
	const std::set<std::string> official = names(readJson(m_game + MANIFEST, "the game's own mods"));
	int n = 0;
	for (const std::string &name : names(readJson(own + MANIFEST, "this world's mods")))
		n += official.count(name) ? 0 : 1;
	return n;
}

std::string WorldServer::folder(const std::string &world)
{
	const std::string dir = world + DIR_DELIM "server";
	return fs::PathExists(dir + MANIFEST) ? dir : "";
}

void WorldServer::terra(const std::string &dir, const std::vector<std::string> &args, const std::string &doing) const
{
	// a mod is a scoped name, maybe pinned, and never a flag terra would read
	static const std::regex spec("^@[a-z0-9][a-z0-9._-]*/[a-z0-9][a-z0-9._-]*(@[0-9]+\\.[0-9]+\\.[0-9]+(-[0-9A-Za-z.-]+)?)?$");
	for (const std::string &a : args)
		if (a.rfind("@", 0) == 0 && !std::regex_match(a, spec))
			throw Refused("\"" + a + "\" isn't a mod (@scope/name, or @scope/name@1.2.3)");
	sidecar::Terra::Ran ran{ 0, "" };
	try {
		ran = sidecar::Terra::locate().run(args, dir);
	} catch (const sidecar::Error &e) {
		throw Refused("Couldn't " + doing + ": " + e.what());
	}
	if (ran.code != 0)
		throw Refused("Couldn't " + doing + ": " + reason(ran.output));
}

namespace {

Json::Value readJson(const std::string &path, const std::string &what)
{
	std::string text;
	if (!fs::ReadFile(path, text))
		throw Refused("Couldn't read " + what + " (" + path + ").");
	Json::Value data;
	Json::CharReaderBuilder reader;
	std::string errors;
	std::istringstream in(text);
	if (!Json::parseFromStream(reader, in, &data, &errors) || !data.isObject())
		throw Refused(what + " is unreadable: " + errors);
	return data;
}

void writeJson(const std::string &path, const Json::Value &value)
{
	Json::StreamWriterBuilder out;
	out["indentation"] = "    ";
	if (!fs::safeWriteToFile(path, Json::writeString(out, value) + "\n"))
		throw Refused("Couldn't write " + path + ".");
}

// each mod of a manifest at its exact version, as package.json dependencies
Json::Value pinned(const Json::Value &manifest)
{
	Json::Value deps(Json::objectValue);
	for (const Json::Value &mod : manifest["mods"]) {
		if (!mod["name"].isString() || !mod["version"].isString())
			throw Refused("A prepared mod is missing its name or version.");
		deps[mod["name"].asString()] = mod["version"].asString();
	}
	return deps;
}

std::set<std::string> names(const Json::Value &manifest)
{
	std::set<std::string> out;
	for (const Json::Value &mod : manifest["mods"])
		out.insert(mod["name"].asString());
	return out;
}

// terra's last words: the line that says what went wrong
std::string reason(const std::string &output)
{
	std::istringstream lines(output);
	std::string line, last;
	while (std::getline(lines, line))
		if (line.find_first_not_of(" \t\r") != std::string::npos)
			last = line;
	return last.empty() ? "terra gave no reason." : last;
}

} // namespace

} // namespace shell
