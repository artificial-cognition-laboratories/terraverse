// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/trace/trace.h"
#include "src/client/views/game/far/view.h"
#include "src/client/views/game/far/tiles/mask.h"
#include "src/client/views/game/far/land/mesh.h"
#include "src/client/views/game/far/tiles/tiles.h"
#include "src/client/views/game/sky/atlas_view.h"
#include "src/client/views/game/camera.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/render/shaders/shader.h"
#include "src/client/render/textures/texturesource.h"
#include "src/shared/base/constants.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/platform/system/profiler.h"
#include "src/shared/platform/settings/settings.h"
#include "library/world/terragen/src/system/planet.h"
#include <ICameraSceneNode.h>
#include <SViewFrustum.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace far {

// A tile splits while it's nearer than this many of its own spans past the
// near world's edge, by the Horizon detail setting (1..4).
static const float SPLIT[4] = { 1.0f, 1.5f, 2.0f, 3.0f };
// The air's depth, in its scale heights: above it the haze has given way
// to the planet seen from above.
static constexpr double AIR_DEPTH = 8;
// What the shader needs while the far view draws (scene units unless said).
struct DrawState
{
	float cut = 0;
	float share = 0;    // 0 down in the air's haze .. 1 above the air, seeing the planet
	float sea = 0;      // the scene y of the sea
	float place[4] = {}; // the eye: map x, z (blocks), altitude (m), unused
	float air = 1;      // m: the air's scale height
	float behind = 0;   // 1: a stand-in, behind everything
	float parts = 0;    // what of a tile's drawn: 0 the land, 1 its skirts, 2 both (far/mesh.cpp)
	float level = -1;   // with far_view_debug_levels: the tile being drawn's level, to tint it by (−1: none)
	// the near world's columns in the tile's region (far/mask.h): 0 none
	float mask = 0;
	float mask_shift[2] = {}; // the tile's grid position to the mask's, cells (+½: a cell's edge)
	float mask_size[2] = { 1, 1 }; // texels
	float mask_base = 0;     // the block the mask's heights are from
	float layer = 0;         // a height over the sea (blocks) to the map's y, just under a face on it
	float range = 0;         // scene units: past this from the eye nothing's drawn, whatever the tile's level
	float show_mask = 0;     // far_view_debug_mask: what the mask drops painted magenta, not dropped
};

class Uniforms : public IShaderUniformSetterRC
{
public:
	explicit Uniforms(std::shared_ptr<const DrawState> state) : m_state(std::move(state)) {}

	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		m_cut_pixel.set(&m_state->cut, services);
		m_share.set(&m_state->share, services);
		m_sea.set(&m_state->sea, services);
		m_place_vertex.set(m_state->place, services);
		m_place_pixel.set(m_state->place, services);
		m_air.set(&m_state->air, services);
		m_behind.set(&m_state->behind, services);
		m_parts.set(&m_state->parts, services);
		m_level.set(&m_state->level, services);
		m_mask.set(&m_state->mask, services);
		m_mask_shift.set(m_state->mask_shift, services);
		m_mask_size.set(m_state->mask_size, services);
		m_mask_base.set(&m_state->mask_base, services);
		m_layer.set(&m_state->layer, services);
		m_range.set(&m_state->range, services);
		m_show_mask.set(&m_state->show_mask, services);
	}

private:
	std::shared_ptr<const DrawState> m_state;
	CachedPixelShaderSetting<float> m_cut_pixel{"farCut"};
	CachedPixelShaderSetting<float> m_share{"farShare"};
	CachedVertexShaderSetting<float> m_sea{"farSea"};
	CachedVertexShaderSetting<float, 4> m_place_vertex{"farPlace"};
	CachedPixelShaderSetting<float, 4> m_place_pixel{"farPlace"};
	CachedPixelShaderSetting<float> m_air{"farAir"};
	CachedPixelShaderSetting<float> m_behind{"farBehind"};
	CachedPixelShaderSetting<float> m_parts{"farParts"};
	CachedPixelShaderSetting<float> m_level{"farLevel"};
	CachedPixelShaderSetting<float> m_mask{"farMask"};
	CachedPixelShaderSetting<float, 2> m_mask_shift{"farMaskShift"};
	CachedPixelShaderSetting<float, 2> m_mask_size{"farMaskSize"};
	CachedPixelShaderSetting<float> m_mask_base{"farMaskBase"};
	CachedPixelShaderSetting<float> m_layer{"farLayer"};
	CachedPixelShaderSetting<float> m_range{"farRange"};
	CachedPixelShaderSetting<float> m_show_mask{"farShowMask"};
};

