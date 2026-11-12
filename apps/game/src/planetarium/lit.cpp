// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/lit.h"
#include "client/shader.h"

namespace planetarium {

struct Lit::Uniforms : public IShaderUniformSetterRC
{
	Light light{};
	CachedPixelShaderSetting<float, 3> m_centre{"litCentre"}, m_sun{"litSun"}, m_second{"litSecond"};
	CachedPixelShaderSetting<float> m_share{"litShare"}, m_on{"litOn"}, m_penumbra{"litPenumbra"}, m_contour{"litContour"},
		m_textured{"litTextured"};
	CachedPixelShaderSetting<float, 4> m_occ[OCCLUDERS] = { {"litOcc0"}, {"litOcc1"}, {"litOcc2"}, {"litOcc3"} };

	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_centre.set(light.centre, s);
		m_sun.set(light.sun, s);
		m_second.set(light.second, s);
		m_share.set(&light.share, s);
		const float on = light.on ? 1 : 0;
		m_on.set(&on, s);
		m_penumbra.set(&light.penumbra, s);
		m_contour.set(&light.contour, s);
		const float textured = light.textured ? 1 : 0;
		m_textured.set(&textured, s);
		for (int i = 0; i < OCCLUDERS; i++)
			m_occ[i].set(light.occluders[i], s);
	}
};

Lit::Lit(IShaderSource *ssrc) : m_uniforms(make_irr<Uniforms>())
{
	const u32 shader = ssrc->getShader("planet_lit_shader", ShaderConstants{}, video::EMT_SOLID, m_uniforms.get());
	m_material.MaterialType = ssrc->getShaderInfo(shader).material;
	m_material.FogEnable = false;
	m_material.BackfaceCulling = false;
	// colour only: the mesh's vertex alpha marks its layers (far/mesh.cpp:
	// 0 a liquid's top), and a window can show alpha
	m_material.ColorMask = video::ECP_RGB;
}

Lit::~Lit() = default;

const video::SMaterial &Lit::material(const Light &light)
{
	m_uniforms->light = light;
	return m_material;
}

} // namespace planetarium
