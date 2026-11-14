// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "client/liquids.h"
#include "client/clientmap.h"
#include "client/shader.h"
#include "settings.h"
#include "exceptions.h"
#include "fill/fill.h"
#include "itemgroup.h"
#include "light.h"
#include "log.h"
#include "mapblock.h"
#include "nodedef.h"
#include "profiler.h"
#include "shape/shape.h"
#include <CMeshBuffer.h>
#include <IImage.h>
#include <ICameraSceneNode.h>
#include <ISceneManager.h>
#include <SViewFrustum.h>
#include <ITexture.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>

#include "shallow.h"
#include "liquidlook.h"

namespace liquids {

static const float GLIDE = 4;      // per s: how fast a shown level closes on its fill (a quarter-second's ease)
static const float SETTLED = 0.002; // of its room: close enough to stop gliding
static const u8 ALPHA = 175;
static const float RELIGHT = 2;    // s between drawing again by the day's light
// a block's half diagonal (nodes)
static const float BLOCK_REACH = MAP_BLOCKSIZE * 0.8660254f;
static const int FLAT_PER_FRAME = 6;   // flat blocks drawn again by the day's light in a frame (no hitch)
// a liquid's surface catches the sky: clear ones (spirit, brine) are still seen
static const float SHEEN = 0.22f;
static const video::SColor SKY(255, 190, 215, 235);

// The grids (sprint 82)
static const s32 NONE = INT_MIN;      // no ground in reach (a wall to the water), dry, or not the sea
static const float NONE_TEXEL = -1e9; // in the height texture: none
static const int ABOVE = 8, BELOW = 24; // nodes over and under the camera they look for water and ground
static const int ALIGN = 4;           // nodes: both grids' corners sit on this, so a coarse tile is wholly in the fine grid or out
static const float POOLS = 0.25;      // s between finding the pools again
static constexpr float G = shallow::G;
static const float HOLD = 3;          // per s: how fast a pool's water closes on the server's amount
static const float HOLD_MOST = 0.25;  // nodes³/s a tile's pool may be filled or drained by, at most
static const float SLACK = 1 / 128.0f; // nodes³ a tile's water may be off before it's pulled back
static const float SEA_HOLD = 4;      // per s: how fast the sea's columns close on its level
static const float STILL = 1e-5;      // node heights moved in a step: still
static const float SLEEP = 1;         // s still before a near tile stops being run
static const float SLEEP_FAR = 0.3;   // ... a far one (it's seen less)
static const float NEAR = 10;         // nodes
static const float DEEP_STILL = 20;   // times STILL a deep column must move to wake the tile beside (calm deep water stays asleep)
static const float WAKE = 4;          // per s: how fast what moves through water drives it at its own speed
static const float SPLASH = 0.03;     // node²/s of flow outward per node/s a drop lands at
static const float CREDIT_FADE = 1;   // per s: drops' water fades as the server's amounts take over
static const float CURRENT = 0.8f;   // nodes/s: how fast a river carries what's on it (its discharge's, to come)
static const float FILM = 1 / 32.0f;  // node heights: thinner isn't a pool's water to add to
static const size_t MOST_DROPS = 3000;
static const float DROP_LIFE = 3;     // s: a drop that's landed nowhere by then is gone
static const u8 DROP_ALPHA = 220;

static const v3s16 SIDES[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

// a grid's step: a column's neighbour's index by side, and its time step
static int stepOf(const Layer::Grid &g, int s)
{
	return s == 0 ? 1 : s == 1 ? -1 : s == 2 ? g.cols : -g.cols;
}
static float dtOf(const Layer::Grid &g)
{
	return g.res >= 16 ? 1 / 100.0f : 1 / 50.0f; // a wave crosses under half a column in one
}
static int mostSteps(const Layer::Grid &g)
{
	return g.res >= 16 ? 3 : 2; // in a frame; past it the water runs slow rather than the frame
}

// What the shader needs for the tile it's drawing
struct WaterDraw
{
	float grid[4] = {};   // scene x, y, z of the grid's corner at its base; columns a side
	float tile[4] = {};   // first column x, z; tops' mean; light
	float colour[4] = {}; // the liquid's, 0..1; columns a node
};

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

static void configure(Layer::Grid &g, int res, int nodes, int follow, int scans)
{
	g.res = res;
	g.nodes = nodes;
	g.follow = follow;
	g.scans = scans;
	g.cols = nodes * res;
	g.tiles = g.cols / 16;
	for (int t = 0; t < g.tiles * g.tiles; t++)
		g.nearest.push_back(t);
	auto away = [&](int t) {
		const float x = t % g.tiles - g.tiles / 2 + 0.5f, z = t / g.tiles - g.tiles / 2 + 0.5f;
		return x * x + z * z;
	};
	std::sort(g.nearest.begin(), g.nearest.end(), [&](int a, int b) { return away(a) < away(b); });
}

Layer::Layer(scene::ISceneManager *smgr, const NodeDefManager *ndef, IShaderSource *shaders) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1), m_ndef(ndef)
{
	m_material.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
	m_material.BackfaceCulling = false;
	m_material.ZWriteEnable = video::EZW_OFF;
	m_material.FogEnable = true;
	// (read once: the grids' textures are this size for as long as the world's open)
	const auto across = [](const char *setting) { return ALIGN * int(std::lround(g_settings->getFloat(setting) * 2 / ALIGN)); };
	configure(m_fine, 16, across("water_fine_reach"), ALIGN, 24);   // a sixteenth
	configure(m_coarse, 4, across("water_coarse_reach"), 2 * ALIGN, 6); // a quarter

	// the grids are drawn on the GPU where the driver reads a texture in a vertex shader (GL 3)
	video::IVideoDriver *driver = smgr->getVideoDriver();
	m_gpu = driver->getDriverType() == video::EDT_OPENGL3;
	if (!m_gpu)
		warningstream << "liquids: water near is run and drawn on OpenGL 3 only; here it's drawn flat" << std::endl;
	m_gpu = m_gpu && g_settings->getBool("water_fine");
	if (!m_gpu)
		return;
	m_draw = std::make_shared<WaterDraw>();
	m_uniforms = make_irr<WaterUniforms>(m_draw);
	const u32 shader = shaders->getShader("liquid_shader", ShaderConstants{}, video::EMT_TRANSPARENT_VERTEX_ALPHA,
			m_uniforms.get());
	m_gpu_material = m_material;
	m_gpu_material.MaterialType = shaders->getShaderInfo(shader).material;
	m_gpu_material.FogEnable = false; // the shader fogs
	m_gpu_material.TextureLayers[0].MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
	m_gpu_material.TextureLayers[0].MagFilter = video::ETMAGF_NEAREST;
	const bool mipmaps = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	for (Grid *g : { &m_fine, &m_coarse }) {
		video::IImage *image = driver->createImage(video::ECF_A32B32G32R32F, core::dimension2du(g->cols, g->cols));
		g->heights = driver->addTexture(g == &m_fine ? "liquids_fine" : "liquids_coarse", image);
		image->drop();
		if (!g->heights)
			throw BaseException("liquids: the driver made no float texture for the water's heights");
		g->heights->grab();
		g->texels.assign(size_t(g->cols) * g->cols * 4, NONE_TEXEL);
		g->upload = true;
	}
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmaps);
	// a tile's shape: per column a top and four sides, each raised by the shader
	m_shape = new scene::SMeshBuffer();
	auto &vertices = m_shape->Vertices->Data;
	auto &indices = m_shape->Indices->Data;
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
	m_shape->setHardwareMappingHint(scene::EHM_STATIC);
	m_shape->recalculateBoundingBox();
	// a level tile's one top (face 5), its depth read at its middle column
	m_whole = new scene::SMeshBuffer();
	for (const v3f &corner : { v3f(0, 1, 0), v3f(0, 1, 16), v3f(16, 1, 16), v3f(16, 1, 0) })
		m_whole->Vertices->Data.emplace_back(corner, v3f(0, 1, 0), video::SColor(255, 255, 255, 255), v2f(5, 8 * 16 + 8));
	for (u16 i : { 0, 1, 2, 2, 3, 0 })
		m_whole->Indices->Data.push_back(i);
	m_whole->setHardwareMappingHint(scene::EHM_STATIC);
	m_whole->recalculateBoundingBox();
}

Layer::~Layer()
{
	for (auto &[pos, b] : m_blocks)
		if (b.mesh)
			b.mesh->drop();
	for (scene::SMeshBuffer *mesh : { m_drop_mesh, m_shape, m_whole })
		if (mesh)
			mesh->drop();
	for (Grid *g : { &m_fine, &m_coarse })
		if (g->heights) {
			SceneManager->getVideoDriver()->removeTexture(g->heights);
			g->heights->drop();
		}
}

static u16 keyOf(v3s16 rel)
{
	return u16((rel.Z * MAP_BLOCKSIZE + rel.Y) * MAP_BLOCKSIZE + rel.X);
}

static v3s16 relOf(u16 k)
{
	return v3s16(k % MAP_BLOCKSIZE, (k / MAP_BLOCKSIZE) % MAP_BLOCKSIZE, k / (MAP_BLOCKSIZE * MAP_BLOCKSIZE));
}

static s64 micros()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// a neighbour's liquid height (node units, from its floor), −1: none, 2: walled (solid there)
static float besideLevel(ClientMap &map, const NodeDefManager *ndef, v3s16 p)
{
	const MapNode n = map.getNode(p);
	if (n.getContent() == CONTENT_IGNORE)
		return 2;
	const ContentFeatures &f = ndef->get(n);
	if (f.drawtype == NDT_LIQUID)
		return 1; // the sea's own water
	const std::optional<shape::Shape> s = map.solidShape(p);
	if (!s && f.walkable && f.param_type_2 != CPT2_LEVELED)
		return 2;
	const fill::Fill *there = map.getFill(p);
	if (!there)
		return -1;
	return fill::Room::of(s ? &*s : nullptr).surface(there->amount / 255.0f);
}

