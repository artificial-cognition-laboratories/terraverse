// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapgen/atlas_land.h"

namespace terra {

std::optional<MapNode> AtlasLand::Reader::node(planet::Atlas::Node n)
{
	const std::optional<planet::Atlas::Cell> cell = m_land.m_atlas.cell(n);
	if (!cell)
		return std::nullopt;
	if (cell->body != m_land.m_columns.body())
		throw std::logic_error("terra::AtlasLand: a node of body " + std::to_string(cell->body) +
				" asked of body " + std::to_string(m_land.m_columns.body()) + "'s land");
	if (cell->region == terragen::Atlas::Region::Core)
		return MapNode(m_land.m_columns.blocks().stone);
	// the cell's layer is its height in its column
	return m_land.m_columns.node(columnAt(m_land.m_atlas.place(*cell)), cell->k);
}

const Column *AtlasLand::Reader::column(planet::Atlas::Node n)
{
	const std::optional<int> body = m_land.m_atlas.bodyAt(n);
	if (body && *body != m_land.m_columns.body())
		throw std::logic_error("terra::AtlasLand: a node of body " + std::to_string(*body) +
				" asked of body " + std::to_string(m_land.m_columns.body()) + "'s land");
	const std::optional<terragen::Place> place = m_land.m_atlas.place(n);
	if (!place)
		return nullptr;
	return &columnAt(*place);
}

const Column &AtlasLand::Reader::columnAt(const terragen::Place &place)
{
	if (!m_column || !(place == m_place)) {
		m_column = m_land.m_columns.atPlace(place);
		m_place = place;
	}
	return *m_column;
}

} // namespace terra
