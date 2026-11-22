// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/physics/skeleton.h"
#include "src/shared/base/irrlichttypes_bloated.h"
#include <S3DVertex.h>
#include <matrix4.h>
#include <vector>

namespace video { class IVideoDriver; }

/*
	A figure drawn without a scene graph: its boxes (physics::eachBoneBox) as they stand, each face
	shaded by which way it looks, as the wiki's blocks are. What the wiki's scenes and the inventory's
	figure draw; the game's own figure (client/world/player/figure) is the same boxes as scene nodes.
*/
namespace figure_mesh {

struct Mesh
{
	std::vector<video::S3DVertex> vertices;
	std::vector<u16> indices;
};

// Posed as `bones`, placed by `root` (its feet at the origin, +z ahead).
Mesh build(const physics::Bones &bones, const core::matrix4 &root);
// Solid, unfogged, into whatever view and projection the driver holds.
void draw(video::IVideoDriver *driver, const Mesh &mesh);

} // namespace figure_mesh
