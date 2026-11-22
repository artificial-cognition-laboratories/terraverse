// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/pixels.h"
#include "src/client/render/shaders/shader.h"
#include "src/shared/base/exceptions.h"
#include <IVideoDriver.h>
#include <S3DVertex.h>
#include <algorithm>
#include <cmath>

namespace planetarium {

namespace {

// An art pixel, in screen pixels: the same at any size of window, so a big
// screen shows more of them, never bigger ones
constexpr float CELL = 3;
constexpr float SHADES = 14; // a channel's

} // namespace

struct Pixels::Uniforms : public IShaderUniformSetterRC
{
	float cell = 1, shades = SHADES;
	float size[2]{}, corner[2]{};
	CachedPixelShaderSetting<float> m_cell{ "pixelsCell" };
	CachedPixelShaderSetting<float> m_shades{ "pixelsShades" };
	CachedPixelShaderSetting<float, 2> m_size{ "pixelsSize" };
	CachedPixelShaderSetting<float, 2> m_corner{ "pixelsCorner" };
	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_cell.set(&cell, s);
		m_shades.set(&shades, s);
		m_size.set(size, s);
		m_corner.set(corner, s);
	}
};

Pixels::Pixels() : m_uniforms(make_irr<Uniforms>())
{
}

Pixels::~Pixels()
{
	if (m_target)
		m_driver->removeTexture(m_target);
}

void Pixels::draw(video::IVideoDriver *driver, core::recti view, const std::function<void(core::recti, float cell)> &scene)
{
	if (!m_shaders) {
		m_shaders.reset(createShaderSource());
		m_material.MaterialType = m_shaders->getShaderInfo(m_shaders->getShader("pixels_shader", ShaderConstants{}, video::EMT_SOLID, m_uniforms.get())).material;
		m_material.FogEnable = false;
		m_material.BackfaceCulling = false;
		m_material.ZBuffer = video::ECFN_DISABLED;
		m_material.ZWriteEnable = video::EZW_OFF;
		m_material.UseMipMaps = false;
		m_material.TextureLayers[0].MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST; // (read texel by texel)
		m_material.TextureLayers[0].MagFilter = video::ETMAGF_NEAREST;
		m_material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		m_material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	}
	const float cell = CELL;
	// drawn whole: the shader makes each art pixel from the block under it
	const core::dimension2du size((u32)std::max(1, view.getWidth()), (u32)std::max(1, view.getHeight()));
	if (!m_target || size != m_size) {
		if (m_target)
			driver->removeTexture(m_target);
		m_target = driver->addRenderTargetTexture(size, "planetarium:pixels", video::ECF_A8R8G8B8);
		if (!m_target)
			throw BaseException("planetarium: pixel art needs a render target");
		m_driver = driver;
		m_size = size;
	}

	driver->setRenderTarget(m_target, video::ECBF_COLOR | video::ECBF_DEPTH, video::SColor(255, 0, 0, 0));
	scene(core::recti(0, 0, size.Width, size.Height), cell);
	driver->setRenderTarget(nullptr, 0); // back to the window, as it was: the page draws over it after

	// the picture over the view, read in blocks by the shader (by the window's
	// pixels, from its lower left, as GL counts them: so it's never upside down)
	const core::dimension2du window = driver->getCurrentRenderTargetSize();
	driver->setViewPort(core::recti(0, 0, window.Width, window.Height));
	const auto x = [&](float px) { return px / window.Width * 2 - 1; };
	const auto y = [&](float px) { return 1 - px / window.Height * 2; };
	const float l = x((f32)view.UpperLeftCorner.X), r = x((f32)view.LowerRightCorner.X);
	const float t = y((f32)view.UpperLeftCorner.Y), b = y((f32)view.LowerRightCorner.Y);
	const video::SColor white(255, 255, 255, 255);
	video::S3DVertex v[4] = {
		{ { l, b, 0 }, v3f(), white, v2f(0, 1) },
		{ { r, b, 0 }, v3f(), white, v2f(1, 1) },
		{ { r, t, 0 }, v3f(), white, v2f(1, 0) },
		{ { l, t, 0 }, v3f(), white, v2f(0, 0) },
	};
	const u16 ix[6] = { 0, 1, 2, 0, 2, 3 };
	m_uniforms->cell = cell;
	m_uniforms->size[0] = (f32)size.Width;
	m_uniforms->size[1] = (f32)size.Height;
	m_uniforms->corner[0] = (f32)view.UpperLeftCorner.X;
	m_uniforms->corner[1] = (f32)(window.Height - view.LowerRightCorner.Y);
	m_material.TextureLayers[0].Texture = m_target;
	driver->setMaterial(m_material);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->setTransform(video::ETS_VIEW, core::IdentityMatrix);
	driver->setTransform(video::ETS_PROJECTION, core::IdentityMatrix);
	driver->drawIndexedTriangleList(v, 4, ix, 2);
}

} // namespace planetarium
