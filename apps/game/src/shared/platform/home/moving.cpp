#include "src/shared/platform/home/moving.h"

#include "src/shared/platform/home/home.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/log.h"
#include <vector>

namespace home::moving {

namespace {

// What a player keeps, as the old layout kept it beside everything else: it all goes to `local`.
const char *const PLAYER[] = { "worlds", "servers", "textures", "parts", "backups", "screenshots", "mod_data" };

void from(const std::string &source, const std::string &root, bool legacy);
bool moveOne(const std::string &from, const std::string &to, std::vector<std::string> &moved);
void note(const std::string &source, const std::string &root, const std::vector<std::string> &moved);

} // namespace

void in(const std::string &root, const std::string &legacy)
{
	if (!fs::CreateAllDirs(root + DIR_DELIM "profiles" DIR_DELIM "local"))
		throw Error("could not create " + root + DIR_DELIM "profiles");
	from(root, root, false);
	if (!legacy.empty() && fs::PathExists(legacy) && !fs::PathsEqual(legacy, root))
		from(legacy, root, true);
}

namespace {

void from(const std::string &source, const std::string &root, bool legacy)
{
	const std::string local = root + DIR_DELIM "profiles" DIR_DELIM "local";
	std::vector<std::string> moved;
	for (const char *name : PLAYER)
		moveOne(source + DIR_DELIM + name, local + DIR_DELIM + name, moved);
	for (const fs::DirListNode &n : fs::GetDirListing(source))
		if (!n.dir && n.name.rfind("account", 0) == 0 && n.name.size() > 5 && n.name.substr(n.name.size() - 5) == ".json")
			moveOne(source + DIR_DELIM + n.name, local + DIR_DELIM + n.name, moved);
	moveOne(source + DIR_DELIM "minetest.conf", root + DIR_DELIM "settings.conf", moved);
	// a source build's other players (--profile two): profiles of their own now
	if (legacy && fs::PathExists(source + DIR_DELIM "profiles"))
		for (const fs::DirListNode &n : fs::GetDirListing(source + DIR_DELIM "profiles"))
			if (n.dir && Home::validName(n.name))
				moveOne(source + DIR_DELIM "profiles" DIR_DELIM + n.name, root + DIR_DELIM "profiles" DIR_DELIM + n.name, moved);
	if (!moved.empty())
		note(source, root, moved);
}

// Moves one thing if it's there and its new place is free: what it moved, in `moved`.
bool moveOne(const std::string &from, const std::string &to, std::vector<std::string> &moved)
{
	if (!fs::PathExists(from) || fs::PathExists(to))
		return false;
	if (!fs::Rename(from, to) && !fs::MoveDir(from, to))
		throw Error("could not move " + from + " to " + to);
	moved.push_back(from + "  →  " + to);
	actionstream << "home: moved " << from << " to " << to << std::endl;
	return true;
}

void note(const std::string &source, const std::string &root, const std::vector<std::string> &moved)
{
	std::string text = "# Moved\n\nTerraverse keeps everything in one place now: " + root +
		" (see .agents/planning/home.md in the Terraverse repo). What was here went to:\n\n";
	for (const std::string &m : moved)
		text += "- " + m + "\n";
	if (!fs::safeWriteToFile(source + DIR_DELIM "MOVED.md", text))
		throw Error("could not write " + source + DIR_DELIM "MOVED.md");
}

} // namespace

} // namespace home::moving