// a quad, its corners in node units, facing `normal`
static void quadInto(scene::SMeshBuffer *mesh, v3f a, v3f c1, v3f c2, v3f d, v3f normal, video::SColor colour)
{
	auto &vertices = mesh->Vertices->Data;
	auto &indices = mesh->Indices->Data;
	const u32 at = vertices.size();
	if (at + 4 > 65535)
		return; // past what one buffer draws (a block's water never is)
	for (const v3f &p : { a, c1, c2, d })
		vertices.emplace_back(p * BS, normal, colour, v2f(0, 0));
	for (u16 i : { 0, 1, 2, 2, 3, 0 })
		indices.push_back(u16(at + i));
}

static scene::SMeshBuffer *emptied(scene::SMeshBuffer *&mesh)
{
	if (!mesh) {
		mesh = new scene::SMeshBuffer();
		mesh->setHardwareMappingHint(scene::EHM_STREAM);
	}
	mesh->Vertices->Data.clear();
	mesh->Indices->Data.clear();
	return mesh;
}

// a liquid's colour lit, its tops catching the sky
static video::SColor tint(u8 r, u8 g, u8 b, float shade, float by, bool top, u8 alpha = ALPHA)
{
	const float k = top ? SHEEN : 0;
	auto mix = [&](u8 own, u32 sky) { return u32(std::clamp((own * (1 - k) + sky * k) * shade * by, 0.0f, 255.0f)); };
	return video::SColor(alpha, mix(r, SKY.getRed()), mix(g, SKY.getGreen()), mix(b, SKY.getBlue()));
}

// How lit liquid in a node is: by the node or the open air over it, the
// brighter (liquid in a pit cut in sand is in a node that's dark itself)
float Layer::shadeAt(ClientMap &map, v3s16 p, u32 daynight)
{
	u8 light = 0;
	const v3s16 up = p + v3s16(0, 1, 0);
	for (v3s16 q : { p, up, up + v3s16(0, 1, 0), up + SIDES[0], up + SIDES[1], up + SIDES[2], up + SIDES[3] }) {
		const MapNode n = map.getNode(q);
		if (n.getContent() != CONTENT_IGNORE)
			light = std::max(light, decode_light(n.getLightBlend(daynight, m_ndef->getLightingFlags(n))));
	}
	return 0.25f + 0.75f * light / 255.0f;
}

// Flat ------------------------------------------------------------------

// A sea node's faces, looked at once when its ground changes: its top if
// nothing's over it, and its sides where what's beside stands lower (not
// against ground: the sea beside a smoothed shore meets the shore's fill)
void Layer::seaFaces(v3s16 p, Shown &s, ClientMap &map)
{
	const float above = besideLevel(map, m_ndef, p + v3s16(0, 1, 0));
	s.top = !(above >= 0 && above <= 1);
	for (int i = 0; i < 4; i++) {
		const v3s16 n = p + SIDES[i];
		const float there = besideLevel(map, m_ndef, n);
		s.from[i] = there >= 1 || (there < 0 && map.solidShape(n)) ? -1 : s8(std::lround(std::max(0.0f, there) * 16));
	}
}

// A block's fills, as they are now: new ones shown at once (a block just
// arrived), changed ones gliding from where they were shown; and the sea's
// blocks that can be seen (its surface, its edges)
void Layer::take(v3s16 blockpos, ClientMap &map, bool ground)
{
	MapBlock *block = map.getBlockNoCreateNoEx(blockpos);
	Block &b = m_blocks[blockpos];
	std::unordered_map<u16, Shown> was;
	was.swap(b.nodes);
	if (!block)
		return;
	// the sea's kept as it was unless its ground changed (a fill's change is often; looking is dear)
	if (!ground)
		for (const auto &[k, s] : was)
			if (s.sea)
				b.nodes[k] = s;
	const v3s16 base = blockpos * MAP_BLOCKSIZE;
	block->m_fills.each([&](v3s16 rel, const fill::Fill &f) {
		const u16 k = keyOf(rel);
		const float target = f.amount / 255.0f;
		auto old = was.find(k);
		Shown s{ old != was.end() ? old->second.level : target, target, f.r, f.g, f.b };
		b.nodes[k] = s;
		if (std::fabs(s.level - s.target) > SETTLED)
			b.gliding = true;
	});
	// what's gone drains away
	for (const auto &[k, s] : was)
		if (!b.nodes.count(k) && !s.sea && s.level > SETTLED) {
			b.nodes[k] = Shown{ s.level, 0, s.r, s.g, s.b };
			b.gliding = true;
		}
	// the sea, where it can be seen
	auto open = [&](v3s16 p) {
		const MapNode n = map.getNode(p);
		return n.getContent() != CONTENT_IGNORE && m_ndef->get(n).drawtype != NDT_LIQUID && !map.getFill(p);
	};
	for (s16 z = 0; z < MAP_BLOCKSIZE && ground; z++)
		for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
			for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
				const ContentFeatures &f = m_ndef->get(block->getNodeNoCheck(x, y, z));
				if (f.drawtype != NDT_LIQUID || !itemgroup_get(f.groups, "liquid_layer"))
					continue;
				const v3s16 p = base + v3s16(x, y, z);
				bool seen = open(p + v3s16(0, 1, 0));
				for (int i = 0; i < 4 && !seen; i++)
					seen = open(p + SIDES[i]);
				if (!seen)
					continue;
				Shown s{ 1, 1, (u8)f.color.getRed(), (u8)f.color.getGreen(), (u8)f.color.getBlue(), true };
				seaFaces(p, s, map);
				b.nodes[keyOf(v3s16(x, y, z))] = s;
			}
}

// The flat mesh: the sea (its tops in runs), and fills the grids don't run,
// each gliding to its level
void Layer::build(v3s16 blockpos, Block &b, ClientMap &map, u32 daynight)
{
	scene::SMeshBuffer *mesh = emptied(b.mesh);
	const v3s16 base = blockpos * MAP_BLOCKSIZE;
	auto quad = [&](v3f a, v3f c1, v3f c2, v3f d, v3f normal, video::SColor colour) {
		quadInto(mesh, a, c1, c2, d, normal, colour);
	};
	// the sea: tops alike along x in one quad; sides as looked at
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
		for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
			for (s16 x = 0; x < MAP_BLOCKSIZE;) {
				auto it = b.nodes.find(keyOf(v3s16(x, y, z)));
				const v3s16 p = base + v3s16(x, y, z);
				if (it == b.nodes.end() || !it->second.sea || owned(p)) {
					x++;
					continue;
				}
				const Shown &s = it->second;
				const float shade = shadeAt(map, p, daynight);
				const v3f o(x - 0.5f, y - 0.5f, z - 0.5f);
				const video::SColor side = tint(s.r, s.g, s.b, shade, 0.85f, false);
				for (int i = 0; i < 4; i++) {
					if (s.from[i] < 0 || owned(p + SIDES[i]))
						continue;
					const float from = o.Y + s.from[i] / 16.0f, to = o.Y + 1;
					const v3f nv(SIDES[i].X, 0, SIDES[i].Z);
					if (SIDES[i].X) {
						const float px = SIDES[i].X > 0 ? o.X + 1 : o.X;
						quad(v3f(px, from, o.Z), v3f(px, to, o.Z), v3f(px, to, o.Z + 1), v3f(px, from, o.Z + 1), nv, side);
					} else {
						const float pz = SIDES[i].Z > 0 ? o.Z + 1 : o.Z;
						quad(v3f(o.X, from, pz), v3f(o.X, to, pz), v3f(o.X + 1, to, pz), v3f(o.X + 1, from, pz), nv, side);
					}
				}
				s16 end = x + 1;
				if (s.top) {
					// the run of tops lit alike beside it
					while (end < MAP_BLOCKSIZE) {
						auto next = b.nodes.find(keyOf(v3s16(end, y, z)));
						const v3s16 q = base + v3s16(end, y, z);
						if (next == b.nodes.end() || !next->second.sea || !next->second.top || owned(q) ||
								shadeAt(map, q, daynight) != shade)
							break;
						end++;
					}
					const video::SColor top = tint(s.r, s.g, s.b, shade, 0.65f, true, u8(ALPHA + 60));
					quad(v3f(o.X, o.Y + 1, o.Z), v3f(o.X, o.Y + 1, o.Z + 1), v3f(o.X + (end - x), o.Y + 1, o.Z + 1),
							v3f(o.X + (end - x), o.Y + 1, o.Z), v3f(0, 1, 0), top);
				}
				x = end;
			}
	// fills
	for (const auto &[k, s] : b.nodes) {
		if (s.sea || s.level <= 0)
			continue;
		const v3s16 rel = relOf(k);
		const v3s16 p = base + rel;
		if (owned(p))
			continue; // a grid draws it
		const float shade = shadeAt(map, p, daynight);
		const v3f o(rel.X - 0.5f, rel.Y - 0.5f, rel.Z - 0.5f);
		const float above = besideLevel(map, m_ndef, p + v3s16(0, 1, 0));
		const bool underneath = above >= 0 && above <= 1;
		const std::optional<shape::Shape> solid = map.solidShape(p); // cut, or smoothed ground
		const shape::Shape *cut = solid ? &*solid : nullptr;
		const float h = fill::Room::of(cut).surface(s.level);
		if (h <= 0)
			continue;
		// as deep as the grids reckon it (so they meet in one colour)
		const float below = besideLevel(map, m_ndef, p - v3s16(0, 1, 0));
		const float deep = std::min(1.0f, h + (below >= 0.999f && below <= 1 ? 1 : 0)); // over liquid, not ground
		const video::SColor top_colour = tint(s.r, s.g, s.b, shade, 1 - 0.35f * deep, true, u8(ALPHA + 60 * deep));
		const video::SColor colour = tint(s.r, s.g, s.b, shade, 0.85f, false);
		const float top = o.Y + h;
		if (!(h >= 0.999f && underneath)) {
			if (!cut) {
				quad(v3f(o.X, top, o.Z), v3f(o.X, top, o.Z + 1), v3f(o.X + 1, top, o.Z + 1), v3f(o.X + 1, top, o.Z),
						v3f(0, 1, 0), top_colour);
			} else {
				// over the open sixteenths of the layer it stands in
				const int layer = std::clamp(int(std::ceil(h * 16)) - 1, 0, 15);
				for (int z = 0; z < 16; z++)
					for (int x = 0; x < 16;) {
						if (cut->has(v3s16(x, layer, z))) {
							x++;
							continue;
						}
						int end = x;
						while (end < 16 && !cut->has(v3s16(end, layer, z)))
							end++;
						const float x0 = o.X + x / 16.0f, x1 = o.X + end / 16.0f;
						const float z0 = o.Z + z / 16.0f, z1 = o.Z + (z + 1) / 16.0f;
						quad(v3f(x0, top, z0), v3f(x0, top, z1), v3f(x1, top, z1), v3f(x1, top, z0), v3f(0, 1, 0), top_colour);
						x = end;
					}
			}
		}
		// the sides: where it stands above what's beside it (open air, a lower fill)
		for (const v3s16 &d : SIDES) {
			const v3s16 n = p + d;
			if (owned(n))
				continue;
			const float there = besideLevel(map, m_ndef, n);
			if (there >= 2 || there >= h || (there < 0 && map.solidShape(n)))
				continue;
			const float from = o.Y + std::max(0.0f, there), to = top;
			const v3f nv(d.X, 0, d.Z);
			if (d.X) {
				const float x = d.X > 0 ? o.X + 1 : o.X;
				quad(v3f(x, from, o.Z), v3f(x, to, o.Z), v3f(x, to, o.Z + 1), v3f(x, from, o.Z + 1), nv, colour);
			} else {
				const float z = d.Z > 0 ? o.Z + 1 : o.Z;
				quad(v3f(o.X, from, z), v3f(o.X, to, z), v3f(o.X + 1, to, z), v3f(o.X + 1, from, z), nv, colour);
			}
		}
	}
	mesh->recalculateBoundingBox();
	mesh->setDirty();
}

