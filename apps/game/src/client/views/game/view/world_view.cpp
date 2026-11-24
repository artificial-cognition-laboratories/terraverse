// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later
// What Game did each frame to show the world (Game::updateFrame, drawScene, updateClouds,
// updateShadows and its sky's client events), moved here whole (sprint 126).

#include "src/client/views/game/view/world_view.h"
#include "src/client/app/clientevent.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/render/renderingengine.h"
#include "src/client/render/shaders/shader.h"
#include "src/client/render/textures/texturesource.h"
#include "src/client/views/game/camera.h"
#include "src/client/views/game/far/view.h"
#include "src/client/views/game/sky/clouds.h"
#include "src/client/views/game/sky/particles.h"
#include "src/client/views/game/sky/sky.h"
#include "src/client/views/planetarium/space/sky.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/world/session/client.h"
#include "src/client/world/session/clientenvironment.h"
#include "src/client/world/sim/water/liquids.h"
#include "src/shared/base/constants.h"
#include "src/shared/game/environment/skyparams.h"
#include "src/shared/map/light/light.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/util/math/numeric.h"
#include "src/shared/util/text/string.h"
#include <ICameraSceneNode.h>
#include <IVideoDriver.h>
#include "src/shared/platform/system/profiler.h"
#include <cmath>

WorldView::WorldView(Opts o) :
	m_client(o.client),
	m_camera(o.camera),
	m_engine(o.engine),
	m_shaders(o.shaders),
	m_textures(o.textures),
	m_draw(o.draw),
	m_clouds(make_irr<Clouds>(o.engine.get_scene_manager(), &o.shaders, -1, myrand())),
	m_sky(make_irr<Sky>(-1, &o.engine, &o.textures, &o.shaders))
{}

WorldView::~WorldView()
{
	m_clouds.reset();
	// the scene manager holds these too
	if (m_space)
		m_space->remove();
	m_space.reset();
	if (m_far)
		m_far->remove();
	m_far.reset();
	if (m_liquids)
		m_liquids->remove();
	m_liquids.reset();
	m_sky.reset();
}

bool WorldView::fogEnabled() const
{
	// the player's setting only takes effect if the fog distance is unlimited, or with the debug priv
	if (m_sky->getFogDistance() < 0 || m_client.checkPrivilege("core:debug"))
		return g_settings->getBool("enable_fog"); // read live: the fog key toggles it
	return true;
}

void WorldView::light(Client &client, const ClientEvent &event)
{
	if (event.type == CE_SET_SKY)
		client.getEnv().setSolar(event.set_sky->solar); // Terra Firma: the real sun, for the light
	else if (event.type == CE_OVERRIDE_DAY_NIGHT_RATIO)
		client.getEnv().setDayNightRatioOverride(event.override_day_night_ratio.do_override,
				event.override_day_night_ratio.ratio_f * 1000.0f);
}

