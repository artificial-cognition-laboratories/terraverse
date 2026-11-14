// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "mapgen/terra_column.h"
#include "planet/atlas.h"
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace terra {

/*
	What the generator places in an atlas planet's cells
	(planning/sprint-32-atlas-lod.md): a cell's block is its column's (the
	column at the cell's own place on the planet) at the cell's layer; the
	core is stone. Asked by storage node: noise that wants a position (ores,
	caves) reads the node's. The map generator and the far view's tiles both
	ask this, so what's drawn from afar is what's generated, block for
	block. Before the game's own planting (trees, plants, dust).
*/
class AtlasLand
{
public:
	AtlasLand(const Columns &columns, const planet::Atlas &atlas);

	const planet::Atlas &atlas() const { return m_atlas; }
	const Columns &columns() const { return m_columns; }

	// Keeps the columns it built, a hashed slot each: a neighbourhood of map
	// blocks stands over the same ground, and a block is 256 columns asked
	// many times over. One per thread.
	class Reader
	{
	public:
		explicit Reader(const AtlasLand &land) : m_land(land) {}

		// The node at a storage node; none where it stores no cell (seam).
		std::optional<MapNode> node(planet::Atlas::Node n);
		// The column a storage node is part of; none for seam and the core.
		const Column *column(planet::Atlas::Node n);
		// A chunk begins: the window may have laid other tiles where its
		// storage columns were (what's kept by place stays good)
		void newChunk() { m_last_xz.reset(); }

	private:
		const Column &columnAt(const terragen::Place &place);

		const AtlasLand &m_land;
		struct Kept
		{
			terragen::Place place;
			std::optional<Column> column;
		};
		static constexpr size_t KEPT = 1 << 13; // a power of two: 32 blocks' worth of columns
		std::vector<Kept> m_kept;
		std::optional<planet::Atlas::Cell> m_last; // the last node's cell, and its column's place
		terragen::Place m_last_place;
		std::optional<std::pair<int32_t, int32_t>> m_last_xz; // the last storage column asked, and its place
		std::optional<terragen::Place> m_last_xz_place;
	};

	// The columns every generator of the same land has built, behind each
	// reader's own: the chunks stacked over a column are made by whichever
	// emerge thread is free, and each built the whole column again (five
	// times over, round a teleport: most of what loading the world cost)
	class Store;

private:
	const Columns &m_columns;
	const planet::Atlas &m_atlas;
	std::shared_ptr<Store> m_store;
};

} // namespace terra
