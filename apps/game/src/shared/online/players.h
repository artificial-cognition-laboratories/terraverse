// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace online {

/*
	Hosting, the player is their ArcLabs username; alone, the engine calls
	them "singleplayer". The first time a world is hosted, the singleplayer's
	body, inventory and metadata become the host's, so a world played alone
	carries on when it's opened to friends. Only the sqlite3 player backend
	(every world the menu makes).
*/
void adoptSingleplayer(const std::string &world_path, const std::string &name);

} // namespace online