// A frame --------------------------------------------------------------

void Layer::update(const std::unordered_set<v3s16> &changed, const std::unordered_set<v3s16> &ground,
		const std::vector<Stream> &streams, const std::vector<Mover> &movers, const Rivers &rivers, ClientMap &map,
		v3s16 offset, v3f camera, std::optional<s16> sea, u32 daynight, float dtime, bool hurrying)
{
	const s64 began = micros();
	m_offset = offset;
	m_camera = camera;
	m_range = map.getControl().range_all ? INFINITY : map.getControl().wanted_range;
	m_sea = sea;
	m_rivers = &rivers;
	for (Grid *g : { &m_fine, &m_coarse })
		g->sea_level = sea ? (*sea + 1) * 16 : INT32_MIN;
	std::unordered_set<v3s16> rebuild;
	auto taken = [&](v3s16 bp, bool g) {
		take(bp, map, g);
		rebuild.insert(bp);
		// a neighbour's sides face this one's liquid
		for (v3s16 d : SIDES)
			if (m_blocks.count(bp + d))
				rebuild.insert(bp + d);
	};
	for (v3s16 bp : ground)
		taken(bp, true);
	for (v3s16 bp : changed)
		if (!ground.count(bp))
			taken(bp, false);
	// drawn again by the day's light only when it's changed (each 2 s it cost ms for nothing)
	m_relight -= dtime;
	const bool relight = m_relight <= 0 && daynight != m_daynight;
	if (m_relight <= 0) {
		m_relight = RELIGHT;
		m_daynight = daynight;
	}
	const float step = std::min(1.0f, GLIDE * dtime);
	for (auto it = m_blocks.begin(); it != m_blocks.end();) {
		Block &b = it->second;
		// unloaded: let it go
		if (!map.getBlockNoCreateNoEx(it->first)) {
			if (b.mesh)
				b.mesh->drop();
			m_relit.erase(it->first);
			m_reflat.erase(it->first);
			it = m_blocks.erase(it);
			continue;
		}
		if (b.gliding) {
			b.gliding = false;
			for (auto n = b.nodes.begin(); n != b.nodes.end();) {
				Shown &s = n->second;
				s.level += (s.target - s.level) * step;
				if (std::fabs(s.target - s.level) <= SETTLED)
					s.level = s.target;
				else
					b.gliding = true;
				if (s.level <= 0 && s.target <= 0)
					n = b.nodes.erase(n);
				else
					++n;
			}
			rebuild.insert(it->first);
		}
		if (relight && !b.nodes.empty())
			m_relit.insert(it->first);
		++it;
	}
	// by the day's light, and as the grids take or let go of liquid, a few at a time
	for (int i = 0; i < FLAT_PER_FRAME && !m_relit.empty(); i++) {
		rebuild.insert(*m_relit.begin());
		m_relit.erase(m_relit.begin());
	}
	rebuild.insert(m_unowned.begin(), m_unowned.end());
	m_unowned.clear();
	// what a grid took too, at once: till the flat mesh lets go of it both draw it (darker, see-through twice)
	rebuild.insert(m_reflat.begin(), m_reflat.end());
	m_reflat.clear();

	// the grids: where they are, and what's changed in them
	std::unordered_set<v3s16> blocks(changed);
	blocks.insert(ground.begin(), ground.end());
	s64 looked = 0, ran = 0;
	// Flown past too fast to see it slosh: the grids wait, and what's new is drawn flat
	const bool grids_run = m_gpu && !hurrying;
	if (grids_run) {
		const s64 t0 = micros();
		for (Grid *g : { &m_fine, &m_coarse }) {
			place(*g);
			const int per = 16 / g->res; // nodes a tile a side
			for (v3s16 bp : blocks)
				for (int tz = 0; tz < g->tiles; tz++) {
					const int z0 = g->origin.Z + tz * per;
					if (z0 + per <= bp.Z * MAP_BLOCKSIZE || z0 >= (bp.Z + 1) * MAP_BLOCKSIZE)
						continue;
					for (int tx = 0; tx < g->tiles; tx++) {
						const int x0 = g->origin.X + tx * per;
						if (x0 + per > bp.X * MAP_BLOCKSIZE && x0 < (bp.X + 1) * MAP_BLOCKSIZE) {
							// a dry tile's ground is only needed when water comes (sprint 85:
							// ground churned by piles rescanned every dry tile round it)
							Tile &k = g->tile[tz * g->tiles + tx];
							const bool dry = !k.wet && !k.sea && !k.awake && k.volume <= 0 && k.target <= 0
									&& !changed.count(bp);
							if (dry && k.known)
								k.stale = true;
							else
								k.scanned = false;
						}
					}
				}
			int scans = 0;
			for (int t : g->nearest)
				if (!g->tile[t].scanned && scans++ < g->scans)
					scan(*g, t, map);
		}
		looked = micros() - t0;
		const s64 t1 = micros();
		pour(streams);
		// what moves: its speed, then its push on the fine grid, or the coarse where the fine isn't
		std::unordered_map<u32, v3f> now;
		for (const Mover &m : movers) {
			now[m.id] = m.feet;
			auto last = m_movers.find(m.id);
			if (last == m_movers.end() || dtime <= 0)
				continue;
			const v3f v = (m.feet - last->second) / dtime;
			if (v.getLength() > 30 || v.getLength() < 0.05f)
				continue; // a jump across the world, or standing
			const v3s16 at(std::lround(m.feet.X), std::lround(m.feet.Y), std::lround(m.feet.Z));
			push(covers(at) ? m_fine : m_coarse, m, v, dtime);
		}
		m_movers.swap(now);
		for (Grid *g : { &m_fine, &m_coarse }) {
			g->pooled -= dtime;
			if (g->pooled <= 0) {
				g->pooled = POOLS;
				pools(*g);
				currents(*g);
			}
			hold(*g, dtime);
			const float dt = dtOf(*g);
			g->owed = std::min(g->owed + dtime, dt * mostSteps(*g));
			while (g->owed >= dt) {
				flow(*g, dt);
				carry(*g, dt);
				g->owed -= dt;
			}
		}
		ran = micros() - t1;
	}
	fall(map, dtime);
	const s64 grids = micros();

	// (at speed a new block's flat mesh could take 20 ms a frame: the rest wait for the next)
	constexpr s64 HURRIED_FLAT_US = 2000;
	for (v3s16 bp : rebuild) {
		if (hurrying && micros() - grids > HURRIED_FLAT_US) {
			m_reflat.insert(bp);
			continue;
		}
		auto it = m_blocks.find(bp);
		if (it != m_blocks.end())
			build(bp, it->second, map, daynight);
	}
	const s64 flat = micros();
	g_profiler->graphAdd("Water flat blocks", rebuild.size());
	if (grids_run)
		for (Grid *g : { &m_fine, &m_coarse })
			run(*g, map, daynight, dtime, relight, blocks);
	buildDrops(map, daynight);
	const s64 drawn = micros();
	g_profiler->graphAdd("Water look [us]", looked);
	g_profiler->graphAdd("Water run [us]", ran);
	g_profiler->graphAdd("Water flat [us]", flat - grids);
	g_profiler->graphAdd("Water mesh [us]", drawn - flat);
}

