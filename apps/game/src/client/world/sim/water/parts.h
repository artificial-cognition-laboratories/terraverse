// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/types.h"
#include <SColor.h>
#include <array>
#include <climits>
#include <optional>
#include <CMeshBuffer.h>

class ClientMap;
class NodeDefManager;

/*
	What the water layer's leaves share: the sides of a column, the frame
	they're run in, and how liquid is lit and laid into a mesh.
*/
namespace liquids {

constexpr s32 NONE = INT_MIN;         // no ground in reach (a wall to the water), dry, or not the sea
constexpr float NONE_TEXEL = -1e9f;   // in the height texture: none
inline const v3s16 SIDES[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

// What a frame is run against
struct Frame
{
	v3s16 offset;              // the scene's origin (the camera offset, nodes)
	v3f camera;                // nodes
	std::optional<s16> sea;    // the sea's highest node: full water at or under it is the sea's
	const Rivers *rivers;      // the rivers near, as the server last said
	u32 daynight;
};

s64 micros();

// A node's place in its map block as a key, and back
namespace keys {
inline u16 of(v3s16 rel) { return u16((rel.Z * 16 + rel.Y) * 16 + rel.X); }
inline v3s16 rel(u16 k) { return v3s16(k % 16, (k / 16) % 16, k / 256); }
} // namespace keys

// How liquid is lit
namespace light {
// How lit liquid in a node is: by the node or the open air over it, the
// brighter (liquid in a pit cut in sand is in a node that's dark itself)
float shade(ClientMap &map, const NodeDefManager &ndef, v3s16 p, u32 daynight);
// A liquid's colour lit, its tops catching the sky
video::SColor tint(u8 r, u8 g, u8 b, float shade, float by, bool top, u8 alpha);
video::SColor tint(u8 r, u8 g, u8 b, float shade, float by, bool top);
} // namespace light

// Quads into a mesh buffer
namespace quads {
// A quad, its corners in node units, facing `normal`
void into(scene::SMeshBuffer *mesh, v3f a, v3f c1, v3f c2, v3f d, v3f normal, video::SColor colour);
// The buffer, made if there's none, emptied
scene::SMeshBuffer *emptied(scene::SMeshBuffer *&mesh);
} // namespace quads

// A neighbour's liquid height (node units, from its floor), −1: none, 2: walled (solid there)
float besideLevel(ClientMap &map, const NodeDefManager &ndef, v3s16 p);

/*
	Held water's surface as a field (sprint 122), as the ground's is (ground/):
	each node's level, in world sixteenths, gives its corners, and a column
	stands at its place between its node's four. A corner is the mean of the
	levels of the nodes there in this node's cluster (those within a block of
	each other: a fall beside it stays a step), so every node in a cluster
	agrees, and the grid's columns and the flat drawing's slopes meet. Sea and
	lake are level, so they stay flat; a river ramps down its run.
*/
namespace held {
// Levels round a node, [z + 1][x + 1] (NONE: not held water); the node's own at [1][1].
using Around = std::array<std::array<s32, 3>, 3>;
// Its corners, world sixteenths: 0 (−x −z), 1 (+x −z), 2 (−x +z), 3 (+x +z)
std::array<float, 4> corners(const Around &around);
// The field at (fx, fz) across the node, 0..1 each
float at(const std::array<float, 4> &corners, float fx, float fz);
// A node's held surface: the held water's top in the column at p's x, z, at
// p's height or a node over or under (world sixteenths, with its river's
// rise); NONE: none there
s32 levelAt(ClientMap &map, const NodeDefManager &ndef, const Rivers &rivers, v3s16 p);
} // namespace held

} // namespace liquids
