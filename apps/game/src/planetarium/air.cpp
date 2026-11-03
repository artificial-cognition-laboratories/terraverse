// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/air.h"
#include "client/shader.h"
#include "exceptions.h"
#include <IVideoDriver.h>
#include <S3DVertex.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace planetarium {

// How far a star moves, or the eye climbs, before its picture is made again.
static constexpr double MOVED = 0.003; // radians: about a day-length's 1/2000
static constexpr double CLIMBED = 0.005; // of the air's depth
// The eye takes a dim star's light partway (as the sheets in scripts/air-check do).
static constexpr double EXPOSURE = 14;
// Fainter than this share of the light, a star's sky doesn't show.
static constexpr double FAINTEST = 0.01;

struct Air::Uniforms : public IShaderUniformSetterRC
{
	float ahead[3]{}, right[3]{}, up[3]{}, way0[3]{}, way1[3]{}, sun0[3]{}, sun1[3]{};
	float dips[2]{};
	float discs[2]{}; // the cosines of the stars' angular radii; 2: not drawn
	float second = 0;
	CachedVertexShaderSetting<float, 3> m_ahead{"airAhead"}, m_right{"airRight"}, m_up{"airUp"};
	CachedPixelShaderSetting<float, 3> m_way0{"airWay0"}, m_way1{"airWay1"}, m_sun0{"airSun0"}, m_sun1{"airSun1"};
	CachedPixelShaderSetting<float, 2> m_discs{"airDiscs"}, m_dips{"airDips"};
	CachedPixelShaderSetting<float> m_second{"airSecond"};

	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_ahead.set(ahead, s);
		m_right.set(right, s);
		m_up.set(up, s);
		m_way0.set(way0, s);
		m_way1.set(way1, s);
		m_sun0.set(sun0, s);
		m_sun1.set(sun1, s);
		m_discs.set(discs, s);
		m_dips.set(dips, s);
		m_second.set(&second, s);
	}
};

Air::Air(IShaderSource *ssrc, const terragen::System &system, size_t body)
{
	const terragen::StarCauses &star = system.spec().star;
	const air::Medium medium = air::mediumOf(system.cards()[body], system.radius(body), star.type);
	if (medium.none())
		return;
	m_sunlight = medium.sunlight;
	m_tints[0] = { 1, 1, 1 };
	if (star.companion.kind != terragen::Pairing::None) {
		const air::Rgb first = air::starColour(star.type), other = air::starColour(star.companion.type);
		for (int c = 0; c < 3; c++)
			m_tints[1][c] = other[c] / first[c];
	}
	m_building = std::async(std::launch::async, [medium]() { return std::make_shared<const air::Tables>(medium); });
	m_uniforms = make_irr<Uniforms>();
	const u32 shader = ssrc->getShader("air_sky_shader", ShaderConstants{}, video::EMT_TRANSPARENT_ALPHA_CHANNEL, m_uniforms.get());
	m_material.MaterialType = ssrc->getShaderInfo(shader).material;
	m_material.BackfaceCulling = false;
	m_material.FogEnable = false;
	m_material.ZBuffer = video::ECFN_DISABLED;
	m_material.ZWriteEnable = video::EZW_OFF;
	m_material.ColorMask = video::ECP_RGB;
	for (int i = 0; i < 2; i++) {
		m_material.TextureLayers[i].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		m_material.TextureLayers[i].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
		m_material.TextureLayers[i].MinFilter = video::ETMINF_LINEAR_MIPMAP_NEAREST;
		m_material.TextureLayers[i].MagFilter = video::ETMAGF_LINEAR;
	}
	m_material.UseMipMaps = false;
}

Air::~Air()
{
	if (!m_driver)
		return;
	for (Part &part : m_parts)
		if (part.texture)
			m_driver->removeTexture(part.texture);
}

void Air::ask(Part &part, double height, double drawn_dip, double real_dip, const Star &star, const air::Rgb &tint)
{
	const double elevation = air::realElevation(std::asin(std::clamp((double)star.way.Y, -1.0, 1.0)), drawn_dip, real_dip);
	const double light = EXPOSURE * std::sqrt(m_sunlight * star.share);
	part.star = star;
	part.shown = star.share >= FAINTEST;
	if (!part.shown || part.making.valid())
		return;
	if (std::abs(elevation - part.elevation) < MOVED && std::abs(height - part.height) < CLIMBED && std::abs(light - part.light) < 0.02 * light)
		return;
	part.elevation = elevation;
	part.height = height;
	part.light = light;
	const std::shared_ptr<const air::Tables> tables = m_tables;
	const double metres = height * (tables->medium().top - tables->medium().radius);
	part.making = std::async(std::launch::async, [tables, metres, elevation, light, tint]() {
		return air::skyImage(*tables, metres, elevation, light, tint);
	});
}