// A grid's tiles into its height texture as their tops move, and uploaded
void Layer::run(Grid &g, ClientMap &map, u32 daynight, float dtime, bool relight, const std::unordered_set<v3s16> &)
{
	for (int t = 0; t < g.tiles * g.tiles; t++) {
		Tile &k = g.tile[t];
		if (!k.known)
			continue; // (looked at again after a climb, it's still drawn as it was meanwhile)
		if (relight)
			k.drawn = false;
		if (k.awake && settle(g, t))
			k.drawn = false;
		if (k.awake && k.drawn)
			surfaceTexels(g, t); // (rippling under its sixteenths, moving or not)
		if (k.drawn)
			continue;
		settle(g, t);
		texels(g, t, map, daynight);
	}
	if (g.upload) {
		// written whole: never read back from the GPU first (that cost tens of ms)
		if (void *to = g.heights->lock(video::ETLM_WRITE_ONLY)) {
			std::memcpy(to, g.texels.data(), g.texels.size() * sizeof(float));
			g.heights->unlock();
		}
		g.upload = false;
	}
}

// The grids -------------------------------------------------------------

bool Layer::covers(v3s16 p) const
{
	const Grid &g = m_fine;
	if (!g.placed || p.X < g.origin.X || p.X >= g.origin.X + g.nodes || p.Z < g.origin.Z || p.Z >= g.origin.Z + g.nodes)
		return false;
	// ready: till the fine tile's looked at and drawn, the coarse one draws there (else a flash of nothing)
	const Tile &t = g.tile[(p.Z - g.origin.Z) * g.tiles + (p.X - g.origin.X)];
	return t.known && t.drawn;
}

bool Layer::owned(v3s16 p) const
{
	for (const Grid *g : { &m_fine, &m_coarse }) {
		if (!g->placed || (g == &m_coarse && covers(p)))
			continue;
		const int nx = p.X - g->origin.X, nz = p.Z - g->origin.Z;
		if (nx < 0 || nx >= g->nodes || nz < 0 || nz >= g->nodes)
			continue;
		const int per = 16 / g->res;
		const Tile &t = g->tile[(nz / per) * g->tiles + nx / per];
		const std::optional<v3s16> &top = g->tops[nz * g->nodes + nx];
		if (t.known && top && *top == p)
			return true;
	}
	return false;
}

int Layer::columnAt(const Grid &g, float x, float z) const
{
	if (!g.placed)
		return -1;
	const int cx = int(std::floor((x - (g.origin.X - 0.5f)) * g.res)),
			cz = int(std::floor((z - (g.origin.Z - 0.5f)) * g.res));
	if (cx < 0 || cx >= g.cols || cz < 0 || cz >= g.cols)
		return -1;
	return cz * g.cols + cx;
}

int Layer::tileOf(const Grid &g, int c) const
{
	return ((c / g.cols) / 16) * g.tiles + (c % g.cols) / 16;
}

// A grid follows the camera, keeping what it has where they overlap; its
// corner's on the align, so the coarse grid's tiles are wholly in the fine one or out
void Layer::place(Grid &g)
{
	const v3s16 at(std::lround(m_camera.X), std::lround(m_camera.Y), std::lround(m_camera.Z));
	auto aligned = [](int v) { return int(std::floor(v / float(ALIGN))) * ALIGN; };
	// its height follows only a climb or fall past `follow` (its reach is by height)
	const s16 y = g.placed && std::abs(at.Y - g.origin.Y) <= g.follow ? g.origin.Y : at.Y;
	const v3s16 want(aligned(at.X - g.nodes / 2), y, aligned(at.Z - g.nodes / 2));
	if (g.placed && want == g.origin)
		return;
	const size_t columns = size_t(g.cols) * g.cols;
	std::vector<s32> floor(columns, NONE), shown(columns, NONE), sea(columns, NONE), wanted(columns, NONE);
	std::vector<float> depth(columns, 0);
	std::vector<std::array<float, 4>> out(columns, std::array<float, 4>{});
	std::vector<std::optional<v3s16>> tops(size_t(g.nodes) * g.nodes);
	std::vector<float> runs(size_t(g.nodes) * g.nodes, 0);
	std::vector<std::array<float, 2>> ways(size_t(g.nodes) * g.nodes, std::array<float, 2>{});
	std::vector<Tile> tiles(size_t(g.tiles) * g.tiles);
	// it keeps all it has where it overlaps (thrown away and started again from the
	// server's amounts, a spreading puddle jumped back to the server's squares);
	// a climb or fall looks at it all again, keeping its water where it stands
	const bool keep = g.placed;
	const bool climbed = g.placed && want.Y != g.origin.Y;
	const int per = 16 / g.res;
	for (int tz = 0; tz < g.tiles && g.placed; tz++)
		for (int tx = 0; tx < g.tiles; tx++) {
			Tile &old = g.tile[tz * g.tiles + tx];
			const int mx = tx + (g.origin.X - want.X) / per, mz = tz + (g.origin.Z - want.Z) / per;
			if (!keep || mx < 0 || mx >= g.tiles || mz < 0 || mz >= g.tiles || !old.known) {
				// left behind: what it drew is drawn flat again, at once
				for (int nz = 0; nz < per; nz++)
					for (int nx = 0; nx < per; nx++)
						if (const auto &top = g.tops[(tz * per + nz) * g.nodes + tx * per + nx])
							m_unowned.insert(getNodeBlockPos(*top));
				continue;
			}
			Tile &t = tiles[mz * g.tiles + mx];
			t = old;
			t.drawn = false; // its texels are written again where it now is
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int from = (tz * 16 + cz) * g.cols + tx * 16 + cx, to = (mz * 16 + cz) * g.cols + mx * 16 + cx;
					floor[to] = g.floor[from];
					depth[to] = g.depth[from];
					out[to] = g.out[from];
					shown[to] = g.shown[from];
					sea[to] = g.sea[from];
					wanted[to] = g.want[from];
				}
			for (int nz = 0; nz < per; nz++)
				for (int nx = 0; nx < per; nx++)
				{
					tops[(mz * per + nz) * g.nodes + mx * per + nx] = g.tops[(tz * per + nz) * g.nodes + tx * per + nx];
					runs[(mz * per + nz) * g.nodes + mx * per + nx] = g.runs[(tz * per + nz) * g.nodes + tx * per + nx];
					ways[(mz * per + nz) * g.nodes + mx * per + nx] = g.ways[(tz * per + nz) * g.nodes + tx * per + nx];
				}
		}
	const bool was = g.placed;
	const v3s16 old = g.origin;
	g.floor.swap(floor);
	g.shown.swap(shown);
	g.sea.swap(sea);
	g.want.swap(wanted);
	g.depth.swap(depth);
	g.out.swap(out);
	g.current.assign(columns, std::array<float, 2>{}); // found again with the pools
	g.tops.swap(tops);
	g.runs.swap(runs);
	g.ways.swap(ways);
	g.tile.swap(tiles);
	g.origin = want;
	g.placed = true;
	if (climbed)
		for (Tile &t : g.tile)
			t.scanned = false;
	if (m_gpu) {
		std::fill(g.texels.begin(), g.texels.end(), NONE_TEXEL);
		g.upload = true; // its tiles' texels are written again as they're drawn
	}
	// what was the old grid's edge walled its water: run that again where it isn't level
	if (keep && was)
		for (int tz = 0; tz < g.tiles; tz++)
			for (int tx = 0; tx < g.tiles; tx++) {
				const int ox = tx + (want.X - old.X) / per, oz = tz + (want.Z - old.Z) / per;
				const bool edge = ox <= 0 || ox >= g.tiles - 1 || oz <= 0 || oz >= g.tiles - 1;
				const int t = tz * g.tiles + tx;
				if (edge && g.tile[t].known && uneven(g, t))
					wake(g, t);
			}
}

