// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "flora/flora.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <memory>
#include <vector>

/*
	A small plant's faces (sprig::Sprig), as the client draws it: its
	voxels' open faces, merged into rectangles a plane at a time, each its
	part's colour. In blocks, from the middle of its foot node (x, z) and
	that node's floor (y), before it's turned. Grown once per plant and kept:
	every sprig of that plant, in every mesh, draws the same faces.

	Further off, coarser: `grain` voxels to a block at most (a voxel's kept
	if any of the plant's is in it, as the part most of them are), so a
	plant's mesh shrinks with distance as cut ground's does.
*/
namespace sprig {

struct Face
{
	v3f corner[4];
	v3s16 normal;
	flora::Part part;
	u32 rgb;     // 0xRRGGBB
};

std::shared_ptr<const std::vector<Face>> facesOf(u32 forest, u32 id, s32 grain = 16);

} // namespace sprig
