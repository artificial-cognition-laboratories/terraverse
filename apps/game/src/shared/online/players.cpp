// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/online/players.h"
#include "src/shared/core/exceptions.h"
#include "src/shared/platform/filesys.h"
#include "src/shared/platform/log.h"
#include "src/shared/platform/settings.h"
#include <sqlite3.h>

namespace online {

static bool exists(sqlite3 *db, const std::string &name)
{
	sqlite3_stmt *stmt = nullptr;
	if (sqlite3_prepare_v2(db, "SELECT 1 FROM player WHERE name = ?", -1, &stmt, nullptr) != SQLITE_OK)
		throw BaseException(std::string("online: reading players: ") + sqlite3_errmsg(db));
	sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
	bool found = sqlite3_step(stmt) == SQLITE_ROW;
	sqlite3_finalize(stmt);
	return found;
}

void adoptSingleplayer(const std::string &world_path, const std::string &name)
{
	Settings world;
	const std::string conf = world_path + DIR_DELIM "world.mt";
	if (!world.readConfigFile(conf.c_str()) || !world.exists("player_backend"))
		return; // the engine decides the backend on first run: nothing to carry over yet
	if (world.get("player_backend") != "sqlite3") {
		warningstream << "online: " << world_path << " keeps players in " << world.get("player_backend")
				<< "; the singleplayer's inventory isn't carried over to " << name << std::endl;
		return;
	}
	const std::string path = world_path + DIR_DELIM "players.sqlite";
	if (!fs::PathExists(path))
		return;

	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
		std::string why = db ? sqlite3_errmsg(db) : "out of memory";
		sqlite3_close(db);
		throw BaseException("online: couldn't open " + path + ": " + why);
	}
	try {
		if (!exists(db, "singleplayer") || exists(db, name)) {
			sqlite3_close(db);
			return;
		}
		// Children first; foreign keys are off (sqlite's default) so the
		// rename doesn't cascade.
		static const char *TABLES[] = { "player_metadata", "player_inventories", "player_inventory_items" };
		std::string sql = "BEGIN;";
		for (const char *table : TABLES)
			sql += std::string("UPDATE ") + table + " SET player = ?1 WHERE player = 'singleplayer';";
		sql += "UPDATE player SET name = ?1 WHERE name = 'singleplayer'; COMMIT;";
		const char *tail = sql.c_str();
		while (*tail) {
			sqlite3_stmt *stmt = nullptr;
			if (sqlite3_prepare_v2(db, tail, -1, &stmt, &tail) != SQLITE_OK)
				throw BaseException(std::string("online: carrying the singleplayer over: ") + sqlite3_errmsg(db));
			if (!stmt)
				break;
			if (sqlite3_bind_parameter_count(stmt) > 0)
				sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
			int rc = sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			if (rc != SQLITE_DONE)
				throw BaseException(std::string("online: carrying the singleplayer over: ") + sqlite3_errmsg(db));
		}
		actionstream << "online: " << name << " is the singleplayer of " << world_path << " from now on" << std::endl;
	} catch (...) {
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		sqlite3_close(db);
		throw;
	}
	sqlite3_close(db);
}

} // namespace online