// A tile looked at: per node in it, the water the server has (the highest
// liquid in reach: the sea's top, a fill) and the ground under each column
void Layer::scan(Grid &g, int t, ClientMap &map)
{
	Tile &tile = g.tile[t];
	const int tx = t % g.tiles, tz = t / g.tiles, per = 16 / g.res;
	const s16 hi = s16(g.origin.Y + ABOVE), lo = s16(g.origin.Y - BELOW);
	struct Node
	{
		enum { OPEN, SOLID, SHAPED } kind;
		std::optional<shape::Shape> shape;
	};
	const bool fresh = !tile.known;
	bool moved = false;
	bool sea = false;
	float target = 0, above = 0;
	const float area = 1.0f / (g.res * g.res); // a column's, nodes²
	for (int nz = 0; nz < per; nz++)
		for (int nx = 0; nx < per; nx++) {
			const s16 x = s16(g.origin.X + tx * per + nx), z = s16(g.origin.Z + tz * per + nz);
			std::vector<Node> nodes;
			std::optional<v3s16> top;
			bool is_sea = false;
			float runs = 0;
			std::array<float, 2> way{};
			float surface = 0; // world sixteenths
			for (s16 y = hi; y >= lo; y--) {
				const v3s16 p(x, y, z);
				const MapNode n = map.getNode(p);
				if (n.getContent() == CONTENT_IGNORE) {
					// not here yet: looked at again when its block comes (it's in the changed blocks then)
					tile.scanned = true;
					return;
				}
				const ContentFeatures &f = m_ndef->get(n);
				Node node{ Node::OPEN, map.solidShape(p) };
				if (node.shape)
					node.kind = Node::SHAPED;
				else if (f.walkable && f.param_type_2 != CPT2_LEVELED)
					node.kind = Node::SOLID;
				if (!top) {
					const fill::Fill *fill = map.getFill(p);
					if (f.drawtype == NDT_LIQUID && itemgroup_get(f.groups, "liquid_layer")) {
						// full water: the sea's at or under its level (a reservoir), and a
						// river's or lake's held at its own (sprint 115); otherwise (blocks
						// of water placed) it's water like a fill's
						top = p;
						const bool held = n.getParam2() & fill::HELD;
						is_sea = (m_sea && y <= *m_sea) || held;
						surface = (y + 1) * 16.0f;
						// a river's level and speed as the weather fills it (sprint 115)
						if (held && m_rivers) {
							auto r = m_rivers->find(getNodeBlockPos(p));
							if (r != m_rivers->end()) {
								surface += r->second.rise;
								runs = r->second.speed;
								way = r->second.way;
							}
						}
						if (!tile.coloured || !tile.sea)
							tile.r = f.color.getRed(), tile.g = f.color.getGreen(), tile.b = f.color.getBlue();
					} else if (fill && fill->amount > 0) {
						top = p;
						surface = y * 16 + 16 * fill::Room::of(node.shape ? &*node.shape : nullptr).surface(fill->amount / 255.0f);
						if (!tile.coloured)
							tile.r = fill->r, tile.g = fill->g, tile.b = fill->b;
					}
					if (top)
						tile.coloured = true;
				}
				const bool ground = node.kind == Node::SOLID;
				nodes.push_back(std::move(node));
				if (ground && !top)
					break; // ground: no water of ours under it
			}
			std::optional<v3s16> &kept = g.tops[(tz * per + nz) * g.nodes + tx * per + nx];
			if (kept != top) {
				if (kept)
					m_unowned.insert(getNodeBlockPos(*kept));
				if (top)
					m_reflat.insert(getNodeBlockPos(*top));
			}
			kept = top;
			g.runs[(tz * per + nz) * g.nodes + tx * per + nx] = runs;
			g.ways[(tz * per + nz) * g.nodes + tx * per + nx] = way;
			sea = sea || is_sea;
			for (int iz = 0; iz < g.res; iz++)
				for (int ix = 0; ix < g.res; ix++) {
					const int c = (tz * 16 + nz * g.res + iz) * g.cols + tx * 16 + nx * g.res + ix;
					// the sixteenth at the column's middle
					const int sx = ix * 16 / g.res + 8 / g.res, sz = iz * 16 / g.res + 8 / g.res;
					s32 floor = NONE;
					for (size_t i = 0; i < nodes.size() && floor == NONE; i++) {
						const s32 y = hi - s32(i);
						if (nodes[i].kind == Node::SOLID)
							floor = (y + 1) * 16;
						else if (nodes[i].kind == Node::SHAPED)
							for (int l = 15; l >= 0; l--)
								if (nodes[i].shape->has(v3s16(sx, l, sz))) {
									floor = y * 16 + l + 1;
									break;
								}
					}
					if (floor == NONE && top)
						floor = lo * 16; // deep water: as far down as it looks is its bottom
					moved = moved || floor != g.floor[c];
					// the ground moved under water: its surface stays where it was (only
					// under water: a dry rut dug got water as deep as it, sprint 84)
					if (!fresh && floor != g.floor[c] && g.floor[c] != NONE && floor != NONE && g.depth[c] > FILM)
						g.depth[c] = std::max(0.0f, g.depth[c] + (g.floor[c] - floor) / 16.0f);
					g.floor[c] = floor;
					const s32 held_at = is_sea && floor != NONE ? s32(surface) : NONE;
					moved = moved || held_at != g.sea[c]; // a river risen or fallen (sprint 115): run to its new level
					g.sea[c] = held_at;
					g.want[c] = top && floor != NONE ? s32(std::lround(surface)) : NONE;
					if (floor == NONE) {
						g.depth[c] = 0;
						continue;
					}
					const float over = top ? std::max(0.0f, (surface - floor) / 16) : 0;
					if (!is_sea) {
						target += over * area;
						// counted as the coast's pools count what they hold: over the sea's level only
						const float under = g.sea_level == INT32_MIN ? 0 : std::max(0.0f, (g.sea_level - floor) / 16.0f);
						above += std::max(0.0f, over - under) * area;
					}
					if (fresh)
						g.depth[c] = over; // first seen: as the server has it
				}
		}
	// first seen beside the sea, its ground under the sea's level where the server has
	// water starts full to it, as the sea would fill it (else it starts dry and is seen filling: the shallows animating)
	if (fresh && g.sea_level != INT32_MIN) {
		bool beside = sea;
		for (int s = 0; s < 4 && !beside; s++) {
			const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
			beside = nx >= 0 && nx < g.tiles && nz >= 0 && nz < g.tiles && g.tile[nz * g.tiles + nx].sea;
		}
		for (int cz = 0; cz < 16 && beside; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int c = (tz * 16 + cz) * g.cols + tx * 16 + cx;
				if (g.floor[c] != NONE && g.want[c] != NONE && g.floor[c] < g.sea_level)
					g.depth[c] = std::max(g.depth[c], (g.sea_level - g.floor[c]) / 16.0f);
			}
	}
	// ... and a sea tile first seen does that for the tiles beside it seen before it
	if (fresh && sea && g.sea_level != INT32_MIN)
		for (int s = 0; s < 4; s++) {
			const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
			if (nx < 0 || nx >= g.tiles || nz < 0 || nz >= g.tiles)
				continue;
			Tile &n = g.tile[nz * g.tiles + nx];
			if (!n.known || n.sea)
				continue;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int c = (nz * 16 + cz) * g.cols + nx * 16 + cx;
					if (g.floor[c] != NONE && g.want[c] != NONE && g.floor[c] < g.sea_level
							&& g.depth[c] < (g.sea_level - g.floor[c]) / 16.0f) {
						g.depth[c] = (g.sea_level - g.floor[c]) / 16.0f;
						n.drawn = false;
					}
				}
		}
	moved = !fresh && (moved || std::fabs(target - tile.target) > SLACK);
	tile.target = target;
	tile.above = above;
	tile.sea = sea;
	tile.known = true;
	tile.scanned = true;
	if (fresh)
		tile.drawn = false;
	// first seen, it's as the server has it: run only if it doesn't meet its neighbours level
	if (moved) {
		tile.drawn = false;
		wake(g, t);
	}
}

// Whether a tile's water doesn't stand level with a known neighbour's along
// their edge (so it'd run): a tile that does is left asleep
bool Layer::uneven(Grid &g, int t)
{
	const int tx = t % g.tiles, tz = t / g.tiles;
	for (int s = 0; s < 4; s++) {
		const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
		if (nx < 0 || nx >= g.tiles || nz < 0 || nz >= g.tiles || !g.tile[nz * g.tiles + nx].known)
			continue;
		for (int i = 0; i < 16; i++) {
			// this tile's edge column on side s, and the one across
			const int cx = SIDES[s].X > 0 ? 15 : SIDES[s].X < 0 ? 0 : i, cz = SIDES[s].Z > 0 ? 15 : SIDES[s].Z < 0 ? 0 : i;
			const int c = (tz * 16 + cz) * g.cols + tx * 16 + cx, n = c + stepOf(g, s);
			if (g.floor[c] == NONE || g.floor[n] == NONE || (g.depth[c] <= 0 && g.depth[n] <= 0))
				continue;
			const float here = g.floor[c] / 16.0f + g.depth[c], there = g.floor[n] / 16.0f + g.depth[n];
			// water runs only downhill from what's wet
			if ((g.depth[c] > 0 && here - there > 1 / 32.0f) || (g.depth[n] > 0 && there - here > 1 / 32.0f))
				return true;
		}
	}
	return false;
}

void Layer::wake(Grid &g, int t)
{
	Tile &k = g.tile[t];
	k.awake = true;
	k.quiet = 0;
	if (k.stale) {
		k.stale = false;
		k.scanned = false;
	}
}

// The pools: tiles with water joined side by side, each held to the server as one
void Layer::pools(Grid &g)
{
	std::vector<int> pool(g.tile.size(), 0);
	int next = 0;
	auto wet = [&](int t) {
		const Tile &k = g.tile[t];
		return k.known && !k.sea && (k.target > 0 || k.volume > 1e-5f || k.credit > 0);
	};
	for (int t = 0; t < int(g.tile.size()); t++) {
		if (pool[t] || !wet(t))
			continue;
		const int id = ++next;
		std::vector<int> open{ t };
		pool[t] = id;
		while (!open.empty()) {
			const int q = open.back();
			open.pop_back();
			const int qx = q % g.tiles, qz = q / g.tiles;
			for (int n : { qx + 1 < g.tiles ? q + 1 : -1, qx > 0 ? q - 1 : -1, qz + 1 < g.tiles ? q + g.tiles : -1,
						 qz > 0 ? q - g.tiles : -1 })
				if (n >= 0 && !pool[n] && wet(n)) {
					pool[n] = id;
					open.push_back(n);
				}
		}
	}
	g.coast.assign(next + 1, 0);
	for (int t = 0; t < int(g.tile.size()); t++) {
		g.tile[t].pool = pool[t];
		const int tx = t % g.tiles, tz = t / g.tiles;
		for (int s = 0; s < 4 && pool[t]; s++) {
			const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
			if (nx >= 0 && nx < g.tiles && nz >= 0 && nz < g.tiles && g.tile[nz * g.tiles + nx].sea)
				g.coast[pool[t]] = 1;
		}
	}
}

