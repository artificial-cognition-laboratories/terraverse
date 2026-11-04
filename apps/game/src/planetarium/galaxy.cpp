// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/galaxy.h"
#include "planetarium/camera.h"
#include "planetarium/shot.h"
#include "client/shader.h"
#include "filesys.h"
#include "porting.h"
#include "exceptions.h"
#include "log.h"
#include <IMeshBuffer.h>
#include <IVideoDriver.h>
#include <CMeshBuffer.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <cmath>
#include <future>
#include <memory>

namespace planetarium {

namespace {

constexpr u32 WIDTH = 2048, HEIGHT = 1024; // the sky's picture
constexpr int STARS = 40000;
constexpr int PER_BUFFER = 16000; // (a mesh buffer's indices are 16 bits: 4 vertices a star)
constexpr double FADE_IN = 1.5; // seconds: the picture arrives after the first frames
constexpr u32 REFERENCE_HEIGHT = 1080; // a star's size is given on a screen this high
constexpr u32 GRAIN = 1024; // the unresolved stars' texture, a side
constexpr u32 PICTURE_VERSION = 3; // bump with a change to milkyway:: (the picture's kept on disk)

// where the galaxy lies in the scene: chosen so the band crosses the main menu's picture (planetarium/shot.h)
const milkyway::Way POLE{ -0.6440f, -0.0970f, -0.7589f }, CENTRE{ -0.7616f, -0.0130f, 0.6480f };

std::string kept()
{
	return porting::path_cache + DIR_DELIM + "milkyway_" + std::to_string(PICTURE_VERSION) + "_" + std::to_string(WIDTH) + ".bin";
}

std::shared_ptr<const milkyway::Picture> paint()
{
	auto out = std::make_shared<milkyway::Picture>();
	out->width = WIDTH;
	out->height = HEIGHT;
	out->rgb.resize((size_t)WIDTH * HEIGHT * 3);
	{
		std::ifstream in(kept(), std::ios::binary);
		// (any other size: an interrupted write. Made again)
		if (in && in.read(reinterpret_cast<char *>(out->rgb.data()), out->rgb.size()) && in.peek() == EOF)
			return out;
	}
	const auto from = std::chrono::steady_clock::now();
	milkyway::Opts opts;
	opts.width = WIDTH;
	opts.height = HEIGHT;
	opts.exposure = 0.45f;
	opts.pole = POLE;
	opts.centre = CENTRE;
	*out = milkyway::bake(opts);
	infostream << "galaxy: baked in " << std::chrono::duration<double>(std::chrono::steady_clock::now() - from).count() << " s" << std::endl;
	fs::CreateAllDirs(porting::path_cache);
	std::ofstream to(kept(), std::ios::binary);
	to.write(reinterpret_cast<const char *>(out->rgb.data()), out->rgb.size());
	if (!to)
		warningstream << "galaxy: couldn't keep the picture at " << kept() << std::endl; // it's only made again
	return out;
}

// once a process: every Planetarium and sky shares it
std::shared_future<std::shared_ptr<const milkyway::Picture>> &picture()
{
	static std::shared_future<std::shared_ptr<const milkyway::Picture>> making = std::async(std::launch::async, paint).share();
	return making;
}

} // namespace

struct Galaxy::SkyUniforms : public IShaderUniformSetterRC
{
	float fade = 0, grain = 1;
	CachedPixelShaderSetting<float> m_fade{ "galaxyFade" }, m_grain{ "galaxyGrain" };
	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_fade.set(&fade, s);
		m_grain.set(&grain, s);
	}
};

struct Galaxy::StarUniforms : public IShaderUniformSetterRC
{
	float pixel[2]{}; // one reference pixel, in clip space
	float fade = 0;
	CachedVertexShaderSetting<float, 2> m_pixel{ "starPixel" };
	CachedPixelShaderSetting<float> m_fade{ "starFade" };
	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_pixel.set(pixel, s);
		m_fade.set(&fade, s);
	}
};

