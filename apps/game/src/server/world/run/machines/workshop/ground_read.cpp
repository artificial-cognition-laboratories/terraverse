// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/machines/workshop/ground_read.h"
#include "src/server/world/run/ground/ground.h"
#include "src/shared/map/map.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/game/content/nodedef.h"
#include "library/world/planet/atlas.h"
#include <array>

namespace workshop {

static_assert(GroundSnapshot::MARGIN == 1 + ground::REACH + 2, "the margin is what smoothing reaches");

int GroundSnapshot::index(v3s16 p) const
{
	const v3s16 r = p - m_block * MAP_BLOCKSIZE + v3s16(MARGIN);
	if (r.X < 0 || r.Y < 0 || r.Z < 0 || r.X >= SIDE || r.Y >= SIDE || r.Z >= SIDE)
		return -1;
	return (r.Z * SIDE + r.Y) * SIDE + r.X;
}

MapNode GroundSnapshot::node(v3s16 p) const
{
	const int i = index(p);
	return i < 0 ? MapNode(CONTENT_IGNORE) : m_nodes[i];
}

const shape::Shape *GroundSnapshot::shape(v3s16 p) const
{
	const int i = index(p);
	if (i < 0)
		return nullptr;
	auto it = m_shapes.find(i);
	return it == m_shapes.end() ? nullptr : &it->second;
}

std::optional<GroundSnapshot> GroundSnapshot::take(Map &map, v3s16 block)
{
	MapBlock *own = map.getBlockNoCreateNoEx(block);
	if (!own)
		return std::nullopt;
	GroundSnapshot s(block);
	s.m_nodes.assign(SIDE * SIDE * SIDE, MapNode(CONTENT_IGNORE));
	// the blocks the margin reaches into, a block at a time: one lookup each
	for (s16 bz = block.Z - 1; bz <= block.Z + 1; bz++)
		for (s16 by = block.Y - 1; by <= block.Y + 1; by++)
			for (s16 bx = block.X - 1; bx <= block.X + 1; bx++) {
				const v3s16 bp(bx, by, bz);
				MapBlock *b = bp == block ? own : map.getBlockNoCreateNoEx(bp);
				if (!b)
					continue; // unknown (CONTENT_IGNORE), as Map::getNode says
				const v3s16 base = bp * MAP_BLOCKSIZE;
				for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
					for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
						for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
							const int i = s.index(base + v3s16(x, y, z));
							if (i < 0)
								continue;
							const MapNode n = b->getNodeNoCheck(v3s16(x, y, z));
							if (bp == block && n.getContent() == CONTENT_IGNORE)
								return std::nullopt; // not all of it known yet
							s.m_nodes[i] = n;
						}
				b->m_shapes.each([&](v3s16 rel, const shape::Shape &shape) {
					if (const int i = s.index(base + rel); i >= 0)
						s.m_shapes.emplace(i, shape);
				});
			}
	return s;
}