// The rivers' currents: held water above the sea (a river's, sprint 115) is
// a staircase of levels, and runs toward its nearest step down: each column's
// way is down the distance to one, found across the grid. A run of one level
// longer than the grid sees has none, and a lake with no step lies still.
void Layer::currents(Grid &g)
{
	const int n = g.cols * g.cols;
	if (int(g.current.size()) != n)
		return;
	auto river = [&](int c) { return g.sea[c] != NONE && g.sea[c] > g.sea_level; };
	auto held = [&](int c) { return g.sea[c] != NONE; };
	std::vector<int> dist(n, -1), open;
	for (int c = 0; c < n; c++) {
		g.current[c] = {};
		if (!river(c))
			continue;
		const int x = c % g.cols, z = c / g.cols;
		for (int s = 0; s < 4; s++) {
			const int nx = x + SIDES[s].X, nz = z + SIDES[s].Z;
			if (nx < 0 || nx >= g.cols || nz < 0 || nz >= g.cols)
				continue;
			const int m = nz * g.cols + nx;
			if (held(m) && g.sea[m] < g.sea[c]) {
				dist[c] = 0;
				open.push_back(c);
				break;
			}
		}
	}
	// outward from the steps across each level's water
	for (size_t i = 0; i < open.size(); i++) {
		const int c = open[i], x = c % g.cols, z = c / g.cols;
		for (int s = 0; s < 4; s++) {
			const int nx = x + SIDES[s].X, nz = z + SIDES[s].Z;
			if (nx < 0 || nx >= g.cols || nz < 0 || nz >= g.cols)
				continue;
			const int m = nz * g.cols + nx;
			if (dist[m] < 0 && river(m) && g.sea[m] == g.sea[c]) {
				dist[m] = dist[c] + 1;
				open.push_back(m);
			}
		}
	}
	for (int c : open) {
		const int x = c % g.cols, z = c / g.cols;
		// how far on to a step beside it: a lower level's is past one (−1), what's
		// not this water as far as here
		auto on = [&](int nx, int nz) {
			if (nx < 0 || nx >= g.cols || nz < 0 || nz >= g.cols)
				return float(dist[c]);
			const int m = nz * g.cols + nx;
			if (held(m) && g.sea[m] < g.sea[c])
				return -1.0f;
			return dist[m] >= 0 && g.sea[m] == g.sea[c] ? float(dist[m]) : float(dist[c]);
		};
		const float vx = on(x - 1, z) - on(x + 1, z), vz = on(x, z - 1) - on(x, z + 1);
		const float len = std::hypot(vx, vz);
		// as fast as the server says its river runs; till it's said, CURRENT
		const int node = (z / g.res) * g.nodes + x / g.res;
		const float speed = g.runs.size() == size_t(g.nodes) * g.nodes && g.runs[node] > 0 ? g.runs[node] : CURRENT;
		if (len > 0)
			g.current[c] = { vx / len * speed, vz / len * speed };
	}
	// where the server's said which way its river runs (down its run), that
	// way: a long level reach has no step to run toward, and still flows
	if (g.ways.size() != size_t(g.nodes) * g.nodes)
		return;
	for (int c = 0; c < n; c++) {
		if (!river(c))
			continue;
		const int x = c % g.cols, z = c / g.cols, node = (z / g.res) * g.nodes + x / g.res;
		const auto &w = g.ways[node];
		if (w[0] == 0 && w[1] == 0)
			continue;
		const float speed = g.runs[node] > 0 ? g.runs[node] : CURRENT;
		g.current[c] = { w[0] * speed, w[1] * speed };
	}
}

// A step of the currents: what stands over or under a river's level (a wake,
// a splash) is carried downstream, from each column into the next of its
// level; the level itself is held, so still water stays as it is
void Layer::carry(Grid &g, float dt)
{
	if (g.current.size() != g.depth.size())
		return;
	const float L = 1.0f / g.res;
	std::vector<std::pair<int, float>> moves;
	for (int t = 0; t < int(g.tile.size()); t++) {
		if (!g.tile[t].awake || !g.tile[t].known)
			continue;
		const int tx = t % g.tiles, tz = t / g.tiles;
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int gx = tx * 16 + cx, gz = tz * 16 + cz, c = gz * g.cols + gx;
				const auto &u = g.current[c];
				if ((u[0] == 0 && u[1] == 0) || g.floor[c] == NONE)
					continue;
				const float rest = std::max(0.0f, (g.sea[c] - g.floor[c]) / 16.0f);
				const float extra = g.depth[c] - rest;
				for (int a = 0; a < 2; a++) {
					if (u[a] == 0)
						continue;
					const int s = a * 2 + (u[a] < 0);
					const int nx = gx + SIDES[s].X, nz = gz + SIDES[s].Z;
					if (nx < 0 || nx >= g.cols || nz < 0 || nz >= g.cols)
						continue;
					const int m = nz * g.cols + nx;
					if (g.floor[m] == NONE || g.sea[m] != g.sea[c])
						continue;
					float amount = extra * std::min(0.25f, std::fabs(u[a]) * dt / L);
					amount = std::clamp(amount, -g.depth[m], g.depth[c]);
					moves.push_back({ c, -amount });
					moves.push_back({ m, amount });
				}
			}
	}
	for (const auto &[c, by] : moves) {
		g.depth[c] = std::max(0.0f, g.depth[c] + by);
		const int t = tileOf(g, c);
		if (!g.tile[t].awake && std::fabs(by) > STILL)
			wake(g, t);
	}
}

// The server's amounts. The sea is a reservoir: its columns close on its
// level, whatever runs into the gaps below it. A pool that isn't the sea
// closes on the server's amount for it (what its tiles hold, and drops
// landed it hasn't sent back yet): water moves freely within it, and what's
// added goes in where its water is, evenly, so it spreads as it would.
void Layer::hold(Grid &g, float dtime)
{
	const float rate = std::min(1.0f, HOLD * dtime), sea_rate = std::min(1.0f, SEA_HOLD * dtime);
	const float area = 1.0f / (g.res * g.res);
	// a column's depth under the sea's level, the sea's: only where the
	// server has water (sprint 84: a rut dug under the sea's level on a beach
	// isn't the sea's; held as the sea's, it flooded and never drained)
	auto under = [&](int c) {
		return g.want[c] == NONE ? 0.0f : std::max(0.0f, (g.sea_level - g.floor[c]) / 16.0f);
	};
	struct Sum
	{
		float target = 0, volume = 0;
		int tiles = 0;
		bool sea = false;
	};
	std::unordered_map<int, Sum> sums;
	for (int t = 0; t < int(g.tile.size()); t++) {
		Tile &k = g.tile[t];
		if (!k.known)
			continue;
		const int first = (t / g.tiles) * 16 * g.cols + (t % g.tiles) * 16;
		if (k.sea && k.awake) {
			float changed = 0;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int c = first + cz * g.cols + cx;
					if (g.floor[c] == NONE)
						continue;
					float by;
					if (g.sea[c] != NONE) {
						// the sea's: at its level, up or down
						by = (std::max(0.0f, (g.sea[c] - g.floor[c]) / 16.0f) - g.depth[c]) * sea_rate;
					} else {
						// the land in a sea tile: what washed over it above the sea's level (and the
						// server's water there) drains; under it, it's the sea's (it ran in from it)
						const s32 cap = g.want[c] == NONE ? g.floor[c] : std::max(g.sea_level, g.want[c]);
						const float most = std::max(0.0f, (cap - g.floor[c]) / 16.0f);
						by = g.depth[c] > most ? (most - g.depth[c]) * sea_rate : 0;
					}
					g.depth[c] += by;
					changed = std::max(changed, std::fabs(by));
				}
			if (changed > STILL)
				k.quiet = 0;
		}
		k.credit *= std::max(0.0f, 1 - CREDIT_FADE * dtime);
		// beside the sea, water under its level is the sea's (it ran in from it): only what's above is held
		const bool coast = k.pool && k.pool < int(g.coast.size()) && g.coast[k.pool] && g.sea_level != INT32_MIN;
		if (k.awake || k.volume > 0 || k.target > 0) {
			float volume = 0;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int c = first + cz * g.cols + cx;
					volume += coast ? std::max(0.0f, g.depth[c] - under(c)) : g.depth[c];
				}
			k.volume = volume * area;
		}
		if (!k.pool)
			continue;
		Sum &sum = sums[k.pool];
		sum.target += (coast ? k.above : k.target) + k.credit;
		sum.volume += k.volume;
		sum.tiles++;
		sum.sea = sum.sea || k.sea;
	}
	for (int t = 0; t < int(g.tile.size()); t++) {
		Tile &k = g.tile[t];
		if (!k.known || !k.pool)
			continue;
		const Sum &sum = sums.at(k.pool);
		const float gap = sum.target - sum.volume, slack = SLACK * sum.tiles;
		if (std::fabs(gap) <= slack)
			continue;
		// never fast: a pool short by a lake fills over seconds, not a frame
		const float by = std::clamp((gap - std::copysign(slack, gap)) * rate, -HOLD_MOST * dtime * sum.tiles,
				HOLD_MOST * dtime * sum.tiles);
		float changed = 0;
		const int first = (t / g.tiles) * 16 * g.cols + (t % g.tiles) * 16;
		if (by > 0) {
			const float add = by * (sum.volume > 1e-6f ? k.volume / sum.volume : 1.0f / sum.tiles);
			if (add <= 0)
				continue;
			// into its water; dry, into its lowest ground
			int wet = 0;
			s32 lowest = INT_MAX;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int c = first + cz * g.cols + cx;
					if (g.floor[c] == NONE)
						continue;
					wet += g.depth[c] > FILM;
					lowest = std::min(lowest, g.floor[c]);
				}
			if (lowest == INT_MAX)
				continue;
			int into = wet;
			if (!wet)
				for (int cz = 0; cz < 16; cz++)
					for (int cx = 0; cx < 16; cx++)
						into += g.floor[first + cz * g.cols + cx] == lowest;
			const float each = add / area / into;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int c = first + cz * g.cols + cx;
					if (g.floor[c] != NONE && (wet ? g.depth[c] > FILM : g.floor[c] == lowest)) {
						g.depth[c] += each;
						changed += each;
					}
				}
		} else if (sum.volume > 0) {
			// what's above the sea's level drains, beside the sea (the rest is the sea's)
			const bool coast = k.pool < int(g.coast.size()) && g.coast[k.pool] && g.sea_level != INT32_MIN;
			const float keep = std::max(0.0f, (sum.volume + by) / sum.volume);
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					float &d = g.depth[first + cz * g.cols + cx];
					const float below = coast ? std::min(d, under(first + cz * g.cols + cx)) : 0;
					d = below + (d - below) * keep;
				}
			changed = k.volume * (1 - keep) / area;
		}
		// woken only by what it changed
		if (changed > STILL)
			wake(g, t);
	}
}

