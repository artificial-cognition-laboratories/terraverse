// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/space/galaxy.h"
#include "src/client/views/planetarium/camera.h"
#include "src/client/render/shaders/shader.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/log.h"
#include <IMeshBuffer.h>
#include <IVideoDriver.h>
#include <CMeshBuffer.h>
#include <algorithm>
#include <fstream>
#include <thread>
#include <string>
#include <cmath>
#include <future>
#include <memory>

namespace planetarium {

namespace {

// The sky's picture: about 650 texels a radian, its dust sharp at 80° across
// a 4K screen. Baked once (seconds: 16 on 16 cores) and kept on disk; the first
// time, a small one's baked first and shown till it's there.
struct Size
{
	u32 width, height;
};
constexpr Size FULL{ 4096, 2048 }, QUICK{ 1024, 512 };
constexpr int STARS = 110000; // a wide lens shows about a fifth: nearly all faint, like the band's own
constexpr int PER_BUFFER = 16000; // (a mesh buffer's indices are 16 bits: 4 vertices a star)
constexpr double FADE_IN = 1.5; // seconds: the picture arrives after the first frames
constexpr u32 GRAIN = 1024; // the unresolved stars' texture, a side
constexpr u32 PICTURE_VERSION = 3; // bump with a change to milkyway:: (the picture's kept on disk)

// where the galaxy lies in the scene: chosen so the band crosses the main menu's picture (planetarium/shot.h)
const milkyway::Way POLE{ -0.6440f, -0.0970f, -0.7589f }, CENTRE{ -0.7616f, -0.0130f, 0.6480f };

std::string kept(Size size)
{
	return porting::path_cache + DIR_DELIM + "milkyway_" + std::to_string(PICTURE_VERSION) + "_" + std::to_string(size.width) + ".bin";
}

// the picture this size as kept, if it is (any other length: an interrupted write)
std::shared_ptr<const milkyway::Picture> load(Size size)
{
	auto out = std::make_shared<milkyway::Picture>();
	out->width = size.width;
	out->height = size.height;
	out->rgb.resize((size_t)size.width * size.height * 3);
	std::ifstream in(kept(size), std::ios::binary);
	if (in && in.read(reinterpret_cast<char *>(out->rgb.data()), out->rgb.size()) && in.peek() == EOF)
		return out;
	return nullptr;
}

std::shared_ptr<const milkyway::Picture> paint(Size size)
{
	if (std::shared_ptr<const milkyway::Picture> was = load(size))
		return was;
	const auto from = std::chrono::steady_clock::now();
	milkyway::Opts opts;
	opts.width = size.width;
	opts.height = size.height;
	opts.exposure = 0.45f;
	opts.pole = POLE;
	opts.centre = CENTRE;
	auto out = std::make_shared<milkyway::Picture>(milkyway::bake(opts));
	infostream << "galaxy: baked " << size.width << " wide in " << std::chrono::duration<double>(std::chrono::steady_clock::now() - from).count() << " s" << std::endl;
	fs::CreateAllDirs(porting::path_cache);
	std::ofstream to(kept(size), std::ios::binary);
	to.write(reinterpret_cast<const char *>(out->rgb.data()), out->rgb.size());
	if (!to)
		warningstream << "galaxy: couldn't keep the picture at " << kept(size) << std::endl; // it's only made again
	return out;
}

// A picture, and its pixels as the texture takes them: made on the pictures'
// thread (on the frame's, the full one's eight million were most of a 120 ms
// hitch as it sharpened)
struct Shown
{
	std::shared_ptr<const milkyway::Picture> sky;
	std::vector<u32> pixels; // A8R8G8B8
};
using Showing = std::shared_future<std::shared_ptr<const Shown>>;

std::shared_ptr<const Shown> shown(std::shared_ptr<const milkyway::Picture> sky)
{
	auto out = std::make_shared<Shown>();
	out->pixels.resize((size_t)sky->width * sky->height);
	for (size_t i = 0; i < out->pixels.size(); i++)
		out->pixels[i] = video::SColor(255, sky->rgb[i * 3], sky->rgb[i * 3 + 1], sky->rgb[i * 3 + 2]).color;
	out->sky = std::move(sky);
	return out;
}

// The pictures, once a process (every Planetarium and sky shares them): the
// quick one to show at first, and the full one (the same one, if it's kept)
struct Pictures
{
	Showing quick, full;
};

const Pictures &pictures()
{
	static const Pictures made = [] {
		auto quick = std::make_shared<std::promise<std::shared_ptr<const Shown>>>();
		auto full = std::make_shared<std::promise<std::shared_ptr<const Shown>>>();
		Pictures out{ quick->get_future().share(), full->get_future().share() };
		// one after the other, on one thread: each bake takes every core
		std::thread([quick, full] {
			try {
				if (std::shared_ptr<const milkyway::Picture> kept = load(FULL)) {
					std::shared_ptr<const Shown> one = shown(std::move(kept));
					quick->set_value(one);
					full->set_value(one);
					return;
				}
				quick->set_value(shown(paint(QUICK)));
				full->set_value(shown(paint(FULL)));
			} catch (...) {
				// (whichever's still owed rethrows it where it's read)
				try { quick->set_exception(std::current_exception()); } catch (const std::future_error &) {}
				full->set_exception(std::current_exception());
			}
		}).detach();
		return out;
	}();
	return made;
}

bool ready(const Showing &f)
{
	return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

} // namespace

struct Galaxy::SkyUniforms : public IShaderUniformSetterRC
{
	float fade = 0;
	float specks = 1; // the specks' texture's repeats a unit of the way's (about a texel a pixel)
	CachedPixelShaderSetting<float> m_fade{ "galaxyFade" };
	CachedPixelShaderSetting<float> m_specks{ "galaxySpecks" };
	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_fade.set(&fade, s);
		m_specks.set(&specks, s);
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
	pictures(); // set going now: it's seconds of work the first frames shouldn't wait on

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
	bool sharp = false; // the full picture's in the texture (else the quick one's)
	void picture(const Shown &sky); // the texture, from it
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

void Galaxy::Shared::picture(const Shown &sky)
{
	// (its pixels borrowed, not copied: the image goes before this returns.
	// Irrlicht takes them non-const though it only reads them here: its shape,
	// upstream, left alone; a copy was 32 MB on the frame)
	video::IImage *image = driver->createImageFromData(video::ECF_A8R8G8B8, core::dimension2du(sky.sky->width, sky.sky->height),
			const_cast<u32 *>(sky.pixels.data()), true, false);
	static u32 count = 0;
	video::ITexture *made = driver->addTexture(("galaxy_" + std::to_string(count++)).c_str(), image);
	image->drop();
	if (!made)
		throw BaseException("galaxy: no texture");
	if (texture)
		driver->removeTexture(texture);
	texture = made;
}

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
	// (pale: a star's colour shows only a little against the dark, as the band's do)
	static const video::SColor HUES[] = { { 255, 205, 220, 255 }, { 255, 232, 238, 255 }, { 255, 255, 250, 242 },
		{ 255, 255, 238, 214 }, { 255, 255, 222, 196 } };
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
		// Most just over the band's own specks, a few standing out: a point of
		// light and a faint glow round it, never a disc (the quad is the glow's
		// reach; the shader draws a pixel-sized core in it)
		const double bright = std::pow(next(), 12);
		const f32 radius = (f32)(1.1 + 2.6 * bright);
		video::SColor c = HUES[std::min(4, (int)(next() * 5))];
		c.setAlpha((u32)(255 * (0.06 + 0.64 * std::pow(bright, 0.6))));
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

void Galaxy::draw(video::IVideoDriver *driver, float aspect, float fov, u32 height, float level)
{
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, 0.1f, 10.f, fov));
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	static std::weak_ptr<Shared> alive;
	if (!m_shared)
		m_shared = alive.lock();
	const Pictures &made = pictures();
	if (!m_shared && ready(made.quick)) {
		const std::shared_ptr<const Shown> sky = made.quick.get();
		auto shared = std::make_shared<Shared>();
		shared->driver = driver;
		shared->picture(*sky);
		shared->sharp = sky == (ready(made.full) ? made.full.get() : nullptr);
		shared->makeGrain();
		shared->makeStars(*sky->sky, m_starMaterial);
		shared->shownAt = std::chrono::steady_clock::now();
		m_shared = shared;
		alive = shared;
	}
	if (m_shared && !m_shared->sharp && ready(made.full)) {
		m_shared->picture(*made.full.get()); // the stars stay: only the glow sharpens
		m_shared->sharp = true;
	}
	if (!m_shared)
		return;
	const double since = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_shared->shownAt).count();
	const float fade = (float)std::min(1.0, since / FADE_IN);
	m_skyUniforms->fade = fade * level;
	// a speck of the unresolved stars about a pixel at the middle of the
	// screen: finer on a bigger screen, never magnified into blots
	m_skyUniforms->specks = std::max(0.93f, OrbitCamera::focal((float)height, fov) / GRAIN);
	m_skyMaterial.TextureLayers[0].Texture = m_shared->texture;
	m_skyMaterial.TextureLayers[1].Texture = m_shared->grain;
	driver->setMaterial(m_skyMaterial);
	driver->drawMeshBuffer(m_sphere);

	// the screen's own pixels: a bigger or finer screen shows the same stars
	// sharper, never bigger
	const f32 pixel = 2.f / std::max(1u, height);
	m_starUniforms->pixel[0] = pixel / aspect;
	m_starUniforms->pixel[1] = pixel;
	m_starUniforms->fade = fade;
	driver->setMaterial(m_starMaterial);
	for (const irr_ptr<scene::SMeshBuffer> &buffer : m_shared->stars)
		driver->drawMeshBuffer(buffer.get());
}

} // namespace planetarium