Galaxy::Galaxy(IShaderSource *ssrc, scene::IMeshBuffer *sphere) :
	m_skyUniforms(make_irr<SkyUniforms>()), m_starUniforms(make_irr<StarUniforms>()), m_sphere(sphere)
{
	picture(); // set going now: it's seconds of work the first frames shouldn't wait on

	m_skyMaterial.MaterialType = ssrc->getShaderInfo(ssrc->getShader("galaxy_sky_shader", ShaderConstants{}, video::EMT_SOLID, m_skyUniforms.get())).material;
	m_skyMaterial.FogEnable = false;
	m_skyMaterial.BackfaceCulling = false;
	m_skyMaterial.ZBuffer = video::ECFN_DISABLED; // behind everything: drawn first
	m_skyMaterial.ZWriteEnable = video::EZW_OFF;
	m_skyMaterial.ColorMask = video::ECP_RGB;
	m_skyMaterial.UseMipMaps = false;
	m_skyMaterial.TextureLayers[0].MinFilter = video::ETMINF_LINEAR_MIPMAP_NEAREST;
	m_skyMaterial.TextureLayers[0].MagFilter = video::ETMAGF_LINEAR;
	// (filtered and mipped: a speck smaller than a pixel fades, it doesn't flicker as the view turns)
	m_skyMaterial.TextureLayers[1].MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
	m_skyMaterial.TextureLayers[1].MagFilter = video::ETMAGF_LINEAR;
	m_skyMaterial.TextureLayers[1].TextureWrapU = video::ETC_REPEAT;
	m_skyMaterial.TextureLayers[1].TextureWrapV = video::ETC_REPEAT;

	m_starMaterial.MaterialType = ssrc->getShaderInfo(ssrc->getShader("galaxy_stars_shader", ShaderConstants{}, video::EMT_TRANSPARENT_VERTEX_ALPHA, m_starUniforms.get())).material;
	m_starMaterial.FogEnable = false;
	m_starMaterial.BackfaceCulling = false;
	m_starMaterial.ZBuffer = video::ECFN_DISABLED;
	m_starMaterial.ZWriteEnable = video::EZW_OFF;
	m_starMaterial.ColorMask = video::ECP_RGB;

}

struct Galaxy::Shared
{
	video::IVideoDriver *driver;
	video::ITexture *texture = nullptr, *grain = nullptr;
	std::vector<irr_ptr<scene::SMeshBuffer>> stars; // they don't change
	std::chrono::steady_clock::time_point shownAt;
	void makeGrain();
	void makeStars(const milkyway::Picture &sky, const video::SMaterial &material);
	~Shared()
	{
		if (texture)
			driver->removeTexture(texture);
		if (grain)
			driver->removeTexture(grain);
	}
};

void Galaxy::Shared::makeGrain()
{
	// the stars too many to draw: most specks faint, a few bright; stored at a
	// third, so 1 (its mean) is about 85. Its mips average out to smooth glow.
	std::vector<u32> pixels((size_t)GRAIN * GRAIN);
	uint32_t seed = 0x6d2b79f5;
	for (u32 &pixel : pixels) {
		seed = seed * 1664525u + 1013904223u;
		const double u = std::max((seed >> 8) / 16777216.0, 0.002);
		const u32 v = (u32)(std::min(std::pow(u, -0.32) * 0.72, 3.0) / 3 * 255);
		pixel = video::SColor(255, v, v, v).color;
	}
	video::IImage *image = driver->createImageFromData(video::ECF_A8R8G8B8, core::dimension2du(GRAIN, GRAIN), pixels.data(), false, false);
	const bool mips = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, true);
	static u32 count = 0;
	grain = driver->addTexture(("galaxy_grain_" + std::to_string(count++)).c_str(), image);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mips);
	image->drop();
	if (!grain)
		throw BaseException("galaxy: no grain texture");
}