// A step of shallow water by virtual pipes, one per column edge (Mei,
// Decaudin & Hu 2007): a flow is sped by the difference in the surfaces
// either side and carried from step to step, so water has momentum (waves,
// sloshing), and no column gives more than it has. Waves travel as over a
// node's depth at most (the sea's deep, and slow is cheap).
void Layer::flow(Grid &g, float dt)
{
	const float L = 1.0f / g.res;
	std::vector<int> awake;
	for (int t = 0; t < int(g.tile.size()); t++)
		if (g.tile[t].awake && g.tile[t].known)
			awake.push_back(t);
	for (int t : awake) {
		const int tx = t % g.tiles, tz = t / g.tiles;
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int gx = tx * 16 + cx, gz = tz * 16 + cz, c = gz * g.cols + gx;
				auto &out = g.out[c];
				const float d = g.depth[c];
				if (g.floor[c] == NONE || d <= 0) {
					out = {};
					continue;
				}
				const bool edge[4] = { gx + 1 >= g.cols, gx == 0, gz + 1 >= g.cols, gz == 0 };
				bool open[4];
				float higher[4] = {};
				for (int s = 0; s < 4; s++) {
					const int n = c + stepOf(g, s);
					open[s] = !edge[s] && g.floor[n] != NONE;
					if (open[s])
						higher[s] = (g.floor[c] - g.floor[n]) / 16.0f + (d - g.depth[n]);
				}
				shallow::flowOut(out, d, dt, L, open, higher);
			}
	}
	for (int t : awake) {
		const int tx = t % g.tiles, tz = t / g.tiles;
		float moved = 0;
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				const int c = (tz * 16 + cz) * g.cols + tx * 16 + cx;
				for (int s = 0; s < 4; s++) {
					const float o = g.out[c][s];
					if (o <= 0)
						continue;
					const int n = c + stepOf(g, s);
					const float amount = o * dt / L;
					g.depth[c] -= amount;
					g.depth[n] += amount;
					moved = std::max(moved, amount);
					// into another tile: it's woken (calm deep water by more), and takes this one's colour if it has none
					const int nt = tileOf(g, n);
					if (nt != t && amount > (g.depth[c] > 1 ? STILL * DEEP_STILL : STILL)) {
						Tile &there = g.tile[nt];
						if (!there.awake)
							wake(g, nt);
						if (!there.coloured) {
							there.r = g.tile[t].r, there.g = g.tile[t].g, there.b = g.tile[t].b;
							there.coloured = true;
						}
					}
				}
			}
		Tile &k = g.tile[t];
		k.quiet = moved < STILL ? k.quiet + dt : 0;
	}
	for (int t : awake) {
		Tile &k = g.tile[t];
		const int first = (t / g.tiles) * 16 * g.cols + (t % g.tiles) * 16;
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++) {
				float &d = g.depth[first + cz * g.cols + cx];
				d = std::max(0.0f, d);
			}
		const int per = 16 / g.res;
		const float away = std::hypot(g.origin.X + (t % g.tiles + 0.5f) * per - m_camera.X,
				g.origin.Z + (t / g.tiles + 0.5f) * per - m_camera.Z);
		if (k.quiet > (away > NEAR ? SLEEP_FAR : SLEEP)) {
			k.awake = false;
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++)
					g.out[first + cz * g.cols + cx] = {};
		}
	}
}

// What moves through water drives the columns it's in at its own speed
// (a bow wave ahead, a trough behind); falling in throws it outward
void Layer::push(Grid &g, const Mover &m, v3f v, float dtime)
{
	const int reach = int(std::ceil(m.radius * g.res));
	const int at = columnAt(g, m.feet.X, m.feet.Z);
	if (at < 0)
		return;
	for (int dz = -reach; dz <= reach; dz++)
		for (int dx = -reach; dx <= reach; dx++) {
			const int cx = at % g.cols + dx, cz = at / g.cols + dz;
			if (cx < 0 || cx >= g.cols || cz < 0 || cz >= g.cols || (dx * dx + dz * dz) > reach * reach)
				continue;
			const int c = cz * g.cols + cx;
			if (g.floor[c] == NONE || g.depth[c] <= 0)
				continue;
			const float surface = g.floor[c] / 16.0f - 0.5f + g.depth[c];
			if (surface < m.feet.Y - 0.1f || surface > m.feet.Y + m.height)
				continue;
			for (int s = 0; s < 4; s++) {
				const float ahead = std::max(0.0f, v.X * SIDES[s].X + v.Z * SIDES[s].Z) + std::max(0.0f, -v.Y) * 0.5f;
				// toward the flow it'd have at the mover's speed, never past it
				const float want = ahead * std::max(g.depth[c], 2 / 16.0f); // thin water's thrown too
				if (want > g.out[c][s])
					g.out[c][s] += (want - g.out[c][s]) * std::min(1.0f, WAKE * dtime);
			}
			wake(g, tileOf(g, c));
		}
}

// Liquid poured: a drop a cup, thrown to land where the server pours it
void Layer::pour(const std::vector<Stream> &streams)
{
	std::uniform_real_distribution<float> jitter(-1, 1), early(0, 0.08f);
	for (const Stream &s : streams) {
		if (s.cups == 0)
			continue;
		const int n = std::min<int>(s.cups, 48);
		const float cups = float(s.cups) / n;
		const v3f way = s.to - s.from;
		const float t = std::clamp(way.getLength() / 7, 0.15f, 0.5f);
		v3f v = way / t;
		v.Y += 0.5f * G * t;
		for (int i = 0; i < n && m_drops.size() < MOST_DROPS; i++) {
			Drop d;
			d.vel = v + v3f(jitter(m_random), jitter(m_random), jitter(m_random)) * 0.25f;
			d.pos = s.from + v3f(jitter(m_random), jitter(m_random), jitter(m_random)) * 0.03f +
					d.vel * early(m_random); // poured through the frame, not at its start
			d.cups = cups;
			d.age = 0;
			d.r = s.r, d.g = s.g, d.b = s.b;
			d.splash = false;
			m_drops.push_back(d);
		}
	}
}

// A drop over a grid lands on its water or the ground under it, adding its
// cup to the column it hits, throwing water out round it (and, landing
// fast, a splash of smaller drops); its water's the tile's till the
// server's amount takes over.
int Layer::land(Grid &g, Drop &d, std::vector<Drop> &thrown)
{
	const int c = columnAt(g, d.pos.X, d.pos.Z);
	if (c < 0 || g.floor[c] == NONE)
		return -1;
	const float surface = g.floor[c] / 16.0f - 0.5f + g.depth[c];
	if (d.pos.Y > surface)
		return 0;
	std::uniform_real_distribution<float> jitter(-1, 1);
	const float speed = d.vel.getLength();
	float kept = d.cups;
	if (!d.splash && g.depth[c] > 0 && speed > 3.5f && thrown.size() + m_drops.size() < MOST_DROPS) {
		for (int k = 0; k < 2; k++) {
			Drop s = d;
			s.splash = true;
			s.cups = d.cups * 0.25f;
			s.age = 0;
			s.vel = v3f(jitter(m_random), 0, jitter(m_random)) * speed * 0.2f +
					v3f(0, speed * (0.2f + 0.15f * jitter(m_random)), 0);
			s.pos.Y = surface + 0.02f;
			thrown.push_back(s);
		}
		kept = d.cups * 0.5f;
	}
	g.depth[c] += kept / 4096 * g.res * g.res; // a cup is a sixteenth-cube
	const int t = tileOf(g, c);
	Tile &tile = g.tile[t];
	tile.credit += kept / 4096;
	if (!tile.coloured) {
		tile.r = d.r, tile.g = d.g, tile.b = d.b;
		tile.coloured = true;
	}
	for (int s = 0; s < 4; s++)
		g.out[c][s] += SPLASH * speed;
	wake(g, t);
	return 1;
}

// Drops fall, landing in the fine grid, else the coarse; beyond both, one
// landing is gone (the server's fill shows it)
void Layer::fall(ClientMap &map, float dtime)
{
	std::vector<Drop> thrown;
	const int steps = std::max(1, int(std::ceil(std::min(dtime, 0.1f) * 60)));
	const float dt = std::min(dtime, 0.1f) / steps;
	for (int i = 0; i < steps; i++)
		m_drops.erase(std::remove_if(m_drops.begin(), m_drops.end(), [&](Drop &d) {
			d.vel.Y -= G * dt;
			d.pos += d.vel * dt;
			d.age += dt;
			if (d.age > DROP_LIFE)
				return true;
			if (m_gpu) {
				const v3s16 at(std::lround(d.pos.X), std::lround(d.pos.Y), std::lround(d.pos.Z));
				const int landed = land(covers(at) ? m_fine : m_coarse, d, thrown);
				if (landed >= 0)
					return landed == 1;
			}
			const v3s16 p(std::lround(d.pos.X), std::lround(d.pos.Y), std::lround(d.pos.Z));
			const MapNode n = map.getNode(p);
			if (n.getContent() == CONTENT_IGNORE)
				return true;
			const ContentFeatures &cf = m_ndef->get(n);
			if (cf.drawtype == NDT_LIQUID)
				return true; // into the sea
			if (const fill::Fill *there = map.getFill(p)) {
				const std::optional<shape::Shape> solid = map.solidShape(p);
				if (d.pos.Y < p.Y - 0.5f + fill::Room::of(solid ? &*solid : nullptr).surface(there->amount / 255.0f))
					return true;
			}
			const int x = std::clamp(int((d.pos.X - (p.X - 0.5f)) * 16), 0, 15);
			const int y = std::clamp(int((d.pos.Y - (p.Y - 0.5f)) * 16), 0, 15);
			const int z = std::clamp(int((d.pos.Z - (p.Z - 0.5f)) * 16), 0, 15);
			if (const std::optional<shape::Shape> solid = map.solidShape(p))
				return solid->has(v3s16(x, y, z));
			return cf.walkable;
		}), m_drops.end());
	m_drops.insert(m_drops.end(), thrown.begin(), thrown.end());
}

