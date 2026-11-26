// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/tiles/tiling.h"
#include "library/world/terragen/src/out/atlas.h"
#include <S3DVertex.h>
#include <vector>

namespace video { class IVideoDriver; }

namespace planetarium {

/*
	The atlas layer: where the planet's regions meet, drawn over its land.
	Each segment's west edge, a meridian from cap circle to cap circle, and
	the two cap circles, a little over the sea (the ground hides them where
	it stands higher, as it hides anything behind it).
*/
class SeamLayer
{
public:
	using Vec = far::Tiling::Vec;

	SeamLayer(const terragen::Atlas::Params &atlas, double sea);

	// The lines, drawn from `eye` (the camera's position: blocks from the
	// centre), with the view and projection already set.
	void draw(video::IVideoDriver *driver, const Vec &eye);

private:
	std::vector<Vec> m_points;          // pairs: each line's two ends, on the body
	std::vector<video::S3DVertex> m_vertices; // the same, from the eye this frame
	std::vector<u16> m_indices;
};

} // namespace planetarium