bool WorldView::heard(ClientEvent &event)
{
	light(m_client, event);
	switch (event.type) {
	case CE_SET_SKY: {
		const SkyboxParams &set = *event.set_sky;
		m_sky->setVisible(false);
		// whether clouds are visible in front of a custom skybox
		m_sky->setCloudsEnabled(set.clouds);
		// clear the old textures out in case we switch rendering type
		m_sky->clearSkyboxTextures();
		if (set.type == "regular") {
			// the mesh skybox, its colours
			m_sky->setVisible(true);
			m_sky->setSkyColors(set.sky_color);
			m_sky->setHorizonTint(set.fog_sun_tint, set.fog_moon_tint, set.fog_tint_type);
		} else if (set.type == "skybox" && set.textures.size() == 6) {
			// no dynamic mesh skybox: the fog's colours, sunrise and sunset tinting, six textures
			m_sky->setVisible(false);
			m_sky->setFallbackBgColor(set.bgcolor);
			m_sky->setHorizonTint(set.fog_sun_tint, set.fog_moon_tint, set.fog_tint_type);
			for (int i = 0; i < 6; i++)
				m_sky->addTextureToSkybox(set.textures[i], i, &m_textures);
		} else {
			// everything else a plain colour
			if (set.type != "plain")
				infostream << "Unknown sky type: " << set.type << std::endl;
			m_sky->setVisible(false);
			m_sky->setFallbackBgColor(set.bgcolor);
			// no directional sun or moon tinting on a plain or unknown sky
			m_sky->setHorizonTint(set.bgcolor, set.bgcolor, "custom");
		}
		m_sky->setBodyOrbitTilt(set.body_orbit_tilt);
		// Terra Firma: the real sun, for the sky (and the light: `light`)
		m_sky->setSolar(set.solar);
		// fog: a server's distance never overrides a smaller one of the player's
		m_sky->setFogDistance(set.fog_distance);
		// the fog distance reset: back to the player's viewing range
		if (set.fog_distance < 0)
			m_draw.wanted_range = g_settings->getS16("viewing_range");
		if (set.fog_start >= 0)
			m_sky->setFogStart(rangelim(set.fog_start, 0.0f, 0.99f));
		else
			m_sky->setFogStart(rangelim(g_settings->getFloat("fog_start"), 0.0f, 0.99f));
		m_sky->setFogColor(set.fog_color);
		m_sky->setAutoCaveBrightness(set.auto_dim_skybox);
		delete event.set_sky;
		return true;
	}
	case CE_SET_SUN:
		m_sky->setSunVisible(event.sun_params->visible);
		m_sky->setSunTexture(event.sun_params->texture, event.sun_params->tonemap, &m_textures);
		m_sky->setSunScale(event.sun_params->scale);
		m_sky->setSunriseVisible(event.sun_params->sunrise_visible);
		m_sky->setSunriseTexture(event.sun_params->sunrise, &m_textures);
		delete event.sun_params;
		return true;
	case CE_SET_MOON:
		m_sky->setMoonVisible(event.moon_params->visible);
		m_sky->setMoonTexture(event.moon_params->texture, event.moon_params->tonemap, &m_textures);
		m_sky->setMoonScale(event.moon_params->scale);
		delete event.moon_params;
		return true;
	case CE_SET_STARS:
		m_sky->setStarsVisible(event.star_params->visible);
		m_sky->setStarCount(event.star_params->count);
		m_sky->setStarColor(event.star_params->starcolor);
		m_sky->setStarScale(event.star_params->scale);
		m_sky->setStarDayOpacity(event.star_params->day_opacity);
		m_sky->setStarSeed(event.star_params->star_seed);
		delete event.star_params;
		return true;
	case CE_OVERRIDE_DAY_NIGHT_RATIO:
		return true; // the light alone (`light`)
	case CE_CLOUD_PARAMS:
		m_clouds->setDensity(event.cloud_params.density);
		m_clouds->setColorBright(video::SColor(event.cloud_params.color_bright));
		m_clouds->setColorAmbient(video::SColor(event.cloud_params.color_ambient));
		m_clouds->setColorShadow(video::SColor(event.cloud_params.color_shadow));
		m_clouds->setHeight(event.cloud_params.height);
		m_clouds->setThickness(event.cloud_params.thickness);
		m_clouds->setSpeed(v2f(event.cloud_params.speed_x, event.cloud_params.speed_y));
		return true;
	default:
		return false;
	}
}

void WorldView::offsetMoved(v3s16 offset)
{
	m_clouds->updateCameraOffset(offset);
}

void WorldView::ensurePlanet()
{
	// Terra Firma: the far view, once the server says this is a terra world. Made after the sky,
	// so it draws right after it. The planet comes before the horizon (Server::SendPlanet). On an
	// atlas world it's the body the player stands on: made again when that's another (kept while
	// they're in seam, between tiles).
	if (!m_client.getHorizon() || !m_client.getAtlas())
		return;
	const v3f feet = m_client.getEnv().getLocalPlayer()->getPosition() / BS;
	const std::optional<int> on = m_client.getAtlas()->bodyAt(planet::Atlas::moverNode(feet.X, feet.Y, feet.Z));
	if (m_far && on && *on != m_far_body) {
		warningstream << "sky: the eye changed body, " << m_far_body << " to " << *on << std::endl;
		m_space->remove(); // the scene manager holds them too
		m_space.reset();
		m_far->remove();
		m_far.reset();
	}
	if (!m_far && on) {
		scene::ISceneManager *smgr = m_engine.get_scene_manager();
		// the sky first: the pass draws them in the order they're made
		m_space = make_irr<planetarium::Sky>(smgr, &m_shaders, *m_client.getAtlas(), *m_client.getHorizon(), *on);
		m_far = make_irr<far::View>(smgr, &m_shaders, &m_textures, m_client.ndef(), *m_client.getHorizon(),
				*m_client.getAtlas(), *on);
		m_space->palette(m_far->palette());
		m_far_body = *on;
	}
}

