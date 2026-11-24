// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>

namespace shell {

/*
	The mods the player is developing: the active profile's mods/ folder (home.md). Their
	contents (blocks and items, and the pictures they draw) come from terra, which loads a
	mod with no engine; the pictures are written under `dir` and served as shell://contents/.
*/
class LocalMods
{
public:
	struct Opts {
		std::string dir;     // where contents' pictures are written: served as shell://contents/
		std::string session; // cache-buster: a mod's pictures change as it's edited
	};

	explicit LocalMods(Opts opts) : m_opts(std::move(opts)) {}

	// Every mod in the active profile's mods/, shaped as the registry's (bridge LocalMod).
	Json::Value list() const;
	// One of them, loaded by terra: its blocks and items as the Library shows them
	// (bridge CreativeItem[]). Throws Refused with terra's reason when the mod doesn't load.
	// Blocking: a worker thread's (bridge onAsync).
	Json::Value contents(const std::string &path) const;

private:
	Opts m_opts;
};

} // namespace shell
