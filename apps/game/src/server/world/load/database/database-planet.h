// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/server/world/load/database/database.h"
#include <functional>
#include <memory>
#include <vector>

namespace planet { class Atlas; }

/*
	An atlas world's map blocks, kept by where they are on their body: a
	block of a tile in the window is saved at its spot
	(planet::Atlas::blockSpot) in its body's database, so a tile moved to
	another slot finds its blocks again. A block of no tile (the seam
	between slots) is the seam the generator makes there: never kept.
*/
class MapDatabasePlanet : public MapDatabase
{
public:
	// `home`: the home body's blocks (the world's own database). `open`:
	// another body's, opened the first time one of its blocks is.
	using Open = std::function<std::unique_ptr<MapDatabase>(int body)>;
	MapDatabasePlanet(std::unique_ptr<MapDatabase> home, const planet::Atlas &atlas, Open open);

	bool saveBlock(const v3s16 &pos, std::string_view data) override;
	void loadBlock(const v3s16 &pos, std::string *block) override;
	bool deleteBlock(const v3s16 &pos) override;
	// Every kept block isn't a place in the map (only the window's are):
	// throws.
	void listAllLoadableBlocks(std::vector<v3s16> &dst) override;

	void beginSave() override;
	void endSave() override;
	bool initialized() const override { return m_kept[0]->initialized(); }
	void verifyDatabase() override;

private:
	MapDatabase &kept(int body); // opened on first use
	bool m_saving = false;       // between beginSave and endSave: a body opened meanwhile begins too
	std::vector<std::unique_ptr<MapDatabase>> m_kept; // by body
	const planet::Atlas &m_atlas;
	Open m_open;
};
