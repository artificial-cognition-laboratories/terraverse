// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/palette.h"
#include "far/terrain.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include "planet/atlas.h"
#include <CMeshBuffer.h>
#include <SMaterial.h>
#include <memory>

namespace video { class ITexture; class IVideoDriver; }

namespace planetarium {

/*
	The tiled body from afar (sprint 114): a sphere under a picture of its
	land, each texel what the column at its middle shows from afar (as a
	tile's cell there would: far::Generator::seenAt), mipmapped. Once the
	body's so small that even its coarsest tiles' cells are under a pixel,
	each pixel showed one column's colour of the many it covered, snow beside
	sea beside forest, a grain; the texture's mipmaps show what they average.

	Its picture's made on threads of its own as it's made, and once made it
	draws. Laid out as the world model's (u east from longitude −π, v south
	from the north pole: terragen::World::place), in the body's own frame.
*/
class Globe
{
public:
	// `threads`: how many make its picture (what it shares the machine with is the tiles')
	Globe(const far::Horizon &horizon, const planet::Atlas &atlas, int body, const far::Palette &palette, unsigned threads);
	~Globe();
	Globe(const Globe &) = delete;
	Globe &operator=(const Globe &) = delete;

	// Its picture's made (and on the GPU, from the first draw). Throws if making it failed.
	bool ready() const;
	// The unit sphere, `world` placing it (scaled to the body's sea, BS), the
	// material's colours its picture's (a SystemLayer::lit material, textured).
	// Only once ready().
	void draw(video::IVideoDriver *driver, const video::SMaterial &material, const core::matrix4 &world);

private:
	struct Making;
	std::shared_ptr<Making> m_making; // (held by its threads too: dropped while making, they stop)
	irr_ptr<scene::SMeshBuffer> m_sphere;
	video::ITexture *m_texture = nullptr; // the driver's, once made
	video::IVideoDriver *m_driver = nullptr;
};

} // namespace planetarium
