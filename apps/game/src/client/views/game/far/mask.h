// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/coverage.h"
#include "src/shared/core/irrlichttypes_bloated.h"
#include <cstdint>
#include <map>

namespace video { class ITexture; class IVideoDriver; }
namespace planet { class Atlas; }

namespace far {

/*
	The near world's columns (far/coverage.h) as the far view's shader reads
	them: a small texture per region of the planet the near world reaches
	into, a texel a map block column of that region's grid, saying which
	blocks of it the near world draws. A far pixel in
	them is discarded, so the far land shows exactly where the near world
	doesn't: no circle, no overlap, no gap.

	A texel: which of the 32 blocks from `base` up (round the eye's) the near
	world has, a bit each, block `base` red's lowest bit, then green, blue,
	alpha. A block out of them is the far view's.
*/
class Mask
{
public:
	struct Face
	{
		video::ITexture *texture;
		int64_t i0, j0; // the grid cell that starts texel (0, 0)'s column
		u32 width, height; // texels
		s16 base;          // the block a texel's lowest bit is
	};

	Mask(video::IVideoDriver *driver, const planet::Atlas &atlas, int body);
	~Mask();
	Mask(const Mask &) = delete;
	Mask &operator=(const Mask &) = delete;

	// Made again when the near world's columns have changed, or the eye's moved
	// to another block's height.
	void update(const Coverage &coverage, s16 eye_block);
	// A region's (its grid in terragen::Atlas::grids, a far TileKey's face); none
	// where the near world isn't.
	const Face *face(int face) const;

private:
	void drop();

	video::IVideoDriver *m_driver;
	const planet::Atlas &m_atlas;
	const int m_body;
	uint64_t m_generation = UINT64_MAX;
	s16 m_eye_block = 0;
	std::map<int, Face> m_faces;
};

} // namespace far
