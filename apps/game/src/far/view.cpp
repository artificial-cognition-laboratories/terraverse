// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "view.h"
#include "far/mesh.h"
#include "far/tiles.h"
#include "client/atlas_view.h"
#include "client/camera.h"
#include "client/clientmap.h"
#include "client/shader.h"
#include "client/texturesource.h"
#include "constants.h"
#include "exceptions.h"
#include "log.h"
#include "mapblock.h"
#include "nodedef.h"
#include "settings.h"
#include "terragen/planet.h"
#include <ICameraSceneNode.h>
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
	float haze = 1;     // distance past the cut over which the air's haze builds, near the ground
	float share = 0;    // 0 down in the air's haze .. 1 above the air, seeing the planet
	float sea = 0;      // the scene y of the sea
	float place[4] = {}; // the eye: map x, z (blocks), altitude (m), unused
	float air = 1;      // m: the air's scale height
};

class Uniforms : public IShaderUniformSetterRC
{
public:
	explicit Uniforms(std::shared_ptr<const DrawState> state) : m_state(std::move(state)) {}

	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		m_cut_vertex.set(&m_state->cut, services);
		m_cut_pixel.set(&m_state->cut, services);
		m_haze.set(&m_state->haze, services);
		m_share.set(&m_state->share, services);
		m_sea.set(&m_state->sea, services);
		m_place_vertex.set(m_state->place, services);
		m_place_pixel.set(m_state->place, services);
		m_air.set(&m_state->air, services);
	}

private:
	std::shared_ptr<const DrawState> m_state;
	CachedVertexShaderSetting<float> m_cut_vertex{"farCut"};
	CachedPixelShaderSetting<float> m_cut_pixel{"farCut"};
	CachedPixelShaderSetting<float> m_haze{"farHaze"};
	CachedPixelShaderSetting<float> m_share{"farShare"};
	CachedVertexShaderSetting<float> m_sea{"farSea"};
	CachedVertexShaderSetting<float, 4> m_place_vertex{"farPlace"};
	CachedPixelShaderSetting<float, 4> m_place_pixel{"farPlace"};
	CachedPixelShaderSetting<float> m_air{"farAir"};
};

static void settingChanged(const std::string &, void *data)
{
	static_cast<View *>(data)->readSettings();
}

static const char *const SETTINGS[] = { "far_view", "far_view_detail", "far_view_max" };

View::View(scene::ISceneManager *smgr, IShaderSource *ssrc, ITextureSource *tsrc,
		const NodeDefManager *ndef, const Horizon &horizon, const planet::Atlas &atlas, int body) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1),
	m_water_level(horizon.water_level),
	m_radius(atlas.body(body).params().radius),
	m_palette(std::make_unique<Palette>(Palette::live(ndef, tsrc, blocksIn(horizon.content)))),
	m_tiles(std::make_unique<Tiles>(atlas, body, horizon, *m_palette, Tiles::threadsFor(0.5f))), // half the machine: the horizon is what a world is first seen by
	m_state(std::make_shared<DrawState>())
{
	m_uniforms = make_irr<Uniforms>(m_state);
	u32 shader = ssrc->getShader("far_globe_shader", ShaderConstants{}, video::EMT_SOLID, m_uniforms.get());
	m_material.MaterialType = ssrc->getShaderInfo(shader).material;
	m_material.BackfaceCulling = false;
	m_material.FogEnable = false; // the shader fogs to the horizon itself
	m_material.UseMipMaps = false;

	// the mask: rewritten as the near world loads, so no mipmaps to go stale
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
}

void View::update(const Camera &camera, ClientMap &map, float near_range, bool range_all,
		std::optional<float> fog_limit)
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
	m_reach = std::max(range, horizon) * BS + m_altitude * BS;
	m_showing = m_enabled && !range_all && range > m_cut;
	if (!m_showing)
		return;

	// a block is a metre: the height counts toward a tile's distance as is
	const Eye at{ { o.x / centre * sea, o.y / centre * sea, o.z / centre * sea }, m_altitude, m_cut, range, m_split };
	m_tiles->update(at);
	m_eye = eye;
	m_culled = camera.getFrustumCuller();
}

void View::OnRegisterSceneNode()
{
	if (IsVisible && m_showing)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SKY_BOX);
	ISceneNode::OnRegisterSceneNode();
}

void View::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	scene::ICameraSceneNode *camera = SceneManager->getActiveCamera();
	if (!camera || SceneManager->getSceneNodeRenderPass() != scene::ESNRP_SKY_BOX)
		return;

	const v3f origin = camera->getAbsolutePosition() - m_eye * BS; // the scene position of map node (0, 0, 0)
	const v3f offset = m_eye * BS - camera->getAbsolutePosition(); // a scene position plus this is a map position
	m_state->cut = m_cut * BS;
	m_state->haze = std::max(m_max - m_cut, 1.0f) * BS;
	m_state->share = m_share;
	m_state->sea = origin.Y + m_water_level * BS;
	m_state->place[0] = m_eye.X;
	m_state->place[1] = m_eye.Z;
	m_state->place[2] = m_altitude;
	m_state->air = (float)terragen::sphere(m_radius).scale_height;

	// Share the camera's depth projection with near blocks and clouds. The
	// latter draw later and must be tested against the actual far terrain.
	driver->setMaterial(m_material);
	for (const Tiles::Drawn &tile : m_tiles->drawn()) {
		const std::optional<core::matrix4> world = m_view->place(tile.middle);
		if (!world)
			continue;
		// a tile out of the camera's sight isn't drawn (the scene is the
		// camera's offset from the map: the culler takes map positions)
		aabb3f box(world->getTranslation());
		for (const auto &part : tile.parts) {
			aabb3f b = part->getBoundingBox();
			world->transformBoxEx(b);
			box.addInternalBox(b);
		}
		if (m_culled(box.getCenter() + offset, box.getExtent().getLength() / 2))
			continue;
		driver->setTransform(video::ETS_WORLD, *world);
		for (const auto &part : tile.parts)
			driver->drawMeshBuffer(part.get());
	}
}

} // namespace far
