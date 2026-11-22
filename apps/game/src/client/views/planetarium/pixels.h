// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_ptr.h"
#include "src/shared/base/irrlichttypes_bloated.h"
#include <functional>
#include <memory>
#include <SMaterial.h>

class IWritableShaderSource;
namespace video { class ITexture; class IVideoDriver; }

/*
	A scene drawn as pixel art, the website's look (apps/site lib/sky.ts):
	drawn whole off screen, then laid over the view in art pixels a few screen
	pixels across, each made from the block under it, its colours stepped
	through an ordered dither (pixels_shader). The
	main menu's picture. Visual only.
*/
namespace planetarium {

class Pixels
{
public:
	Pixels();
	~Pixels();

	// Draws `scene` (given the picture's own view, and an art pixel's screen pixels) into `view` as pixel art.
	void draw(video::IVideoDriver *driver, core::recti view, const std::function<void(core::recti, float cell)> &scene);

private:
	struct Uniforms;
	irr_ptr<Uniforms> m_uniforms;
	std::unique_ptr<IWritableShaderSource> m_shaders; // made on the first frame, with the driver
	video::SMaterial m_material;
	video::ITexture *m_target = nullptr;
	video::IVideoDriver *m_driver = nullptr; // the target's, to take it off
	core::dimension2du m_size; // the target's
};

} // namespace planetarium
