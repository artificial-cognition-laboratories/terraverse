// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "src/server/world/load/database/database-files.h"
#include "src/shared/base/convert_json.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/debug.h"
#include "src/shared/platform/system/filesys.h"
#include "src/server/players/player_sao.h"
#include "src/shared/util/text/string.h"
#include <json/json.h>
#include <cassert>

// !!! WARNING !!!
// This backend is intended to be used on Minetest 0.4.16 only for the transition backend
// for player files

PlayerDatabaseFiles::PlayerDatabaseFiles(const std::string &savedir) : m_savedir(savedir)
{
	fs::CreateDir(m_savedir);
}

void PlayerDatabaseFiles::deSerialize(RemotePlayer *p, std::istream &is,
	 const std::string &playername, PlayerSAO *sao)
{
	Settings args("PlayerArgsEnd");

	if (!args.parseConfigLines(is)) {
		throw SerializationError("PlayerArgsEnd of player " + playername + " not found!");
	}

	p->m_dirty = true;
	//args.getS32("version"); // Version field value not used
	p->m_name = args.get("name");

	if (sao) {
		try {
			sao->setHPRaw(args.getU16("hp"));
		} catch(SettingNotFoundException &e) {
			sao->setHPRaw(PLAYER_MAX_HP_DEFAULT);
		}

		try {
			sao->setBasePosition(args.getV3F("position").value_or(v3f()));
		} catch (SettingNotFoundException &e) {}

		try {
			sao->setLookPitch(args.getFloat("pitch"));
		} catch (SettingNotFoundException &e) {}
		try {
			sao->setPlayerYaw(args.getFloat("yaw"));
		} catch (SettingNotFoundException &e) {}

		try {
			sao->setBreath(args.getU16("breath"), false);
		} catch (SettingNotFoundException &e) {}

		try {
			const std::string &extended_attributes = args.get("extended_attributes");
			std::istringstream iss(extended_attributes);
			Json::CharReaderBuilder builder;
			builder.settings_["collectComments"] = false;
			std::string errs;

			Json::Value attr_root;
			Json::parseFromStream(builder, iss, &attr_root, &errs);

			const Json::Value::Members attr_list = attr_root.getMemberNames();
			for (const auto &it : attr_list) {
				Json::Value attr_value = attr_root[it];
				sao->getMeta().setString(it, attr_value.asString());
			}
			sao->getMeta().setModified(false);
		} catch (SettingNotFoundException &e) {}
	}

	try {
		p->inventory.deSerialize(is);
	} catch (SerializationError &e) {
		errorstream << "Failed to deserialize player inventory. player_name="
			<< p->getName() << " " << e.what() << std::endl;
	}

	if (!p->inventory.getList("craftpreview") && p->inventory.getList("craftresult")) {
		// Convert players without craftpreview
		p->inventory.addList("craftpreview", 1);

		bool craftresult_is_preview = true;
		if(args.exists("craftresult_is_preview"))
			craftresult_is_preview = args.getBool("craftresult_is_preview");
		if(craftresult_is_preview)
		{
			// Clear craftresult
			p->inventory.getList("craftresult")->changeItem(0, ItemStack());
		}
	}
}

void PlayerDatabaseFiles::serialize(RemotePlayer *p, std::ostream &os)
{
	// Utilize a Settings object for storing values
	Settings args("PlayerArgsEnd");
	args.setS32("version", 1);
	args.set("name", p->getName());

	PlayerSAO *sao = p->getPlayerSAO();
	// This should not happen
	sanity_check(sao);
	args.setU16("hp", sao->getHP());
	args.setV3F("position", sao->getBasePosition());
	args.setFloat("pitch", sao->getLookPitch());
	args.setFloat("yaw", sao->getRotation().Y);
	args.setU16("breath", sao->getBreath());

	std::string extended_attrs;
	{
		// serializeExtraAttributes
		Json::Value json_root;

		const StringMap &attrs = sao->getMeta().getStrings();
		for (const auto &attr : attrs) {
			json_root[attr.first] = attr.second;
		}

		extended_attrs = fastWriteJson(json_root);
	}
	args.set("extended_attributes", extended_attrs);

	args.writeLines(os);

	p->inventory.serialize(os);
}

void PlayerDatabaseFiles::savePlayer(RemotePlayer *player)
{
	fs::CreateDir(m_savedir);

	std::string savedir = m_savedir + DIR_DELIM;
	std::string path = savedir + player->getName();
	bool path_found = false;
	RemotePlayer testplayer("", NULL);

	for (u32 i = 0; i < PLAYER_FILE_ALTERNATE_TRIES && !path_found; i++) {
		if (!fs::PathExists(path)) {
			path_found = true;
			continue;
		}

		// Open and deserialize file to check player name
		auto is = open_ifstream(path.c_str(), true);
		if (!is.good())
			return;

		deSerialize(&testplayer, is, path, NULL);
		is.close();
		if (testplayer.getName() == player->getName()) {
			path_found = true;
			continue;
		}

		path = savedir + player->getName() + itos(i);
	}

	if (!path_found) {
		errorstream << "Didn't find free file for player " << player->getName()
				<< std::endl;
		return;
	}

	// Open and serialize file
	std::ostringstream ss(std::ios_base::binary);
	serialize(player, ss);
	if (!fs::safeWriteToFile(path, ss.str())) {
		infostream << "Failed to write " << path << std::endl;
	}

	player->onSuccessfulSave();
}

bool PlayerDatabaseFiles::removePlayer(const std::string &name)
{
	std::string players_path = m_savedir + DIR_DELIM;
	std::string path = players_path + name;

	RemotePlayer temp_player("", NULL);
	for (u32 i = 0; i < PLAYER_FILE_ALTERNATE_TRIES; i++) {
		// Open file and deserialize
		auto is = open_ifstream(path.c_str(), false);
		if (!is.good())
			continue;

		deSerialize(&temp_player, is, path, NULL);
		is.close();

		if (temp_player.getName() == name) {
			fs::DeleteSingleFileOrEmptyDirectory(path, true);
			return true;
		}

		path = players_path + name + itos(i);
	}

	return false;
}

bool PlayerDatabaseFiles::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	std::string players_path = m_savedir + DIR_DELIM;
	std::string path = players_path + player->getName();

	const std::string player_to_load = player->getName();
	for (u32 i = 0; i < PLAYER_FILE_ALTERNATE_TRIES; i++) {
		// Open file and deserialize
		auto is = open_ifstream(path.c_str(), false);
		if (!is.good())
			continue;

		deSerialize(player, is, path, sao);
		is.close();

		if (player->getName() == player_to_load)
			return true;

		path = players_path + player_to_load + itos(i);
	}

	infostream << "Player file for player " << player_to_load << " not found" << std::endl;
	return false;
}

void PlayerDatabaseFiles::listPlayers(std::vector<std::string> &res)
{
	std::vector<fs::DirListNode> files = fs::GetDirListing(m_savedir);
	// list files into players directory
	for (auto it = files.begin(); it != files.end(); ++it) {
		// Ignore directories
		if (it->dir)
			continue;

		const std::string &filename = it->name;
		std::string full_path = m_savedir + DIR_DELIM + filename;
		auto is = open_ifstream(full_path.c_str(), true);
		if (!is.good())
			continue;

		RemotePlayer player(filename.c_str(), NULL);
		// Null env & dummy peer_id
		PlayerSAO playerSAO(NULL, &player, 15789, false);

		deSerialize(&player, is, "", &playerSAO);
		is.close();

		res.emplace_back(player.getName());
	}
}