static void settingChanged(const std::string &, void *data)
{
	static_cast<View *>(data)->readSettings();
}

static const char *const SETTINGS[] = { "far_view", "far_view_detail", "far_view_max", "far_view_debug_levels", "far_view_debug_mask" };

// How far from the eye (blocks) the nearest part of a map block column is
// that the near world doesn't draw, up to `most`: none of the far land is
// nearer. A column it draws is open at every block of it it doesn't have
// (flying, the air round the eye is loaded before the ground under it).
static float openFrom(const Coverage &coverage, v3f eye, float most)
{
	const auto &columns = coverage.columns();
	const s32 reach = (s32)std::ceil(most / MAP_BLOCKSIZE) + 1;
	const s32 ex = (s32)std::floor(eye.X / MAP_BLOCKSIZE), ez = (s32)std::floor(eye.Z / MAP_BLOCKSIZE);
	float open = most;
	for (s32 dz = -reach; dz <= reach; dz++)
		for (s32 dx = -reach; dx <= reach; dx++) {
			const v2s16 c((s16)(ex + dx), (s16)(ez + dz));
			// the nearest point of its square (a node's centre is whole: its edge ±½)
			const float x0 = c.X * MAP_BLOCKSIZE - 0.5f, z0 = c.Y * MAP_BLOCKSIZE - 0.5f;
			const float nx = std::clamp(eye.X, x0, x0 + MAP_BLOCKSIZE), nz = std::clamp(eye.Z, z0, z0 + MAP_BLOCKSIZE);
			const float across = std::hypot(eye.X - nx, eye.Z - nz);
			// up or down to the nearest block of it the near world doesn't have
			float up = 0;
			const auto it = columns.find(c);
			if (it != columns.end()) {
				const std::vector<s16> &ys = it->second;
				const auto has = [&](s32 y) { return std::find(ys.begin(), ys.end(), y) != ys.end(); };
				const s32 own = (s32)std::floor((eye.Y + 0.5f) / MAP_BLOCKSIZE);
				if (has(own)) {
					s32 below = own, above = own;
					while (has(below - 1))
						below--;
					while (has(above + 1))
						above++;
					up = std::min(eye.Y - (below * MAP_BLOCKSIZE - 0.5f), (above + 1) * MAP_BLOCKSIZE - 0.5f - eye.Y);
				}
			}
			open = std::min(open, std::hypot(across, up));
		}
	return open;
}

// The share of the way to the nearest far land the near plane may sit at.
// `m_open` is a distance from the eye; the near plane cuts by depth along the
// view, and land at a screen corner is only cos(its angle) as deep: taken as
// half the distance, far land off to the side was clipped, the stand-ins
// behind showing through it as flat slabs as you flew.
static float nearShare(const scene::ICameraSceneNode *camera)
{
	const float tan_y = std::tan(camera->getFOV() / 2), tan_x = tan_y * camera->getAspectRatio();
	const float corner = std::atan(std::hypot(tan_x, tan_y)); // the widest angle on screen
	return 0.9f * std::cos(corner);
}

View::View(scene::ISceneManager *smgr, IShaderSource *ssrc, ITextureSource *tsrc,
		const NodeDefManager *ndef, const Horizon &horizon, const planet::Atlas &atlas, int body) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1),
	m_water_level(horizon.water_level),
	m_radius(atlas.body(body).params().radius),
	m_grids(atlas.body(body).grids()),
	m_palette(std::make_unique<Palette>(Palette::live(ndef, tsrc, blocksIn(horizon.content)))),
	m_tiles(std::make_unique<Tiles>(atlas, body, horizon, *m_palette, Tiles::threadsFor(0.5f), smgr->getVideoDriver())), // half the machine: the horizon is what a world is first seen by
	m_state(std::make_shared<DrawState>()),
	m_mask(std::make_unique<Mask>(smgr->getVideoDriver(), atlas, body))
{
	m_uniforms = make_irr<Uniforms>(m_state);
	u32 shader = ssrc->getShader("far_globe_shader", ShaderConstants{}, video::EMT_SOLID, m_uniforms.get());
	m_material.MaterialType = ssrc->getShaderInfo(shader).material;
	m_material.BackfaceCulling = false;
	m_material.FogEnable = false; // the shader fogs to the horizon itself
	m_material.UseMipMaps = false;
	// the mask (far/mask.h): a texel a column, never blended
	video::SMaterialLayer &layer = m_material.TextureLayers[0];
	layer.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
	layer.MagFilter = video::ETMAGF_NEAREST;
	layer.TextureWrapU = video::ETC_CLAMP_TO_EDGE;
	layer.TextureWrapV = video::ETC_CLAMP_TO_EDGE;

	// drawn whatever the camera looks at: the tiles surround it
	setAutomaticCulling(scene::EAC_OFF);

	readSettings();
	for (const char *name : SETTINGS)
		g_settings->registerChangedCallback(name, &settingChanged, this);
	infostream << "far view: " << horizon.code << ", "
			<< horizon.content.species.size() << " tree species" << std::endl;
}