void Galaxy::Shared::makeStars(const milkyway::Picture &sky, const video::SMaterial &material)
{
	// the same sky every time; most faint, a few bright (as magnitudes go).
	// Where the picture's bright there are more: the band's glow comes apart
	// into them, and dust that hides its light hides its stars. Coloured by how hot.
	uint32_t seed = 0x9e3779b9;
	const auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0; };
	static const video::SColor HUES[] = { { 255, 165, 192, 255 }, { 255, 222, 232, 255 }, { 255, 255, 248, 238 },
		{ 255, 255, 228, 180 }, { 255, 255, 196, 150 } };
	for (int n = 0; n < STARS;) {
		const double y = next() * 2 - 1, a = next() * 2 * M_PI, r = std::sqrt(1 - y * y);
		const u32 px = std::min(sky.width - 1, (u32)(a / (2 * M_PI) * sky.width));
		const u32 py = std::min(sky.height - 1, (u32)((0.5 - std::asin(y) / M_PI) * sky.height));
		const u8 *at = &sky.rgb[((size_t)py * sky.width + px) * 3];
		const double light = (at[0] + at[1] + at[2]) / (3.0 * 255);
		if (next() > 0.06 + light * light * 3)
			continue;
		if (n % PER_BUFFER == 0) {
			stars.push_back(make_irr<scene::SMeshBuffer>());
			stars.back()->getMaterial() = material;
			stars.back()->setHardwareMappingHint(scene::EHM_STATIC);
		}
		scene::SMeshBuffer &buffer = *stars.back();
		const double bright = std::pow(next(), 16); // the few that stand out
		const f32 radius = (f32)(0.9 + 3.6 * bright);
		video::SColor c = HUES[std::min(4, (int)(next() * 5))];
		c.setAlpha((u32)(255 * (0.14 + 0.86 * std::pow(bright, 0.35))));
		const v3f way((f32)(r * std::cos(a)), (f32)y, (f32)(r * std::sin(a)));
		auto &v = buffer.Vertices->Data;
		auto &ix = buffer.Indices->Data;
		const u16 first = (u16)v.size();
		for (const v2f &corner : { v2f(-1, -1), v2f(1, -1), v2f(1, 1), v2f(-1, 1) })
			v.emplace_back(way, v3f(radius, 0, 0), c, corner);
		for (u16 k : { 0, 1, 2, 0, 2, 3 })
			ix.push_back(first + k);
		n++;
	}
	for (const irr_ptr<scene::SMeshBuffer> &buffer : stars)
		buffer->recalculateBoundingBox();
}

Galaxy::~Galaxy()
{
}

void Galaxy::draw(video::IVideoDriver *driver, float aspect, float fov, v2f shift, float level)
{
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, 0.1f, 10.f, shift, fov));
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	static std::weak_ptr<Shared> alive;
	if (!m_shared)
		m_shared = alive.lock();
	auto &making = picture();
	if (!m_shared && making.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		const std::shared_ptr<const milkyway::Picture> made = making.get();
		std::vector<u32> pixels((size_t)WIDTH * HEIGHT);
		for (size_t i = 0; i < pixels.size(); i++)
			pixels[i] = video::SColor(255, made->rgb[i * 3], made->rgb[i * 3 + 1], made->rgb[i * 3 + 2]).color;
		video::IImage *image = driver->createImageFromData(video::ECF_A8R8G8B8, core::dimension2du(WIDTH, HEIGHT), pixels.data(), false, false);
		auto shared = std::make_shared<Shared>();
		shared->driver = driver;
		shared->texture = driver->addTexture("galaxy", image);
		image->drop();
		if (!shared->texture)
			throw BaseException("galaxy: no texture");
		shared->makeGrain();
		shared->makeStars(*made, m_starMaterial);
		shared->shownAt = std::chrono::steady_clock::now();
		m_shared = shared;
		alive = shared;
	}
	if (!m_shared)
		return;
	const double since = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_shared->shownAt).count();
	const float fade = (float)std::min(1.0, since / FADE_IN);
	m_skyUniforms->fade = fade * level;
	// the specks about as big on screen through any lens: two fixed scales, the
	// wide lens's and the long one's, crossfaded. One scale changing with the lens would slide them across the sky
	const float wide = std::tan(OrbitCamera::FOV / 2), longer = std::tan(HOME_SHOT.fov / 2);
	m_skyUniforms->grain = std::clamp((wide / std::tan(fov / 2) - 1) / (wide / longer - 1), 0.0f, 1.0f);
	m_skyMaterial.TextureLayers[0].Texture = m_shared->texture;
	m_skyMaterial.TextureLayers[1].Texture = m_shared->grain;
	driver->setMaterial(m_skyMaterial);
	driver->drawMeshBuffer(m_sphere);

	// (a 1080p screen's pixels, kept in share on others, so a picture looks the same)
	const f32 pixel = 2.f / REFERENCE_HEIGHT;
	m_starUniforms->pixel[0] = pixel / aspect;
	m_starUniforms->pixel[1] = pixel;
	m_starUniforms->fade = fade;
	driver->setMaterial(m_starMaterial);
	for (const irr_ptr<scene::SMeshBuffer> &buffer : m_shared->stars)
		driver->drawMeshBuffer(buffer.get());
}

} // namespace planetarium
