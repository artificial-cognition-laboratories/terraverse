// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include "src/shared/game/content/subgames.h"
#include "src/shared/platform/system/log.h" // errorstream
#include "src/shared/platform/online/launch.h" // Terraverse

// Information provided from "main"
// Start information for server or client
struct GameParams
{
	GameParams() = default;

	std::string world_path;
	SubgameSpec game_spec;
	bool is_dedicated_server;
	u16 socket_port;
	// Terraverse: a TypeScript server folder, its mods run by the sidecar (sprint 119)
	std::string server_folder;
	std::string probe; // Terraverse: a check's (src/probe)
	// Terraverse: the star system to make the world from if it's new, and to check it
	// against if it isn't (terra serve passes server.config.ts's `system`)
	std::string world_code;
};

struct GameClientData
{
	bool isSinglePlayer() const { return mode == GM_SINGLEPLAYER; }
	bool isAnyServer() const
	{ return mode == GM_HOST_AND_JOIN || mode == GM_SINGLEPLAYER; }

	std::string name;
	std::string password;
	std::string address; //< non-empty when joining a server

	enum Mode {
		GM_SINGLEPLAYER,
		GM_HOST_AND_JOIN,
		GM_JOIN,
		GM_Undefined
	} mode = GM_Undefined;
};

// Information provided by ClientLauncher
// Client-only data (joining or hosting server)
struct GameStartData : GameParams, GameClientData
{
	GameStartData() = default;

	// "world_path" must be kept in sync!
	WorldSpec world_spec;

	// Terraverse: hosting for friends, or joining one (src/online)
	online::Launch online;
};

/// Data provided to Lua for error reporting by the main menu
struct GameErrorData
{
	GameErrorData() = default;

	/// Raise and log an error
	/// @param msg Empty string means no error
	void setError(const std::string &msg, bool reconnect = false)
	{
		message = msg;
		reconnect_requested = reconnect;
		errorstream << msg << std::endl;
	}

	// Whether the server has requested a reconnect
	bool reconnect_requested = false;
	std::string message;
};
