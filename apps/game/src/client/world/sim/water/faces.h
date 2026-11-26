// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/parts.h"
#include <CMeshBuffer.h>
#include <unordered_map>

class ClientMap;
class NodeDefManager;

namespace liquids {

class Grid;

// A node's liquid as the flat drawing has it
struct Shown
{
	float level, target; // of its open space; a full block's: its top, a node high (a river's top node: less)
	u8 r, g, b;
	bool sea = false;  // a full block of liquid (the sea, a river), not a fill
	bool held = false; // ... a river's or lake's: its top slopes through its corners
	// the sea's, looked at when its ground changes (not each time it's drawn):
	bool top = false;                // its top seen
	s8 from[4] = { -1, -1, -1, -1 }; // its sides: the sixteenth they stand from (−1: none)
};

/*
	A map block's flat liquid, as faces into its mesh: the sea's tops (alike
	ones in runs) and the sides where it stands over what's beside; a river's
	top sloping through its corners (held::corners, as the grid's columns
	stand); and fills, a top over the open sixteenths they stand in. What the
	grid draws, it leaves.
*/
class Faces
{
public:
	struct Opts
	{
		const NodeDefManager &ndef;
		const Grid &grid;
	};
	explicit Faces(Opts o);

	struct Block
	{
		v3s16 blockpos;
		const std::unordered_map<u16, Shown> &nodes; // by keys::of
		scene::SMeshBuffer *mesh;                    // emptied: faces are added
		const Rivers &rivers;
		u32 daynight;
	};
	void build(const Block &b, ClientMap &map);

private:
	void sea(const Block &b, ClientMap &map);
	void riverTop(const Block &b, ClientMap &map, v3s16 p, v3f o, const Shown &s, video::SColor colour, video::SColor top);
	void fills(const Block &b, ClientMap &map);

	const NodeDefManager &m_ndef;
	const Grid &m_grid;
};

} // namespace liquids