void WorldView::step(float dtime, const Frame &frame)
{
	LocalPlayer *player = m_client.getEnv().getLocalPlayer();
	ClientMap &map = m_client.getEnv().getClientMap();

	// fog range
	if (m_sky->getFogDistance() >= 0)
		m_draw.wanted_range = MYMIN(m_draw.wanted_range, m_sky->getFogDistance());
	if (m_draw.range_all && m_sky->getFogDistance() < 0)
		m_fog_range = FOG_RANGE_ALL;
	else
		m_fog_range = m_draw.wanted_range * BS;

	ensurePlanet();
	if (m_space)
		m_space->update(m_camera, map, m_client.getEnv().getDayCount() + m_client.getEnv().getTimeOfDayF());
	// Terra Firma: land fades into the air's own horizon
	if (m_space)
		m_sky->setFogColor(m_space->fog().value_or(video::SColor(0, 0, 0, 0))); // (no alpha: the sky's own)
	// Terra Firma: liquid lying in the world (sprint 81), drawn on its own
	if (!m_liquids)
		m_liquids = make_irr<liquids::Layer>(liquids::Layer::Opts{ /*smgr*/ m_engine.get_scene_manager(), /*ndef*/ m_client.ndef(), /*shaders*/ &m_shaders });
	{
		// what pushes water aside (sprint 82): the player and the objects near
		std::vector<liquids::Mover> movers;
		movers.push_back({ 0, player->getPosition() / BS, 0.35f, 1.7f });
		std::vector<DistanceSortedActiveObject> near;
		m_client.getEnv().getActiveObjects(player->getPosition(), 32 * BS, near);
		for (const DistanceSortedActiveObject &o : near) {
			aabb3f box{ v3f(0) };
			if (o.obj->isLocalPlayer() || !o.obj->getSelectionBox(&box))
				continue;
			const v3f at = o.obj->getPosition() / BS;
			movers.push_back({ u32(o.obj->getId()) + 1, at + v3f(0, box.MinEdge.Y / BS, 0),
					std::max(box.getExtent().X, box.getExtent().Z) / (2 * BS), box.getExtent().Y / BS });
		}
		ScopeProfiler water_timed(g_profiler, "Water all [us]", SPT_GRAPH_ADD, PRECISION_MICRO);
		m_liquids->update(m_client.takeLiquidsChanged(), m_client.takeLiquidGround(), m_client.takeStreams(), movers, m_client.rivers(),
				map, m_camera.getOffset(), m_camera.getPosition() / BS,
				m_client.getHorizon() ? std::optional<s16>(m_client.getHorizon()->water_level) : std::nullopt,
				m_client.getEnv().getDayNightRatio(), dtime, player->getSpeed().getLength() / BS > 30);
	}
	if (m_far) {
		std::optional<float> fog_limit;
		if (m_sky->getFogDistance() >= 0)
			fog_limit = m_sky->getFogDistance();
		m_far->update(m_camera, map, m_draw.wanted_range, m_draw.range_all, fog_limit, player->getSpeed() / BS);
		if (m_far->showing())
			m_fog_range = m_far->reach();
	}

	// general brightness
	const u32 daynight_ratio = m_client.getEnv().getDayNightRatio();
	const float time_brightness = decode_light_f((float)daynight_ratio / 1000.0f);
	float direct_brightness;
	bool sunlight_seen;
	if (!m_sky->getAutoCaveBrightness() || frame.see_through) {
		direct_brightness = time_brightness;
		sunlight_seen = true;
	} else {
		const float old_brightness = m_sky->getBrightness();
		direct_brightness = map.getBackgroundBrightness(MYMIN(m_fog_range * 1.2, 60 * BS),
				daynight_ratio, (int)(old_brightness * 255.5), &sunlight_seen) / 255.0;
	}

	// the time of day, smoothed
	float time_of_day_smooth = m_time_of_day_smooth;
	const float time_of_day = m_client.getEnv().getTimeOfDayF();
	static const float maxsm = 0.05f;
	static const float todsm = 0.05f;
	if (std::fabs(time_of_day - time_of_day_smooth) > maxsm &&
			std::fabs(time_of_day - time_of_day_smooth + 1.0) > maxsm &&
			std::fabs(time_of_day - time_of_day_smooth - 1.0) > maxsm)
		time_of_day_smooth = time_of_day;
	if (time_of_day_smooth > 0.8 && time_of_day < 0.2)
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm) + (time_of_day + 1.0) * todsm;
	else
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm) + time_of_day * todsm;
	m_time_of_day_smooth = time_of_day_smooth;

	// Terra Firma: the date the smoothed time belongs to, which lags across midnight
	const float smooth_lag = time_of_day_smooth - time_of_day;
	m_sky->setDay(m_client.getEnv().getDayCount() + (smooth_lag > 0.5f ? -1 : smooth_lag < -0.5f ? 1 : 0));
	m_sky->update(time_of_day_smooth, time_brightness, direct_brightness,
			sunlight_seen, m_camera.getCameraMode(), player->getYaw(), player->getPitch());

	updateClouds(dtime);
	m_client.getParticleManager()->step(dtime);

	/*
		Terra Firma: the block draw list is made again when what it shows changes: the camera
		into another block (ClientMap::updateCamera), turned much, or the offset moved at once;
		meshes come or gone soon after (at most every MESHES_DELTA: flying, they come every
		frame); and every second whatever happens. It was every 200 ms besides, a spike each
		time over every block loaded.
	*/
	m_update_draw_list_timer += dtime;
	m_touch_blocks_timer += dtime;
	constexpr float update_draw_list_delta = 1.0f;
	constexpr float meshes_delta = 0.25f;
	constexpr float touch_mapblock_delta = 4.0f;
	const v3f camera_direction = m_camera.getDirection();
	// only one of updateDrawList, touchMapBlocks or updateShadows a frame (the else-ifs are meant)
	if (m_update_draw_list_timer >= update_draw_list_delta
			|| m_update_draw_list_last_cam_dir.getDistanceFrom(camera_direction) > 0.2f
			|| frame.offset_moved
			|| map.needsUpdateDrawList()
			|| (map.haveMeshesChanged() && m_update_draw_list_timer >= meshes_delta)) {
		map.updateDrawList();
		m_update_draw_list_timer = 0;
		m_update_draw_list_last_cam_dir = camera_direction;
	} else if (m_touch_blocks_timer > touch_mapblock_delta) {
		map.touchMapBlocks();
		m_touch_blocks_timer = 0;
	} else if (RenderingEngine::get_shadow_renderer()) {
		updateShadows();
	}
}

