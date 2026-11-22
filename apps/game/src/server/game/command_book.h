// Terra Firma: every command a server answers, the engine's and its mods', and what each player
// may run of them (the page's palette lists exactly that).
#pragma once

#include "src/server/app/mods/sidecar/sidecar.h"
#include <string>
#include <vector>

class Server;
class RemotePlayer;

/*
	The commands typed after `/`: the engine's own (server/game/commands.h) and the mods'
	(sidecar::Host::Command, replaced whole whenever the mods change them). It runs a line,
	lists them for /help, and sends each player the ones they may run as the UI value
	`commands`, on joining and whenever the mods' set changes.
*/
class CommandBook
{
public:
	struct Opts {
		Server &server;
	};

	explicit CommandBook(Opts opts) : m_opts(opts) {}

	// A line typed after `/`; the reply, if any. `player` is null for the server's console.
	std::string run(const std::string &name, RemotePlayer *player, const std::string &line);
	// The mods' whole set, replacing the last; every player is told what they may run now.
	void setMods(std::vector<sidecar::Host::Command> mods);
	// This player's list, to their page.
	void tell(RemotePlayer *player);

private:
	std::string help(const std::string &name, bool console);
	bool may(const std::string &name, const char *permission);

	Opts m_opts;
	std::vector<sidecar::Host::Command> m_mods;
};
