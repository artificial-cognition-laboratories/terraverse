// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/machines/workshop/library.h"

#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/util/data/serialize.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace workshop {

namespace {

const char MAGIC[] = "TFB1";
const char EXT[] = ".tfb";

} // namespace

std::string Blueprint::write() const
{
	std::ostringstream os(std::ios::binary);
	os.write(MAGIC, 4);
	writeU16(os, u16(parts.size()));
	for (const Part &p : parts) {
		writeV3S16(os, p.offset);
		os << serializeString32(p.machine);
	}
	return os.str();
}

Blueprint Blueprint::read(const std::string &text)
{
	std::istringstream is(text, std::ios::binary);
	char magic[4];
	is.read(magic, 4);
	if (is.gcount() != 4 || std::string(magic, 4) != std::string(MAGIC, 4))
		throw SerializationError("blueprint: not a blueprint");
	Blueprint out;
	out.parts.resize(readU16(is));
	if (out.parts.empty())
		throw SerializationError("blueprint: no machines");
	for (Part &p : out.parts) {
		p.offset = readV3S16(is);
		p.machine = deSerializeString32(is);
	}
	return out;
}

Library::Library(const std::string &root, const std::string &world_path) : m_root(root)
{
	if (world_path.empty())
		return;
	// a world's from before: copied, never moved (the world's still the player's)
	const std::string old = world_path + DIR_DELIM + "blueprints";
	for (const fs::DirListNode &player : fs::GetDirListing(old)) {
		if (!player.dir)
			continue;
		const std::string from = old + DIR_DELIM + player.name, to = dir(player.name);
		for (const fs::DirListNode &n : fs::GetDirListing(from)) {
			const std::string target = to + DIR_DELIM + n.name;
			if (n.dir || fs::PathExists(target))
				continue;
			std::string text;
			if (!fs::ReadFile(from + DIR_DELIM + n.name, text))
				throw FileNotGoodException("can't read " + from + DIR_DELIM + n.name);
			if (!fs::CreateAllDirs(to) || !fs::safeWriteToFile(target, text))
				throw FileNotGoodException("can't write " + target);
		}
	}
}

bool Library::named(const std::string &name)
{
	if (name.empty() || name.size() > LONGEST_NAME || name.front() == ' ' || name.back() == ' ')
		return false;
	return std::all_of(name.begin(), name.end(), [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
				|| c == ' ' || c == '-' || c == '_';
	});
}

void Library::save(const std::string &player, const std::string &name, const Blueprint &blueprint)
{
	if (!named(name))
		throw std::invalid_argument("not a blueprint's name: \"" + name + "\"");
	const std::string d = dir(player);
	if (!fs::CreateAllDirs(d))
		throw FileNotGoodException("can't make " + d);
	const std::string path = d + DIR_DELIM + name + EXT;
	if (!fs::safeWriteToFile(path, blueprint.write()))
		throw FileNotGoodException("can't write " + path);
}

Blueprint Library::load(const std::string &player, const std::string &name) const
{
	if (!named(name))
		throw std::out_of_range("no blueprint \"" + name + "\"");
	const std::string path = dir(player) + DIR_DELIM + name + EXT;
	if (!fs::PathExists(path))
		throw std::out_of_range("no blueprint \"" + name + "\"");
	std::string text;
	if (!fs::ReadFile(path, text))
		throw FileNotGoodException("can't read " + path);
	return Blueprint::read(text);
}

void Library::remove(const std::string &player, const std::string &name)
{
	const std::string path = dir(player) + DIR_DELIM + name + EXT;
	if (!named(name) || !fs::PathExists(path))
		throw std::out_of_range("no blueprint \"" + name + "\"");
	if (!fs::DeleteSingleFileOrEmptyDirectory(path))
		throw FileNotGoodException("can't delete " + path);
}

std::vector<std::string> Library::names(const std::string &player) const
{
	std::vector<std::string> out;
	const std::string ext(EXT);
	for (const fs::DirListNode &n : fs::GetDirListing(dir(player))) {
		if (n.dir || n.name.size() <= ext.size() || n.name.compare(n.name.size() - ext.size(), ext.size(), ext) != 0)
			continue;
		const std::string name = n.name.substr(0, n.name.size() - ext.size());
		if (named(name))
			out.push_back(name);
	}
	std::sort(out.begin(), out.end());
	return out;
}

std::string Library::dir(const std::string &player) const
{
	return m_root + DIR_DELIM + player;
}

} // namespace workshop
