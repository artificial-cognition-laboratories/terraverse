// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/draw.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include "src/client/world/sim/water/shallow.h"
#include "src/client/views/game/water/liquidlook.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/render/shaders/shader.h"
#include "src/shared/base/exceptions.h"
#include <CMeshBuffer.h>
#include <ICameraSceneNode.h>
#include <IImage.h>
#include <ISceneManager.h>
#include <ITexture.h>
#include <IVideoDriver.h>
#include <SViewFrustum.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace liquids {

// What the shader needs for the tile it's drawing
struct WaterDraw
{
	float grid[4] = {};   // scene x, y, z of the grid's corner at its base; columns a side
	float tile[4] = {};   // first column x, z; tops' mean; light
	float colour[4] = {}; // the liquid's, 0..1; columns a node
};

namespace {

class WaterUniforms : public IShaderUniformSetterRC
{
public:
	explicit WaterUniforms(std::shared_ptr<const WaterDraw> draw) : m_draw(std::move(draw)) {}
	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		m_grid.set(m_draw->grid, services);
		m_tile.set(m_draw->tile, services);
		m_colour.set(m_draw->colour, services);
	}

private:
	std::shared_ptr<const WaterDraw> m_draw;
	CachedVertexShaderSetting<float, 4> m_grid{ "waterGrid" };
	CachedVertexShaderSetting<float, 4> m_tile{ "waterTile" };
	CachedVertexShaderSetting<float, 4> m_colour{ "waterColour" };
};

// A tile's shape: per column a top and four sides, each raised by the shader
scene::SMeshBuffer *tileShape()
{
	auto *shape = new scene::SMeshBuffer();
	auto &vertices = shape->Vertices->Data;
	auto &indices = shape->Indices->Data;
	for (int c = 0; c < 256; c++) {
		const float x = c % 16, z = c / 16;
		const v3f corners[5][4] = {
			{ { x, 1, z }, { x, 1, z + 1 }, { x + 1, 1, z + 1 }, { x + 1, 1, z } },         // top
			{ { x + 1, 0, z }, { x + 1, 1, z }, { x + 1, 1, z + 1 }, { x + 1, 0, z + 1 } }, // +x
			{ { x, 0, z + 1 }, { x, 1, z + 1 }, { x, 1, z }, { x, 0, z } },                 // −x
			{ { x + 1, 0, z + 1 }, { x + 1, 1, z + 1 }, { x, 1, z + 1 }, { x, 0, z + 1 } }, // +z
			{ { x, 0, z }, { x, 1, z }, { x + 1, 1, z }, { x + 1, 0, z } },                 // −z
		};
		for (int f = 0; f < 5; f++) {
			const u16 first = u16(vertices.size());
			const v3f normal = f == 0 ? v3f(0, 1, 0) : v3f(SIDES[f - 1].X, 0, SIDES[f - 1].Z);
			for (const v3f &corner : corners[f])
				vertices.emplace_back(corner, normal, video::SColor(255, 255, 255, 255), v2f(f, c));
			for (u16 i : { 0, 1, 2, 2, 3, 0 })
				indices.push_back(first + i);
		}
	}
	shape->setHardwareMappingHint(scene::EHM_STATIC);
	shape->recalculateBoundingBox();
	return shape;
}

// A level tile's one top (face 5), its depth read at its middle column
scene::SMeshBuffer *wholeShape()
{
	auto *whole = new scene::SMeshBuffer();
	for (const v3f &corner : { v3f(0, 1, 0), v3f(0, 1, 16), v3f(16, 1, 16), v3f(16, 1, 0) })
		whole->Vertices->Data.emplace_back(corner, v3f(0, 1, 0), video::SColor(255, 255, 255, 255), v2f(5, 8 * 16 + 8));
	for (u16 i : { 0, 1, 2, 2, 3, 0 })
		whole->Indices->Data.push_back(i);
	whole->setHardwareMappingHint(scene::EHM_STATIC);
	whole->recalculateBoundingBox();
	return whole;
}

} // namespace

