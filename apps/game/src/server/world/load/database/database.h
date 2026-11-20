// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "src/shared/base/irr_v3d.h"
#include "src/shared/base/irrlichttypes.h"
#include "src/shared/util/text/string.h"

class Database
{
public:
	virtual void beginSave() = 0;
	virtual void endSave() = 0;

	/// @return true if database connection is open
	virtual bool initialized() const { return true; }

	/// Open and initialize the database if needed
	virtual void verifyDatabase() {};
};

class MapDatabase : public Database
{
public:
	virtual ~MapDatabase() = default;

	virtual bool saveBlock(const v3s16 &pos, std::string_view data) = 0;
	virtual void loadBlock(const v3s16 &pos, std::string *block) = 0;
	virtual bool deleteBlock(const v3s16 &pos) = 0;

	static s64 getBlockAsInteger(const v3s16 &pos);
	static v3s16 getIntegerAsBlock(s64 i);

	virtual void listAllLoadableBlocks(std::vector<v3s16> &dst) = 0;

	// Terra Firma: blocks by a 32-bit position (a planet's: its spot in
	// blocks, database/database-planet.h). Backends that can't keep one throw.
	virtual bool saveBlockAt(const v3s32 &pos, std::string_view data);
	virtual void loadBlockAt(const v3s32 &pos, std::string *block);
	virtual bool deleteBlockAt(const v3s32 &pos);
};

class PlayerSAO;
class RemotePlayer;

class PlayerDatabase
{
public:
	virtual ~PlayerDatabase() = default;

	virtual void savePlayer(RemotePlayer *player) = 0;
	virtual bool loadPlayer(RemotePlayer *player, PlayerSAO *sao) = 0;
	virtual bool removePlayer(const std::string &name) = 0;
	virtual void listPlayers(std::vector<std::string> &res) = 0;
};

