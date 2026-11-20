// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_aabb3d.h"
#include "src/shared/base/irr_v3d.h"
#include "src/shared/map/map.h"
#include "src/shared/map/voxel/voxel.h"
#include <line3d.h>
#include <memory>
#include <optional>
#include <string>

class IGameDef;

/*
	Piece: blocks on a grid of their own (sprint 47), cut to sixteenths as
	the world's are, not fixed to the world: an entity carries one (its
	`piece` property) and draws it. Its blocks are held in a small Map of
	its own, so the world's shapes, mesher and tools work on it unchanged.
	Local node (0, 0, 0) is at the entity's position; up to SIDE a side.

	An object (piece::Object, sprint 49) is two: its design and what's
	built of it.

	Written by node name (a palette), so the same text is sent, saved
	(the entity's staticdata) and read back whatever ids the nodes have.
*/
namespace piece {

constexpr s16 SIDE = 16;

class Piece
{
public:
	explicit Piece(IGameDef *gamedef);
	~Piece();
	Piece(Piece &&) = default;
	Piece &operator=(Piece &&) = default;

	// Throws SerializationError on text that isn't a piece, or names a
	// node this game doesn't have.
	void read(const std::string &text);
	std::string write() const;

	Map &map() { return *m_map; }
	// Every node moved by `by` (what's moved off its grid throws: the
	// caller sees there's room).
	void shift(v3s16 by);
	// What's at a node of its grid, read only.
	MapNode node(v3s16 p) const { return m_map->getNode(p); }
	const shape::Shape *shape(v3s16 p) const { return m_map->getShape(p); }
	bool empty() const;

	// Its grid's nodes (0..SIDE-1 each way): what the tools may change.
	static VoxelArea area() { return VoxelArea(v3s16(0), v3s16(SIDE - 1)); }

	// Each node that's there (not air), with its shape if it's cut.
	template <class F>
	void each(const F &f) const;

	struct Hit
	{
		v3s16 node;
		v3f point;   // local, BS
		v3s16 normal;
	};
	// The first of its blocks (as cut) a line meets, in local BS.
	std::optional<Hit> raycast(const core::line3d<f32> &line) const;

private:
	IGameDef *m_gamedef;
	std::unique_ptr<Map> m_map;
};

template <class F>
void Piece::each(const F &f) const
{
	for (s16 x = 0; x < SIDE; x++)
		for (s16 y = 0; y < SIDE; y++)
			for (s16 z = 0; z < SIDE; z++) {
				const MapNode n = m_map->getNode({ x, y, z });
				if (n.getContent() != CONTENT_AIR && n.getContent() != CONTENT_IGNORE)
					f(v3s16(x, y, z), n, m_map->getShape({ x, y, z }));
			}
}

} // namespace piece