Draw::Draw(Opts o) :
	m_smgr(o.smgr), m_ndef(o.ndef), m_grid(o.grid), m_material(o.material),
	m_draw(std::make_shared<WaterDraw>()), m_uniforms(make_irr<WaterUniforms>(m_draw))
{
	video::IVideoDriver *driver = m_smgr->getVideoDriver();
	const u32 shader = o.shaders->getShader("liquid_shader", ShaderConstants{}, video::EMT_TRANSPARENT_VERTEX_ALPHA,
			m_uniforms.get());
	m_material.MaterialType = o.shaders->getShaderInfo(shader).material;
	m_material.FogEnable = false; // the shader fogs
	m_material.TextureLayers[0].MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
	m_material.TextureLayers[0].MagFilter = video::ETMAGF_NEAREST;
	const bool mipmaps = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	video::IImage *image = driver->createImage(video::ECF_A32B32G32R32F, core::dimension2du(m_grid.cols, m_grid.cols));
	m_heights = driver->addTexture("liquids_fine", image);
	image->drop();
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmaps);
	if (!m_heights)
		throw BaseException("liquids: the driver made no float texture for the water's heights");
	m_heights->grab();
	m_texels.assign(size_t(m_grid.cols) * m_grid.cols * 4, NONE_TEXEL);
	m_shape = tileShape();
	m_whole = wholeShape();
}

Draw::~Draw()
{
	m_shape->drop();
	m_whole->drop();
	m_smgr->getVideoDriver()->removeTexture(m_heights);
	m_heights->drop();
}

void Draw::moved()
{
	std::fill(m_texels.begin(), m_texels.end(), NONE_TEXEL);
	m_upload = true;
}

void Draw::texels(ClientMap &map, u32 daynight, bool relight)
{
	Grid &g = m_grid;
	for (int t = 0; t < g.tiles * g.tiles; t++) {
		Tile &k = g.tile[t];
		if (!k.known)
			continue; // (looked at again after a climb, it's still drawn as it was meanwhile)
		if (relight)
			k.drawn = false;
		if (k.awake && settle(t))
			k.drawn = false;
		if (k.awake && k.drawn)
			surfaceTexels(t); // (rippling under its sixteenths, moving or not)
		if (k.drawn)
			continue;
		settle(t);
		tileTexels(t, map, daynight);
	}
	if (m_upload) {
		// written whole: never read back from the GPU first (that cost tens of ms)
		if (void *to = m_heights->lock(video::ETLM_WRITE_ONLY)) {
			std::memcpy(to, m_texels.data(), m_texels.size() * sizeof(float));
			m_heights->unlock();
		}
		m_upload = false;
	}
}

// Where each column's top is drawn, on a sixteenth, moving only when its
// water's well past it (still water stays still on screen); any water at
// all is a sixteenth deep at least (a puddle thinner than the grid is a
// sheet, not specks)
bool Draw::settle(int t)
{
	Grid &g = m_grid;
	bool moved = false;
	const int first = g.firstOf(t);
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			s32 &shown = g.shown[c];
			const s32 was = shown;
			if (g.floor[c] == NONE) {
				shown = NONE;
				moved = moved || was != NONE;
				continue;
			}
			shown = shallow::topShown(shown, g.floor[c], g.depth[c], NONE);
			moved = moved || shown != was;
		}
	return moved;
}

// A tile's columns' ripples and foam into the height texture (liquidlook.h)
void Draw::surfaceTexels(int t)
{
	Grid &g = m_grid;
	const int first = g.firstOf(t);
	const auto top = [&](int c) { return g.floor[c] == NONE || g.depth[c] <= 0 ? -1e9f : g.floor[c] / 16.0f + g.depth[c]; };
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx, gx = c % g.cols, gz = c / g.cols;
			const std::array<float, 4> beside = { gx + 1 < g.cols ? top(c + 1) : -1e9f, gx > 0 ? top(c - 1) : -1e9f,
				gz + 1 < g.cols ? top(c + g.cols) : -1e9f, gz > 0 ? top(c - g.cols) : -1e9f };
			const look::Surface s = look::surface(top(c), beside, 1.0f / g.res, g.out[c], g.depth[c]);
			m_texels[c * 4 + 2] = s.light;
			m_texels[c * 4 + 3] = s.foam;
		}
	m_upload = true;
}

