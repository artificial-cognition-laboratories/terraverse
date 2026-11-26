// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/faces.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <CMeshBuffer.h>

class ClientMap;
class NodeDefManager;
namespace video { class IVideoDriver; }

namespace liquids {

class Grid;

/*
	Liquid drawn flat, a mesh per map block (sprint 81): each node's fill as a
	surface over its open space at the height that much liquid stands at, and
	sides where it stands above what's beside it; and the sea's full blocks
	where they can be seen, so the sea meets the shore's fills. A changed fill
	glides to its new level. What the grid draws, it leaves.
*/
class Flat
{
public:
	struct Opts
	{
		const NodeDefManager &ndef;
		const Grid &grid;
	};
	explicit Flat(Opts o);
	~Flat();
	Flat(const Flat &) = delete;
	Flat &operator=(const Flat &) = delete;

	// A frame's start: the blocks whose fills changed, and whose nodes did (or
	// that arrived), taken; levels glide; blocks unloaded are let go. Whether
	// the day's light changed (each tile's drawn again by it).
	bool begin(const std::unordered_set<v3s16> &changed, const std::unordered_set<v3s16> &ground, ClientMap &map,
			const Rivers &rivers, v3f camera, u32 daynight, float dtime);
	// Blocks whose liquid the grid let go of (drawn again at once, else a gap)
	// and took (drawn again too: till the flat mesh lets go of it both draw it)
	void letGo(const std::vector<v3s16> &blocks);
	void taken(const std::vector<v3s16> &blocks);
	// The blocks to draw again, so long a frame at most (the rest wait for the
	// next): how many there were
	size_t rebuild(ClientMap &map, u32 daynight, bool hurrying);
	void render(video::IVideoDriver *driver, v3s16 offset);
	// Map blocks it has liquid of
	size_t blocks() const { return m_blocks.size(); }

private:
	struct Block
	{
		std::unordered_map<u16, Shown> nodes; // by position in the block
		scene::SMeshBuffer *mesh = nullptr;
		bool gliding = false;
		float rise = 0; // its river's, as it was drawn (nodes)
	};

	void take(v3s16 blockpos, ClientMap &map, bool ground);
	void seaFaces(v3s16 p, Shown &s, ClientMap &map);
	void build(v3s16 blockpos, Block &b, ClientMap &map, u32 daynight);

	const NodeDefManager &m_ndef;
	const Grid &m_grid;
	Faces m_faces;
	std::unordered_map<v3s16, Block> m_blocks;
	std::unordered_set<v3s16> m_rebuild; // this frame's
	std::unordered_set<v3s16> m_relit;   // to draw again by the day's light, a few a frame
	std::unordered_set<v3s16> m_reflat;  // to draw again: the grid took their liquid, or the last frame ran out of time
	std::unordered_set<v3s16> m_unowned; // ... the grid let go of it: drawn again at once (else a gap)
	v3f m_camera;
	const Rivers *m_rivers = nullptr; // this frame's: a river's top stands at its block's rise
	// how far the map's blocks are drawn (nodes): past it the far view draws
	// the sea, so the flat sea stops where they do
	float m_range = 0;
	float m_relight = 0; // s until the light's looked at again
	u32 m_daynight = 0;  // the day's light it was last drawn by
};

} // namespace liquids
