// Terra Firma: the engine's own chat commands (sprint 119): an admin's tools for what the
// engine owns (its bodies, its rivers). Everything else typed is chat's (Server::handleChat):
// a line, or a mod's command, which TypeScript answers (the sidecar, @terraverse/commands).
#pragma once

#include <string>
#include <vector>

class Server;
class RemotePlayer;

struct EngineCommand
{
	const char *name;
	const char *params;      // as /help shows them: "<body> [<latitude> <longitude>]"
	const char *description;
	const char *permission;  // core:…, or null: anyone may
	// The reply. `player` is null for the server's console. Throws std::invalid_argument for
	// what's typed wrong, said back to whoever typed it.
	std::string (*run)(Server *server, RemotePlayer *player, const std::string &args);
};

const std::vector<EngineCommand> &engineCommands();