// A tile's columns into the height texture: each top and ground, over the grid's base
void Draw::tileTexels(int t, ClientMap &map, u32 daynight)
{
	Grid &g = m_grid;
	Tile &tile = g.tile[t];
	tile.drawn = true;
	const s32 base = g.origin.Y * 16;
	const int first = g.firstOf(t);
	float mean = 0;
	int wet = 0;
	s32 one = g.shown[first];
	float low = 1e9f, high = -1e9f;
	bool level = true;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			const s32 shown = g.shown[c], floor = g.floor[c];
			m_texels[c * 4] = shown == NONE ? NONE_TEXEL : float(shown - base);
			m_texels[c * 4 + 1] = floor == NONE ? NONE_TEXEL : float(floor - base);
			level = level && shown == one && shown != NONE && shown - floor >= 16; // deep: shaded alike either way
			if (shown != NONE) {
				mean += shown - base;
				wet++;
				high = std::max(high, float(shown - base));
				low = std::min(low, float(floor - base));
			}
		}
	surfaceTexels(t);
	tile.low = low - 16; // a side stands down to its neighbour's ground, which may be lower
	tile.high = high;
	tile.wet = wet > 0;
	tile.level = level;
	if (wet) {
		tile.mean = mean / wet;
		// lit by the air just over its highest top, the brightest of a few places in it
		// (one place at its mean height was often inside the shore's sand: black)
		const int per = g.per();
		const s16 y = s16(std::floor((high + base) / 16.0f));
		float shade = 0;
		for (const v2s16 at : { v2s16(per / 2, per / 2), v2s16(0, 0), v2s16(per - 1, 0), v2s16(0, per - 1), v2s16(per - 1, per - 1) })
			shade = std::max(shade, light::shade(map, m_ndef,
					v3s16(g.origin.X + (t % g.tiles) * per + at.X, y, g.origin.Z + (t / g.tiles) * per + at.Y), daynight));
		tile.shade = shade;
	}
	m_upload = true;
}

// The grid's wet tiles: a level one whose neighbours stand no lower as one
// quad, the rest column by column; only where the camera's looking
void Draw::render(video::IVideoDriver *driver, v3s16 offset)
{
	Grid &g = m_grid;
	if (!g.placed)
		return;
	driver->setTransform(video::ETS_WORLD, core::matrix4());
	m_draw->grid[0] = (g.origin.X - 0.5f - offset.X) * BS;
	m_draw->grid[1] = (g.origin.Y - 0.5f - offset.Y) * BS;
	m_draw->grid[2] = (g.origin.Z - 0.5f - offset.Z) * BS;
	m_draw->grid[3] = g.cols;
	m_draw->colour[3] = g.res;
	m_material.TextureLayers[0].Texture = m_heights;
	const int per = g.per();
	const scene::SViewFrustum *frustum = m_smgr->getActiveCamera()->getViewFrustum();
	for (int t = 0; t < int(g.tile.size()); t++) {
		const Tile &tile = g.tile[t];
		if (!tile.wet || !tile.drawn)
			continue;
		const int tx = t % g.tiles, tz = t / g.tiles;
		const v3f lo(m_draw->grid[0] + tx * per * BS, m_draw->grid[1] + tile.low / 16 * BS, m_draw->grid[2] + tz * per * BS);
		const aabb3f box(lo, lo + v3f(per * BS, (tile.high - tile.low) / 16 * BS, per * BS));
		bool seen = true;
		for (int i = 0; i < scene::SViewFrustum::VF_PLANE_COUNT && seen; i++)
			seen = box.classifyPlaneRelation(frustum->planes[i]) != core::ISREL3D_FRONT;
		if (!seen)
			continue;
		bool whole = tile.level && !tile.awake; // (awake: it may be rippling)
		for (int s = 0; s < 4 && whole; s++) {
			const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
			if (nx < 0 || nx >= g.tiles || nz < 0 || nz >= g.tiles)
				continue;
			const Tile &n = g.tile[nz * g.tiles + nx];
			whole = n.drawn && n.level && n.mean >= tile.mean;
		}
		m_draw->tile[0] = tx * 16;
		m_draw->tile[1] = tz * 16;
		m_draw->tile[2] = tile.mean;
		m_draw->tile[3] = tile.shade;
		m_draw->colour[0] = tile.r / 255.0f;
		m_draw->colour[1] = tile.g / 255.0f;
		m_draw->colour[2] = tile.b / 255.0f;
		driver->setMaterial(m_material);
		driver->drawMeshBuffer(whole ? m_whole : m_shape);
	}
}

} // namespace liquids
