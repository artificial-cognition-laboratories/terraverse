// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

/*
Dummy database class
*/

#include "src/server/world/load/database/database-dummy.h"
#include "src/shared/game/objects/remoteplayer.h"


bool Database_Dummy::saveBlock(const v3s16 &pos, std::string_view data)
{
	auto lock = std::lock_guard(m_mutex);

	m_database[getBlockAsInteger(pos)] = data;
	return true;
}

void Database_Dummy::loadBlock(const v3s16 &pos, std::string *block)
{
	auto lock = std::lock_guard(m_mutex);

	s64 i = getBlockAsInteger(pos);
	auto it = m_database.find(i);
	if (it == m_database.end()) {
		block->clear();
		return;
	}

	*block = it->second;
}

bool Database_Dummy::deleteBlock(const v3s16 &pos)
{
	auto lock = std::lock_guard(m_mutex);

	return m_database.erase(getBlockAsInteger(pos)) > 0;
}

void Database_Dummy::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	auto lock = std::lock_guard(m_mutex);

	dst.reserve(m_database.size());
	for (auto x = m_database.begin(); x != m_database.end(); ++x) {
		dst.push_back(getIntegerAsBlock(x->first));
	}
}

void Database_Dummy::savePlayer(RemotePlayer *player)
{
	auto lock = std::lock_guard(m_mutex);

	m_player_database.insert(player->getName());
}

bool Database_Dummy::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	auto lock = std::lock_guard(m_mutex);

	return m_player_database.find(player->getName()) != m_player_database.end();
}

bool Database_Dummy::removePlayer(const std::string &name)
{
	auto lock = std::lock_guard(m_mutex);

	m_player_database.erase(name);
	return true;
}

void Database_Dummy::listPlayers(std::vector<std::string> &res)
{
	auto lock = std::lock_guard(m_mutex);

	for (const auto &player : m_player_database) {
		res.emplace_back(player);
	}
}








