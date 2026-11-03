// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/rim.h"
#include "client/shader.h"
#include "exceptions.h"
#include <IMeshBuffer.h>
#include <IVideoDriver.h>
#include <chrono>
#include <cmath>
#include <string>

namespace planetarium {

// As the sky (planetarium/air.cpp).
static constexpr double EXPOSURE = 14;
// How much of the air's depth the rim shows: past this the real air's all but gone.
static constexpr double REACH = 0.5;

struct Rim::Uniforms : public IShaderUniformSetterRC
{
	float centre[3]{}, sun[3]{}, ground = 1, share = (float)SHARE;
	CachedPixelShaderSetting<float, 3> m_centre{"rimCentre"}, m_sun{"rimSun"};
	CachedPixelShaderSetting<float> m_ground{"rimGround"}, m_share{"rimShare"};

	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_centre.set(centre, s);
		m_sun.set(sun, s);
		m_ground.set(&ground, s);
		m_share.set(&share, s);
	}
};

Rim::Rim(IShaderSource *ssrc, const terragen::System &system) : m_system(system), m_uniforms(make_irr<Uniforms>())
{
	const u32 shader = ssrc->getShader("air_rim_shader", ShaderConstants{}, video::EMT_TRANSPARENT_ALPHA_CHANNEL, m_uniforms.get());
	m_material.MaterialType = ssrc->getShaderInfo(shader).material;
	m_material.FogEnable = false;
	m_material.ZBuffer = video::ECFN_DISABLED; // drawn straight after its body, far to near
	m_material.ZWriteEnable = video::EZW_OFF;
	m_material.ColorMask = video::ECP_RGB;
	m_material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
	m_material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	m_material.TextureLayers[0].MinFilter = video::ETMINF_LINEAR_MIPMAP_NEAREST;
	m_material.TextureLayers[0].MagFilter = video::ETMAGF_LINEAR;
	m_material.UseMipMaps = false;
}

Rim::~Rim()
{
	if (m_driver)
		for (auto &[body, part] : m_parts)
			if (part.texture)
				m_driver->removeTexture(part.texture);
}

void Rim::draw(video::IVideoDriver *driver, size_t body, const v3f &at, float ground, const v3f &sun, scene::IMeshBuffer *mesh)
{
	m_driver = driver;
	auto found = m_parts.find(body);
	if (found == m_parts.end()) {
		Part fresh;
		const air::Medium medium = air::mediumOf(m_system.cards()[body], m_system.radius(body), m_system.spec().star.type);
		fresh.none = medium.none();
		if (!fresh.none)
			fresh.making = std::async(std::launch::async, [medium]() {
				return air::rimImage(air::Tables(medium), REACH, EXPOSURE * std::sqrt(medium.sunlight));
			});
		found = m_parts.emplace(body, std::move(fresh)).first;
	}
	Part &part = found->second;
	if (part.none)
		return;
	if (part.making.valid() && part.making.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		const air::RimImage image = part.making.get();
		video::IImage *blank = driver->createImage(video::ECF_A8R8G8B8, { air::RimImage::W, air::RimImage::H });
		static u32 made = 0; // (a name of its own, per layer that's alive)
		part.texture = driver->addTexture(("air_rim_" + std::to_string(made++)).c_str(), blank);
		blank->drop();
		if (!part.texture)
			throw BaseException("air: no rim texture");
		auto *out = static_cast<u8 *>(part.texture->lock(video::ETLM_WRITE_ONLY));
		if (!out)
			throw BaseException("air: the rim texture wouldn't lock");
		const u32 pitch = part.texture->getPitch();
		for (int y = 0; y < air::RimImage::H; y++)
			for (int x = 0; x < air::RimImage::W; x++) {
				const u8 *in = &image.rgb[((size_t)y * air::RimImage::W + x) * 3];
				u8 *to = out + y * pitch + x * 4;
				to[0] = in[2]; // (A8R8G8B8 is BGRA in memory)
				to[1] = in[1];
				to[2] = in[0];
				to[3] = 255;
			}
		part.texture->unlock();
	}
	const float shell = ground * (float)(1 + SHARE);
	if (!part.texture || at.getLength() <= shell)
		return; // from inside the shell, the sky's the air
	Uniforms &u = *m_uniforms;
	for (int c = 0; c < 3; c++) {
		u.centre[c] = c == 0 ? at.X : c == 1 ? at.Y : at.Z;
		u.sun[c] = c == 0 ? sun.X : c == 1 ? sun.Y : sun.Z;
	}
	u.ground = ground;
	m_material.TextureLayers[0].Texture = part.texture;
	driver->setMaterial(m_material);
	core::matrix4 world;
	world.setScale(shell);
	world.setTranslation(at);
	driver->setTransform(video::ETS_WORLD, world);
	driver->drawMeshBuffer(mesh);
}

} // namespace planetarium
