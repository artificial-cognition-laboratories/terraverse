// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/globe.h"
#include "far/mesh.h"
#include "exceptions.h"
#include "terragen/world.h"
#include <IVideoDriver.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace planetarium {

// Its picture's texels round and from pole to pole: a texel's some 16 blocks
// on a planet 16 000 round, finer than a pixel till the coarsest tiles' cells are
static constexpr s32 WIDE = 1024, HIGH = 512;
// How much steeper slopes are shaded than they are (the planet shader's RELIEF)
static constexpr double RELIEF = 3;
// The sphere's pieces round and from pole to pole
static constexpr int AROUND = 128, ACROSS = 64;

struct Globe::Making
{
	far::Horizon horizon;
	planet::Atlas atlas;
	int body;
	far::Palette palette;
	std::once_flag made;
	std::unique_ptr<far::Generator> generator;
	std::vector<video::SColor> texels = std::vector<video::SColor>((size_t)WIDE * HIGH);
	std::vector<s32> heights = std::vector<s32>((size_t)WIDE * HIGH); // its surface's (the water's where it's wet), for its shade
	double texel = 1; // blocks round the equator a texel is
	std::atomic<s32> next_row{ 0 };
	std::atomic<unsigned> working{ 0 }; // threads not done yet
	std::atomic<bool> stop{ false };
	std::mutex mutex; // (the error's)
	std::string error;

	Making(const far::Horizon &h, const planet::Atlas &a, int b, const far::Palette &p) :
		horizon(h), atlas(a), body(b), palette(p)
	{
	}

	// rows, as long as there are, on one thread
	void run()
	{
		try {
			std::call_once(made, [&] { generator = std::make_unique<far::Generator>(horizon, atlas, body); });
			const far::Generator &gen = *generator;
			// read at the texel's width: what's narrower would only be one column's chance
			const double round = 2 * M_PI * gen.world().radius() / terragen::ONE;
			texel = round / WIDE;
			const terragen::World::Coarse coarse(gen.world(), std::max<int64_t>(1, (int64_t)texel), false);
			for (s32 j; !stop && (j = next_row++) < HIGH;)
				for (s32 i = 0; i < WIDE; i++) {
					const terragen::Place at = terragen::World::place(terragen::ratio(2 * i + 1, 2 * WIDE), terragen::ratio(2 * j + 1, 2 * HIGH));
					const far::Cell cell = gen.seenAt(at);
					video::SColor c = cell.space ? video::SColor(255, 0, 0, 0) : far::surface(cell, palette).toSColor();
					c.setAlpha(255);
					texels[(size_t)j * WIDE + i] = c;
					heights[(size_t)j * WIDE + i] = cell.space ? 0 : std::max(cell.ground, cell.water);
				}
		} catch (const std::exception &e) {
			std::lock_guard lock(mutex);
			error = e.what();
			stop = true;
		}
		working--;
	}
};

Globe::Globe(const far::Horizon &horizon, const planet::Atlas &atlas, int body, const far::Palette &palette, unsigned threads) :
	m_making(std::make_shared<Making>(horizon, atlas, body, palette))
{
	if (threads == 0)
		throw BaseException("planetarium: a globe needs a thread to make it");
	m_making->working = threads;
	for (unsigned i = 0; i < threads; i++)
		std::thread([making = m_making] { making->run(); }).detach();

	// the unit sphere, its normals its points, its texture coordinates its
	// place as the world model lays its picture out (u east from −π, v south)
	m_sphere = make_irr<scene::SMeshBuffer>();
	auto &v = m_sphere->Vertices->Data;
	auto &ix = m_sphere->Indices->Data;
	for (int b = 0; b <= ACROSS; b++)
		for (int a = 0; a <= AROUND; a++) {
			const float u = (float)a / AROUND, w = (float)b / ACROSS;
			const double lon = 2 * M_PI * u - M_PI, lat = M_PI / 2 - M_PI * w;
			const v3f p((f32)(std::cos(lat) * std::cos(lon)), (f32)std::sin(lat), (f32)(std::cos(lat) * std::sin(lon)));
			v.emplace_back(p, p, video::SColor(255, 255, 255, 255), v2f(u, w));
		}
	for (int b = 0; b < ACROSS; b++)
		for (int a = 0; a < AROUND; a++) {
			const u16 p = (u16)(b * (AROUND + 1) + a), q = (u16)(p + AROUND + 1);
			ix.insert(ix.end(), { p, (u16)(p + 1), q, (u16)(p + 1), (u16)(q + 1), q });
		}
	m_sphere->recalculateBoundingBox();
	m_sphere->setHardwareMappingHint(scene::EHM_STATIC);
}