void WorldView::updateClouds(float dtime)
{
	if (!m_sky->getCloudsVisible()) {
		m_clouds->setVisible(false);
		return;
	}
	m_clouds->setVisible(true);
	m_clouds->step(dtime);
	// the camera's position isn't enough for a third-person camera
	v3f camera_node_position = m_camera.getCameraNode()->getPosition();
	const v3s16 camera_offset = m_camera.getOffset();
	camera_node_position.X = camera_node_position.X + camera_offset.X * BS;
	camera_node_position.Y = camera_node_position.Y + camera_offset.Y * BS;
	camera_node_position.Z = camera_node_position.Z + camera_offset.Z * BS;
	m_clouds->update(camera_node_position, m_sky->getCloudColor());
	if (m_clouds->isCameraInsideCloud() && fogEnabled()) {
		// inside a cloud, with fog: its colours are the sky's, and it isn't drawn
		const video::SColor clouds_dark = m_clouds->getColor().getInterpolated(video::SColor(255, 0, 0, 0), 0.9);
		m_sky->overrideColors(clouds_dark, m_clouds->getColor());
		m_sky->setInClouds(true);
		m_fog_range = std::fmin(m_fog_range * 0.5f, 32.0f * BS);
		m_clouds->setVisible(false);
	}
}

void WorldView::updateShadows()
{
	ShadowRenderer *shadow = RenderingEngine::get_shadow_renderer();
	if (!shadow)
		return;
	const auto &lighting = m_client.getEnv().getLocalPlayer()->getLighting();
	shadow->setShadowTint(lighting.shadow_tint);
	const float offset_constant = 10000.0f;
	v3f light;
	if (const std::optional<planetarium::Sky::Light> real = m_space ? m_space->light() : std::nullopt) {
		// Terra Firma: the stock sun's hidden on a planet; the real star casts them
		shadow->setShadowIntensity(real->strength);
		light = real->way;
	} else if (lighting.shadow_direction.getLengthSQ() > 0.0f) {
		// a custom shadow direction: no sun or moon visibility check
		shadow->setShadowIntensity(lighting.shadow_intensity);
		light = lighting.shadow_direction;
	} else {
		const bool is_day = m_sky->getSunDirection().Y > 0.0f;
		const bool is_shadow_visible = is_day ? m_sky->getSunVisible() : m_sky->getMoonVisible();
		shadow->setShadowIntensity(is_shadow_visible ? lighting.shadow_intensity : 0.0f);
		light = is_day ? m_sky->getSunDirection() : m_sky->getMoonDirection();
	}
	shadow->getDirectionalLight().setDirection(light * offset_constant);
	shadow->setTimeOfDay(std::fmod(m_sky->getLightTimeOfDay(), 1.0f));
	shadow->getDirectionalLight().updateFrustum(&m_camera, &m_client);
}

void WorldView::fog()
{
	video::IVideoDriver *driver = m_engine.get_video_driver();
	const video::SColor fog_color = m_sky->getFogColor();
	if (fogEnabled())
		driver->setFog(fog_color, video::EFT_FOG_LINEAR, m_fog_range * m_sky->getFogStart(), m_fog_range * 1.0f,
				0.f /* unused */, false /* pixel fog */, true /* range fog */);
	else
		driver->setFog(fog_color, video::EFT_FOG_LINEAR, FOG_RANGE_ALL, FOG_RANGE_ALL + 100 * BS,
				0.f /* unused */, false /* pixel fog */, false /* range fog */);
}

video::SColor WorldView::skyColor() const
{
	return m_sky->getSkyColor();
}

void WorldView::draw(bool show_hud, bool wield, bool crosshair)
{
	m_engine.draw_scene(m_sky->getSkyColor(), show_hud, wield, crosshair);
}
