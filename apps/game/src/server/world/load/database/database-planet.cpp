// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/load/database/database-planet.h"
#include "src/shared/base/exceptions.h"
#include "library/world/planet/atlas.h"

MapDatabasePlanet::MapDatabasePlanet(std::unique_ptr<MapDatabase> home, const planet::Atlas &atlas, Open open) :
	m_kept((size_t)atlas.bodies()), m_atlas(atlas), m_open(std::move(open))
{
	m_kept[planet::Atlas::HOME] = std::move(home);
}

MapDatabase &MapDatabasePlanet::kept(int body)
{
	std::unique_ptr<MapDatabase> &db = m_kept.at(body);
	if (!db) {
		db = m_open(body);
		if (!db)
			throw BaseException("body " + std::to_string(body) + "'s map database didn't open");
		if (m_saving)
			db->beginSave();
	}
	return *db;
}

bool MapDatabasePlanet::saveBlock(const v3s16 &pos, std::string_view data)
{
	const std::optional<planet::Atlas::Spot> s = m_atlas.blockSpot({ pos.X, pos.Y, pos.Z });
	return s ? kept(s->body).saveBlockAt(v3s32(s->x, s->y, s->z), data) : true; // seam: made again as it was
}

void MapDatabasePlanet::loadBlock(const v3s16 &pos, std::string *block)
{
	block->clear();
	if (const std::optional<planet::Atlas::Spot> s = m_atlas.blockSpot({ pos.X, pos.Y, pos.Z }))
		kept(s->body).loadBlockAt(v3s32(s->x, s->y, s->z), block);
}

bool MapDatabasePlanet::deleteBlock(const v3s16 &pos)
{
	const std::optional<planet::Atlas::Spot> s = m_atlas.blockSpot({ pos.X, pos.Y, pos.Z });
	return s ? kept(s->body).deleteBlockAt(v3s32(s->x, s->y, s->z)) : true;
}

void MapDatabasePlanet::beginSave()
{
	m_saving = true;
	for (const std::unique_ptr<MapDatabase> &db : m_kept)
		if (db)
			db->beginSave();
}

void MapDatabasePlanet::endSave()
{
	m_saving = false;
	for (const std::unique_ptr<MapDatabase> &db : m_kept)
		if (db)
			db->endSave();
}

void MapDatabasePlanet::verifyDatabase()
{
	for (const std::unique_ptr<MapDatabase> &db : m_kept)
		if (db)
			db->verifyDatabase();
}

void MapDatabasePlanet::listAllLoadableBlocks(std::vector<v3s16> &)
{
	throw BaseException("a planet's blocks aren't all in the map at once: listing them is not wired yet");
}