std::vector<aabb3f> GroundSnapshot::boxes(const NodeDefManager *ndef, const planet::Atlas *atlas, Seam seam) const
{
	std::vector<aabb3f> boxes;
	const content_t open = seam == Seam::Open ? ndef->getId("core:seam") : CONTENT_IGNORE;
	std::vector<aabb3f> nb;
	const v3s16 base = m_block * MAP_BLOCKSIZE;
	// each node's ground as smoothing sees it, and whether the atlas cuts it, found once
	std::vector<MapNode> seen(SIDE * SIDE * SIDE);
	std::vector<s8> seen_known(SIDE * SIDE * SIDE, -1); // -1 unread; else whether it's a cube (atlas's cut)
	auto cell = [&](v3s16 q) -> int {
		const int i = index(q);
		if (i >= 0 && seen_known[i] < 0) {
			seen[i] = ground::seen(m_nodes[i], shape(q));
			seen_known[i] = atlas && atlas->cut({ q.X, q.Y, q.Z }).has_value();
		}
		return i;
	};
	auto get = [&](v3s16 q) {
		const int i = cell(q);
		return i < 0 ? ground::seen(MapNode(CONTENT_IGNORE), nullptr) : seen[i];
	};
	auto cube = [&](v3s16 q) {
		const int i = cell(q);
		return i < 0 ? atlas && atlas->cut({ q.X, q.Y, q.Z }).has_value() : seen_known[i] == 1;
	};
	const ground::Ground smoothing(*ndef);
	std::unordered_map<v3s16, std::optional<ground::Top>> tops;
	// as Map::groundTop, from what's held here
	auto top = [&](v3s16 p) -> const std::optional<ground::Top> & {
		auto it = tops.find(p);
		if (it == tops.end()) {
			std::optional<ground::Top> t;
			if (!shape(p) && !shape(p + v3s16(0, 1, 0)))
				t = smoothing.top(get, cube, p);
			it = tops.emplace(p, t).first;
		}
		return it->second;
	};
	// a plain whole block: inside the ground, it's left out. Asked of each
	// node and its six neighbours, so each is weighed once (sprint 85)
	constexpr int SPAN = MAP_BLOCKSIZE + 2;
	std::array<s8, SPAN * SPAN * SPAN> known;
	known.fill(-1);
	auto whole = [&](v3s16 rel) {
		s8 &k = known[((rel.Z + 1) * SPAN + rel.Y + 1) * SPAN + rel.X + 1];
		if (k >= 0)
			return k == 1;
		const v3s16 p = base + rel;
		const ContentFeatures &f = ndef->get(node(p));
		bool is = f.walkable && f.drawtype == NDT_NORMAL && !shape(p);
		// loose ground is smoothed only at its surface: under something
		// walkable (not cut) it's whole, no columns to read
		if (is && f.loose) {
			const v3s16 up = p + v3s16(0, 1, 0);
			const ContentFeatures &over = ndef->get(node(up));
			if (!(over.walkable && !shape(up)))
				is = !top(p);
		}
		k = is ? 1 : 0;
		return is;
	};
	// a connected node box's neighbours (MapNode::getNeighbors, from what's held)
	auto neighbours = [&](MapNode n, v3s16 p) {
		const ContentFeatures &f = ndef->get(n);
		u8 bits = 0;
		if (f.drawtype != NDT_NODEBOX || f.node_box.type != NODEBOX_CONNECTED)
			return bits;
		static const std::array<std::pair<v3s16, u8>, 6> WAYS = { {
			{ v3s16(0, 1, 0), 1 }, { v3s16(0, -1, 0), 2 }, { v3s16(0, 0, -1), 4 },
			{ v3s16(-1, 0, 0), 8 }, { v3s16(0, 0, 1), 16 }, { v3s16(1, 0, 0), 32 },
		} };
		for (const auto &[d, bit] : WAYS)
			if (ndef->nodeboxConnects(n, node(p + d), bit))
				bits |= bit;
		return bits;
	};
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
		for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
			for (s16 z = 0; z < MAP_BLOCKSIZE; z++) {
				const v3s16 rel(x, y, z);
				const v3s16 p = base + rel;
				const MapNode n = node(p);
				if (!ndef->get(n).walkable || (open != CONTENT_IGNORE && n.getContent() == open))
					continue;
				const bool inner = x > 0 && y > 0 && z > 0 && x < MAP_BLOCKSIZE - 1 && y < MAP_BLOCKSIZE - 1
						&& z < MAP_BLOCKSIZE - 1;
				if (inner && whole(rel) && whole(rel + v3s16(1, 0, 0)) && whole(rel - v3s16(1, 0, 0))
						&& whole(rel + v3s16(0, 1, 0)) && whole(rel - v3s16(0, 1, 0))
						&& whole(rel + v3s16(0, 0, 1)) && whole(rel - v3s16(0, 0, 1)))
					continue;
				nb.clear();
				if (whole(rel))
					nb.push_back(aabb3f(v3f(-0.5f * BS), v3f(0.5f * BS))); // buried, at the block's edge: no columns read
				else if (const shape::Shape *cut = shape(p))
					cut->boxes(nb);
				else if (const std::optional<ground::Top> &t = top(p))
					t->boxes(nb);
				else
					n.getCollisionBoxes(ndef, &nb, neighbours(n, p));
				for (aabb3f box : nb) {
					box.MinEdge = box.MinEdge / BS + v3f::from(rel);
					box.MaxEdge = box.MaxEdge / BS + v3f::from(rel);
					if (box.getVolume() > 0)
						boxes.push_back(box);
				}
			}
	return boxes;
}

} // namespace workshop
