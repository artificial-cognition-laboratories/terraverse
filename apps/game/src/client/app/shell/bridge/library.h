// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/app/shell/bridge/local_mods.h"
#include "src/client/app/shell/bridge/registry.h"
#include "src/client/app/shell/host/shell.h"
#include <string>
#include "src/client/app/shell/bridge/local_servers.h"

namespace shell {

/*
	The Library's verbs, answered for the whole session (the menu's Library and
	the one opened in a world alike): the profile's own mods (mods.local,
	mods.contents), the ArcLabs registry (registry.*), and public servers
	(servers.*, not connected yet: said, never faked).
*/
class Library
{
public:
	struct Opts {
		Shell &shell;
		std::string contents_dir; // where local mods' pictures are written (Shell::contentsDir)
		std::string api;          // the ArcLabs backend (arclabs_url)
		std::string session;      // cache-buster for those pictures
		const LocalServers &local_servers;
	};
	explicit Library(Opts opts);

private:
	Intents m_intents; // first: removed last, after the verbs' owners below
	const LocalServers &m_local_servers;
	LocalMods m_local_mods;
	Registry m_registry;
};

} // namespace shell