void Air::update(double height, double drawn_dip, const Star &first, const std::optional<Star> &second)
{
	if (m_building.valid() && m_building.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		m_tables = m_building.get(); // (rethrows what went wrong)
	if (!m_tables)
		return;
	const air::Medium &m = m_tables->medium();
	const double real_dip = std::acos(m.radius / (m.radius + height * (m.top - m.radius)));
	m_dips[0] = (float)drawn_dip;
	m_dips[1] = (float)real_dip;
	m_tint = air::sunTint(*m_tables, height * (m.top - m.radius),
			air::realElevation(std::asin(std::clamp((double)first.way.Y, -1.0, 1.0)), drawn_dip, real_dip));
	ask(m_parts[0], height, drawn_dip, real_dip, first, m_tints[0]);
	if (second)
		ask(m_parts[1], height, drawn_dip, real_dip, *second, m_tints[1]);
}

void Air::step(Part &part, video::IVideoDriver *driver)
{
	if (!part.making.valid() || part.making.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return;
	const air::SkyImage image = part.making.get();
	part.horizon = image.horizon;
	part.sun = image.sun;
	if (!part.texture) {
		video::IImage *blank = driver->createImage(video::ECF_A8R8G8B8, { air::SkyImage::W, air::SkyImage::H });
		// (a name of its own: a sky rebuilt on another body must not be handed the old one's)
		static u32 made = 0;
		part.texture = driver->addTexture(("air_sky_" + std::to_string(made++)).c_str(), blank);
		blank->drop();
		if (!part.texture)
			throw BaseException("air: no sky texture");
	}
	auto *out = static_cast<u8 *>(part.texture->lock(video::ETLM_WRITE_ONLY));
	if (!out)
		throw BaseException("air: the sky texture wouldn't lock");
	const u32 pitch = part.texture->getPitch();
	for (int y = 0; y < air::SkyImage::H; y++)
		for (int x = 0; x < air::SkyImage::W; x++) {
			const u8 *in = &image.rgba[((size_t)y * air::SkyImage::W + x) * 4];
			u8 *to = out + y * pitch + x * 4;
			to[0] = in[2]; // (A8R8G8B8 is BGRA in memory)
			to[1] = in[1];
			to[2] = in[0];
			to[3] = in[3];
		}
	part.texture->unlock();
}

std::optional<video::SColor> Air::horizon() const
{
	const Part &part = m_parts[0];
	if (!part.texture)
		return std::nullopt;
	air::Rgb sum = part.shown ? part.horizon : air::Rgb{};
	if (m_parts[1].shown && m_parts[1].texture)
		for (int c = 0; c < 3; c++)
			sum[c] += m_parts[1].horizon[c];
	const auto byte = [](double v) { return (u32)std::lround(std::clamp(v, 0.0, 1.0) * 255); };
	return video::SColor(255, byte(sum[0]), byte(sum[1]), byte(sum[2]));
}

static void copy(float *to, const v3f &v)
{
	to[0] = v.X;
	to[1] = v.Y;
	to[2] = v.Z;
}

void Air::draw(video::IVideoDriver *driver, const View &view)
{
	m_driver = driver;
	if (!m_tables)
		return;
	for (Part &part : m_parts)
		step(part, driver);
	const Part &a = m_parts[0], &b = m_parts[1];
	const bool first = a.texture && a.shown, second = b.texture && b.shown;
	if (!first && !second)
		return;
	Uniforms &u = *m_uniforms;
	copy(u.ahead, view.ahead);
	copy(u.right, view.right * (view.tan_half * view.aspect));
	copy(u.up, view.up * view.tan_half);
	copy(u.way0, a.star.way);
	copy(u.way1, b.star.way);
	for (int c = 0; c < 3; c++) {
		u.sun0[c] = (float)a.sun[c];
		u.sun1[c] = (float)b.sun[c];
	}
	u.discs[0] = first ? a.star.cos_radius : 2;
	u.discs[1] = second ? b.star.cos_radius : 2;
	u.second = second ? 1 : 0;
	u.dips[0] = m_dips[0];
	u.dips[1] = m_dips[1];
	m_material.TextureLayers[0].Texture = first ? a.texture : b.texture;
	m_material.TextureLayers[1].Texture = second ? b.texture : a.texture;
	// (a first star too faint has its picture stand in: the shader adds only the second)
	u.discs[0] = first ? u.discs[0] : 2;
	driver->setMaterial(m_material);
	const core::matrix4 identity;
	driver->setTransform(video::ETS_WORLD, identity);
	driver->setTransform(video::ETS_VIEW, identity);
	driver->setTransform(video::ETS_PROJECTION, identity);
	video::S3DVertex v[4];
	v[0].Pos = v3f(-1, -1, 0);
	v[1].Pos = v3f(1, -1, 0);
	v[2].Pos = v3f(1, 1, 0);
	v[3].Pos = v3f(-1, 1, 0);
	const u16 indices[6] = { 0, 1, 2, 0, 2, 3 };
	driver->drawVertexPrimitiveList(v, 4, indices, 2, video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT);
}

} // namespace planetarium
