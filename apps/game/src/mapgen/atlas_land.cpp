// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapgen/atlas_land.h"
#include <cstring>
#include <map>
#include <string>

namespace terra {

// Slots by place, a lock to each shard of them; a column's copied in and
// out (a few hundred bytes against the tens of µs it took to build)
class AtlasLand::Store
{
public:
	bool find(const terragen::Place &place, uint32_t hash, Column &out)
	{
		Shard &shard = m_shards[hash % SHARDS];
		std::lock_guard<std::mutex> hold(shard.lock);
		const Slot &slot = shard.slots[(hash / SHARDS) % SLOTS];
		if (!slot.column || !(slot.place == place))
			return false;
		out = *slot.column;
		return true;
	}
	void keep(const terragen::Place &place, uint32_t hash, const Column &column)
	{
		Shard &shard = m_shards[hash % SHARDS];
		std::lock_guard<std::mutex> hold(shard.lock);
		Slot &slot = shard.slots[(hash / SHARDS) % SLOTS];
		slot.place = place;
		slot.column = column;
	}

private:
	static constexpr size_t SHARDS = 64, SLOTS = 1 << 12; // 2¹⁸ columns: some 40 chunks' ground, ~65 MB
	struct Slot
	{
		terragen::Place place;
		std::optional<Column> column;
	};
	struct Shard
	{
		std::mutex lock;
		std::vector<Slot> slots = std::vector<Slot>(SLOTS);
	};
	std::unique_ptr<Shard[]> m_shards = std::make_unique<Shard[]>(SHARDS);
};

// One store per land: the same world, body, water and blocks (what a
// column's built from), so every generator of a server shares it
AtlasLand::AtlasLand(const Columns &columns, const planet::Atlas &atlas) : m_columns(columns), m_atlas(atlas)
{
	static std::mutex lock;
	static std::map<std::string, std::weak_ptr<Store>> stores;
	const Blocks &b = columns.blocks();
	std::string key = columns.world().code().text + "|" + std::to_string(columns.body()) + "|" + std::to_string(columns.waterLevel()) + "|";
	key.append(reinterpret_cast<const char *>(&b), sizeof b);
	std::lock_guard<std::mutex> hold(lock);
	std::weak_ptr<Store> &kept = stores[key];
	m_store = kept.lock();
	if (!m_store) {
		m_store = std::make_shared<Store>();
		kept = m_store;
	}
}

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
	// the cell's layer is its height in its column; its column's place is
	// its layer's (kept: the generator asks a column's nodes one after
	// another, and its place cost more than its node)
	if (!m_last || m_last->body != cell->body || m_last->region != cell->region || m_last->index != cell->index ||
			m_last->i != cell->i || m_last->j != cell->j) {
		m_last = *cell;
		m_last_place = m_land.m_atlas.place(*cell);
	}
	return m_land.m_columns.node(columnAt(m_last_place), cell->k);
}

const Column *AtlasLand::Reader::column(planet::Atlas::Node n)
{
	// (a storage column's body and place are the same at every height: kept for the next asked)
	if (!m_last_xz || m_last_xz->first != n.x || m_last_xz->second != n.z) {
		const std::optional<int> body = m_land.m_atlas.bodyAt(n);
		if (body && *body != m_land.m_columns.body())
			throw std::logic_error("terra::AtlasLand: a node of body " + std::to_string(*body) +
					" asked of body " + std::to_string(m_land.m_columns.body()) + "'s land");
		m_last_xz = { n.x, n.z };
		m_last_xz_place = m_land.m_atlas.place(n);
	}
	if (!m_last_xz_place)
		return nullptr;
	return &columnAt(*m_last_xz_place);
}

const Column &AtlasLand::Reader::columnAt(const terragen::Place &place)
{
	if (m_kept.empty())
		m_kept.resize(KEPT);
	// a block's places differ in their low bits; the odd multipliers mix them in
	const uint32_t hash = (uint32_t)place.x * 73856093u ^ (uint32_t)place.y * 19349663u ^ (uint32_t)place.z * 83492791u;
	Kept &kept = m_kept[(hash ^ hash >> 16) & (KEPT - 1)];
	if (!kept.column || !(place == kept.place)) {
		Column c;
		const uint32_t mixed = hash * 0x9E3779B1u ^ hash >> 15;
		if (!m_land.m_store->find(place, mixed, c)) {
			c = m_land.m_columns.atPlace(place);
			m_land.m_store->keep(place, mixed, c);
		}
		kept.column = c;
		kept.place = place;
	}
	return *kept.column;
}

} // namespace terra