View::~View()
{
	g_settings->deregisterAllChangedCallbacks(this);
	m_tiles.reset(); // before the palette its workers read
}

void View::readSettings()
{
	m_enabled = g_settings->getBool("far_view");
	int detail = std::clamp((int)std::lround(g_settings->getFloat("far_view_detail")), 1, 4);
	m_split = SPLIT[detail - 1];
	m_max = g_settings->getFloat("far_view_max");
	m_debug_levels = g_settings->getBool("far_view_debug_levels");
	m_debug_mask = g_settings->getBool("far_view_debug_mask");
}

void View::update(const Camera &camera, ClientMap &map, float near_range, bool range_all,
		std::optional<float> fog_limit, v3f velocity)
{
	const v3f eye = (camera.getCameraNode()->getPosition() + intToFloat(camera.getOffset(), BS)) / BS;
	m_view = map.atlasView();
	const std::optional<planet::Atlas::Frame> frame = m_view ? m_view->eye() : std::nullopt;
	m_showing = false;
	if (!frame)
		return; // the eye has no grid this frame (seam): nothing is drawn from it

	// the eye over the sea, and how high it is
	const terragen::Atlas::Vec &o = frame->origin;
	const double centre = std::sqrt(o.x * o.x + o.y * o.y + o.z * o.z);
	const double r = m_radius, sea = r + m_water_level + 1;
	m_altitude = (float)std::max(centre - sea, 0.0);
	const double h = m_altitude;
	// up through the air the haze gives way to the planet seen from above
	m_share = (float)std::clamp(h / (AIR_DEPTH * terragen::sphere(r).scale_height), 0.0, 1.0);

	// Out to the horizon and no farther: past it the planet hides the land,
	// but for peaks, which show over it as far again as they'd see the
	// eye's sea from (on a small planet a fixed distance is most of the way
	// round: the whole shell drawn, nearly all of it behind the ground).
	// The eye's own horizon is always reached; the setting caps the peaks'.
	const float horizon = (float)std::sqrt(2 * r * h + h * h); // m
	const double peak = Tiles::RELIEF;
	const float over = (float)std::sqrt(2 * r * peak + peak * peak);
	float range = std::min({ horizon + over, std::max(m_max, 1.1f * horizon), (float)(M_PI * r) });
	if (fog_limit)
		range = std::min(range, *fog_limit);
	m_cut = near_range;
	m_range = std::hypot(range, m_altitude);
	m_reach = std::max(range, horizon) * BS + m_altitude * BS;
	m_showing = m_enabled && !range_all && range > m_cut;
	if (!m_showing)
		return;
	m_map = &map; // the mask's made from it as it's drawn (render)

	// a block is a metre: the height counts toward a tile's distance as is
	// The detail goes by the player's viewing range, never the near world's
	// range this frame: that jumps as a flight changes tier (Body::nearRange),
	// and tiles chosen by it flipped between whole and split, a coarse one
	// standing over the land and gone again (held a few seconds, in bursts).
	m_detail_cut = std::max(m_cut, (float)g_settings->getS16("viewing_range"));
	const Eye at{ { o.x / centre * sea, o.y / centre * sea, o.z / centre * sea }, m_altitude, m_detail_cut, range, m_split, true };
	Eye moving = at;
	if (const float pace = velocity.getLength(); pace > 1)
		for (int i = 0; i < 3; i++) {
			const float share = (i == 0 ? velocity.X : i == 1 ? velocity.Y : velocity.Z) / pace;
			moving.heading.x += frame->axes[i].x * share;
			moving.heading.y += frame->axes[i].y * share;
			moving.heading.z += frame->axes[i].z * share;
		}
	ScopeProfiler timed(g_profiler, "Far view update [us]", SPT_GRAPH_ADD, PRECISION_MICRO);
	m_tiles->update(moving);
	// what it chose (the debugger's counters): stand-ins mean holes the backdrop shows through
	g_profiler->avg("Far view: tiles drawn [#]", m_tiles->drawn().size());
	g_profiler->avg("Far view: stand-ins [#]", m_tiles->behind().size());
	g_profiler->avg("Far view: tiles wanted [#]", m_tiles->pending());
	m_eye = eye;
}