Globe::~Globe()
{
	m_making->stop = true;
	if (m_texture)
		m_driver->removeTexture(m_texture);
}

bool Globe::ready() const
{
	if (m_making->working > 0)
		return false;
	std::lock_guard lock(m_making->mutex);
	if (!m_making->error.empty())
		throw BaseException("planetarium: the globe's picture: " + m_making->error);
	return true;
}

void Globe::draw(video::IVideoDriver *driver, const video::SMaterial &material, const core::matrix4 &world)
{
	if (!ready())
		throw BaseException("planetarium: a globe drawn before its picture's made");
	if (!m_texture) {
		video::IImage *image = driver->createImage(video::ECF_A8R8G8B8, core::dimension2du(WIDE, HIGH));
		const Making &m = *m_making;
		for (s32 j = 0; j < HIGH; j++)
			for (s32 i = 0; i < WIDE; i++) {
				// its relief shaded in, as the tiles' slopes are (the planet
				// shader's hillshade, planet_lit_shader: light from the north-west
				// 45° up, slopes steepened RELIEF times, a 60% part of the colour);
				// the sphere's own slopes are only the ball's
				const auto h = [&](s32 a, s32 b) {
					return (double)m.heights[(size_t)std::clamp(b, 0, HIGH - 1) * WIDE + ((a % WIDE) + WIDE) % WIDE];
				};
				const double lat = M_PI / 2 - M_PI * (j + 0.5) / HIGH;
				const double east = m.texel * std::max(std::cos(lat), 0.05), north = m.texel;
				const double gx = (h(i + 1, j) - h(i - 1, j)) / (2 * east), gz = (h(i, j - 1) - h(i, j + 1)) / (2 * north);
				const double nx = -gx * RELIEF, nz = -gz * RELIEF, len = std::sqrt(nx * nx + 1 + nz * nz);
				// the light (−½, √½, ½) in east, up, north; flat ground's dot is √½
				const double hill = std::clamp((-0.5 * nx + M_SQRT1_2 + 0.5 * nz) / len / M_SQRT1_2, 0.3, 1.45);
				const double shade = 1 + 0.6 * (hill - 1);
				const video::SColor c = m.texels[(size_t)j * WIDE + i];
				const auto by = [&](u32 v) { return (u32)std::clamp(v * shade, 0.0, 255.0); };
				image->setPixel(i, j, video::SColor(255, by(c.getRed()), by(c.getGreen()), by(c.getBlue())));
			}
		// mipmapped whatever the game's setting: what they average is the point
		const bool mipmaps = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
		driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, true);
		m_texture = driver->addTexture("planetarium:globe:" + std::to_string((uintptr_t)this), image);
		driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmaps);
		image->drop();
		if (!m_texture)
			throw BaseException("planetarium: the globe's texture couldn't be made");
		m_driver = driver;
		m_making->texels = {};
		m_making->heights = {};
	}
	video::SMaterial m = material;
	m.setTexture(0, m_texture);
	video::SMaterialLayer &layer = m.TextureLayers[0];
	layer.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
	layer.MagFilter = video::ETMAGF_LINEAR;
	layer.AnisotropicFilter = 8;
	layer.TextureWrapU = video::ETC_REPEAT; // round the world: its edges meet
	layer.TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	m.BackfaceCulling = true;
	driver->setMaterial(m);
	driver->setTransform(video::ETS_WORLD, world);
	driver->drawMeshBuffer(m_sphere.get());
}

} // namespace planetarium
