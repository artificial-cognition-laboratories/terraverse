// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>

class Server;
class Client;

namespace shell {

/*
	The server the game is connected to, as the debugger's Server tab shows it.
	Local (this process runs it): its game, world, uptime, lag, players and
	mods. Remote: only what the connection tells the client, and `local` false.
*/
Json::Value describeServer(Server *local, Client &client);

} // namespace shell
