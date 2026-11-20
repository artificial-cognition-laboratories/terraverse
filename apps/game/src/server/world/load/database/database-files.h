// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

// !!! WARNING !!!
// This backend is intended to be used on Minetest 0.4.16 only for the transition backend
// for player files

#include "src/server/world/load/database/database.h"
#include <unordered_map>
#include <unordered_set>
#include <json/json.h> // for Json::Value

class PlayerDatabaseFiles : public PlayerDatabase
{
public:
	PlayerDatabaseFiles(const std::string &savedir);
	virtual ~PlayerDatabaseFiles() = default;

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

private:
	void deSerialize(RemotePlayer *p, std::istream &is, const std::string &playername,
			PlayerSAO *sao);
	/*
		serialize() writes a bunch of text that can contain
		any characters except a '\0', and such an ending that
		deSerialize stops reading exactly at the right point.
	*/
	void serialize(RemotePlayer *p, std::ostream &os);

	std::string m_savedir;
};


