// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <map>
#include <set>
#include <string>
#include <mutex>
#include "src/server/world/load/database/database.h"
#include "src/shared/core/irrlichttypes.h"

class Database_Dummy : public MapDatabase, public PlayerDatabase
{
public:
	bool saveBlock(const v3s16 &pos, std::string_view data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);


	void beginSave() {}
	void endSave() {}

private:
	// We could have used 3 mutexes, one per container, but as it's only for testing purposes, one mutex is enough.
	std::mutex m_mutex;
	std::map<s64, std::string> m_database;
	std::set<std::string> m_player_database;
	std::unordered_map<std::string, StringMap> m_mod_storage_database;
};