// Where each column's top is drawn, on a sixteenth, moving only when its
// water's well past it (still water stays still on screen); any water at
// all is a sixteenth deep at least (a puddle thinner than the grid is a
// sheet, not specks)
bool Layer::settle(Grid &g, int t)
{
	bool moved = false;
	const int first = (t / g.tiles) * 16 * g.cols + (t % g.tiles) * 16;
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

// A tile's columns' ripples and foam into the height texture (client/liquidlook.h)
void Layer::surfaceTexels(Grid &g, int t)
{
	const int first = (t / g.tiles) * 16 * g.cols + (t % g.tiles) * 16;
	const auto top = [&](int c) { return g.floor[c] == NONE || g.depth[c] <= 0 ? -1e9f : g.floor[c] / 16.0f + g.depth[c]; };
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx, gx = c % g.cols, gz = c / g.cols;
			const std::array<float, 4> beside = { gx + 1 < g.cols ? top(c + 1) : -1e9f, gx > 0 ? top(c - 1) : -1e9f,
				gz + 1 < g.cols ? top(c + g.cols) : -1e9f, gz > 0 ? top(c - g.cols) : -1e9f };
			const look::Surface s = look::surface(top(c), beside, 1.0f / g.res, g.out[c], g.depth[c]);
			g.texels[c * 4 + 2] = s.light;
			g.texels[c * 4 + 3] = s.foam;
		}
	g.upload = true;
}

// A tile's columns into the height texture: each top and ground, over the grid's base
void Layer::texels(Grid &g, int t, ClientMap &map, u32 daynight)
{
	Tile &tile = g.tile[t];
	tile.drawn = true;
	const s32 base = g.origin.Y * 16;
	const int first = (t / g.tiles) * 16 * g.cols + (t % g.tiles) * 16;
	float mean = 0;
	int wet = 0;
	s32 one = g.shown[first];
	float low = 1e9f, high = -1e9f;
	bool level = true;
	for (int cz = 0; cz < 16; cz++)
		for (int cx = 0; cx < 16; cx++) {
			const int c = first + cz * g.cols + cx;
			const s32 shown = g.shown[c], floor = g.floor[c];
			g.texels[c * 4] = shown == NONE ? NONE_TEXEL : float(shown - base);
			g.texels[c * 4 + 1] = floor == NONE ? NONE_TEXEL : float(floor - base);
			level = level && shown == one && shown != NONE && shown - floor >= 16; // deep: shaded alike either way
			if (shown != NONE) {
				mean += shown - base;
				wet++;
				high = std::max(high, float(shown - base));
				low = std::min(low, float(floor - base));
			}
		}
	surfaceTexels(g, t);
	tile.low = low - 16; // a side stands down to its neighbour's ground, which may be lower
	tile.high = high;
	tile.wet = wet > 0;
	tile.level = level;
	if (wet) {
		tile.mean = mean / wet;
		// lit by the air just over its highest top, the brightest of a few places in it
		// (one place at its mean height was often inside the shore's sand: black)
		const int per = 16 / g.res;
		const s16 y = s16(std::floor((high + base) / 16.0f));
		float shade = 0;
		for (const v2s16 at : { v2s16(per / 2, per / 2), v2s16(0, 0), v2s16(per - 1, 0), v2s16(0, per - 1), v2s16(per - 1, per - 1) })
			shade = std::max(shade, shadeAt(map, v3s16(g.origin.X + (t % g.tiles) * per + at.X, y, g.origin.Z + (t / g.tiles) * per + at.Y), daynight));
		tile.shade = shade;
	}
	g.upload = true;
}

// A grid's wet tiles: a level one whose neighbours stand no lower as one
// quad, the rest column by column; the coarse grid's not where the fine is
void Layer::draw(Grid &g, video::IVideoDriver *driver)
{
	if (!g.placed)
		return;
	m_draw->grid[0] = (g.origin.X - 0.5f - m_offset.X) * BS;
	m_draw->grid[1] = (g.origin.Y - 0.5f - m_offset.Y) * BS;
	m_draw->grid[2] = (g.origin.Z - 0.5f - m_offset.Z) * BS;
	m_draw->grid[3] = g.cols;
	m_draw->colour[3] = g.res;
	m_gpu_material.TextureLayers[0].Texture = g.heights;
	const int per = 16 / g.res;
	const scene::SViewFrustum *frustum = SceneManager->getActiveCamera()->getViewFrustum();
	for (int t = 0; t < int(g.tile.size()); t++) {
		const Tile &tile = g.tile[t];
		if (!tile.wet || !tile.drawn)
			continue;
		const int tx = t % g.tiles, tz = t / g.tiles;
		// not drawn where the camera isn't looking
		const v3f lo(m_draw->grid[0] + tx * per * BS, m_draw->grid[1] + tile.low / 16 * BS, m_draw->grid[2] + tz * per * BS);
		const aabb3f box(lo, lo + v3f(per * BS, (tile.high - tile.low) / 16 * BS, per * BS));
		bool seen = true;
		for (int i = 0; i < scene::SViewFrustum::VF_PLANE_COUNT && seen; i++)
			seen = box.classifyPlaneRelation(frustum->planes[i]) != core::ISREL3D_FRONT;
		if (!seen)
			continue;
		if (&g == &m_coarse) {
			bool fine = true;
			for (int nz = 0; nz < per && fine; nz++)
				for (int nx = 0; nx < per && fine; nx++)
					fine = covers(v3s16(g.origin.X + tx * per + nx, 0, g.origin.Z + tz * per + nz));
			if (fine)
				continue; // the fine grid's
		}
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
		driver->setMaterial(m_gpu_material);
		driver->drawMeshBuffer(whole ? m_whole : m_shape);
	}
}

// Drops as sixteenth-cubes, in nodes from the scene's origin
void Layer::buildDrops(ClientMap &map, u32 daynight)
{
	scene::SMeshBuffer *mesh = emptied(m_drop_mesh);
	auto &vertices = mesh->Vertices->Data;
	auto &indices = mesh->Indices->Data;
	const v3f origin = intToFloat(m_offset, 1);
	static const float H = 1 / 32.0f;
	// five faces (it's seen from above and the sides), each corners and a normal
	static const v3f FACES[5][5] = {
		{ { -H, H, -H }, { -H, H, H }, { H, H, H }, { H, H, -H }, { 0, 1, 0 } },
		{ { H, -H, -H }, { H, H, -H }, { H, H, H }, { H, -H, H }, { 1, 0, 0 } },
		{ { -H, -H, H }, { -H, H, H }, { -H, H, -H }, { -H, -H, -H }, { -1, 0, 0 } },
		{ { H, -H, H }, { H, H, H }, { -H, H, H }, { -H, -H, H }, { 0, 0, 1 } },
		{ { -H, -H, -H }, { -H, H, -H }, { H, H, -H }, { H, -H, -H }, { 0, 0, -1 } },
	};
	for (const Drop &d : m_drops) {
		if (vertices.size() + 20 > 65535)
			break;
		const v3s16 p(std::lround(d.pos.X), std::lround(d.pos.Y), std::lround(d.pos.Z));
		const float shade = shadeAt(map, p, daynight);
		const v3f at = d.pos - origin;
		for (int f = 0; f < 5; f++) {
			const float by = shade * (f == 0 ? 1.1f : 0.85f);
			const video::SColor colour(DROP_ALPHA, u32(std::min(255.0f, d.r * by)), u32(std::min(255.0f, d.g * by)),
					u32(std::min(255.0f, d.b * by)));
			const u16 first = u16(vertices.size());
			for (int c = 0; c < 4; c++)
				vertices.emplace_back((at + FACES[f][c]) * BS, FACES[f][4], colour, v2f(0, 0));
			for (u16 i : { 0, 1, 2, 2, 3, 0 })
				indices.push_back(first + i);
		}
	}
	mesh->recalculateBoundingBox();
	mesh->setDirty();
}

void Layer::OnRegisterSceneNode()
{
	if (IsVisible)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);
	ISceneNode::OnRegisterSceneNode();
}

void Layer::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	if (SceneManager->getSceneNodeRenderPass() != scene::ESNRP_TRANSPARENT)
		return;
	driver->setMaterial(m_material);
	for (const auto &[bp, b] : m_blocks) {
		if (!b.mesh || b.mesh->getIndexCount() == 0)
			continue;
		// as ClientMap culls its blocks: by the block's middle and reach
		const v3f middle = intToFloat(bp * MAP_BLOCKSIZE, 1) + v3f(MAP_BLOCKSIZE * 0.5f - 0.5f);
		if (middle.getDistanceFrom(m_camera) > m_range + BLOCK_REACH)
			continue;
		core::matrix4 world;
		world.setTranslation(intToFloat(bp * MAP_BLOCKSIZE - m_offset, BS));
		driver->setTransform(video::ETS_WORLD, world);
		driver->drawMeshBuffer(b.mesh);
	}
	if (m_gpu) {
		driver->setTransform(video::ETS_WORLD, core::matrix4());
		draw(m_coarse, driver);
		draw(m_fine, driver);
		driver->setMaterial(m_material);
	}
	if (m_drop_mesh && m_drop_mesh->getIndexCount() > 0) {
		driver->setTransform(video::ETS_WORLD, core::matrix4());
		driver->drawMeshBuffer(m_drop_mesh);
	}
}

} // namespace liquids
