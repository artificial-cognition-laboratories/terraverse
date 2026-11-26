// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/space/galaxy.h"
#include "src/client/views/planetarium/camera.h"
#include "src/client/views/planetarium/space/picture.h"
#include "src/client/views/planetarium/space/stars.h"
#include "library/trace/trace.h"
#include "src/client/render/shaders/shader.h"
#include "src/shared/base/exceptions.h"
#include <IMeshBuffer.h>
#include <IVideoDriver.h>
#include <CMeshBuffer.h>
#include <algorithm>
#include <thread>
#include <string>
#include <cmath>
#include <future>
#include <memory>

namespace planetarium {

namespace {

// The sky's picture (picture.h): the first time it's baked, a small one's
// baked first and shown till it's there.
constexpr double FADE_IN = 1.5; // seconds: the picture arrives after the first frames

// A picture, and its pixels as the texture takes them: made on the pictures'
// thread (on the frame's, the full one's eight million were most of a 120 ms
// hitch as it sharpened)
struct Shown
{
	std::shared_ptr<const milkyway::Picture> sky;
	std::vector<u32> pixels; // A8R8G8B8
	// (the first shown only: the stars stay as the picture sharpens)
	std::vector<irr_ptr<scene::SMeshBuffer>> stars;
	std::vector<u32> grain;
};
using Showing = std::shared_future<std::shared_ptr<const Shown>>;

std::shared_ptr<Shown> shown(std::shared_ptr<const milkyway::Picture> sky)
{
	auto out = std::make_shared<Shown>();
	out->pixels.resize((size_t)sky->width * sky->height);
	for (size_t i = 0; i < out->pixels.size(); i++)
		out->pixels[i] = video::SColor(255, sky->rgb[i * 3], sky->rgb[i * 3 + 1], sky->rgb[i * 3 + 2]).color;
	out->sky = std::move(sky);
	return out;
}

// The stars made beside the first picture shown (their brightness follows it)
void withStars(Shown &first)
{
	first.stars = stars::of(*first.sky);
	first.grain = stars::grain();
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
				if (std::shared_ptr<const milkyway::Picture> kept = picture::find(picture::FULL)) {
					std::shared_ptr<Shown> one = shown(std::move(kept));
					withStars(*one);
					quick->set_value(one);
					full->set_value(one);
					return;
				}
				std::shared_ptr<Shown> first = shown(picture::paint(picture::QUICK));
				withStars(*first);
				quick->set_value(first);
				full->set_value(shown(picture::paint(picture::FULL)));
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
	float open = 1; // the eye's (Eye): how much of the band's light shows
	float art = 0; // pixel art: the band alone, its contrast up, no specks (Frame::art)
	float specks = 1; // the specks' texture's repeats a unit of the way's (about a texel a pixel)
	CachedPixelShaderSetting<float> m_fade{ "galaxyFade" };
	CachedPixelShaderSetting<float> m_open{ "galaxyOpen" };
	CachedPixelShaderSetting<float> m_art{ "galaxyArt" };
	CachedPixelShaderSetting<float> m_specks{ "galaxySpecks" };
	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_fade.set(&fade, s);
		m_open.set(&open, s);
		m_art.set(&art, s);
		m_specks.set(&specks, s);
	}
};

struct Galaxy::StarUniforms : public IShaderUniformSetterRC
{
	float pixel[2]{}; // one reference pixel, in clip space
	float fade = 0;
	float cut = 0; // the faintest seen, by alpha: more go as the eye closes
	CachedVertexShaderSetting<float, 2> m_pixel{ "starPixel" };
	CachedPixelShaderSetting<float> m_fade{ "starFade" };
	CachedPixelShaderSetting<float> m_cut{ "starCut" };
	void onSetUniforms(video::IMaterialRendererServices *s) override
	{
		m_pixel.set(pixel, s);
		m_fade.set(&fade, s);
		m_cut.set(&cut, s);
	}
};

Galaxy::Galaxy(IShaderSource *ssrc) :
	m_skyUniforms(make_irr<SkyUniforms>()), m_starUniforms(make_irr<StarUniforms>())
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
	void makeGrain(const std::vector<u32> &made);
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

void Galaxy::Shared::makeGrain(const std::vector<u32> &made)
{
	video::IImage *image = driver->createImageFromData(video::ECF_A8R8G8B8, core::dimension2du(stars::GRAIN, stars::GRAIN), const_cast<u32 *>(made.data()), false, false);
	const bool mips = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, true);
	static u32 count = 0;
	grain = driver->addTexture(("galaxy_grain_" + std::to_string(count++)).c_str(), image);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mips);
	image->drop();
	if (!grain)
		throw BaseException("galaxy: no grain texture");
}



Galaxy::~Galaxy()
{
}

std::weak_ptr<Galaxy::Shared> &Galaxy::alive()
{
	static std::weak_ptr<Shared> shared;
	return shared;
}

std::shared_ptr<const void> Galaxy::held()
{
	return alive().lock();
}

void Galaxy::draw(video::IVideoDriver *driver, const Frame &frame)
{
	const float aspect = frame.aspect, fov = frame.fov, level = frame.level, open = frame.open;
	const u32 height = frame.height;
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, 0.1f, 10.f, fov));
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	if (!m_shared)
		m_shared = alive().lock();
	const Pictures &made = pictures();
	if (!m_shared && ready(made.quick)) {
		const std::shared_ptr<const Shown> sky = made.quick.get();
		auto shared = std::make_shared<Shared>();
		shared->driver = driver;
		{
			TF_ZONE("Space: galaxy: picture");
			shared->picture(*sky);
		}
		shared->sharp = sky == (ready(made.full) ? made.full.get() : nullptr);
		{
			TF_ZONE("Space: galaxy: grain");
			shared->makeGrain(sky->grain);
		}
		{
			TF_ZONE("Space: galaxy: stars");
			shared->stars = sky->stars;
			for (const irr_ptr<scene::SMeshBuffer> &buffer : shared->stars)
				buffer->getMaterial() = m_starMaterial;
		}
		shared->shownAt = std::chrono::steady_clock::now();
		m_shared = shared;
		alive() = shared;
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
	m_skyUniforms->open = open;
	m_skyUniforms->art = frame.art > 0 ? 1.f : 0.f;
	// a speck of the unresolved stars about a pixel at the middle of the
	// screen: finer on a bigger screen, never magnified into blots
	m_skyUniforms->specks = std::max(0.93f, OrbitCamera::focal((float)height, fov) / stars::GRAIN);
	m_skyMaterial.TextureLayers[0].Texture = m_shared->texture;
	m_skyMaterial.TextureLayers[1].Texture = m_shared->grain;
	driver->setMaterial(m_skyMaterial);
	driver->drawMeshBuffer(frame.sphere);

	// the screen's own pixels: a bigger or finer screen shows the same stars
	// sharper, never bigger
	const f32 pixel = 2.f / std::max(1u, height);
	m_starUniforms->pixel[0] = pixel / aspect;
	m_starUniforms->pixel[1] = pixel;
	// the faint ones go as the eye closes; the ones left keep their light
	m_starUniforms->fade = fade * (frame.art > 0 ? 0.7f : 1.f); // (as pixel art each is a whole art pixel: brighter for it)
	// (as pixel art too: the pixel pass makes each a dot of its own shade)
	m_starUniforms->cut = 0.1f + 0.22f * (1 - open);
	driver->setMaterial(m_starMaterial);
	for (const irr_ptr<scene::SMeshBuffer> &buffer : m_shared->stars)
		driver->drawMeshBuffer(buffer.get());
}

} // namespace planetarium