void View::OnRegisterSceneNode()
{
	if (IsVisible && m_showing)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SKY_BOX);
	ISceneNode::OnRegisterSceneNode();
}

void View::render()
{
	TF_ZONE("Far view render"); // Terra Firma: inside Draw3D, timed (the debugger)
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	scene::ICameraSceneNode *camera = SceneManager->getActiveCamera();
	if (!camera || SceneManager->getSceneNodeRenderPass() != scene::ESNRP_SKY_BOX)
		return;

	const v3f origin = camera->getAbsolutePosition() - m_eye * BS; // the scene position of map node (0, 0, 0)
	// The near world's columns as they are now, not at update(): the draw list
	// is made again after it (Game), and a frame behind, a block just drawn by
	// the near world was drawn by this too, its land a few blocks off over it,
	// and one just let go was drawn by neither, a hole.
	m_material.setTexture(0, nullptr); // (the material lets go of the mask's old textures before they go)
	m_mask->update(m_map->coverage(), (s16)std::floor((m_eye.Y + 0.5f) / MAP_BLOCKSIZE));
	m_open = openFrom(m_map->coverage(), m_eye, m_cut);
	// The camera's frustum as it draws this frame (its sides; the near and far
	// planes aren't the far view's). Not Camera::getFrustumCuller's, taken in
	// update(): the node's frustum is made as it renders, so that was the last
	// frame's, against this frame's camera offset. Flying, as the offset stepped,
	// it was a step off for a frame and culled tiles in plain sight: holes, the
	// neighbours' skirts standing round them.
	const scene::SViewFrustum &frustum = *camera->getViewFrustum();
	const auto culled = [&](const v3f &centre, f32 radius) {
		for (int p : { scene::SViewFrustum::VF_LEFT_PLANE, scene::SViewFrustum::VF_RIGHT_PLANE,
					 scene::SViewFrustum::VF_BOTTOM_PLANE, scene::SViewFrustum::VF_TOP_PLANE })
			if (frustum.planes[p].getDistanceTo(centre) > radius)
				return true;
		return false;
	};
	m_state->cut = m_cut * BS;
	m_state->range = m_range * BS;
	m_state->show_mask = m_debug_mask ? 1 : 0;
	m_state->share = m_share;
	m_state->sea = origin.Y + m_water_level * BS;
	m_state->place[0] = m_eye.X;
	m_state->place[1] = m_eye.Z;
	m_state->place[2] = m_altitude;
	m_state->air = (float)terragen::sphere(m_radius).scale_height;

	// The near world's columns aren't drawn (far/mask.h): it draws them after,
	// and the far land shows wherever it doesn't, a column at a time.
	m_state->layer = m_water_level + 0.99f;
	const video::ITexture *bound = nullptr;
	bool set = false;
	const auto masked = [&](const TileKey &key) {
		const Mask::Face *f = m_mask->face(key.face);
		m_state->mask = f ? 1 : 0;
		video::ITexture *texture = f ? f->texture : nullptr;
		if (f) {
			const terragen::Atlas::Grid &g = m_grids.at(key.face);
			// in double: grid positions are large, their differences small
			m_state->mask_shift[0] = (float)((double)g.low[0] + (double)key.x * key.span() + 0.5 - (double)f->i0);
			m_state->mask_shift[1] = (float)((double)g.low[1] + (double)key.z * key.span() + 0.5 - (double)f->j0);
			m_state->mask_size[0] = (float)f->width;
			m_state->mask_size[1] = (float)f->height;
			m_state->mask_base = f->base;
		}
		if (!set || texture != bound) {
			m_material.setTexture(0, texture);
			driver->setMaterial(m_material);
			bound = texture;
			set = true;
		}
	};
	// Each tile's place and whether it's in sight, found once a frame for the
	// passes below (each found them again: three times over every tile, most of
	// the far view's time drawing). `near`: how near its nearest point may be.
	struct Placed
	{
		const Tiles::Drawn *tile;
		core::matrix4 world;
		float near;
	};
	const auto placed = [&](const std::vector<Tiles::Drawn> &tiles) {
		std::vector<Placed> out;
		out.reserve(tiles.size());
		for (const Tiles::Drawn &tile : tiles) {
			const std::optional<core::matrix4> world = m_view->place(tile.middle);
			if (!world)
				continue;
			// a tile out of the camera's sight isn't drawn (in the scene, as the frustum is)
			aabb3f box(world->getTranslation());
			for (const auto &part : tile.parts) {
				aabb3f b = part->getBoundingBox();
				world->transformBoxEx(b);
				box.addInternalBox(b);
			}
			const float reach = box.getExtent().getLength() / 2;
			if (culled(box.getCenter(), reach))
				continue;
			out.push_back({ &tile, *world, box.getCenter().getDistanceFrom(camera->getAbsolutePosition()) - reach });
		}
		return out;
	};
	// `under`: the near world's ground, unmasked, for the backdrop (only near it)
	const auto draw = [&](const std::vector<Placed> &tiles, bool under) {
		for (const Placed &p : tiles) {
			if (under && p.near > m_state->cut)
				continue;
			driver->setTransform(video::ETS_WORLD, p.world);
			masked(p.tile->key);
			if (under)
				m_state->mask = 0;
			m_state->level = m_debug_levels ? (float)p.tile->key.level : -1; // (the shader's told at each draw)
			for (const auto &part : p.tile->parts)
				driver->drawMeshBuffer(part.get());
		}
	};
	const std::vector<Tiles::Drawn> behind_tiles = m_tiles->behind(), drawn_tiles = m_tiles->drawn();
	std::vector<Placed> behind, drawn;
	{
		TF_ZONE("Far view: place");
		behind = placed(behind_tiles);
		drawn = placed(drawn_tiles);
	}
	// The backdrop: what shows only through gaps in the land, its depth dropped
	// after, so it can never cover any. The stand-ins (where a finer tile isn't
	// built yet) and every tile's skirts (far/mesh.cpp), in the far view's own
	// depth (in the camera's, a tenth of a block near, kilometres off their own
	// walls came through their tops, flickering); then, in the camera's, the
	// land under the near world, which shows only through the cracks between
	// its blocks' meshes.
	const core::matrix4 projection = driver->getTransform(video::ETS_PROJECTION);
	core::matrix4 own;
	own.buildProjectionMatrixPerspectiveFovLH(camera->getFOV(), camera->getAspectRatio(),
			std::max(m_open * nearShare(camera) * BS, 0.1f * BS), std::max(camera->getFarValue(), m_reach));
	driver->setMaterial(m_material);
	driver->setTransform(video::ETS_PROJECTION, own);
	{
		TF_ZONE("Far view: backdrop");
		m_state->behind = 1;
		m_state->parts = 2;
		draw(behind, false);
		m_state->behind = 0;
		m_state->parts = 1;
		draw(drawn, false);
	}
	driver->clearBuffers(video::ECBF_DEPTH);
	driver->setTransform(video::ETS_PROJECTION, projection);
	{
		TF_ZONE("Far view: under");
		m_state->parts = 2;
		draw(drawn, true);
	}
	m_state->mask = 0;
	driver->clearBuffers(video::ECBF_DEPTH);
	// The land, alone (no stand-ins, no skirts: a stand-in drawn level with it
	// stood higher than the finer land beside it and hid it, flat slabs coming
	// and going as tiles were built; a skirt, a wall round a tile not built
	// yet), in its own depth: against the camera's near plane, a tenth of a
	// block, kilometres off a tile's walls behind its ground came through.
	// The near world draws over it, and only where it has land.
	driver->setTransform(video::ETS_PROJECTION, own);
	m_state->parts = 0;
	{
		TF_ZONE("Far view: land");
		draw(drawn, false);
	}
	driver->setTransform(video::ETS_PROJECTION, projection);
	driver->clearBuffers(video::ECBF_DEPTH);
}

} // namespace far
