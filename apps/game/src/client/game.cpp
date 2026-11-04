// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "game_internal.h"
#include "client/solar.h"
#include "piece/frame.h"
#include "piece/header.h"
#include "piece/machine.h"

#include <cmath>
#include <set>
#include <ISceneCollisionManager.h>
#include <csignal>
#include "client/atlas_view.h" // Terra Firma
#include "physics/tuning.h"
#include "client/body.h" // Terra Firma
#include "client/gameui.h"
#include "client/inputhandler.h"
#include "client/texturepaths.h"
#include "client/keys.h"
#include "client/mapblock_mesh.h"
#include "client/sound.h"
#include "clientmap.h"
#include "clientmedia.h" // For clientMediaUpdateCacheCopy
#include "config.h"
#include "content_cao.h"
#include "content/subgames.h"
#include "client/event_manager.h"
#include "fontengine.h"
#include "itemdef.h"
#include "gameparams.h"
#include "gettext.h"
#include "gui/guiChatConsole.h"
#include "texturesource.h"
#include "gui/mainmenumanager.h"
#include "gui/profilergraph.h"
#include "localplayer.h"
#include "minimap.h"
#include "network/networkexceptions.h"
#include "nodedef.h"         // Needed for determining pointing to nodes
#include "nodemetadata.h"
#include "particles.h"
#include "porting.h"
#include "profiler.h"
#include "raycast.h"
#include "server.h"
#include "settings.h"
#include "shader.h"
#include "sound_maker.h"
#include "threading/lambda.h"
#include "translation.h"
#include "util/basic_macros.h"
#include "util/directiontables.h"
#include "util/quicktune_shortcutter.h"
#include "version.h"
#include "script/scripting_client.h"
#include "hud.h"
#include <AnimatedMeshSceneNode.h>
#include <ICameraSceneNode.h>
#include "util/tracy_wrapper.h"
#include "item_visuals_manager.h"
#include "config.h"
#if USE_SHELL
#include "shell/shell.h"
#include "shell/social.h"
#include "online/gate.h"
#include "online/players.h"
#endif

#if USE_SOUND
	#include "client/sound/sound_openal.h"
#endif

typedef s32 SamplerLayer_t;


class GameGlobalShaderUniformSetter : public IShaderUniformSetter
{
	Sky *m_sky;
	Client *m_client;

	CachedVertexShaderSetting<float> m_animation_timer_vertex{"animationTimer"};
	CachedPixelShaderSetting<float> m_animation_timer_pixel{"animationTimer"};
	CachedVertexShaderSetting<float>
		m_animation_timer_delta_vertex{"animationTimerDelta"};
	CachedPixelShaderSetting<float>
		m_animation_timer_delta_pixel{"animationTimerDelta"};
	int m_crack_animation_length_i;
	CachedPixelShaderSetting<float> m_crack_animation_length{"crackAnimationLength"};
	int m_crack_level_i = -1;
	CachedPixelShaderSetting<float> m_crack_level{"crackLevel"};
	int m_crack_texture_scale_i = 0;
	CachedPixelShaderSetting<float> m_crack_texture_scale{"crackTextureScale"};
	CachedPixelShaderSetting<float, 3> m_day_light{"dayLight"};
	CachedPixelShaderSetting<float, 3> m_minimap_yaw{"yawVec"};
	CachedPixelShaderSetting<float, 3> m_camera_offset_pixel{"cameraOffset"};
	CachedVertexShaderSetting<float, 3> m_camera_offset_vertex{"cameraOffset"};
	CachedPixelShaderSetting<float, 3> m_camera_position_pixel{"cameraPosition"};
	CachedVertexShaderSetting<float, 3> m_camera_position_vertex{"cameraPosition"};
	CachedVertexShaderSetting<float, 2> m_texel_size0_vertex{"texelSize0"};
	CachedPixelShaderSetting<float, 2> m_texel_size0_pixel{"texelSize0"};
	v2f m_texel_size0;

	CachedStructPixelShaderSetting<float, 7> m_exposure_params_pixel{
		"exposureParams",
		std::array<const char*, 7> {
			"luminanceMin", "luminanceMax", "exposureCorrection",
			"speedDarkBright", "speedBrightDark", "centerWeightPower",
			"compensationFactor"
		}};
	float m_user_exposure_compensation;
	bool m_bloom_enabled;
	CachedPixelShaderSetting<float> m_bloom_intensity_pixel{"bloomIntensity"};
	CachedPixelShaderSetting<float> m_bloom_strength_pixel{"bloomStrength"};
	CachedPixelShaderSetting<float> m_bloom_radius_pixel{"bloomRadius"};
	CachedPixelShaderSetting<float> m_saturation_pixel{"saturation"};
	bool m_volumetric_light_enabled;
	CachedPixelShaderSetting<float, 3>
		m_sun_position_pixel{"sunPositionScreen"};
	CachedPixelShaderSetting<float> m_sun_brightness_pixel{"sunBrightness"};
	CachedPixelShaderSetting<float, 3>
		m_moon_position_pixel{"moonPositionScreen"};
	CachedPixelShaderSetting<float> m_moon_brightness_pixel{"moonBrightness"};
	CachedPixelShaderSetting<float>
		m_volumetric_light_strength_pixel{"volumetricLightStrength"};

	static constexpr std::array<const char*, 1> SETTING_CALLBACKS = {
		"exposure_compensation",
	};

public:
	void onSettingsChange(const std::string &name)
	{
		if (name == "exposure_compensation")
			m_user_exposure_compensation = g_settings->getFloat("exposure_compensation", -1.0f, 1.0f);
	}

	static void settingsCallback(const std::string &name, void *userdata)
	{
		reinterpret_cast<GameGlobalShaderUniformSetter*>(userdata)->onSettingsChange(name);
	}

	void setSky(Sky *sky) { m_sky = sky; }

	GameGlobalShaderUniformSetter(Sky *sky, Game *game) :
		m_sky(sky),
		m_client(game->getClient())
	{
		for (auto &name : SETTING_CALLBACKS)
			g_settings->registerChangedCallback(name, settingsCallback, this);

		m_user_exposure_compensation = g_settings->getFloat("exposure_compensation", -1.0f, 1.0f);
		m_bloom_enabled = g_settings->getBool("enable_bloom");
		m_volumetric_light_enabled = g_settings->getBool("enable_volumetric_lighting") && m_bloom_enabled;
		m_crack_animation_length_i = game->crack_animation_length;
	}

	~GameGlobalShaderUniformSetter()
	{
		g_settings->deregisterAllChangedCallbacks(this);
	}

	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		u32 daynight_ratio = (float)m_client->getEnv().getDayNightRatio();
		video::SColorf sunlight;
		get_sunlight_color(&sunlight, daynight_ratio);
		// Terra Firma: daylight through this world's air
		sunlight.r *= solar::tint().r;
		sunlight.g *= solar::tint().g;
		sunlight.b *= solar::tint().b;
		m_day_light.set(sunlight, services);

		u32 animation_timer = m_client->getEnv().getFrameTime() % 1000000;
		float animation_timer_f = (float)animation_timer / 100000.f;
		m_animation_timer_vertex.set(&animation_timer_f, services);
		m_animation_timer_pixel.set(&animation_timer_f, services);

		float animation_timer_delta_f = (float)m_client->getEnv().getFrameTimeDelta() / 100000.f;
		m_animation_timer_delta_vertex.set(&animation_timer_delta_f, services);
		m_animation_timer_delta_pixel.set(&animation_timer_delta_f, services);

		if (m_client->getMinimap()) {
			v3f minimap_yaw = m_client->getMinimap()->getYawVec();
			m_minimap_yaw.set(minimap_yaw, services);
		}

		v3f offset = intToFloat(m_client->getCamera()->getOffset(), BS);
		m_camera_offset_pixel.set(offset, services);
		m_camera_offset_vertex.set(offset, services);

		v3f camera_position = m_client->getCamera()->getPosition();
		m_camera_position_pixel.set(camera_position, services);
		m_camera_position_vertex.set(camera_position, services);

		m_texel_size0_vertex.set(m_texel_size0, services);
		m_texel_size0_pixel.set(m_texel_size0, services);

		{
			float tmp = m_crack_animation_length_i;
			m_crack_animation_length.set(&tmp, services);
			tmp = m_crack_level_i;
			m_crack_level.set(&tmp, services);
			tmp = m_crack_texture_scale_i;
			m_crack_texture_scale.set(&tmp, services);
		}

		const auto &lighting = m_client->getEnv().getLocalPlayer()->getLighting();

		const AutoExposure &exposure_params = lighting.exposure;
		std::array<float, 7> exposure_buffer = {
			std::pow(2.0f, exposure_params.luminance_min),
			std::pow(2.0f, exposure_params.luminance_max),
			exposure_params.exposure_correction,
			exposure_params.speed_dark_bright,
			exposure_params.speed_bright_dark,
			exposure_params.center_weight_power,
			powf(2.f, m_user_exposure_compensation)
		};
		m_exposure_params_pixel.set(exposure_buffer.data(), services);

		if (m_bloom_enabled) {
			float intensity = std::max(lighting.bloom_intensity, 0.0f);
			m_bloom_intensity_pixel.set(&intensity, services);
			float strength_factor = std::max(lighting.bloom_strength_factor, 0.0f);
			m_bloom_strength_pixel.set(&strength_factor, services);
			float radius = std::max(lighting.bloom_radius, 0.0f);
			m_bloom_radius_pixel.set(&radius, services);
		}

		float saturation = lighting.saturation;
		m_saturation_pixel.set(&saturation, services);

		if (m_volumetric_light_enabled) {
			// Map directional light to screen space
			auto camera_node = m_client->getCamera()->getCameraNode();
			core::matrix4 transform = camera_node->getProjectionMatrix();
			transform *= camera_node->getViewMatrix();

			if (m_sky->getSunVisible()) {
				v3f sun_position = camera_node->getAbsolutePosition() +
						10000.f * m_sky->getSunDirection();
				transform.transformVect(sun_position);
				sun_position.normalize();

				m_sun_position_pixel.set(sun_position, services);

				float sun_brightness = core::clamp(107.143f * m_sky->getSunDirection().Y, 0.f, 1.f);
				m_sun_brightness_pixel.set(&sun_brightness, services);
			} else {
				m_sun_position_pixel.set(v3f(0.f, 0.f, -1.f), services);

				float sun_brightness = 0.f;
				m_sun_brightness_pixel.set(&sun_brightness, services);
			}

			if (m_sky->getMoonVisible()) {
				v3f moon_position = camera_node->getAbsolutePosition() +
						10000.f * m_sky->getMoonDirection();
				transform.transformVect(moon_position);
				moon_position.normalize();

				m_moon_position_pixel.set(moon_position, services);

				float moon_brightness = core::clamp(107.143f * m_sky->getMoonDirection().Y, 0.f, 1.f);
				m_moon_brightness_pixel.set(&moon_brightness, services);
			} else {
				m_moon_position_pixel.set(v3f(0.f, 0.f, -1.f), services);

				float moon_brightness = 0.f;
				m_moon_brightness_pixel.set(&moon_brightness, services);
			}

			float volumetric_light_strength = lighting.volumetric_light_strength;
			m_volumetric_light_strength_pixel.set(&volumetric_light_strength, services);
		}
	}

	void onSetMaterial(const video::SMaterial &material) override
	{
		// This is set only for node materials which have a crack, see mapblock_mesh.cpp.
		auto pair = MapBlockMesh::unpackCrackMaterialParam(material.MaterialTypeParam);
		m_crack_level_i = pair.first;
		m_crack_texture_scale_i = pair.second;

		video::ITexture *texture = material.getTexture(0);
		if (texture) {
			core::dimension2du size = texture->getSize();
			m_texel_size0 = v2f(1.f / size.Width, 1.f / size.Height);
		} else {
			m_texel_size0 = v2f();
		}
	}
};


class GameGlobalShaderUniformSetterFactory : public IShaderUniformSetterFactory
{
	Sky *m_sky = nullptr;
	Game *m_game;
	std::vector<GameGlobalShaderUniformSetter*> created_nosky;
public:
	GameGlobalShaderUniformSetterFactory(Game *game) :
		m_game(game)
	{}

	void setSky(Sky *sky)
	{
		m_sky = sky;
		for (GameGlobalShaderUniformSetter *ggscs : created_nosky) {
			ggscs->setSky(m_sky);
		}
		created_nosky.clear();
	}

	virtual IShaderUniformSetter* create(const std::string &name)
	{
		if (str_starts_with(name, "shadow/"))
			return nullptr;
		auto *scs = new GameGlobalShaderUniformSetter(m_sky, m_game);
		if (!m_sky)
			created_nosky.push_back(scs);
		return scs;
	}
};

class NodeShaderConstantSetter : public IShaderConstantSetter
{
public:
	NodeShaderConstantSetter() = default;
	~NodeShaderConstantSetter() = default;

	void onGenerate(const std::string &name, ShaderConstants &constants) override
	{
		if (constants.find("MATERIAL_TYPE") == constants.end())
			return; // not a node shader
		[[maybe_unused]] const auto material_type =
			static_cast<MaterialType>(std::get<int>(constants["MATERIAL_TYPE"]));

#define PROVIDE(constant) constants[ #constant ] = (int)constant

		PROVIDE(TILE_MATERIAL_BASIC);
		PROVIDE(TILE_MATERIAL_ALPHA);
		PROVIDE(TILE_MATERIAL_LIQUID_TRANSPARENT);
		PROVIDE(TILE_MATERIAL_LIQUID_OPAQUE);
		PROVIDE(TILE_MATERIAL_WAVING_LEAVES);
		PROVIDE(TILE_MATERIAL_WAVING_PLANTS);
		PROVIDE(TILE_MATERIAL_OPAQUE);
		PROVIDE(TILE_MATERIAL_WAVING_LIQUID_BASIC);
		PROVIDE(TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT);
		PROVIDE(TILE_MATERIAL_WAVING_LIQUID_OPAQUE);
		PROVIDE(TILE_MATERIAL_PLAIN);
		PROVIDE(TILE_MATERIAL_PLAIN_ALPHA);

#undef PROVIDE

		bool enable_waving_water = g_settings->getBool("enable_waving_water");
		constants["ENABLE_WAVING_WATER"] = enable_waving_water ? 1 : 0;
		if (enable_waving_water) {
			constants["WATER_WAVE_HEIGHT"] = g_settings->getFloat("water_wave_height");
			constants["WATER_WAVE_LENGTH"] = g_settings->getFloat("water_wave_length");
			constants["WATER_WAVE_SPEED"] = g_settings->getFloat("water_wave_speed");
		}
		switch (material_type) {
			case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
			case TILE_MATERIAL_WAVING_LIQUID_BASIC:
				constants["MATERIAL_WAVING_LIQUID"] = 1;
				break;
			default:
				constants["MATERIAL_WAVING_LIQUID"] = 0;
				break;
		}
		switch (material_type) {
			case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
			case TILE_MATERIAL_WAVING_LIQUID_BASIC:
			case TILE_MATERIAL_LIQUID_TRANSPARENT:
				constants["MATERIAL_WATER_REFLECTIONS"] = 1;
				break;
			default:
				constants["MATERIAL_WATER_REFLECTIONS"] = 0;
				break;
		}

		constants["ENABLE_WAVING_LEAVES"] = g_settings->getBool("enable_waving_leaves") ? 1 : 0;
		constants["ENABLE_WAVING_PLANTS"] = g_settings->getBool("enable_waving_plants") ? 1 : 0;
	}
};

/****************************************************************************
 ****************************************************************************/

Game::Game() :
	m_chat_log_buf(g_logger),
	m_game_ui(new GameUI())
{
	clearTextureNameCache();

	const char *settings[] = {
		"chat_log_level", "doubletap_jump", "toggle_sneak_key", "toggle_aux1_key",
		"enable_fog", "mouse_sensitivity",
		"repeat_place_time", "repeat_dig_time", "noclip", "free_move", "fog_start",
		"cinematic", "cinematic_camera_smoothing", "camera_smoothing", "invert_mouse",
		"enable_hotbar_mouse_wheel", "invert_hotbar_mouse_wheel", "pause_on_lost_focus",
		"view_debug_text", "view_profiler", "view_wireframe", "view_bboxes", "view_block_bounds",
	};
	for (auto s : settings)
		g_settings->registerChangedCallback(s, &settingChangedCallback, this);
	for (const physics::TuningKnob &knob : physics::TUNING_KNOBS)
		g_settings->registerChangedCallback(knob.setting, &settingChangedCallback, this);

	readSettings();
}


Game::~Game()
{
#if USE_SHELL
	// an error that escaped the_game skipped shutdown(): the capture would
	// call into this Game after it's gone
	if (input)
		input->capture({});
#endif
	delete client;
	soundmaker.reset();
	sound_manager.reset();

	delete server;

	delete hud;
	delete camera;
	delete quicktune;
	delete eventmgr;
	delete texture_src;
	delete shader_src;
	delete nodedef_manager;
	delete itemdef_manager;
	delete draw_control;

	clearTextureNameCache();

	g_settings->deregisterAllChangedCallbacks(this);

	if (m_rendering_engine)
		m_rendering_engine->finalize();
}

bool Game::startup(volatile std::sig_atomic_t *kill,
		InputHandler *input,
		RenderingEngine *rendering_engine,
		GameStartData &start_data,
		GameErrorData &errordata,
		ChatBackend *chat_backend)
{

	// "cache"
	m_rendering_engine        = rendering_engine;
	device                    = m_rendering_engine->get_raw_device();
	this->kill                = kill;
	this->errordata           = &errordata;
	this->input               = input;
	this->chat_backend        = chat_backend;
	simple_singleplayer_mode  = start_data.isSinglePlayer();

	input->reloadKeybindings();

	driver = device->getVideoDriver();
	smgr = m_rendering_engine->get_scene_manager();

	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, g_settings->getBool("mip_map"));

	// Reinit runData
	runData = GameRunData();
	runData.time_from_last_punch = 10.0;

	m_game_ui->initFlags();
	if (g_settings->getBool("show_debug")) {
		m_flags.debug_state = 1;
		m_game_ui->m_flags.show_minimal_debug = true;
	}

	applyView();

	m_first_loop_after_window_activation = true;

	g_client_translations->clear();

	if (!init(start_data))
		return false;

	if (!createClient(start_data))
		return false;

	m_rendering_engine->initialize(client, hud);

	m_game_formspec.init(client, m_rendering_engine, input);

#if USE_SHELL
	sanity_check(g_shell);
	m_shell_session = std::make_unique<shell::Session>(shell::Session::Opts{
		*g_shell, client, driver, guienv, m_hand });
	// the Planetarium, while shown, takes the mouse where the page doesn't;
	// the keyboard only when its key opened it (a screen over it keeps its own)
	input->capture([this](const SEvent &event) {
		if (!planetariumShown())
			return false;
		if (event.EventType == EET_KEY_INPUT_EVENT) {
			if (!m_planetarium_alone)
				return false;
			const std::vector<KeyPress> keys = getKeySetting("keymap_planetarium");
			const bool ours = std::find(keys.begin(), keys.end(), KeyPress(event.KeyInput)) != keys.end();
			if (event.KeyInput.PressedDown && (ours || event.KeyInput.Key == KEY_ESCAPE))
				m_planetarium_closing = true;
			return true; // the player stands still meanwhile
		}
		// fresh from relative mode the device's mouse position has run off the window until
		// the mouse next moves, and in game it's over the whole window: the wheel always counts
		if (event.EventType == EET_MOUSE_INPUT_EVENT && event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
			SEvent wheel = event;
			wheel.MouseInput.X = wheel.MouseInput.Y = 0;
			return m_shell_session->planetarium().handle(wheel);
		}
		return m_shell_session->planetarium().handle(event);
	});
#endif

	return true;
}


void Game::run()
{
	ZoneScoped;

	ProfilerGraph graph;
	RunStats stats = {};
	CameraOrientation cam_view_target = {};
	CameraOrientation cam_view = {};
	FpsControl draw_times;
	f32 dtime; // in seconds

	// Clear the profiler
	{
		Profiler::GraphValues dummyvalues;
		g_profiler->graphPop(dummyvalues);
	}

	draw_times.reset();

	m_touch_simulate_aux1 = g_settings->getBool("fast_move")
			&& client->checkPrivilege("fast");

	const core::dimension2du initial_screen_size(
			g_settings->getU16("screen_w"),
			g_settings->getU16("screen_h")
		);
	const bool initial_window_maximized = !g_settings->getBool("fullscreen") &&
			g_settings->getBool("window_maximized");

#ifdef __ANDROID__
	porting::setPlayingNowNotification(true);
#endif

	auto framemarker = FrameMarker("Game::run()-frame").started();

	while (m_rendering_engine->run()
			&& !(*kill || g_gamecallback->shutdown_requested
			|| (server && server->isShutdownRequested()))) {

		framemarker.end();

		// Calculate dtime =
		//    m_rendering_engine->run() from this iteration
		//  + Sleep time until the wanted FPS are reached
		draw_times.limit(device, &dtime);

		framemarker.start();

		g_fontengine->handleReload();

		const auto current_dynamic_info = ClientDynamicInfo::getCurrent();
		if (!current_dynamic_info.equal(client_display_info)) {
			client_display_info = current_dynamic_info;
			dynamic_info_send_timer = 0.2f;
		}

		if (dynamic_info_send_timer > 0.0f) {
			dynamic_info_send_timer -= dtime;
			if (dynamic_info_send_timer <= 0.0f) {
				client->sendUpdateClientInfo(current_dynamic_info);
			}
		}

		// Prepare render data for next iteration

		updateStats(&stats, draw_times, dtime);
		updateInteractTimers(dtime);

		if (!checkConnection())
			break;
		if (!m_game_formspec.handleCallbacks())
			break;

		processQueues();

		m_game_ui->clearInfoText();

		updateProfilers(stats, draw_times, dtime);

		// Update camera offset once before doing anything.
		// In contrast to other updates the latency of this doesn't matter,
		// since it's invisible to the user. But it needs to be consistent.
		updateCameraOffset();

		processUserInput(dtime);
		// Terra Firma: carried over a planet's edge, the view turns with the ground
		if (const f32 turn = client->getEnv().getLocalPlayer()->takePlanetTurn()) {
			cam_view_target.camera_yaw += turn;
			cam_view.camera_yaw += turn;
		}
		// Update camera before player movement to avoid camera lag of one frame
		updateCameraDirection(&cam_view_target, dtime);
		if (m_cache_cam_smoothing <= 0.0f) {
			cam_view.camera_yaw = cam_view_target.camera_yaw;
			cam_view.camera_pitch = cam_view_target.camera_pitch;
		} else {
			f32 cam_damp_lambda = 1.0f / m_cache_cam_smoothing * dtime;
			cam_view.camera_yaw = damp(
					cam_view.camera_yaw,
					cam_view_target.camera_yaw,
					cam_damp_lambda
			);
			cam_view.camera_pitch = damp(
					cam_view.camera_pitch,
					cam_view_target.camera_pitch,
					cam_damp_lambda
			);
		}
		updatePlayerControl(cam_view);

		updatePauseState();
		if (m_is_paused)
			dtime = 0.0f;

		step(dtime);

		processClientEvents(&cam_view_target);
		updateDebugState();
		// Update camera here so it is in-sync with CAO position
		updateCamera(dtime);
		updateSound(dtime);
		processPlayerInteraction(dtime, m_game_ui->m_flags.show_hud);
		{
			ScopeProfiler timed(g_profiler, "Update frame [us]", SPT_GRAPH_ADD, PRECISION_MICRO);
			updateFrame(&graph, &stats, dtime, cam_view);
		}
		updateProfilerGraphs(&graph);

		if (m_does_lost_focus_pause_game && !device->isWindowFocused() && !isMenuActive()) {
#if USE_SHELL
			m_shell_session->screens().open("pause");
#else
			m_game_formspec.showPauseMenu();
#endif
		}
	}

	framemarker.end();

#ifdef __ANDROID__
	porting::setPlayingNowNotification(false);
#endif

	RenderingEngine::autosaveScreensizeAndCo(initial_screen_size, initial_window_maximized);
}


void Game::shutdown()
{
#if USE_SHELL
	input->capture({}); // its callback holds the session
	m_shell_session.reset();
	if (m_online_host) {
		if (server)
			server->setStrayHandler(nullptr); // its receive thread calls into the host
		m_online_host.reset(); // before the server closes its socket
	}
#endif
	// Delete text and menus first
	m_game_ui->clearText();
	m_game_formspec.reset();
	while (g_menumgr.menuCount() > 0) {
		g_menumgr.deleteFront();
	}

	if (g_touchcontrols)
		g_touchcontrols->hide();

	// Restore normal mouse cursor
	auto *cur_control = device->getCursorControl();
	if (cur_control) {
		cur_control->setVisible(true);
		cur_control->setRelativeMode(false);
	}

	clouds.reset();
	if (space)
		space->remove(); // the scene manager holds it too
	space.reset();
	if (far_view)
		far_view->remove(); // the scene manager holds it too
	far_view.reset();
	if (liquid_layer)
		liquid_layer->remove(); // the scene manager holds it too
	liquid_layer.reset();

	gui_chat_console.reset();

	sky.reset();

	// only if the shutdown progress bar isn't shown yet
	if (m_shutdown_progress == 0.0f)
		showOverlayMessage(N_("Shutting down..."), 0, 0);

	chat_backend->addMessage(L"", L"# Disconnected.");
	chat_backend->addMessage(L"", L"");

	if (client) {
		client->Stop();
		while (!client->isShutdown()) {
			assert(texture_src != NULL);
			assert(shader_src != NULL);
			texture_src->processQueue();
			shader_src->processQueue();
			sleep_ms(100);
		}
	}

	delete client;
	client = nullptr;
	soundmaker.reset();
	sound_manager.reset();

	auto stop_thread = runInThread([=] {
		delete server;
		server = nullptr;
	}, "ServerStop");

	FpsControl fps_control;
	fps_control.reset();

	while (stop_thread->isRunning()) {
		m_rendering_engine->run();
		f32 dtime;
		fps_control.limit(device, &dtime);
		showOverlayMessage(N_("Shutting down..."), dtime, 0, &m_shutdown_progress);
	}

	stop_thread->rethrow();

	// to be continued in Game::~Game
}


/****************************************************************************/
/****************************************************************************
 Startup
 ****************************************************************************/
/****************************************************************************/

bool Game::init(GameStartData &start_data)
{
	texture_src = createTextureSource();

	showOverlayMessage(N_("Loading..."), 0, 0);

	shader_src = createShaderSource();

	itemdef_manager = createItemDefManager();
	nodedef_manager = createNodeDefManager();

	m_item_visuals_manager = std::make_unique<ItemVisualsManager>();

	eventmgr = new EventManager();
	quicktune = new QuicktuneShortcutter();

	if (!(texture_src && shader_src && itemdef_manager && nodedef_manager
			&& eventmgr && quicktune))
		return false;

	if (!initSound())
		return false;

	// Create a server if not connecting to an existing one
	if (start_data.address.empty()) {
		if (!createServer(start_data))
			return false;
	}

	return true;
}

bool Game::initSound()
{
#if USE_SOUND
	if (g_sound_manager_singleton.get()) {
		infostream << "Attempting to use OpenAL audio" << std::endl;
		sound_manager = createOpenALSoundManager(g_sound_manager_singleton.get(),
				std::make_unique<SoundFallbackPathProvider>());
		if (!sound_manager)
			infostream << "Failed to initialize OpenAL audio" << std::endl;
	} else {
		infostream << "Sound disabled." << std::endl;
	}
#endif

	if (!sound_manager) {
		infostream << "Using dummy audio." << std::endl;
		sound_manager = std::make_unique<DummySoundManager>();
	}

	soundmaker = std::make_unique<SoundMaker>(sound_manager.get(), nodedef_manager);
	soundmaker->registerReceiver(eventmgr);

	return true;
}

bool Game::createServer(GameStartData &start_data)
{
	showOverlayMessage(N_("Creating server..."), 0, 5);

	std::string bind_str;
	if (simple_singleplayer_mode) {
		// Make the simple singleplayer server only accept connections from localhost,
		// which also makes Windows Defender not show a warning.
		bind_str = "127.0.0.1";
	} else {
		bind_str = g_settings->get("bind_address");
	}

	Address bind_addr(0, 0, 0, 0, start_data.socket_port);

	bool ipv6 = g_settings->getBool("ipv6_server");
#if USE_SHELL
	// Terra Firma: hosting for friends is IPv4 (STUN, punching: src/online)
	ipv6 = ipv6 && start_data.online.kind != online::Launch::Kind::Host;
#endif
	if (ipv6)
		bind_addr.setAddress(static_cast<IPv6AddressBytes*>(nullptr));
	try {
		bind_addr.Resolve(bind_str.c_str());
	} catch (const ResolveError &e) {
		warningstream << "Resolving bind address \"" << bind_str
			<< "\" failed: " << e.what()
			<< " -- Listening on all addresses." << std::endl;
	}
	if (bind_addr.isIPv6() && !g_settings->getBool("enable_ipv6")) {
		errordata->setError(fmtgettext("Unable to listen on %s because IPv6 is disabled",
			bind_addr.serializeString().c_str()));
		return false;
	}

	UDPSocket server_socket = UDPSocket::Create(bind_addr);
	// in singleplayer mode the OS assigns us the port in bind
	start_data.socket_port = server_socket.GetLocalAddress().getPort();

#if USE_SHELL
	// Terra Firma: hosted for friends; a world played alone carries over.
	if (start_data.online.kind == online::Launch::Kind::Host) {
		online::adoptSingleplayer(start_data.world_spec.path, start_data.name);
		// Returns at once: where the world can be reached is learned alongside.
		m_online_host = std::make_unique<online::Host>(start_data.online.api,
				server_socket.GetHandle(), start_data.socket_port);
	}
#endif

	server = new Server(start_data.world_spec.path, start_data.game_spec,
		simple_singleplayer_mode, std::move(server_socket),
		false, nullptr, &(errordata->message));
#if USE_SHELL
	if (m_online_host) {
		server->setGate(std::make_unique<online::Gate>(online::Gate::Opts{
			start_data.online.api, start_data.name, start_data.password, g_version_hash }));
		online::Host *host = m_online_host.get();
		server->setStrayHandler([host](const Address &sender, const u8 *data, size_t size) {
			host->stray(sender, data, size);
		});
	}
#endif

	auto start_thread = runInThread([=] {
		server->start();
		copyServerClientCache();
	}, "ServerStart");

	input->clear();
	bool success = true;

	FpsControl fps_control;
	fps_control.reset();

	while (start_thread->isRunning()) {
		if (!m_rendering_engine->run() || input->cancelPressed())
			success = false;
		f32 dtime;
		fps_control.limit(device, &dtime);

		if (success)
			showOverlayMessage(N_("Creating server..."), dtime, 5);
		else
			showOverlayMessage(N_("Shutting down..."), dtime, 0, &m_shutdown_progress);
	}

	start_thread->rethrow();

	return success;
}

void Game::copyServerClientCache()
{
	// It would be possible to let the client directly read the media files
	// from where the server knows they are. But aside from being more complicated
	// it would also *not* fill the media cache and cause slower joining of
	// remote servers.
	// (Imagine that you launch a game once locally and then connect to a server.)

	assert(server);
	auto map = server->getMediaList();
	u32 n = 0;
	for (auto &it : map) {
		assert(it.first.size() == 20); // SHA1
		if (clientMediaUpdateCacheCopy(it.first, it.second))
			n++;
	}
	infostream << "Copied " << n << " files directly from server to client cache"
		<< std::endl;
}

bool Game::createClient(const GameStartData &start_data)
{
	std::string *error_message = &(errordata->message);

	showOverlayMessage(N_("Creating client..."), 0, 10);

	draw_control = new MapDrawControl();
	if (!draw_control)
		return false;

	bool could_connect, connect_aborted;
	if (!connectToServer(start_data, &could_connect, &connect_aborted))
		return false;

	if (!could_connect) {
		if (error_message->empty() && !connect_aborted) {
			// Should not happen if error messages are set properly
			errordata->setError(gettext("Connection failed for unknown reason"));
		}
		return false;
	}

	if (!getServerContent(&connect_aborted)) {
		if (error_message->empty() && !connect_aborted) {
			// Should not happen if error messages are set properly
			errordata->setError(gettext("Connection failed for unknown reason"));
		}
		return false;
	}

	// Pre-calculate crack length
	{
		auto size = texture_src->getTextureDimensions("crack_anylength.png");
		if (size.Width && size.Height)
			crack_animation_length = size.Height / size.Width;
		else
			crack_animation_length = 5;
	}

	shader_src->addShaderConstantSetter(
		std::make_unique<NodeShaderConstantSetter>());

	auto scsf_up = std::make_unique<GameGlobalShaderUniformSetterFactory>(this);
	auto* scsf = scsf_up.get();
	shader_src->addShaderUniformSetterFactory(std::move(scsf_up));

	shader_src->addShaderUniformSetterFactory(
		std::make_unique<FogShaderUniformSetterFactory>());

	ShadowRenderer::preInit(shader_src);

	// Update cached textures, meshes and materials
	client->afterContentReceived();
	// Load received SSCSMs
	client->loadSSCSM();

	/* Camera
	 */
	camera = new Camera(*draw_control, client, m_rendering_engine);
	if (client->modsLoaded())
		client->getScript()->on_camera_ready(camera);
	client->setCamera(camera);

	/* Clouds
	 */
	clouds = make_irr<Clouds>(smgr, shader_src, -1, myrand());

	/* Skybox
	 */
	sky = make_irr<Sky>(-1, m_rendering_engine, texture_src, shader_src);
	scsf->setSky(sky.get());

	if (!initGui())
		return false;

	/* Set window caption
	 */
	auto driver_name = driver->getName();
	std::string str = std::string(PROJECT_NAME_C) +
			" " + g_version_hash + " [";
	str += simple_singleplayer_mode ? gettext("Singleplayer")
			: gettext("Multiplayer");
	str += "] [";
	str += driver_name;
	str += "]";

	device->setWindowCaption(utf8_to_wide(str).c_str());

	LocalPlayer *player = client->getEnv().getLocalPlayer();
	player->hurt_tilt_timer = 0;
	player->hurt_tilt_strength = 0;

	hud = new Hud(client, player, &player->inventory);

	mapper = client->getMinimap();

	if (mapper && client->modsLoaded())
		client->getScript()->on_minimap_ready(mapper);

	return true;
}

bool Game::shouldShowTouchControls()
{
	if (!device->supportsTouchEvents())
		return false;

	const std::string &touch_controls = g_settings->get("touch_controls");
	if (touch_controls == "auto")
		return RenderingEngine::getLastPointerType() == PointerType::Touch;
	return is_yes(touch_controls);
}

bool Game::initGui()
{
	m_game_ui->init();

	// Remove stale "recent" chat messages from previous connections
	chat_backend->clearRecentChat();

	// Make sure the size of the recent messages buffer is right
	chat_backend->applySettings();

	// Chat backend and console
	gui_chat_console = make_irr<GUIChatConsole>(guienv, guienv->getRootGUIElement(),
			-1, chat_backend, client, &g_menumgr);

	if (shouldShowTouchControls())
		g_touchcontrols = new TouchControls(device, texture_src);

	return true;
}

bool Game::connectToServer(const GameStartData &start_data,
		bool *connect_ok, bool *connection_aborted)
{
	std::string *error_message = &(errordata->message);

	*connect_ok = false;	// Let's not be overly optimistic
	*connection_aborted = false;
	const auto &address_name = start_data.address;

	showOverlayMessage(N_("Resolving address..."), 0, 15);

	Address connect_address(0, 0, 0, 0, start_data.socket_port);
	Address fallback_address;

	try {
		connect_address.Resolve(address_name.c_str(), &fallback_address);

		if (connect_address.isAny()) {
			// replace with localhost IP
			if (connect_address.isIPv6()) {
				IPv6AddressBytes addr_bytes;
				addr_bytes.bytes[15] = 1;
				connect_address.setAddress(&addr_bytes);
			} else {
				connect_address.setAddress(127, 0, 0, 1);
			}
		}
	} catch (ResolveError &e) {
		*error_message = fmtgettext("Couldn't resolve address: %s", e.what());

		errorstream << *error_message << std::endl;
		return false;
	}

	// this shouldn't normally happen since Address::Resolve() checks for enable_ipv6
	if (g_settings->getBool("enable_ipv6")) {
		// empty
	} else if (connect_address.isIPv6()) {
		*error_message = fmtgettext("Unable to connect to %s because IPv6 is disabled", connect_address.serializeString().c_str());
		errorstream << *error_message << std::endl;
		return false;
	} else if (fallback_address.isIPv6()) {
		fallback_address = Address();
	}

	fallback_address.setPort(connect_address.getPort());
	if (fallback_address.isValid()) {
		infostream << "Resolved two addresses for \"" << address_name
			<< "\" isIPv6[0]=" << connect_address.isIPv6()
			<< " isIPv6[1]=" << fallback_address.isIPv6() << std::endl;
	} else {
		infostream << "Resolved one address for \"" << address_name
			<< "\" isIPv6=" << connect_address.isIPv6() << std::endl;
	}


	try {
		client = new Client(start_data.name.c_str(),
				start_data.password,
				*draw_control, texture_src, shader_src,
				itemdef_manager, nodedef_manager, sound_manager.get(), eventmgr,
				m_rendering_engine,
				m_item_visuals_manager.get(),
				start_data.allow_login_or_register);
	} catch (const BaseException &e) {
		*error_message = fmtgettext("Error creating client: %s", e.what());
		errorstream << *error_message << std::endl;
		return false;
	}

	client->migrateModStorage();
	client->m_simple_singleplayer_mode = simple_singleplayer_mode;
	client->m_internal_server = !!server;

	/*
		Wait for server to accept connection
	*/

#if USE_SHELL
	m_online_kind = start_data.online.kind;
	m_online_host_name = start_data.online.host_name;
	if (start_data.online.kind == online::Launch::Kind::Join) {
		// From the socket STUN mapped: the host is punching toward it.
		client->connect(connect_address, address_name, std::move(*start_data.online.socket));
		fallback_address = Address();
	} else {
		client->connect(connect_address, address_name);
	}
#else
	client->connect(connect_address, address_name);
#endif

	try {
		input->clear();

		FpsControl fps_control;
		f32 dtime;
		f32 wait_time = 0; // in seconds
		bool did_fallback = false;

		fps_control.reset();

		auto framemarker = FrameMarker("Game::connectToServer()-frame").started();

		while (m_rendering_engine->run()) {

			framemarker.end();
			fps_control.limit(device, &dtime);
			framemarker.start();

			// Update client and server
			step(dtime);

			// End condition
			if (client->getState() == LC_Init) {
				*connect_ok = true;
				break;
			}

			// Break conditions
			if (*connection_aborted)
				break;

			if (!checkConnection())
				break;

			if (input->cancelPressed()) {
				*connection_aborted = true;
				infostream << "Connect aborted [Escape]" << std::endl;
				break;
			}

			wait_time += dtime;
			if (server) {
				// never time out
			} else if (wait_time > GAME_FALLBACK_TIMEOUT && !did_fallback) {
				if (!client->hasServerReplied() && fallback_address.isValid()) {
					client->connect(fallback_address, address_name);
				}
				did_fallback = true;
			} else if (wait_time > GAME_CONNECTION_TIMEOUT) {
				*error_message = gettext("Connection timed out.");
#if USE_SHELL
				if (m_online_kind == online::Launch::Kind::Join)
					*error_message = "Couldn't reach " + m_online_host_name + "'s world. One of your "
						"networks may not allow direct connections between games.";
#endif
				errorstream << *error_message << std::endl;
				break;
			}

			// Update status
			showOverlayMessage(N_("Connecting to server..."), dtime, 20);
		}
		framemarker.end();
	} catch (con::PeerNotFoundException &e) {
		warningstream << "This should not happen. Please report a bug." << std::endl;
		return false;
	}

	return true;
}

bool Game::getServerContent(bool *aborted)
{
	input->clear();

	FpsControl fps_control;
	f32 dtime; // in seconds

	fps_control.reset();

	auto framemarker = FrameMarker("Game::getServerContent()-frame").started();
	while (m_rendering_engine->run()) {
		framemarker.end();
		fps_control.limit(device, &dtime);
		framemarker.start();

		// Update client and server
		step(dtime);

		// End condition
		if (client->mediaReceived() && client->itemdefReceived() &&
				client->nodedefReceived()) {
			return true;
		}

		// Error conditions
		if (!checkConnection())
			return false;

		if (client->getState() < LC_Init) {
			errordata->message = gettext("Client disconnected");
			errorstream << errordata->message << std::endl;
			return false;
		}

		if (input->cancelPressed()) {
			*aborted = true;
			infostream << "Connect aborted [Escape]" << std::endl;
			return false;
		}

		const char* units[] = {gettext("KiB"), gettext("MiB")};
		auto adjust_unit = [&units](float &v) -> const char* {
			int i = (v > 900) ? 1 : 0;
			v /= (i == 1) ? 1024.0f : 1.0f;
			return units[i];
		};

		// Display status
		int progress = 25;

		if (!client->itemdefReceived()) {
			progress = 25;
			m_rendering_engine->draw_load_screen(wstrgettext("Item definitions..."),
					guienv, texture_src, dtime, progress);
		} else if (!client->nodedefReceived()) {
			progress = 30;
			m_rendering_engine->draw_load_screen(wstrgettext("Node definitions..."),
					guienv, texture_src, dtime, progress);
		} else {
			std::ostringstream message;
			std::fixed(message);
			std::string sub_message;
			message << gettext("Media...");
			float received_ratio = 0.f;
			s32 received = 0, total = 0;
			size_t received_size = 0;
			if (client->mediaReceiveProgress(received, total, received_size)) {
				if (total > 0)
					received_ratio = ((float) received) / total;

				message.precision(0);
				message << " " << received_ratio * 100.f << "%";

				float adjusted_size = ((float) received_size) / 1024.0f;
				auto unit = adjust_unit(adjusted_size);
				sub_message = fmtgettext("Files: %d / %d", received, total) + "\n"
							+ fmtgettext("Size: %.2f %s", adjusted_size, unit);
			}

			// 30% -> 65%
			progress = 30 + std::ceil(received_ratio * 35 + 0.5f);
			m_rendering_engine->draw_load_screen(utf8_to_wide(message.str()), guienv,
					texture_src, dtime, progress, nullptr, utf8_to_wide(sub_message));
		}
	}
	framemarker.end();

	*aborted = true;
	infostream << "Connect aborted [device]" << std::endl;
	return false;
}


/****************************************************************************/
/****************************************************************************
 Run
 ****************************************************************************/
/****************************************************************************/

inline void Game::updateInteractTimers(f32 dtime)
{
	if (runData.nodig_delay_timer >= 0)
		runData.nodig_delay_timer -= dtime;

	if (runData.object_hit_delay_timer >= 0)
		runData.object_hit_delay_timer -= dtime;

	runData.time_from_last_punch += dtime;
}


/* returns false if game should exit, otherwise true
 */
bool Game::checkConnection()
{
	if (client->accessDenied()) {
		// May be mod-provided, thus may contain color and translation
		const std::string reason = wide_to_utf8(
			unescape_translate(utf8_to_wide(client->accessDeniedReason())));

		errordata->setError(
			fmtgettext("Access denied. Reason: %s", reason.c_str()),
			client->reconnectRequested()
		);
		return false;
	}

	return true;
}

void Game::processQueues()
{
	texture_src->processQueue();
	shader_src->processQueue();
}

void Game::updateDebugState()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	// debug UI and wireframe
	bool has_debug = client->checkPrivilege("debug");
	bool has_basic_debug = has_debug || (player->hud_flags & HUD_FLAG_BASIC_DEBUG);

	if (m_game_ui->m_flags.show_basic_debug) {
		if (!has_basic_debug)
			m_game_ui->m_flags.show_basic_debug = false;
	} else if (m_game_ui->m_flags.show_minimal_debug) {
		if (has_basic_debug)
			m_game_ui->m_flags.show_basic_debug = true;
	}
	if (!has_basic_debug)
		hud->disableBlockBounds();
	if (!has_debug) {
		draw_control->show_wireframe = false;
		smgr->setGlobalDebugData(0, bbox_debug_flag);
		m_flags.disable_camera_update = false;
		m_game_formspec.disableDebugView();
	}

	// noclip
	draw_control->allow_noclip = m_cache_enable_noclip && client->checkPrivilege("noclip");
}

void Game::updateProfilers(const RunStats &stats, const FpsControl &draw_times,
		f32 dtime)
{
	float profiler_print_interval =
			g_settings->getFloat("profiler_print_interval");
	bool print_to_log = true;

	// Update game UI anyway but don't log
	if (profiler_print_interval <= 0) {
		print_to_log = false;
		profiler_print_interval = 3;
	}

	// Update graphs
	g_profiler->graphAdd("Time non-rendering [us]",
		draw_times.busy_time - stats.drawtime);
	g_profiler->graphAdd("Sleep [us]", draw_times.sleep_time);

	g_profiler->graphSet("FPS", 1.0f / dtime);

	auto stats2 = driver->getFrameStats();
	g_profiler->avg("Irr: drawcalls", stats2.Drawcalls);
	if (stats2.Drawcalls > 0)
		g_profiler->avg("Irr: primitives per drawcall",
			stats2.PrimitivesDrawn / float(stats2.Drawcalls));
	g_profiler->avg("Irr: HW buffers uploaded", stats2.HWBuffersUploaded);
	g_profiler->avg("Irr: HW buffers active", stats2.HWBuffersActive);
	u32 skinned_meshes = stats2.SWSkinnedMeshes + stats2.HWSkinnedMeshes;
	if (skinned_meshes > 0) {
		f32 use_pct = std::floor(100.0f * stats2.HWSkinnedMeshes / skinned_meshes);
		g_profiler->avg("Irr: HW skinning use [%]", use_pct);
	}

	if (profiler_interval.step(dtime, profiler_print_interval)) {
		if (print_to_log) {
			infostream << "Profiler:" << std::endl;
			g_profiler->print(infostream);
		}

		m_game_ui->updateProfiler();
		g_profiler->clear();
	}
}

void Game::updateStats(RunStats *stats, const FpsControl &draw_times,
		f32 dtime)
{

	f32 jitter;
	Jitter *jp;

	/* Time average and jitter calculation
	 */
	jp = &stats->dtime_jitter;
	jp->avg = jp->avg * 0.96 + dtime * 0.04;

	jitter = dtime - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;

	jp->counter += dtime;

	if (jp->counter > 0.0) {
		jp->counter -= 3.0;
		jp->max_sample = jp->max;
		jp->max_fraction = jp->max_sample / (jp->avg + 0.001);
		jp->max = 0.0;
	}

	/* Busytime average and jitter calculation
	 */
	jp = &stats->busy_time_jitter;
	jp->avg = jp->avg + draw_times.getBusyMs() * 0.02;

	jitter = draw_times.getBusyMs() - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;
	if (jitter < jp->min)
		jp->min = jitter;

	jp->counter += dtime;

	if (jp->counter > 0.0) {
		jp->counter -= 3.0;
		jp->max_sample = jp->max;
		jp->min_sample = jp->min;
		jp->max = 0.0;
		jp->min = 0.0;
	}
}



/****************************************************************************
 Input handling
 ****************************************************************************/

void Game::processUserInput(f32 dtime)
{
	bool desired = shouldShowTouchControls();
	if (desired && !g_touchcontrols) {
		g_touchcontrols = new TouchControls(device, texture_src);

	} else if (!desired && g_touchcontrols) {
		delete g_touchcontrols;
		g_touchcontrols = nullptr;
	}

	// Reset input if window not active or some menu is active
	if (!device->isWindowActive() || isMenuActive() || guienv->hasFocus(gui_chat_console.get())) {
		if (m_game_focused) {
			m_game_focused = false;
			infostream << "Game lost focus" << std::endl;
			input->releaseAllKeys();
		} else {
			input->clear();
		}

		if (g_touchcontrols)
			g_touchcontrols->hide();

	} else {
		if (g_touchcontrols) {
			/* on touchcontrols step may generate own input events which ain't
			 * what we want in case we just did clear them */
			g_touchcontrols->show();
			g_touchcontrols->step(dtime);
		}

		m_game_focused = true;
	}

	if (!guienv->hasFocus(gui_chat_console.get()) && gui_chat_console->isOpen()
		&& !gui_chat_console->isMyDescendant(guienv->getFocus()))
	{
		gui_chat_console->closeConsoleAtOnce();
	}

	// Input handler step() (used by the random input generator)
	input->step(dtime);

#ifdef __ANDROID__
	if (!m_game_formspec.handleAndroidUIInput())
		handleAndroidChatInput();
#endif

	// Increase timer for double tap of "keymap_jump"
	if (m_cache_doubletap_jump && runData.jump_timer_up <= 0.2f)
		runData.jump_timer_up += dtime;
	if (m_cache_doubletap_jump && runData.jump_timer_down <= 0.4f)
		runData.jump_timer_down += dtime;

	processKeyInput();
	processItemSelection(&runData.new_playeritem);
}


void Game::processKeyInput()
{
	// Terra Firma: one build mode (sprint 70), Tab in and out. What's
	// added is an object unless Alt's held, when it's the world's. The
	// workpiece stays on the bench either way: only F (or walking off)
	// puts it down.
	m_hand.setWorld(isKeyDown(KeyType::ALT));
	if (wasKeyPressed(KeyType::TOOLS)) {
		m_hand.toggle();
		m_draw.reset();
		m_game_ui->showStatusText(utf8_to_wide(m_hand.out()
				? "Building · 1–6 shapes · R tools · wheel size · Alt: into the world · " + m_hand.describe()
				: std::string("Hands")));
		return;
	}
	// Terra Firma: with Ctrl while building, a part copied, pasted, saved
	// (sprint 74): asked as the rail asks, where what's pointed at is known
	if (m_hand.out() && isKeyDown(KeyType::SNEAK)) {
		for (const auto &[key, verb] : { std::pair(KeyType::COPY, Hand::Verb::Copy),
				std::pair(KeyType::PASTE, Hand::Verb::Paste), std::pair(KeyType::SAVE_PART, Hand::Verb::Name) })
			if (wasKeyPressed(key)) {
				m_hand.ask(Hand::ACTIONS[size_t(verb)].id, "");
				return;
			}
	}
	// Terra Firma: N frees the cursor (the rails, the menus; the world
	// pointed at through it) and takes it back
	if (wasKeyPressed(KeyType::CURSOR)) {
		m_cursor = !m_cursor;
		m_naming.reset();
		closeMenu();
		m_game_ui->showTranslatedStatusText(m_cursor ? "The cursor's free (N: look again)" : "Looking");
		return;
	}
	// Terra Firma: what's carried is turned in the hands a quarter at a
	// time (sprint 77): E about the up, Q tipped toward them; Shift the
	// other way. Four of either is where it began. handleCarry does it
	if (m_carry) {
		const int way = isKeyDown(KeyType::AUX1) ? -1 : 1;
		bool asked = false;
		if (wasKeyDown(KeyType::TURN_RIGHT)) {
			m_carry->turn_asked += way;
			asked = true;
		}
		if (wasKeyDown(KeyType::TURN_LEFT)) {
			m_carry->tilt_asked += way;
			asked = true;
		}
		if (asked)
			return;
	}
	if (wasKeyDown(KeyType::DROP)) {
		dropSelectedItem(isKeyDown(KeyType::AUX1));
	} else if (wasKeyDown(KeyType::AUTOFORWARD)) {
		toggleAutoforward();
	} else if (wasKeyDown(KeyType::BACKWARD)) {
		if (g_settings->getBool("continuous_forward"))
			toggleAutoforward();
	} else if (wasKeyDown(KeyType::INVENTORY)) {
#if USE_SHELL
		m_shell_session->screens().open("inventory");
#else
		m_game_formspec.showPlayerInventory(nullptr);
#endif
#if USE_SHELL
	} else if (wasKeyDown(KeyType::LAB)) {
		m_shell_session->screens().open("lab");
	} else if (wasKeyDown(KeyType::WIKI)) {
		m_shell_session->screens().open("wiki");
#endif
	} else if (wasKeyDown(KeyType::PLANETARIUM)) {
		openPlanetarium();
	} else if (m_hand.out() && wasKeyDown(KeyType::TOOL_NEXT)) {
		m_hand.nextBrush();
		showTool();
	} else if (m_hand.out() && wasKeyDown(KeyType::MIRROR)) {
		m_hand.ask(Hand::ACTIONS[size_t(Hand::Verb::Mirror)].id, "");
	} else if (wasKeyDown(KeyType::UNDO)) {
		m_hand.ask(Hand::ACTIONS[size_t(Hand::Verb::Undo)].id, "");
	} else if (const GenericCAO *small = !m_driving && !m_hand.out() && m_lit && wasKeyDown(KeyType::FOCUS)
			? dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_lit)) : nullptr;
			small && small->getMachine() && small->getMachine()->scale() < 1) {
		// Terra Firma: a small thing goes in the hand (sprint 130: tf_tools says if it can)
		client->sendUiEvent("tf_tools:take", std::to_string(m_lit));
	} else if (!m_driving && wasKeyDown(KeyType::FOCUS)) { // (driving, F gets up)
		m_hand.ask(Hand::ACTIONS[size_t(Hand::Verb::Focus)].id, "");
	} else if (m_menu && input->cancelPressed()) {
		closeMenu();
	} else if (m_naming && input->cancelPressed()) {
		m_naming.reset(); // not saved after all
		m_cursor = false;
	} else if (m_draw && input->cancelPressed()) {
		m_draw.reset(); // a shape stopped, not drawn
	} else if (m_hold && m_part && input->cancelPressed()) {
		pickPart(std::nullopt);
	} else if (input->cancelPressed()) {
#ifdef __ANDROID__
		m_android_chat_open = false;
#endif
		if (!gui_chat_console->isOpenInhibited()) {
#if USE_SHELL
			m_shell_session->screens().open("pause");
#else
			m_game_formspec.showPauseMenu();
#endif
		}
	} else if (wasKeyDown(KeyType::CHAT)) {
		openConsole(0.2, L"");
	} else if (wasKeyDown(KeyType::CMD)) {
		openConsole(0.2, L"/");
	} else if (wasKeyDown(KeyType::CMD_LOCAL)) {
		if (client->modsLoaded())
			openConsole(0.2, L".");
		else
			m_game_ui->showTranslatedStatusText("Client side scripting is disabled");
	} else if (wasKeyDown(KeyType::CONSOLE)) {
		openConsole(core::clamp(g_settings->getFloat("console_height"), 0.1f, 1.0f));
	} else if (wasKeyDown(KeyType::FREEMOVE)) {
		toggleFreeMove();
	} else if (wasKeyDown(KeyType::JUMP)) {
		toggleFreeMoveAlt();
	} else if (wasKeyDown(KeyType::PITCHMOVE)) {
		togglePitchMove();
	} else if (wasKeyDown(KeyType::FASTMOVE)) {
		toggleFast();
	} else if (wasKeyDown(KeyType::NOCLIP)) {
		toggleNoClip();
#if USE_SOUND
	} else if (wasKeyDown(KeyType::MUTE)) {
		bool new_mute_sound = !g_settings->getBool("mute_sound");
		g_settings->setBool("mute_sound", new_mute_sound);
		if (new_mute_sound)
			m_game_ui->showTranslatedStatusText("Sound muted");
		else
			m_game_ui->showTranslatedStatusText("Sound unmuted");
	} else if (wasKeyDown(KeyType::INC_VOLUME)) {
		float new_volume = g_settings->getFloat("sound_volume", 0.0f, 0.9f) + 0.1f;
		g_settings->setFloat("sound_volume", new_volume);
		std::wstring msg = fwgettext("Volume changed to %d%%", myround(new_volume * 100));
		m_game_ui->showStatusText(msg);
	} else if (wasKeyDown(KeyType::DEC_VOLUME)) {
		float new_volume = g_settings->getFloat("sound_volume", 0.1f, 1.0f) - 0.1f;
		g_settings->setFloat("sound_volume", new_volume);
		std::wstring msg = fwgettext("Volume changed to %d%%", myround(new_volume * 100));
		m_game_ui->showStatusText(msg);
#else
	} else if (wasKeyDown(KeyType::MUTE) || wasKeyDown(KeyType::INC_VOLUME)
			|| wasKeyDown(KeyType::DEC_VOLUME)) {
		m_game_ui->showTranslatedStatusText("Sound system is not supported on this build");
#endif
	} else if (wasKeyDown(KeyType::CINEMATIC)) {
		toggleCinematic();
	} else if (wasKeyPressed(KeyType::SCREENSHOT)) {
		client->requestScreenshot();
	} else if (wasKeyPressed(KeyType::TOGGLE_BLOCK_BOUNDS)) {
		toggleBlockBounds();
	} else if (wasKeyPressed(KeyType::TOGGLE_HUD)) {
		m_game_ui->toggleHud();
	} else if (wasKeyPressed(KeyType::MINIMAP)) {
		bool opened_map = false;
#if USE_SHELL
		if (m_shell_session && client->getHorizon()) {
			m_shell_session->screens().open("map");
			opened_map = true;
		}
#endif
		if (!opened_map)
			toggleMinimap(isKeyDown(KeyType::AUX1));
	} else if (wasKeyPressed(KeyType::TOGGLE_CHAT)) {
		m_game_ui->toggleChat(client);
	} else if (wasKeyPressed(KeyType::TOGGLE_FOG)) {
		toggleFog();
	} else if (wasKeyDown(KeyType::TOGGLE_UPDATE_CAMERA)) {
		toggleUpdateCamera();
	} else if (wasKeyPressed(KeyType::CAMERA_MODE) && !(m_hand.out() && isKeyDown(KeyType::SNEAK))) {
		camera->toggleCameraMode();
		updateCameraMode();
	} else if (wasKeyPressed(KeyType::TOGGLE_DEBUG)) {
		toggleDebug();
	} else if (wasKeyPressed(KeyType::TOGGLE_PROFILER)) {
		m_game_ui->toggleProfiler();
	} else if (isKeyDown(KeyType::ALT) && wasKeyDown(KeyType::INCREASE_VIEWING_RANGE)) {
		// Terra Firma: Alt+= mirrors, and steps its plane round (sprint 68)
		if (!m_hand.out())
			m_hand.takeOut();
		stepMirror();
	} else if (wasKeyDown(KeyType::INCREASE_VIEWING_RANGE)) {
		increaseViewRange();
	} else if (wasKeyDown(KeyType::DECREASE_VIEWING_RANGE)) {
		decreaseViewRange();
	} else if (wasKeyPressed(KeyType::RANGESELECT)) {
		toggleFullViewRange();
	} else if (wasKeyDown(KeyType::ZOOM)) {
		checkZoomEnabled();
	} else if (wasKeyDown(KeyType::QUICKTUNE_NEXT)) {
		quicktune->next();
	} else if (wasKeyDown(KeyType::QUICKTUNE_PREV)) {
		quicktune->prev();
	} else if (wasKeyDown(KeyType::QUICKTUNE_INC)) {
		quicktune->inc();
	} else if (wasKeyDown(KeyType::QUICKTUNE_DEC)) {
		quicktune->dec();
	}

	if (!isKeyDown(KeyType::JUMP) && runData.reset_jump_timer) {
		runData.reset_jump_timer = false;
		runData.jump_timer_up = 0.0f;
	}

	if (quicktune->hasMessage()) {
		m_game_ui->showStatusText(utf8_to_wide(quicktune->getMessage()));
	}
}

void Game::processItemSelection(u16 *new_playeritem)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	*new_playeritem = player->getWieldIndex();
	u16 max_item = player->getMaxHotbarItemcount();
	if (max_item == 0)
		return;
	max_item -= 1;

	/* Item selection using mouse wheel
	 */
	s32 wheel = input->getMouseWheel();
	// Terra Firma: grabbing, the wheel brings the hand nearer or further
	if (m_grab && wheel != 0) {
		constexpr f32 STEP = 0.25f * BS, NEAREST = 0.75f * BS, FURTHEST = 4.f * BS;
		m_grab->distance = std::clamp(m_grab->distance + (wheel > 0 ? STEP : -STEP), NEAREST, FURTHEST);
		wheel = 0;
	}
	// Terra Firma: with the tools out the wheel sizes them
	if (m_hand.out() && wheel != 0) {
		m_hand.resize(wheel > 0 ? 1 : -1);
		showTool();
		wheel = 0;
	}
	if (!m_enable_hotbar_mouse_wheel)
		wheel = 0;
	if (m_invert_hotbar_mouse_wheel)
		wheel *= -1;

	s32 dir = wheel;

	if (wasKeyDown(KeyType::HOTBAR_NEXT))
		dir = -1;

	if (wasKeyDown(KeyType::HOTBAR_PREV))
		dir = 1;

	if (dir < 0)
		*new_playeritem = *new_playeritem < max_item ? *new_playeritem + 1 : 0;
	else if (dir > 0)
		*new_playeritem = *new_playeritem > 0 ? *new_playeritem - 1 : max_item;
	// else dir == 0

	// Terra Firma: building, the slot keys pick the shape drawn, not the
	// item held (sprint 70: the shapes' bar sits on the hotbar); with Alt
	// they're the hotbar's again
	if (m_hand.out() && !isKeyDown(KeyType::ALT)) {
		for (size_t i = 0; i < size_t(sculpt::Tool::Count); i++)
			if (wasKeyDown((GameKeyType) (KeyType::SLOT_1 + i))) {
				m_hand.pick(sculpt::KINDS[i].id);
				showTool();
			}
		*new_playeritem = MYMIN(*new_playeritem, max_item);
		return;
	}

	/* Item selection using hotbar slot keys
	 */
	for (u16 i = 0; i <= max_item; i++) {
		if (wasKeyDown((GameKeyType) (KeyType::SLOT_1 + i))) {
			*new_playeritem = i;
			break;
		}
	}

	if (g_touchcontrols) {
		std::optional<u16> selection = g_touchcontrols->getHotbarSelection();
		if (selection)
			*new_playeritem = *selection;
	}

	// Clamp selection again in case it wasn't changed but max_item was
	*new_playeritem = MYMIN(*new_playeritem, max_item);
}


void Game::dropSelectedItem(bool single_item)
{
	IDropAction *a = new IDropAction();
	a->count = single_item ? 1 : 0;
	a->from_inv.setCurrentPlayer();
	a->from_list = "main";
	a->from_i = client->getEnv().getLocalPlayer()->getWieldIndex();
	client->inventoryAction(a);
}

void Game::openConsole(float scale, const wchar_t *line)
{
	assert(scale > 0.0f && scale <= 1.0f);

#ifdef __ANDROID__
	if (!porting::hasPhysicalKeyboardAndroid()) {
		porting::showTextInputDialog("", "", 2);
		m_android_chat_open = true;
	} else {
#endif
	if (gui_chat_console->isOpenInhibited())
		return;
	gui_chat_console->openConsole(scale);
	if (line) {
		gui_chat_console->setCloseOnEnter(true);
		gui_chat_console->replaceAndAddToHistory(line);
	}
#ifdef __ANDROID__
	} // else
#endif
}

#ifdef __ANDROID__
void Game::handleAndroidChatInput()
{
	// It has to be a text input
	if (m_android_chat_open && porting::getLastInputDialogType() == porting::TEXT_INPUT) {
		porting::AndroidDialogState dialogState = porting::getInputDialogState();
		if (dialogState == porting::DIALOG_INPUTTED) {
			std::string text = porting::getInputDialogMessage();
			client->typeChatMessage(utf8_to_wide(text));
		}
		if (dialogState != porting::DIALOG_SHOWN)
			m_android_chat_open = false;
	}
}
#endif

void Game::toggleFreeMove()
{
	bool free_move = !g_settings->getBool("free_move");
	g_settings->set("free_move", bool_to_cstr(free_move));

#if USE_SHELL
	return; // Terra Firma: flying shows itself, as Minecraft's does: no status text
#endif
	if (free_move) {
		if (client->checkPrivilege("fly")) {
			m_game_ui->showTranslatedStatusText("Fly mode enabled");
		} else {
			m_game_ui->showTranslatedStatusText("Fly mode enabled (note: no 'fly' privilege)");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Fly mode disabled");
	}
}

void Game::toggleFreeMoveAlt()
{
	if (!runData.reset_jump_timer) {
		runData.jump_timer_down_before = runData.jump_timer_down;
		runData.jump_timer_down = 0.0f;
	}

	// key down (0.2 s max.), then key up (0.2 s max.), then key down
	if (m_cache_doubletap_jump && runData.jump_timer_up < 0.2f &&
			runData.jump_timer_down_before < 0.4f) // 0.2 + 0.2
		toggleFreeMove();

	runData.reset_jump_timer = true;
}


void Game::togglePitchMove()
{
	bool pitch_move = !g_settings->getBool("pitch_move");
	g_settings->set("pitch_move", bool_to_cstr(pitch_move));

	if (pitch_move) {
		m_game_ui->showTranslatedStatusText("Pitch move mode enabled");
	} else {
		m_game_ui->showTranslatedStatusText("Pitch move mode disabled");
	}
}


void Game::toggleFast()
{
	bool fast_move = !g_settings->getBool("fast_move");
	bool has_fast_privs = client->checkPrivilege("fast");
	g_settings->set("fast_move", bool_to_cstr(fast_move));

	if (fast_move) {
		if (has_fast_privs) {
			m_game_ui->showTranslatedStatusText("Fast mode enabled");
		} else {
			m_game_ui->showTranslatedStatusText("Fast mode enabled (note: no 'fast' privilege)");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Fast mode disabled");
	}

	m_touch_simulate_aux1 = fast_move && has_fast_privs;
}


void Game::toggleNoClip()
{
	bool noclip = !g_settings->getBool("noclip");
	g_settings->set("noclip", bool_to_cstr(noclip));

	if (noclip) {
		if (client->checkPrivilege("noclip")) {
			m_game_ui->showTranslatedStatusText("Noclip mode enabled");
		} else {
			m_game_ui->showTranslatedStatusText("Noclip mode enabled (note: no 'noclip' privilege)");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Noclip mode disabled");
	}
}

void Game::toggleCinematic()
{
	bool cinematic = !g_settings->getBool("cinematic");
	g_settings->set("cinematic", bool_to_cstr(cinematic));

	if (cinematic)
		m_game_ui->showTranslatedStatusText("Cinematic mode enabled");
	else
		m_game_ui->showTranslatedStatusText("Cinematic mode disabled");
}

void Game::toggleBlockBounds()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	if (!(client->checkPrivilege("debug") || (player->hud_flags & HUD_FLAG_BASIC_DEBUG))) {
		m_game_ui->showTranslatedStatusText("Can't show block bounds (disabled by game or mod)");
		return;
	}
	enum Hud::BlockBoundsMode newmode = hud->toggleBlockBounds();
	switch (newmode) {
		case Hud::BLOCK_BOUNDS_OFF:
			m_game_ui->showTranslatedStatusText("Block bounds hidden");
			break;
		case Hud::BLOCK_BOUNDS_CURRENT:
			m_game_ui->showTranslatedStatusText("Block bounds shown for current block");
			break;
		case Hud::BLOCK_BOUNDS_NEAR:
			m_game_ui->showTranslatedStatusText("Block bounds shown for nearby blocks");
			break;
		default:
			break;
	}
}

// Autoforward by toggling continuous forward.
void Game::toggleAutoforward()
{
	bool autorun_enabled = !g_settings->getBool("continuous_forward");
	g_settings->set("continuous_forward", bool_to_cstr(autorun_enabled));

	if (autorun_enabled)
		m_game_ui->showTranslatedStatusText("Automatic forward enabled");
	else
		m_game_ui->showTranslatedStatusText("Automatic forward disabled");
}

// Terra Firma: the planet this world is, in the Planetarium, over the
// whole window, by its key. Its key or Escape closes it; meanwhile it has the
// mouse and the keyboard (the player stands still). Under a page's screen
// (the map) the page shows and hides it, and keys go on to the screen.
void Game::openPlanetarium()
{
#if USE_SHELL
	if (const std::optional<std::string> why = m_shell_session->showWorld()) {
		m_game_ui->showStatusText(utf8_to_wide(*why));
		return;
	}
	m_planetarium_alone = true;
	input->releaseAllKeys();
#endif
}

void Game::closePlanetarium()
{
#if USE_SHELL
	m_shell_session->planetarium().hide();
#endif
	m_planetarium_alone = false;
	m_planetarium_closing = false;
	m_first_loop_after_window_activation = true; // the mouse moved while it was open
}

bool Game::planetariumShown()
{
#if USE_SHELL
	return m_shell_session && m_shell_session->planetarium().shown();
#else
	return false;
#endif
}

void Game::toggleMinimap(bool shift_pressed)
{
	if (!mapper || !m_game_ui->m_flags.show_hud || !g_settings->getBool("enable_minimap"))
		return;

	if (shift_pressed)
		mapper->toggleMinimapShape();
	else
		mapper->nextMode();

	// TODO: When legacy minimap is deprecated, keep only HUD minimap stuff here

	// Not so satisying code to keep compatibility with old fixed mode system
	// -->
	u32 hud_flags = client->getEnv().getLocalPlayer()->hud_flags;

	// If radar is disabled, try to find a non radar mode or fall back to 0
	if (!(hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE))
		while (mapper->getModeIndex() &&
				mapper->getModeDef().type == MINIMAP_TYPE_RADAR)
			mapper->nextMode();
	// <--
	// End of 'not so satifying code'
	if (hud && hud->hasElementOfType(HUD_ELEM_MINIMAP))
		m_game_ui->showStatusText(utf8_to_wide(mapper->getModeDef().label));
	else
		m_game_ui->showTranslatedStatusText("Minimap currently disabled by game or mod");
}

void Game::toggleFog()
{
	bool flag = !g_settings->getBool("enable_fog");
	g_settings->setBool("enable_fog", flag);
	bool allowed = sky->getFogDistance() < 0 || client->checkPrivilege("debug");
	if (!allowed)
		m_game_ui->showTranslatedStatusText("Fog enabled by game or mod");
	else if (flag)
		m_game_ui->showTranslatedStatusText("Fog enabled");
	else
		m_game_ui->showTranslatedStatusText("Fog disabled");
}


void Game::applyView()
{
	const bool text = g_settings->getBool("view_debug_text"), graph = g_settings->getBool("view_profiler");
	const bool wireframe = g_settings->getBool("view_wireframe"), boxes = g_settings->getBool("view_bboxes");
	const std::string bounds = g_settings->get("view_block_bounds");
	const auto changed = [](bool now, bool &was) { const bool c = now != was; was = now; return c; };
	if (changed(text, m_view.text)) {
		m_game_ui->m_flags.show_minimal_debug = text;
		m_game_ui->m_flags.show_basic_debug = text;
	}
	if (changed(graph, m_view.graph))
		m_game_ui->m_flags.show_profiler_graph = graph;
	if (changed(wireframe, m_view.wireframe))
		draw_control->show_wireframe = wireframe;
	if (changed(boxes, m_view.boxes))
		smgr->setGlobalDebugData(boxes ? bbox_debug_flag : 0, boxes ? 0 : bbox_debug_flag);
	if (bounds != m_view.bounds) {
		m_view.bounds = bounds;
		const Hud::BlockBoundsMode want = bounds == "near" ? Hud::BLOCK_BOUNDS_NEAR :
				bounds == "current" ? Hud::BLOCK_BOUNDS_CURRENT : Hud::BLOCK_BOUNDS_OFF;
		while (hud->toggleBlockBounds() != want) {} // (it only steps)
	}
}

void Game::toggleDebug()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	bool has_debug = client->checkPrivilege("debug");
	bool has_basic_debug = has_debug || (player->hud_flags & HUD_FLAG_BASIC_DEBUG);

	// Initial: No debug info
	// 1x toggle: Debug text
	// 2x toggle: Debug text with profiler graph
	// 3x toggle: Debug text and wireframe (needs "debug" priv)
	// 4x toggle: Debug text and bbox (needs "debug" priv)
	//
	// The debug text can be in 2 modes: minimal and basic.
	// * Minimal: Only technical client info that not gameplay-relevant
	// * Basic: Info that might give gameplay advantage, e.g. pos, angle
	// Basic mode is used when player has the debug HUD flag set,
	// otherwise the Minimal mode is used.

	auto &state = m_flags.debug_state;
	state = (state + 1) % 5;
	if (state >= 3 && !has_debug)
		state = 0;

	m_game_ui->m_flags.show_minimal_debug = state > 0;
	m_game_ui->m_flags.show_basic_debug = state > 0 && has_basic_debug;
	m_game_ui->m_flags.show_profiler_graph = state == 2;
	draw_control->show_wireframe = state == 3;
	smgr->setGlobalDebugData(state == 4 ? bbox_debug_flag : 0,
			state == 4 ? 0 : bbox_debug_flag);

	if (state == 1) {
		m_game_ui->showTranslatedStatusText("Debug info shown");
	} else if (state == 2) {
		m_game_ui->showTranslatedStatusText("Profiler graph shown");
	} else if (state == 3) {
		if (driver->getDriverType() == video::EDT_OGLES2)
			m_game_ui->showTranslatedStatusText("Wireframe not supported by video driver");
		else
			m_game_ui->showTranslatedStatusText("Wireframe shown");
	} else if (state == 4) {
		m_game_ui->showTranslatedStatusText("Bounding boxes shown");
	} else {
		m_game_ui->showTranslatedStatusText("All debug info hidden");
	}
}


void Game::toggleUpdateCamera()
{
	auto &flag = m_flags.disable_camera_update;
	flag = client->checkPrivilege("debug") ? !flag : false;
	if (flag)
		m_game_ui->showTranslatedStatusText("Camera update disabled");
	else
		m_game_ui->showTranslatedStatusText("Camera update enabled");
}


void Game::increaseViewRange()
{
	s16 range = g_settings->getS16("viewing_range");
	s16 range_new = range + 10;
	s16 server_limit = sky->getFogDistance();

	if (range_new >= 4000) {
		range_new = 4000;
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing range changed to %d (the maximum), but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing range changed to %d (the maximum)", range_new);
		m_game_ui->showStatusText(msg);
	} else {
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing range changed to %d, but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing range changed to %d", range_new);
		m_game_ui->showStatusText(msg);
	}
	g_settings->set("viewing_range", itos(range_new));
}


void Game::decreaseViewRange()
{
	s16 range = g_settings->getS16("viewing_range");
	s16 range_new = range - 10;
	s16 server_limit = sky->getFogDistance();

	if (range_new <= 20) {
		range_new = 20;
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing changed to %d (the minimum), but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing changed to %d (the minimum)", range_new);
		m_game_ui->showStatusText(msg);
	} else {
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing range changed to %d, but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing range changed to %d", range_new);
		m_game_ui->showStatusText(msg);
	}
	g_settings->set("viewing_range", itos(range_new));
}


void Game::toggleFullViewRange()
{
	draw_control->range_all = !draw_control->range_all;
	if (draw_control->range_all) {
		if (sky->getFogDistance() >= 0) {
			m_game_ui->showTranslatedStatusText("Unlimited viewing range enabled, but forbidden by game or mod");
		} else {
			m_game_ui->showTranslatedStatusText("Unlimited viewing range enabled");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Unlimited viewing range disabled");
	}
}


void Game::checkZoomEnabled()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	if (player->getZoomFOV() < 0.001f || player->getFov().fov > 0.0f)
		m_game_ui->showTranslatedStatusText("Zoom currently disabled by game or mod");
}

void Game::updateCameraDirection(CameraOrientation *cam, float dtime)
{
	auto *cur_control = device->getCursorControl();

	// Terra Firma: the object the server made for us, focused when it's here
	if (const std::optional<std::pair<u16, workshop::Handed>> held = client->takeHeld())
		m_awaited = held;
	if (m_awaited && pieceMiddle(m_awaited->first)) {
		if (m_awaited->second == workshop::Handed::Focused) {
			focusOn(m_awaited->first);
		} else if (auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_awaited->first))) {
			// carried off, over whatever was carried: a paste till the next
			// click; pulled off, while the button's down (let go before it
			// came: a click, so till the next one)
			if (m_carry)
				client->sendHold(m_carry->object, workshop::Grip::Free);
			m_carry.reset();
			m_cursor = false; // taken from the rail: the view's back to carry it with
			const bool pulled = m_awaited->second == workshop::Handed::Pulled;
			const bool sticky = !pulled || !isKeyDown(KeyType::DIG);
			startCarry(m_awaited->first, pulled && m_pulled_at ? *m_pulled_at : obj->getPosition(), sticky);
			m_game_ui->showTranslatedStatusText(sticky
					? "Click to put it on, or down · E: turn it · Q: tip it"
					: "Let go to put it on, or down · E: turn it · Q: tip it");
		}
		m_pulled_at.reset();
		m_awaited.reset();
	}

	// Terra Firma: an object on the workbench, turned by its handle
	if (m_hold)
		benchStep(dtime);
	// Terra Firma: the cursor free (Tab): the view stays, the mouse points
	if (m_cursor) {
		if (cur_control) {
			cur_control->setRelativeMode(false);
			if (!cur_control->isVisible())
				cur_control->setVisible(true);
		}
		m_first_loop_after_window_activation = true;
		return;
	}

	/* On Linux and Windows, enabling relative mouse mode somehow results
	in simulated mouse events being generated from touch events, even though
	SDL_HINT_MOUSE_TOUCH_EVENTS and SDL_HINT_TOUCH_MOUSE_EVENTS are set to 0.
	Since we have our own code to synthesize mouse events from touch events,
	this results in duplicated input. To avoid that, we don't enable relative
	mouse mode if we're in touchscreen mode. */
	if (cur_control)
		cur_control->setRelativeMode(!g_touchcontrols && !isMenuActive() && !planetariumShown());

	if ((device->isWindowActive() && device->isWindowFocused()
			&& !isMenuActive() && !planetariumShown()) || input->isRandom()) {

		if (cur_control && !input->isRandom()) {
			// Mac OSX gets upset if this is set every frame
			if (cur_control->isVisible())
				cur_control->setVisible(false);
		}

		if (m_first_loop_after_window_activation && !g_touchcontrols) {
			m_first_loop_after_window_activation = false;

			// Discard any movement that happened while the window was inactive
			input->getMouseMovement();
			input->setMousePos(driver->getScreenSize().Width / 2,
				driver->getScreenSize().Height / 2);
		} else {
			updateCameraOrientation(cam, dtime);
		}

	} else {
		// Mac OSX gets upset if this is set every frame
		if (cur_control && !cur_control->isVisible())
			cur_control->setVisible(true);

		m_first_loop_after_window_activation = true;
	}
	if (g_touchcontrols)
		m_first_loop_after_window_activation = true;
}

// Get the factor to multiply with sensitivity to get the same mouse/joystick
// responsiveness independently of FOV.
f32 Game::getSensitivityScaleFactor() const
{
	f32 fov_y = client->getCamera()->getFovY();

	// Multiply by a constant such that it becomes 1.0 at 72 degree FOV and
	// 16:9 aspect ratio to minimize disruption of existing sensitivity
	// settings.
	return std::tan(fov_y / 2.0f) * 1.3763819f;
}

bool Game::isTouchShootlineUsed() const
{
	return g_touchcontrols && g_touchcontrols->isShootlineAvailable() &&
			camera->getCameraMode() == CAMERA_MODE_FIRST;
}

// Terra Firma: whether the player's own body is on its feet, not flown, swum or climbed
// by Luanti's movement, where the crouch key means "down"
bool Game::bodyIsLoose(LocalPlayer &player) const
{
	const PlayerSettings &s = player.getPlayerSettings();
	return (client->checkPrivilege("fly") && s.free_move) || player.in_liquid || player.is_climbing;
}

// Terra Firma: a tap of the crouch key stays crouched till the next; held, it's crouched
// till let go. Z lies down the same way. Jumping stands up. While building, Ctrl is Ctrl.
bool Game::sneakNow(LocalPlayer &player)
{
	constexpr u64 TAP = 250; // ms
	const bool held = isKeyDown(KeyType::SNEAK), lying = isKeyDown(KeyType::PRONE), running = isKeyDown(KeyType::AUX1);
	// its own edges, frame by frame: this runs at another point in the frame from the
	// interaction that clears the input's, and a press could fall between them
	const bool pressed = held && !m_crouch_key, lie = lying && !m_prone_key, run = running && !m_run_key;
	m_crouch_key = held;
	m_prone_key = lying;
	m_run_key = running;
	const bool jumping = isKeyDown(KeyType::JUMP);
	if (jumping && !m_jump_key && !m_hand.out())
		client->getBody().pressJump();
	if (pressed && !m_hand.out())
		client->getBody().pressCrouch();
	m_jump_key = jumping;
	if (bodyIsLoose(player)) {
		m_crouch = m_prone = false;
		return held;
	}
	// (Ctrl is Ctrl while building, so nothing could undo a crouch kept through it)
	if (m_hand.out()) {
		m_crouch = m_prone = false;
		return false;
	}
	if (pressed) {
		m_crouch_since = porting::getTimeMs();
		m_crouch = !m_crouch || m_prone;
		m_prone = false;
	}
	if (m_crouch_since && !held) {
		if (porting::getTimeMs() - m_crouch_since > TAP)
			m_crouch = false;
		m_crouch_since = 0;
	}
	if (lie) {
		m_prone = !m_prone;
		m_crouch = false;
	}
	if (run) // setting off at a run gets up
		m_crouch = m_prone = false;
	if (isKeyDown(KeyType::JUMP) || (client->getBody().takeMoveEnded() && !held))
		m_crouch = m_prone = false;
	return m_crouch;
}

void Game::updateCameraOrientation(CameraOrientation *cam, float dtime)
{
	f32 sens_scale = getSensitivityScaleFactor();

	if (g_touchcontrols) {
		// User setting is already applied by TouchControls.
		cam->camera_yaw   += g_touchcontrols->getYawChange()   * sens_scale;
		cam->camera_pitch += g_touchcontrols->getPitchChange() * sens_scale;
	} else {
		v2s32 dist = input->getMouseMovement();

		if (m_invert_mouse || camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT) {
			dist.Y = -dist.Y;
		}

		cam->camera_yaw   -= dist.X * m_cache_mouse_sensitivity * sens_scale;
		cam->camera_pitch += dist.Y * m_cache_mouse_sensitivity * sens_scale;

		// Keep the cursor centered so that it appears in the middle of
		// the screen when a menu opens.
		if (dist.X != 0 || dist.Y != 0) {
			v2s32 center(driver->getScreenSize().Width / 2, driver->getScreenSize().Height / 2);
			input->setMousePos(center.X, center.Y);
		}
	}

	// Keyboard look
	const f32 rate = dtime * sens_scale;

	if (input->isKeyDown(KeyType::CAMERA_YAW_LEFT))
		cam->camera_yaw += input->getAxisValue(KeyType::CAMERA_YAW_LEFT) * rate;
	if (input->isKeyDown(KeyType::CAMERA_YAW_RIGHT))
		cam->camera_yaw -= input->getAxisValue(KeyType::CAMERA_YAW_RIGHT) * rate;
	if (input->isKeyDown(KeyType::CAMERA_PITCH_UP))
		cam->camera_pitch -= input->getAxisValue(KeyType::CAMERA_PITCH_UP) * rate;
	if (input->isKeyDown(KeyType::CAMERA_PITCH_DOWN))
		cam->camera_pitch += input->getAxisValue(KeyType::CAMERA_PITCH_DOWN) * rate;

	cam->camera_pitch = rangelim(cam->camera_pitch, -90, 90);
}


// Get the state of an optionally togglable key
bool Game::getTogglableKeyState(GameKeyType key, bool toggling_enabled, bool prev_key_state)
{
	if (!toggling_enabled)
		return isKeyDown(key);
	else
		return prev_key_state ^ wasKeyPressed(key);
}


void Game::updatePlayerControl(const CameraOrientation &cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	// In free move (fly), the "toggle_sneak_key" setting would prevent precise
	// up/down movements. Hence, enable the feature only during 'normal' movement.
	const bool allow_sneak_toggle = m_cache_toggle_sneak_key &&
		!(player->getPlayerSettings().free_move && client->checkPrivilege("fly"));

	//TimeTaker tt("update player control", NULL, PRECISION_NANO);

	float forward_value = getAxisValue(KeyType::FORWARD);
	if (player->getPlayerSettings().continuous_forward &&
			client->activeObjectsReceived() && !player->isDead()) {
		// autoforward if set: move at maximum speed
		forward_value = 1.0f;
	}

	PlayerControl control(
		forward_value,
		getAxisValue(KeyType::BACKWARD),
		getAxisValue(KeyType::LEFT),
		getAxisValue(KeyType::RIGHT),
		isKeyDown(KeyType::JUMP) || player->getAutojump(),
		getTogglableKeyState(KeyType::AUX1,  m_cache_toggle_aux1_key, player->control.aux1),
		sneakNow(*player),
		isKeyDown(KeyType::ZOOM),
		isKeyDown(KeyType::DIG),
		isKeyDown(KeyType::PLACE),
		cam.camera_pitch,
		cam.camera_yaw
	);
	control.setMovementFromKeys();
	control.pour = isKeyDown(KeyType::POUR); // Terra Firma: pouring (sprint 80)
	control.prone = m_prone && !bodyIsLoose(*player);

	/* For touch, simulate holding down AUX1 (fast move) if the user has
	 * the fast_move setting toggled on. If there is an aux1 key defined for
	 * touch then its meaning is inverted (i.e. holding aux1 means walk and
	 * not fast)
	 */
	if (g_touchcontrols && m_touch_simulate_aux1) {
		control.aux1 = control.aux1 ^ true;
	}

	client->setPlayerControl(control);
	client->getBody().brake(isKeyDown(KeyType::BRAKE)); // Terra Firma: thrusting's brake

	//tt.stop();
}

void Game::updatePauseState()
{
	bool was_paused = this->m_is_paused;
	bool alone = this->simple_singleplayer_mode;
#if USE_SHELL
	// Terra Firma: a world hosted for friends pauses only while no friend is in it
	alone = alone || (m_online_host && server && server->clientCount() <= 1);
#endif
	this->m_is_paused = alone && g_menumgr.pausesGame();

	if (!was_paused && this->m_is_paused) {
		this->pauseAnimation();
		this->sound_manager->pauseAll();
	} else if (was_paused && !this->m_is_paused) {
		this->resumeAnimation();
		this->sound_manager->resumeAll();
	}
}


inline void Game::step(f32 dtime)
{
	ZoneScoped;

	if (server) {
		float fps_max = !device->isWindowFocused() && simple_singleplayer_mode ?
				g_settings->getFloat("fps_max_unfocused") :
				g_settings->getFloat("fps_max");
		fps_max = std::max(fps_max, 1.0f);
		/*
		 * Unless you have a barebones game, running the server at more than 60Hz
		 * is hardly realistic and you're at the point of diminishing returns.
		 * fps_max is also not necessarily anywhere near the FPS actually achieved
		 * (also due to vsync).
		 */
		fps_max = std::min(fps_max, 60.0f);

		server->setStepSettings(Server::StepSettings{
				1.0f / fps_max,
				m_is_paused
			});

		server->step();
	}

	if (!m_is_paused)
		{
			ScopeProfiler timed(g_profiler, "Client step [us]", SPT_GRAPH_ADD, PRECISION_MICRO);
			client->step(dtime);
		}
}

static void pauseNodeAnimation(PausedNodesList &paused, scene::ISceneNode *node) {
	if (!node)
		return;
	for (auto &&child: node->getChildren())
		pauseNodeAnimation(paused, child);
	if (node->getType() != scene::ESNT_ANIMATED_MESH)
		return;
	auto animated_node = static_cast<scene::AnimatedMeshSceneNode *>(node);
	std::vector<PausedNode::Track> tracks;
	for (auto &[track, spec] : animated_node->getAnimation().tracks) {
		tracks.emplace_back(PausedNode::Track{track, spec.fps});
		spec.fps = 0.0f;
	}
	paused.emplace_back(PausedNode{grab(animated_node), tracks});
}

void Game::pauseAnimation()
{
	pauseNodeAnimation(paused_animated_nodes, smgr->getRootSceneNode());
}

void Game::resumeAnimation()
{
	for (const auto &paused: paused_animated_nodes) {
		for (const PausedNode::Track &track: paused.tracks) {
			auto &spec = paused.node->getAnimation().tracks[track.id];
			spec.fps = track.fps;
		}
	}
	paused_animated_nodes.clear();
}

const ClientEventHandler Game::clientEventHandler[CLIENTEVENT_MAX] = {
	{&Game::handleClientEvent_None},
	{&Game::handleClientEvent_PlayerDamage},
	{&Game::handleClientEvent_PlayerForceMove},
	{&Game::handleClientEvent_DeathscreenLegacy},
	{&Game::handleClientEvent_ShowFormSpec},
	{&Game::handleClientEvent_ShowCSMFormSpec},
	{&Game::handleClientEvent_ShowPauseMenuFormSpec},
	{&Game::handleClientEvent_HandleParticleEvent},
	{&Game::handleClientEvent_HandleParticleEvent},
	{&Game::handleClientEvent_HandleParticleEvent},
	{&Game::handleClientEvent_HudAdd},
	{&Game::handleClientEvent_HudRemove},
	{&Game::handleClientEvent_HudChange},
	{&Game::handleClientEvent_SetSky},
	{&Game::handleClientEvent_SetSun},
	{&Game::handleClientEvent_SetMoon},
	{&Game::handleClientEvent_SetStars},
	{&Game::handleClientEvent_OverrideDayNightRatio},
	{&Game::handleClientEvent_CloudParams},
	{&Game::handleClientEvent_UpdateCamera},
};

void Game::handleClientEvent_None(ClientEvent *event, CameraOrientation *cam)
{
	FATAL_ERROR("ClientEvent type None received");
}

void Game::handleClientEvent_PlayerDamage(ClientEvent *event, CameraOrientation *cam)
{
	if (client->modsLoaded())
		client->getScript()->on_damage_taken(event->player_damage.amount);

	if (!event->player_damage.effect)
		return;

	// Damage flash and hurt tilt are not used at death
	if (client->getHP() > 0) {
		LocalPlayer *player = client->getEnv().getLocalPlayer();

		f32 hp_max = player->getCAO() ?
			player->getCAO()->getProperties().hp_max : PLAYER_MAX_HP_DEFAULT;
		f32 damage_ratio = event->player_damage.amount / hp_max;

		if (g_settings->getBool("hurt_flash_enabled")) {
			runData.damage_flash += 95.0f + 64.f * damage_ratio;
			runData.damage_flash = MYMIN(runData.damage_flash, 127.0f);
		}

		player->hurt_tilt_timer = 1.5f;
		player->hurt_tilt_strength =
			rangelim(damage_ratio * 5.0f, 1.0f, 4.0f);
	}

	// Play damage sound
	client->getEventManager()->put(new SimpleTriggerEvent(MtEvent::PLAYER_DAMAGE));
}

void Game::handleClientEvent_PlayerForceMove(ClientEvent *event, CameraOrientation *cam)
{
	cam->camera_yaw = event->player_force_move.yaw;
	cam->camera_pitch = event->player_force_move.pitch;
}

void Game::handleClientEvent_DeathscreenLegacy(ClientEvent *event, CameraOrientation *cam)
{
	m_game_formspec.showDeathFormspecLegacy();
}

void Game::handleClientEvent_ShowFormSpec(ClientEvent *event, CameraOrientation *cam)
{
	auto &fs = event->show_formspec;

	if (fs.formname->empty() && !fs.formspec->empty()) {
		m_game_formspec.showPlayerInventory(fs.formspec);
	} else {
		m_game_formspec.showFormSpec(*fs.formspec, *fs.formname);
	}

	delete fs.formspec;
	delete fs.formname;
}

void Game::handleClientEvent_ShowCSMFormSpec(ClientEvent *event, CameraOrientation *cam)
{
	m_game_formspec.showCSMFormSpec(*event->show_formspec.formspec,
		*event->show_formspec.formname);

	delete event->show_formspec.formspec;
	delete event->show_formspec.formname;
}

void Game::handleClientEvent_ShowPauseMenuFormSpec(ClientEvent *event, CameraOrientation *cam)
{
	m_game_formspec.showPauseMenuFormSpec(*event->show_formspec.formspec,
		*event->show_formspec.formname);

	delete event->show_formspec.formspec;
	delete event->show_formspec.formname;
}

void Game::handleClientEvent_HandleParticleEvent(ClientEvent *event,
		CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	client->getParticleManager()->handleParticleEvent(event, client, player);
}

void Game::handleClientEvent_HudAdd(ClientEvent *event, CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	u32 server_id = event->hudadd->server_id;
	// ignore if we already have a HUD with that ID
	auto i = m_hud_server_to_client.find(server_id);
	if (i != m_hud_server_to_client.end()) {
		delete event->hudadd;
		return;
	}

	auto e = std::make_unique<HudElement>();
	e->type   = static_cast<HudElementType>(event->hudadd->type);
	e->pos    = event->hudadd->pos;
	e->name   = event->hudadd->name;
	e->scale  = event->hudadd->scale;
	e->text   = event->hudadd->text;
	e->number = event->hudadd->number;
	e->item   = event->hudadd->item;
	e->dir    = event->hudadd->dir;
	e->align  = event->hudadd->align;
	e->offset = event->hudadd->offset;
	e->world_pos = event->hudadd->world_pos;
	e->size      = v2f::from(event->hudadd->size);
	e->z_index   = event->hudadd->z_index;
	e->text2     = event->hudadd->text2;
	e->style     = event->hudadd->style;
	e->hideable  = event->hudadd->hideable;
	m_hud_server_to_client[server_id] = player->hud.add(std::move(e));

	delete event->hudadd;
}

void Game::handleClientEvent_HudRemove(ClientEvent *event, CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	auto i = m_hud_server_to_client.find(event->hudrm.id);
	if (i != m_hud_server_to_client.end()) {
		player->hud.remove(i->second);
		m_hud_server_to_client.erase(i);
	}

}

void Game::handleClientEvent_HudChange(ClientEvent *event, CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	HudElement *e = nullptr;

	auto i = m_hud_server_to_client.find(event->hudchange->id);
	if (i != m_hud_server_to_client.end()) {
		e = player->hud.get(i->second);
	}

	if (e == nullptr) {
		delete event->hudchange;
		return;
	}

#define CASE_SET(statval, prop, dataprop) \
	case statval: \
		e->prop = event->hudchange->dataprop; \
		break

	switch (event->hudchange->stat) {
		CASE_SET(HUD_STAT_POS, pos, v2fdata);

		CASE_SET(HUD_STAT_NAME, name, sdata);

		CASE_SET(HUD_STAT_SCALE, scale, v2fdata);

		CASE_SET(HUD_STAT_TEXT, text, sdata);

		CASE_SET(HUD_STAT_NUMBER, number, data);

		CASE_SET(HUD_STAT_ITEM, item, data);

		CASE_SET(HUD_STAT_DIR, dir, data);

		CASE_SET(HUD_STAT_ALIGN, align, v2fdata);

		CASE_SET(HUD_STAT_OFFSET, offset, v2fdata);

		CASE_SET(HUD_STAT_WORLD_POS, world_pos, v3fdata);

		CASE_SET(HUD_STAT_SIZE, size, v2fdata);

		CASE_SET(HUD_STAT_Z_INDEX, z_index, data);

		CASE_SET(HUD_STAT_TEXT2, text2, sdata);

		CASE_SET(HUD_STAT_STYLE, style, data);

		CASE_SET(HUD_STAT_HIDEABLE, hideable, data);

		case HudElementStat_END:
			break;
	}

#undef CASE_SET

	delete event->hudchange;
}

void Game::handleClientEvent_SetSky(ClientEvent *event, CameraOrientation *cam)
{
	sky->setVisible(false);
	// Whether clouds are visible in front of a custom skybox.
	sky->setCloudsEnabled(event->set_sky->clouds);

	// Clear the old textures out in case we switch rendering type.
	sky->clearSkyboxTextures();
	// Handle according to type
	if (event->set_sky->type == "regular") {
		// Shows the mesh skybox
		sky->setVisible(true);
		// Update mesh based skybox colours if applicable.
		sky->setSkyColors(event->set_sky->sky_color);
		sky->setHorizonTint(
			event->set_sky->fog_sun_tint,
			event->set_sky->fog_moon_tint,
			event->set_sky->fog_tint_type
		);
	} else if (event->set_sky->type == "skybox" &&
			event->set_sky->textures.size() == 6) {
		// Disable the dynamic mesh skybox:
		sky->setVisible(false);
		// Set fog colors:
		sky->setFallbackBgColor(event->set_sky->bgcolor);
		// Set sunrise and sunset fog tinting:
		sky->setHorizonTint(
			event->set_sky->fog_sun_tint,
			event->set_sky->fog_moon_tint,
			event->set_sky->fog_tint_type
		);
		// Add textures to skybox.
		for (int i = 0; i < 6; i++)
			sky->addTextureToSkybox(event->set_sky->textures[i], i, texture_src);
	} else {
		// Handle everything else as plain color.
		if (event->set_sky->type != "plain")
			infostream << "Unknown sky type: "
				<< (event->set_sky->type) << std::endl;
		sky->setVisible(false);
		sky->setFallbackBgColor(event->set_sky->bgcolor);
		// Disable directional sun/moon tinting on plain or invalid skyboxes.
		sky->setHorizonTint(
			event->set_sky->bgcolor,
			event->set_sky->bgcolor,
			"custom"
		);
	}

	// Orbit Tilt:
	sky->setBodyOrbitTilt(event->set_sky->body_orbit_tilt);
	// Terra Firma: the real sun, for the sky and the light
	sky->setSolar(event->set_sky->solar);
	client->getEnv().setSolar(event->set_sky->solar);

	// fog
	// do not override a potentially smaller client setting.
	sky->setFogDistance(event->set_sky->fog_distance);

	// if the fog distance is reset, switch back to the client's viewing_range
	if (event->set_sky->fog_distance < 0)
		draw_control->wanted_range = g_settings->getS16("viewing_range");

	if (event->set_sky->fog_start >= 0)
		sky->setFogStart(rangelim(event->set_sky->fog_start, 0.0f, 0.99f));
	else
		sky->setFogStart(rangelim(g_settings->getFloat("fog_start"), 0.0f, 0.99f));

	sky->setFogColor(event->set_sky->fog_color);

	sky->setAutoCaveBrightness(event->set_sky->auto_dim_skybox);

	delete event->set_sky;
}

void Game::handleClientEvent_SetSun(ClientEvent *event, CameraOrientation *cam)
{
	sky->setSunVisible(event->sun_params->visible);
	sky->setSunTexture(event->sun_params->texture,
		event->sun_params->tonemap, texture_src);
	sky->setSunScale(event->sun_params->scale);
	sky->setSunriseVisible(event->sun_params->sunrise_visible);
	sky->setSunriseTexture(event->sun_params->sunrise, texture_src);
	delete event->sun_params;
}

void Game::handleClientEvent_SetMoon(ClientEvent *event, CameraOrientation *cam)
{
	sky->setMoonVisible(event->moon_params->visible);
	sky->setMoonTexture(event->moon_params->texture,
		event->moon_params->tonemap, texture_src);
	sky->setMoonScale(event->moon_params->scale);
	delete event->moon_params;
}

void Game::handleClientEvent_SetStars(ClientEvent *event, CameraOrientation *cam)
{
	sky->setStarsVisible(event->star_params->visible);
	sky->setStarCount(event->star_params->count);
	sky->setStarColor(event->star_params->starcolor);
	sky->setStarScale(event->star_params->scale);
	sky->setStarDayOpacity(event->star_params->day_opacity);
	sky->setStarSeed(event->star_params->star_seed);
	delete event->star_params;
}

void Game::handleClientEvent_OverrideDayNightRatio(ClientEvent *event,
		CameraOrientation *cam)
{
	client->getEnv().setDayNightRatioOverride(
		event->override_day_night_ratio.do_override,
		event->override_day_night_ratio.ratio_f * 1000.0f);
}

void Game::handleClientEvent_CloudParams(ClientEvent *event, CameraOrientation *cam)
{
	clouds->setDensity(event->cloud_params.density);
	clouds->setColorBright(video::SColor(event->cloud_params.color_bright));
	clouds->setColorAmbient(video::SColor(event->cloud_params.color_ambient));
	clouds->setColorShadow(video::SColor(event->cloud_params.color_shadow));
	clouds->setHeight(event->cloud_params.height);
	clouds->setThickness(event->cloud_params.thickness);
	clouds->setSpeed(v2f(event->cloud_params.speed_x, event->cloud_params.speed_y));
}

void Game::handleClientEvent_UpdateCamera(ClientEvent *event, CameraOrientation *cam)
{
	// no parameters to update here, this just makes sure the camera is in the
	// state it should be after something was changed.
	updateCameraMode();
}

void Game::processClientEvents(CameraOrientation *cam)
{
	while (client->hasClientEvents()) {
		std::unique_ptr<ClientEvent> event(client->getClientEvent());
		FATAL_ERROR_IF(event->type >= CLIENTEVENT_MAX, "Invalid clientevent type");
		const ClientEventHandler& evHandler = clientEventHandler[event->type];
		(this->*evHandler.handler)(event.get(), cam);
	}
}

void Game::updateChat(f32 dtime)
{
	auto color_for = [](LogLevel level) -> const char* {
		switch (level) {
		case LL_ERROR  : return "\x1b(c@#F00)"; // red
		case LL_WARNING: return "\x1b(c@#EE0)"; // yellow
		case LL_INFO   : return "\x1b(c@#BBB)"; // grey
		case LL_VERBOSE: return "\x1b(c@#888)"; // dark grey
		case LL_TRACE  : return "\x1b(c@#888)"; // dark grey
		default        : return "";
		}
	};

	// Get new messages from error log buffer
	std::vector<LogEntry> entries = m_chat_log_buf.take();
	for (const auto& entry : entries) {
		std::string line;
		line.append(color_for(entry.level)).append(entry.combined);
		chat_backend->addMessage(L"", utf8_to_wide(line));
	}

	// Get new messages from client
	std::wstring message;
	while (client->getChatMessage(message)) {
		chat_backend->addUnparsedMessage(message);
	}

	// Remove old messages
	chat_backend->step(dtime);

	// Display all messages in a static text element
	auto &buf = chat_backend->getRecentBuffer();
	if (buf.getLinesModified()) {
		buf.resetLinesModified();
		m_game_ui->setChatText(chat_backend->getRecentChat(), buf.getLineCount());
	}

	// Make sure that the size is still correct
	m_game_ui->updateChatSize();
}

void Game::updateCamera(f32 dtime)
{
	ClientEnvironment &env = client->getEnv();
	LocalPlayer *player = env.getLocalPlayer();

	// For interaction purposes, get info about the held item
	ItemStack playeritem, hand;
	{
		ItemStack selected;
		playeritem = player->getWieldedItem(&selected, &hand);
	}

	ToolCapabilities playeritem_toolcap =
		playeritem.getToolCapabilities(itemdef_manager, &hand);

	float full_punch_interval = playeritem_toolcap.full_punch_interval;
	float tool_reload_ratio = runData.time_from_last_punch / full_punch_interval;

	tool_reload_ratio = std::min(tool_reload_ratio, 1.0f);
	camera->update(player, dtime, tool_reload_ratio);
	camera->step(dtime);

	if (!m_flags.disable_camera_update) {
		client->getEnv().getClientMap().updateCamera(camera->getPosition(),
			camera->getDirection(), camera->getFovMax(), camera->getOffset(),
			player->light_color);
	}
}

void Game::updateCameraMode()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	// Obey server choice
	if (player->allowed_camera_mode != CAMERA_MODE_ANY)
		camera->setCameraMode(player->allowed_camera_mode);

	GenericCAO *playercao = player->getCAO();
	if (playercao) {
		// Make the player visible depending on camera mode.
		playercao->updateMeshCulling();
		playercao->setChildrenVisible(camera->getCameraMode() > CAMERA_MODE_FIRST);
	}
}

void Game::updateCameraOffset()
{
	ClientEnvironment &env = client->getEnv();

	v3s16 old_camera_offset = camera->getOffset();

	camera->updateOffset();

	v3s16 camera_offset = camera->getOffset();

	m_camera_offset_changed = camera_offset != old_camera_offset;
	if (!m_camera_offset_changed)
		return;

	if (!m_flags.disable_camera_update) {
		auto *shadow = RenderingEngine::get_shadow_renderer();
		if (shadow) {
			shadow->getDirectionalLight().updateCameraOffset(camera);
			// FIXME: I bet we can be smarter about this and don't need to redraw
			// the shadow map at all, but this is for someone else to figure out.
			shadow->setForceUpdateShadowMap();
		}

		env.getClientMap().updateCamera(camera->getPosition(),
			camera->getDirection(), camera->getFovMax(), camera_offset,
			env.getLocalPlayer()->light_color);

		env.updateCameraOffset(camera_offset);
		clouds->updateCameraOffset(camera_offset);
	}
}

void Game::updateSound(f32 dtime)
{
	// Update sound listener
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	ClientActiveObject *parent = player->getParent();
	v3s16 camera_offset = camera->getOffset();
	sound_manager->updateListener(
			(1.0f/BS) * camera->getCameraNode()->getPosition()
					+ intToFloat(camera_offset, 1.0f),
			(1.0f/BS) * (parent ? parent->getVelocity() : player->getSpeed()),
			camera->getDirection(),
			camera->getCameraNode()->getUpVector());

	sound_volume_control(sound_manager.get(), device->isWindowActive());

	// Update sound maker
	ClientMap &map = client->getEnv().getClientMap();
	MapNode n = map.getNode(player->getFootstepNodePos());
	soundmaker->update(dtime, player->makes_footstep_sound,
			nodedef_manager->get(n).sound_footstep);
}


void Game::processPlayerInteraction(f32 dtime, bool show_hud)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	const v3f camera_direction = camera->getDirection();
	const v3s16 camera_offset  = camera->getOffset();

	/*
		Calculate what block is the crosshair pointing to
	*/

	ItemStack selected_item, hand_item;
	const ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

	const ItemDefinition &selected_def = tool_item.getDefinition(itemdef_manager);
	f32 d = getToolRange(tool_item, hand_item, itemdef_manager);

	core::line3d<f32> shootline;

	switch (camera->getCameraMode()) {
	case CAMERA_MODE_ANY:
	case CameraMode_END:
		assert(false);
		break;
	case CAMERA_MODE_FIRST:
		// Shoot from camera position, with bobbing
		shootline.start = camera->getPosition();
		break;
	case CAMERA_MODE_THIRD:
		// Shoot from player head, no bobbing
		shootline.start = camera->getHeadPosition();
		break;
	case CAMERA_MODE_THIRD_FRONT:
		shootline.start = camera->getHeadPosition();
		// prevent player pointing anything in front-view
		d = 0;
		break;
	case CAMERA_MODE_SHOULDER:
		// Terra Firma: along the crosshair, but reach counts from level with the head
		shootline.start = camera->getPosition() + camera_direction *
				(camera->getHeadPosition() - camera->getPosition()).dotProduct(camera_direction);
		break;
	}
	shootline.end = shootline.start + camera_direction * BS * d;

	// Terra Firma: the cursor free, through it, as far as the hand reaches
	if (m_cursor) {
		core::line3d<f32> through = smgr->getSceneCollisionManager()->getRayFromScreenCoordinates(
				input->getMousePos(), camera->getCameraNode());
		const v3f offset = intToFloat(camera->getOffset(), BS);
		const v3f dir = (through.end - through.start).normalize();
		shootline.start = through.start + offset;
		shootline.end = shootline.start + dir * BS * d;
	}

	if (isTouchShootlineUsed()) {
		shootline = g_touchcontrols->getShootline();
		// Scale shootline to the acual distance the player can reach
		shootline.end = shootline.start +
				shootline.getVector().normalize() * BS * d;
		shootline.start += intToFloat(camera_offset, BS);
		shootline.end += intToFloat(camera_offset, BS);
	}

	m_pointed_from = shootline; // Terra Firma: pieces are pointed at again in their own grid
	PointedThing pointed = updatePointedThing(shootline,
			selected_def.liquids_pointable,
			selected_def.pointabilities,
			!runData.btn_down_for_dig,
			camera_offset);

	// Terra Firma: machines are pointed at by what they're built of, not by
	// a box round them (sprint 69): through the box's empty corners, what's
	// behind; a part nearer than the block hit, the part
	{
		const v3f from = shootline.start;
		const auto far = [&](const PointedThing &p) {
			return p.type == POINTEDTHING_NOTHING ? shootline.getLength() : p.intersection_point.getDistanceFrom(from);
		};
		if (pointed.type == POINTEDTHING_OBJECT) {
			auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(pointed.object_id));
			if (obj && obj->getMachine() && !aimAtPiece(pointed.object_id))
				pointed = updatePointedThing(shootline, selected_def.liquids_pointable, selected_def.pointabilities,
						false, camera_offset);
		}
		std::vector<DistanceSortedActiveObject> near;
		client->getEnv().getActiveObjects(from, shootline.getLength() + 4 * BS, near);
		for (const DistanceSortedActiveObject &d : near) {
			auto *obj = dynamic_cast<GenericCAO *>(d.obj);
			if (!obj || !obj->getMachine() || (pointed.type == POINTEDTHING_OBJECT && pointed.object_id == obj->getId()))
				continue;
			const std::optional<Aim> aim = aimAtPiece(obj->getId());
			if (!aim)
				continue;
			v3f hit = aim->point;
			aim->frame.transformVect(hit);
			if (hit.getDistanceFrom(from) >= far(pointed))
				continue;
			const v3f normal = aim->frame.rotateAndScaleVect(v3f::from(aim->normal)).normalize();
			pointed = PointedThing(obj->getId(), hit, normal, normal, hit.getDistanceFromSQ(from), PointabilityType::POINTABLE);
		}
	}

	if (pointed != runData.pointed_old)
		infostream << "Pointing at " << pointed.dump() << std::endl;

	if (g_touchcontrols) {
		auto mode = selected_def.touch_interaction.getMode(selected_def, pointed.type);
		g_touchcontrols->applyContextControls(mode);
		// applyContextControls may change dig/place input.
		// Update again so that TOSERVER_INTERACT packets have the correct controls set.
		player->control.dig = isKeyDown(KeyType::DIG);
		player->control.place = isKeyDown(KeyType::PLACE);
	}

	// Note that updating the selection mesh every frame is not particularly efficient,
	// but the halo rendering code is already inefficient so there's no point in optimizing it here
	hud->updateSelectionMesh(camera_offset);

	// Allow digging again if button is not pressed
	if (runData.digging_blocked && !isKeyDown(KeyType::DIG))
		runData.digging_blocked = false;

	/*
		Stop digging when
		- releasing dig button
		- pointing away from node
	*/
	if (runData.digging) {
		if (wasKeyReleased(KeyType::DIG)) {
			infostream << "Dig button released (stopped digging)" << std::endl;
			runData.digging = false;
		} else if (pointed != runData.pointed_old) {
			if (pointed.type == POINTEDTHING_NODE
					&& runData.pointed_old.type == POINTEDTHING_NODE
					&& pointed.node_undersurface
							== runData.pointed_old.node_undersurface) {
				// Still pointing to the same node, but a different face.
				// Don't reset.
			} else {
				infostream << "Pointing away from node (stopped digging)" << std::endl;
				runData.digging = false;
				hud->updateSelectionMesh(camera_offset);
			}
		}

		if (!runData.digging) {
			client->interact(INTERACT_STOP_DIGGING, runData.pointed_old);
			client->setCrack(-1, v3s16(0, 0, 0));
			runData.dig_time = 0.0;
		}
	} else if (runData.dig_instantly && wasKeyReleased(KeyType::DIG)) {
		// Remove e.g. torches faster when clicking instead of holding dig button
		runData.nodig_delay_timer = 0;
		runData.dig_instantly = false;
	}

	if (!runData.digging && runData.btn_down_for_dig && !isKeyDown(KeyType::DIG))
		runData.btn_down_for_dig = false;

	runData.punching = false;

	soundmaker->m_player_leftpunch_sound = SoundSpec();
	soundmaker->m_player_leftpunch_sound2 = pointed.type != POINTEDTHING_NOTHING ?
		selected_def.sound_use : selected_def.sound_use_air;

	// Prepare for repeating, unless we're not supposed to
	if (isKeyDown(KeyType::PLACE) && !g_settings->getBool("safe_dig_and_place"))
		runData.repeat_place_timer += dtime;
	else
		runData.repeat_place_timer = 0;

	// Terra Firma: what's been let go rolls on here, drawn where it is here,
	// till it's still and the server's to draw again (sprint 72)
	{
		std::set<u16> now;
		for (const u16 id : client->getBody().coasting())
			if (auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id)))
				if (const std::optional<std::pair<v3f, v3f>> here = client->getBody().grabbed(id)) {
					obj->applyPosed(*here);
					now.insert(id);
				}
		for (const u16 id : m_coasting)
			if (!now.count(id) && !(m_grab && m_grab->object == id))
				if (auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id)))
					obj->setPosed(std::nullopt);
		m_coasting = std::move(now);
	}
	// Terra Firma: what the page's rail asked, now what's pointed at is known
	for (const Hand::Ask &ask : m_hand.take())
		handleAsk(ask, pointed);
	// Terra Firma: an object pointed at in the world is lit, and what a
	// press would do said: with the tools away it's carried, out it's
	// worked on where it is
	// (with the tools out on the bench they draw, so nothing else is lit;
	// with them away, anything but the workpiece is)
	const bool benched = m_hold && m_hand.out() && m_hand.creating();
	const u16 lit = !benched && !m_carry && pointed.type == POINTEDTHING_OBJECT && pieceMiddle(pointed.object_id)
			&& !(m_hold && m_hold->object() == pointed.object_id)
			? pointed.object_id : 0;
	// the part of it looked at lit, not all of it (sprint 64)
	std::optional<u64> lit_part;
	if (lit)
		if (const std::optional<Aim> aim = aimAtPiece(lit))
			lit_part = aim->part;
	if (lit != m_lit || lit_part != m_lit_part) {
		for (const u16 id : { m_lit, lit })
			if (auto *obj = id ? dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id)) : nullptr)
				obj->setLook({ std::nullopt, id == lit ? lit_part : std::nullopt, false, id == lit && !lit_part });
		m_lit_part = lit_part;
	}
	if (lit != m_lit) {
		const GenericCAO *small = lit ? dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(lit)) : nullptr;
		if (small && !m_hand.out() && small->getMachine() && small->getMachine()->scale() < 1)
			m_game_ui->showTranslatedStatusText(holdingSmall() ? "Left click: strike · F: pick up" : "F: pick up");
		else if (lit)
			m_game_ui->showTranslatedStatusText(m_hand.out() ? "Draw on it here · F to pick it up · Tab: hands to carry it"
					: "Hold left to grab it here · Alt: carry it · F to pick it up and work on it");
		m_lit = lit;
	}
	gridStep();
	// Terra Firma: Shift-click on a socket, another of the last thing
	// plugged in put there (sprint 64)
	const std::optional<std::pair<u16, u64>> again = !m_carry && !m_hand.out() && isKeyDown(KeyType::AUX1)
			&& wasKeyPressed(KeyType::DIG) ? socketAt() : std::nullopt;
	const GenericCAO *struck = !m_carry && !m_grab && !m_menu && !m_hand.out() && !m_driving && lit
			&& wasKeyPressed(KeyType::DIG) && holdingSmall()
			? dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(lit)) : nullptr;
	if (struck && (!struck->getMachine() || struck->getMachine()->scale() == 1))
		struck = nullptr;
	if (again) {
		client->sendPlugAgain(again->first, again->second);
	} else if (struck) {
		// Terra Firma: one small thing struck with another (sprint 130): where,
		// and which way, on its grid; tf_tools says what comes of it
		if (const std::optional<Aim> aim = aimAtPiece(lit)) {
			const shape::Sixteenth s = shape::hit(aim->node, aim->point, aim->normal);
			core::matrix4 into;
			if (!aim->frame.getInverse(into))
				throw BaseException("a part's frame has no inverse");
			v3f in = m_pointed_from.end - m_pointed_from.start;
			in = into.rotateAndScaleVect(in);
			in.normalize();
			std::ostringstream json;
			json << "{\"object\":" << lit << ",\"cell\":[" << s.node.X * shape::STEPS + s.cell.X << ","
				<< s.node.Y * shape::STEPS + s.cell.Y << "," << s.node.Z * shape::STEPS + s.cell.Z << "],\"in\":["
				<< in.X << "," << in.Y << "," << in.Z << "]}";
			client->sendUiEvent("tf_tools:strike", json.str());
		}
	} else if (m_driving) {
		// Terra Firma: sat in from an object's menu (sprint 69, 71)
		handleDrive(pointed, dtime);
	} else if (m_grab || (!m_carry && !m_menu && !m_hand.out() && wasKeyPressed(KeyType::DIG) && lit
			&& !isKeyDown(KeyType::ALT))) {
		// Terra Firma: with the tools away, an object pressed on is grabbed
		// where it's pressed (sprint 68)
		handleGrab(pointed, dtime);
	} else if (m_carry || (!m_menu && !m_hand.out() && wasKeyPressed(KeyType::DIG) && lit)) {
		// Terra Firma: Alt-pressed, it's carried whole
		handleCarry(pointed, dtime);
	} else if (m_menu) {
		// Terra Firma: an object's menu, in the world, takes the clicks
		menuStep(dtime);
	} else if (fitStep(lit)) {
		// Terra Firma: a seat or an engine held: fitted where it's outlined (sprint 73)
	} else if (!m_hand.out() && wasKeyPressed(KeyType::PLACE) && m_hover_joint) {
		// Terra Firma: right-click on a joint looked at: its own menu (sprint 72)
		openJointMenu(*m_hover_joint);
	} else if (!m_hand.out() && wasKeyPressed(KeyType::PLACE) && (lit || (m_hold && aimAtPiece(m_hold->object())))) {
		// Terra Firma: right-click on an object: its menu (sprint 71)
		const u16 object = lit ? lit : m_hold->object();
		if (const std::optional<Aim> aim = aimAtPiece(object))
			openMenu(*aim);
	} else if (!m_hand.out() && m_hold && wasKeyPressed(KeyType::DIG) && aimAtPiece(m_hold->object())) {
		// Terra Firma: with the tools away, a part of the workpiece pressed
		// on is pulled off, with what hangs from it, and carried while the
		// button's down (sprint 77); Shift-clicked, it's selected
		const Aim aim = *aimAtPiece(m_hold->object());
		const GenericCAO *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(aim.piece));
		const piece::Machine *m = obj ? obj->getMachine() : nullptr;
		if (isKeyDown(KeyType::AUX1) || !m) {
			selectPart(aim);
		} else if (m->parts().front().object.id() == aim.part) {
			m_game_ui->showTranslatedStatusText("The rest hangs from this part: pull the others off it");
		} else {
			v3f at = aim.point;
			aim.frame.transformVect(at);
			m_pulled_at = at;
			client->sendPull(aim.piece, aim.part);
		}
	} else if (m_hand.out()) {
		// Terra Firma: the tools in hand make strokes, on the world or what's held
		handleTools(pointed);
	} else if (selected_def.usable && isKeyDown(KeyType::DIG)) {
		if (wasKeyPressed(KeyType::DIG) && (!client->modsLoaded() ||
				!client->getScript()->on_item_use(selected_item, pointed)))
			client->interact(INTERACT_USE, pointed);
	} else if (pointed.type == POINTEDTHING_NODE && m_hold
			&& intToFloat(pointed.node_undersurface, 1).getDistanceFrom(m_hold->held()) <= m_hold->radius() + 2) {
		// Terra Firma: the ground round the workpiece isn't dug (sprint 77:
		// a press that missed a part dug under it)
		if (wasKeyPressed(KeyType::DIG))
			m_game_ui->showTranslatedStatusText("Not round the workpiece · F to put it down");
	} else if (pointed.type == POINTEDTHING_NODE) {
		handlePointingAtNode(pointed, selected_item, hand_item, dtime);
	} else if (pointed.type == POINTEDTHING_OBJECT) {
		v3f player_position  = player->getPosition();
		bool basic_debug_allowed = client->checkPrivilege("debug") || (player->hud_flags & HUD_FLAG_BASIC_DEBUG);
		handlePointingAtObject(pointed, tool_item, hand_item, player_position,
				m_game_ui->m_flags.show_basic_debug && basic_debug_allowed);
	} else if (isKeyDown(KeyType::DIG)) {
		// When button is held down in air, show continuous animation
		runData.punching = true;
		// Run callback even though item is not usable
		if (wasKeyPressed(KeyType::DIG) && client->modsLoaded())
			client->getScript()->on_item_use(selected_item, pointed);
	} else if (wasKeyPressed(KeyType::PLACE)) {
		handlePointingAtNothing(selected_item);
	}

	runData.pointed_old = pointed;

	if (runData.punching || wasKeyPressed(KeyType::DIG))
		camera->setDigging(0); // dig animation

	input->clearWasKeyPressed();
	input->clearWasKeyReleased();
	// Ensure DIG & PLACE are marked as handled
	wasKeyDown(KeyType::DIG);
	wasKeyDown(KeyType::PLACE);
}


PointedThing Game::updatePointedThing(
	const core::line3d<f32> &shootline,
	bool liquids_pointable,
	const std::optional<Pointabilities> &pointabilities,
	bool look_for_object,
	const v3s16 &camera_offset)
{
	std::vector<aabb3f> *selectionboxes = hud->getSelectionBoxes();
	selectionboxes->clear();
	hud->getSelectionLines()->clear();
	hud->setSelectedFaceNormal(v3f());
	static thread_local const bool show_entity_selectionbox = g_settings->getBool(
		"show_entity_selectionbox");

	ClientEnvironment &env = client->getEnv();
	ClientMap &map = env.getClientMap();
	const NodeDefManager *nodedef = map.getNodeDefManager();

	runData.selected_object = NULL;
	hud->pointing_at_object = false;

	RaycastState s(shootline, look_for_object, liquids_pointable, pointabilities);
	PointedThing result;
	env.continueRaycast(&s, &result);
	if (result.type == POINTEDTHING_OBJECT) {
		hud->pointing_at_object = true;

		runData.selected_object = client->getEnv().getActiveObject(result.object_id);
		aabb3f selection_box{{0.0f, 0.0f, 0.0f}};
		if (show_entity_selectionbox && runData.selected_object->doShowSelectionBox() &&
				runData.selected_object->getSelectionBox(&selection_box)) {
			v3f pos = runData.selected_object->getPosition();
			selectionboxes->push_back(selection_box);
			hud->setSelectionPos(pos, camera_offset);
			GenericCAO* gcao = dynamic_cast<GenericCAO*>(runData.selected_object);
			if (gcao != nullptr && gcao->getProperties().rotate_selectionbox)
				hud->setSelectionRotationRadians(gcao->getSceneNode()
						->getAbsoluteTransformation().getRotationRadians());
			else
				hud->setSelectionRotationRadians(v3f());
		}
		hud->setSelectedFaceNormal(result.raw_intersection_normal);
	} else if (result.type == POINTEDTHING_NODE) {
		// Update selection boxes
		MapNode n = map.getNode(result.node_undersurface);
		std::vector<aabb3f> boxes;
		// Terra Firma: a shaped node (cut, smoothed ground) is outlined
		// round what's there, not each of its boxes
		std::vector<aabb3f> shaped;
		if (map.getShapedBoxes(result.node_undersurface, shaped)) {
			if (!shaped.empty()) {
				aabb3f all = shaped[0];
				for (const aabb3f &b : shaped)
					all.addInternalBox(b);
				boxes.push_back(all);
			}
		} else {
			n.getSelectionBoxes(nodedef, &boxes,
				n.getNeighbors(result.node_undersurface, &map));
		}

		f32 d = 0.002f * BS;
		for (aabb3f box : boxes) {
			box.MinEdge -= v3f(d, d, d);
			box.MaxEdge += v3f(d, d, d);
			selectionboxes->push_back(box);
		}
		hud->setSelectionPos(intToFloat(result.node_undersurface, BS),
			camera_offset);
		hud->setSelectionRotationRadians(v3f());
		hud->setSelectedFaceNormal(result.intersection_normal);
		// Terra Firma: on a planet, the cell as it's drawn (over a seam, cut)
		if (AtlasView *view = map.atlasView())
			if (auto lines = view->outline(result.node_undersurface, *selectionboxes))
				*hud->getSelectionLines() = std::move(*lines);
	}

	// Terra Firma: the tools: a ghost of what a dig would take, its own
	// edges drawn, on the world or a piece
	// (drawing only: selecting and joining outline the part instead)
	const sculpt::Act act = brushNow().act;
	if (m_draw && m_draw->pulling) {
		if (const std::optional<sculpt::Stroke> stroke = drawn(*m_draw))
			showGhost(m_draw->start, *stroke, camera_offset);
	} else if (m_draw) {
		if (const std::optional<sculpt::Stroke> stroke = strokeAt(m_draw->start, act, m_draw->to))
			showGhost(m_draw->start, *stroke, camera_offset);
	} else if (const std::optional<Aim> aim = aimFor(result)) {
		if (const std::optional<sculpt::Stroke> stroke = strokeAt(*aim, act))
			showGhost(*aim, *stroke, camera_offset);
	}

	// Update selection mesh light level and vertex colors
	if (!selectionboxes->empty()) {
		v3f pf = hud->getSelectionPos();
		v3s16 p = floatToInt(pf, BS);

		// Get selection mesh light level
		MapNode n = map.getNode(p);
		u16 node_light = getInteriorLight(n, -1, nodedef);
		u16 light_level = node_light;

		for (const v3s16 &dir : g_6dirs) {
			n = map.getNode(p + dir);
			node_light = getInteriorLight(n, -1, nodedef);
			if (node_light > light_level)
				light_level = node_light;
		}

		u32 daynight_ratio = client->getEnv().getDayNightRatio();
		video::SColor c;
		final_color_blend(&c, light_level, daynight_ratio);

		// Modify final color a bit with time
		u32 timer = client->getEnv().getFrameTime() % 5000;
		float timerf = (float) (core::PI * ((timer / 2500.0) - 0.5));
		float sin_r = 0.08f * std::sin(timerf);
		float sin_g = 0.08f * std::sin(timerf + core::PI * 0.5f);
		float sin_b = 0.08f * std::sin(timerf + core::PI);
		c.setRed(core::clamp(core::round32(c.getRed() * (0.8 + sin_r)), 0, 255));
		c.setGreen(core::clamp(core::round32(c.getGreen() * (0.8 + sin_g)), 0, 255));
		c.setBlue(core::clamp(core::round32(c.getBlue() * (0.8 + sin_b)), 0, 255));

		// Set mesh final color
		hud->setSelectionMeshColor(c);
	}
	hud->setSelectionThrough(m_ghost_shown);
	m_ghost_shown = false;
	// Terra Firma: what's pointed at may have been changed above (a machine
	// looked through, a nearer part over a node, sprint 69): the object
	// selected is the one it ends on, or none (a stale one crashed the punch)
	runData.selected_object = result.type == POINTEDTHING_OBJECT
			? client->getEnv().getActiveObject(result.object_id) : nullptr;
	return result;
}


void Game::handlePointingAtNothing(const ItemStack &playerItem)
{
	infostream << "Attempted to place item while pointing at nothing" << std::endl;
	PointedThing fauxPointed;
	fauxPointed.type = POINTEDTHING_NOTHING;
	client->interact(INTERACT_ACTIVATE, fauxPointed);
}


void Game::handlePointingAtNode(const PointedThing &pointed,
	const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime)
{
	v3s16 nodepos = pointed.node_undersurface;
	v3s16 neighborpos = pointed.node_abovesurface;

	/*
		Check information text of node
	*/

	ClientMap &map = client->getEnv().getClientMap();

	// Terra Firma: left click uses what's in the hand, and only something made to
	// dig digs (the `digs` group); carving by hand is build mode's (sprint 130)
	if (itemgroup_get(selected_item.getDefinition(itemdef_manager).groups, "digs") > 0
			&& runData.nodig_delay_timer <= 0.0 && isKeyDown(KeyType::DIG)
			&& !runData.digging_blocked
			&& client->checkPrivilege("interact")) {
		handleDigging(pointed, nodepos, selected_item, hand_item, dtime);
	}

	// This should be done after digging handling
	NodeMetadata *meta = map.getNodeMetadata(nodepos);

	if (meta) {
		m_game_ui->setInfoText(unescape_translate(utf8_to_wide(
			meta->getString("infotext"))));
	} else {
		MapNode n = map.getNode(nodepos);

		if (nodedef_manager->get(n).name == "unknown") {
			m_game_ui->setInfoText(L"Unknown node");
		}
	}

	if ((wasKeyPressed(KeyType::PLACE) ||
			runData.repeat_place_timer >= m_repeat_place_time) &&
			client->checkPrivilege("interact")) {
		runData.repeat_place_timer = 0;
		infostream << "Place button pressed while looking at ground" << std::endl;

		// Placing animation (always shown for feedback)
		camera->setDigging(1);

		soundmaker->m_player_rightpunch_sound = SoundSpec();

		// If the wielded item has node placement prediction,
		// make that happen
		// And also set the sound and send the interact
		// But first check for meta formspec and rightclickable
		auto &def = selected_item.getDefinition(itemdef_manager);
		bool placed = nodePlacement(def, selected_item, nodepos, neighborpos,
			pointed, meta);

		if (placed && client->modsLoaded())
			client->getScript()->on_placenode(pointed, def);
	}
}

std::optional<Game::Aim> Game::aimAt(const PointedThing &pointed) const
{
	if (!m_hand.out())
		return std::nullopt;
	// in the world, what's pointed at: the ground, or an object (shaping
	// the world, a workpiece floating by is just another object)
	if (!m_hold || !m_hand.creating()) {
		// the world's cut and painted where it's hit (sprint 70: one build
		// mode); added to, it's a new object unless Alt's held
		if (pointed.type == POINTEDTHING_NODE)
			return Aim{ 0, 0, core::matrix4(), pointed.node_undersurface, pointed.intersection_point,
				floatToInt(pointed.intersection_normal, 1) };
		if (pointed.type != POINTEDTHING_OBJECT)
			return std::nullopt;
		return aimAtPiece(pointed.object_id);
	}
	// on the bench, only the workpiece (the ground round it isn't cut by a
	// stroke that misses it); but a new part's drawn on the ground too
	// (sprint 61: the parts to bring to it), the nearer of the two
	std::optional<Aim> ground;
	if (pointed.type == POINTEDTHING_NODE && brushNow().apart)
		ground = Aim{ 0, 0, core::matrix4(), pointed.node_undersurface, pointed.intersection_point,
			floatToInt(pointed.intersection_normal, 1) };
	const u16 id = m_hold->object();
	const std::optional<Aim> held = aimAtPiece(id);
	if (!ground)
		return held;
	if (!held)
		return ground;
	// both: the nearer
	v3f at = held->point;
	held->frame.transformVect(at);
	return (at - m_pointed_from.start).getLengthSQ() <= (ground->point - m_pointed_from.start).getLengthSQ()
			? held : ground;
}

std::optional<Game::Aim> Game::aimAtPiece(u16 id) const
{
	// a machine: the ray again in each part's own grid, the nearest hit
	// (holding it with a part picked, drawing, only that part: selecting,
	// any, so another can be Shift-clicked)
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id));
	if (!obj || !obj->getMachine())
		return std::nullopt;
	const bool alone = m_hold && m_part && m_hand.out();
	std::optional<Aim> best;
	f32 best_d = 0;
	for (const piece::Machine::Part &part : obj->getMachine()->parts()) {
		if (alone && *m_part != part.object.id())
			continue;
		const core::matrix4 frame = obj->getFrame() * obj->getMachine()->scaling() * part.pose.matrix();
		core::matrix4 into;
		if (!frame.getInverse(into))
			throw BaseException("a part's frame has no inverse");
		v3f start = m_pointed_from.start, end = m_pointed_from.end;
		into.transformVect(start);
		into.transformVect(end);
		const std::optional<piece::Piece::Hit> hit = part.object.design().raycast({ start, end });
		if (!hit)
			continue;
		const f32 d = (hit->point - start).getLengthSQ();
		if (!best || d < best_d) {
			best = Aim{ id, part.object.id(), frame, hit->node, hit->point, hit->normal };
			best_d = d;
		}
	}
	return best;
}

bool Game::holdingSmall() const
{
	ItemStack selected, hand;
	const ItemStack &held = client->getEnv().getLocalPlayer()->getWieldedItem(&selected, &hand);
	return !held.metadata.getString("piece").empty();
}

const Hand::BrushKind &Game::brushNow() const
{
	// Shift with Add or Cut pulls a flat face out or in (sprint 65); once
	// a stroke's begun, what it began as
	const bool pull = m_draw ? m_draw->pull
			: input->isKeyDown(KeyType::AUX1) && (m_hand.brush() == Hand::Brush::Add || m_hand.brush() == Hand::Brush::Cut);
	return pull ? Hand::BRUSHES[size_t(Hand::Brush::Pull)] : m_hand.acting();
}

std::optional<Game::Aim> Game::aimFor(const PointedThing &pointed)
{
	// creating, adding, a socket of the workpiece looked at: drawn from
	// its middle, out of its face (sprint 65: a wheel on its axle)
	m_aimed_socket.reset();
	if (m_hand.creating() && m_hold && brushNow().apart)
		if (const std::optional<std::pair<u16, u64>> at = socketAt(); at && at->first == m_hold->object())
			if (const std::optional<Aim> aim = socketAim(at->first, at->second)) {
				m_aimed_socket = at->second;
				return aim;
			}
	return aimAt(pointed);
}

std::optional<Game::Aim> Game::socketAim(u16 object, u64 id) const
{
	const piece::Machine::Socket *k = socketOf(object, id);
	if (!k)
		return std::nullopt;
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object));
	const piece::Pose &pose = partPose(object, k->part);
	// the socket in its part's grid: its point, its axis, the node it's on
	const v3f q = pose.unapply(k->point);
	v3s16 n;
	const piece::Turn &t = piece::turn(pose.turn);
	for (int a = 0; a < 3; a++)
		n[a] = s16(k->axis.X * t.axes[a].X + k->axis.Y * t.axes[a].Y + k->axis.Z * t.axes[a].Z);
	const v3f inside = q - v3f::from(n) * 0.5f;
	const v3s16 node(std::floor(inside.X / shape::STEPS), std::floor(inside.Y / shape::STEPS),
			std::floor(inside.Z / shape::STEPS));
	return Aim{ object, k->part, obj->getFrame() * pose.matrix(), node, (q / shape::STEPS - v3f(0.5f)) * BS, n };
}

const piece::Machine::Socket *Game::socketOf(u16 object, u64 id) const
{
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object));
	if (!obj || !obj->getMachine())
		return nullptr;
	for (const piece::Machine::Socket &k : obj->getMachine()->sockets())
		if (k.id == id)
			return &k;
	return nullptr;
}

const piece::Machine::Socket *Game::mirrorSocket(u16 object, const piece::Machine::Socket &k,
		const sculpt::Stroke::Mirror &mirror) const
{
	// the mirror is in the part's own sixteenths: the socket's point there,
	// mirrored, back in the machine's
	const piece::Pose &pose = partPose(object, k.part);
	v3f local = pose.unapply(k.point);
	local[mirror.axis] = mirror.twice - local[mirror.axis];
	const v3f there = pose.apply(local);
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object));
	for (const piece::Machine::Socket &o : obj->getMachine()->sockets())
		if (o.id != k.id && o.point.getDistanceFrom(there) < 1.f)
			return &o;
	return nullptr;
}

std::optional<sculpt::Stroke> Game::strokeAt(const Aim &aim, sculpt::Act act, std::optional<shape::Sixteenth> to) const
{
	// past the player's reach the server would refuse it (the focus view's
	// camera can be further out than the player stands)
	v3f there = aim.point;
	aim.frame.transformVect(there);
	const v3f eye = client->getEnv().getLocalPlayer()->getEyePosition();
	if (eye.getDistanceFrom(there) > (sculpt::REACH + m_hand.size() / 16.f) * BS)
		return std::nullopt;
	sculpt::Stroke s{ m_hand.tool(), m_hand.size(), shape::hit(aim.node, aim.point, aim.normal),
		aim.normal, act, std::nullopt, to, std::nullopt };
	if (m_hand.mirrored()) {
		// across the focus's middle (in its frame); outside the focus view,
		// across the pointed block's, left to right as the player faces
		const v3f look = camera->getDirection();
		int axis = mirrorAxis(0, std::abs(look.X) > std::abs(look.Z) ? 2 : 0);
		f32 middle = aim.node[axis];
		if (m_hold && m_hold->object() == aim.piece) {
			// the machine's middle, across its axis left to right, in the
			// part's own grid
			const piece::Pose &pose = partPose(aim.piece, aim.part);
			const int machine_axis = mirrorAxis(m_hold->object(), sideAxis(m_hold->object()));
			for (int a = 0; a < 3; a++)
				if (piece::turn(pose.turn).axes[a][machine_axis] != 0)
					axis = a;
			const v3f local = pose.unapply((m_hold->middle() + v3f(0.5f)) * shape::STEPS) / shape::STEPS - v3f(0.5f);
			middle = local[axis];
		}
		s.mirror = sculpt::Stroke::Mirror{ axis, s32(std::lround(middle * 2 * shape::STEPS)) + shape::STEPS };
	}
	if (!s.valid())
		return std::nullopt;
	return s;
}

void Game::handleTools(const PointedThing &pointed)
{
	if (!client->checkPrivilege("interact"))
		return;
	// the primary button draws or selects, from what it's pressed on;
	// nothing's done till it's let go (and a shape, till it's pulled and
	// clicked). The other button turns what's held.
	if (!m_draw) {
		if (!wasKeyPressed(KeyType::DIG))
			return;
		const Hand::BrushKind &brush = brushNow();
		const std::optional<Aim> aim = aimFor(pointed);
		if (!aim)
			return;
		m_draw = Draw{ *aim, std::nullopt, false, 1, std::nullopt, false, porting::getTimeMs() };
		m_draw->pull = brush.pulls;
		m_draw->socket = m_aimed_socket;
		m_drag = {}; // not a turn
		closeMenu();
		if (brush.pulls) {
			// the flat stretch pressed on, pulled from now
			const auto flat = flatFace(*aim);
			if (!flat) {
				m_draw.reset();
				return;
			}
			m_draw->to = flat->second;
			m_draw->pulling = true;
			m_draw->pull_from = flat->first;
		}
		return;
	}
	const Hand::BrushKind &brush = brushNow();

	if (!m_draw->pulling) {
		// the footprint, as it's dragged; let go, pulled. Let go before it's
		// dragged (a click), it's still the footprint till the next click:
		// a quick press doesn't skip to pulling
		m_draw->to = dragTo(*m_draw);
		if (const std::optional<sculpt::Stroke> s = strokeAt(m_draw->start, brush.act, m_draw->to))
			m_game_ui->showStatusText(utf8_to_wide(std::string(sculpt::KINDS[size_t(s->tool)].name) + " · " + s->measure()
					+ (m_draw->clicked ? " · click to end it, then pull it out"
					: m_draw->to ? "" : " · drag it out, or click")));
		if (wasKeyPressed(KeyType::PLACE)) {
			m_draw.reset();
			m_place_spent = true; // a stroke stopped, not the workpiece turned or its menu opened
			return;
		}
		// a drag: held a while and moved (a click's small slip of the mouse
		// at a sixteenth's size isn't one)
		constexpr u64 CLICK = 250; // ms
		const bool dragged = m_draw->to && porting::getTimeMs() - m_draw->pressed_at > CLICK;
		if (m_draw->clicked ? wasKeyPressed(KeyType::DIG) : !isKeyDown(KeyType::DIG) && dragged)
			m_draw->pulling = true;
		else if (!m_draw->clicked && !isKeyDown(KeyType::DIG))
			m_draw->clicked = true;
		return;
	}
	m_draw->deep = pullDepth(*m_draw);
	const std::optional<sculpt::Stroke> s = drawn(*m_draw);
	if (s)
		m_game_ui->showStatusText(utf8_to_wide(std::string(brush.pulls ? "Pull" : sculpt::KINDS[size_t(s->tool)].name)
				+ " · " + s->measure() + (brush.pulls ? " · let go to make it" : " · click to make it, Esc to stop")));
	// the other button stops it
	if (wasKeyPressed(KeyType::PLACE)) {
		m_draw.reset();
		m_place_spent = true;
		return;
	}
	// a face's pulled as it's held, let go it's made; a shape's made on a click
	const bool made = brush.pulls ? !isKeyDown(KeyType::DIG) : wasKeyPressed(KeyType::DIG);
	if (!made)
		return;
	if (s) {
		// a part of its own on the ground, or out of a socket (joined as it
		// says, the socket taken up: sprint 65); on the workpiece, more of
		// the part drawn on
		const piece::Machine::Socket *socket = m_draw->socket ? socketOf(m_draw->start.piece, *m_draw->socket) : nullptr;
		const bool apart = brush.apart && (!m_draw->start.piece || socket);
		client->sendStroke(m_draw->start.piece, m_draw->start.part, *s,
				apart ? std::optional<piece::Joint>(socket ? socket->joint : piece::Joint::Fixed) : std::nullopt);
		if (socket) {
			const u64 id = socket->id;
			// and its mirror's, drawn as its mirror
			if (s->mirror)
				if (const piece::Machine::Socket *across = mirrorSocket(m_draw->start.piece, *socket, *s->mirror))
					client->sendUnsocket(m_draw->start.piece, across->id);
			client->sendUnsocket(m_draw->start.piece, id);
		}
	}
	m_draw.reset();
}

std::optional<sculpt::Stroke> Game::drawn(const Draw &draw) const
{
	// pulled out, the brush adds; in, it cuts (or, adding, it's at least a sixteenth out)
	const Hand::BrushKind &brush = brushNow();
	sculpt::Act act = brush.act;
	if (brush.pulls)
		act = draw.deep >= 0 ? sculpt::Act::Add : sculpt::Act::Cut;
	const s32 into = act == sculpt::Act::Add ? std::max(1, draw.deep) : std::max(1, -draw.deep);
	std::optional<sculpt::Stroke> s = brush.pulls
			? sculpt::Stroke{ sculpt::Tool::Box, 1, *draw.pull_from, draw.start.normal, act, std::nullopt, draw.to, std::nullopt }
			: strokeAt(draw.start, act, draw.to);
	if (!s)
		return std::nullopt;
	s->deep = std::min<s32>(into, sculpt::MAX_DEEP);
	if (!s->valid())
		return std::nullopt;
	return s;
}

s32 Game::pullDepth(const Draw &draw) const
{
	// the line out of the face, and the cursor's ray, in the face's frame
	core::matrix4 into;
	if (!draw.start.frame.getInverse(into))
		throw BaseException("an aim's frame has no inverse");
	v3f s = m_pointed_from.start, e = m_pointed_from.end;
	into.transformVect(s);
	into.transformVect(e);
	const shape::Sixteenth from = draw.pull_from ? *draw.pull_from
			: shape::hit(draw.start.node, draw.start.point, draw.start.normal);
	const v3s32 g = sculpt::world(from);
	const int a = draw.start.normal.X ? 0 : draw.start.normal.Y ? 1 : 2;
	const int sign = draw.start.normal.X + draw.start.normal.Y + draw.start.normal.Z;
	v3f p0 = (v3f(g.X, g.Y, g.Z) + v3f(0.5f)) / shape::STEPS - v3f(0.5f);
	p0[a] = f32(g[a] + (sign > 0 ? 1 : 0)) / shape::STEPS - 0.5f;
	p0 *= BS;
	const std::optional<f32> t = sculpt::nearestAlong(p0, v3f::from(draw.start.normal), { s, e });
	if (!t)
		return draw.deep; // looking along it: as it was
	return s32(std::lround(*t / BS * shape::STEPS));
}

std::optional<std::pair<shape::Sixteenth, shape::Sixteenth>> Game::flatFace(const Aim &aim) const
{
	// what's solid there, in the aim's grid: the world's, or the part's
	const piece::Piece *part = nullptr;
	if (aim.piece) {
		auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(aim.piece));
		const piece::Machine::Part *p = obj && obj->getMachine() ? obj->getMachine()->part(aim.part) : nullptr;
		if (!p)
			return std::nullopt;
		part = &p->object.design();
	}
	Map &world = client->getEnv().getClientMap();
	const NodeDefManager *ndef = client->ndef();
	auto solid = [&](v3s32 w) {
		const shape::Sixteenth c = sculpt::local(w);
		bool valid = true;
		const MapNode n = part ? part->node(c.node) : world.getNode(c.node, &valid);
		if (!valid || n.getContent() == CONTENT_AIR || n.getContent() == CONTENT_IGNORE
				|| ndef->get(n).drawtype != NDT_NORMAL)
			return false;
		const shape::Shape *cut = part ? part->shape(c.node) : world.getShape(c.node);
		return !cut || cut->has(c.cell);
	};
	// the face: solid on this side, open past it; spread across its plane
	const v3s32 start = sculpt::world(shape::hit(aim.node, aim.point, aim.normal));
	const v3s32 n(aim.normal.X, aim.normal.Y, aim.normal.Z);
	auto face = [&](v3s32 w) { return solid(w) && !solid(w + n); };
	if (!face(start))
		return std::nullopt;
	const int a = n.X ? 0 : n.Y ? 1 : 2, u = (a + 1) % 3, v = (a + 2) % 3;
	std::set<std::pair<s32, s32>> seen{ { start[u], start[v] } };
	std::vector<v3s32> todo{ start };
	v3s32 lo = start, hi = start;
	while (!todo.empty()) {
		const v3s32 c = todo.back();
		todo.pop_back();
		for (const auto &[du, dv] : { std::pair{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }) {
			v3s32 q = c;
			q[u] += du;
			q[v] += dv;
			// no wider than a stroke's footprint
			if (std::abs(q[u] - start[u]) >= sculpt::MAX_SIZE / 2 || std::abs(q[v] - start[v]) >= sculpt::MAX_SIZE / 2)
				continue;
			if (!seen.insert({ q[u], q[v] }).second || !face(q))
				continue;
			for (const int b : { u, v }) {
				lo[b] = std::min(lo[b], q[b]);
				hi[b] = std::max(hi[b], q[b]);
			}
			todo.push_back(q);
		}
	}
	return std::make_pair(sculpt::local(lo), sculpt::local(hi));
}

std::optional<shape::Sixteenth> Game::dragTo(const Draw &draw) const
{
	const Aim &aim = draw.start;
	core::matrix4 into;
	if (!aim.frame.getInverse(into))
		throw BaseException("an aim's frame has no inverse");
	v3f s = m_pointed_from.start, e = m_pointed_from.end;
	into.transformVect(s);
	into.transformVect(e);
	// the face's plane, in its frame (BS): node p spans p ± BS/2
	const shape::Sixteenth from = shape::hit(aim.node, aim.point, aim.normal);
	const v3s32 g = sculpt::world(from);
	const int a = aim.normal.X ? 0 : aim.normal.Y ? 1 : 2;
	const int sign = aim.normal.X + aim.normal.Y + aim.normal.Z;
	const f32 plane = (f32(g[a] + (sign > 0 ? 1 : 0)) / shape::STEPS - 0.5f) * BS;
	const f32 along = e[a] - s[a];
	if (std::abs(along) < 1e-4f)
		return draw.to; // the ray runs along the face: where it was
	const f32 t = (plane - s[a]) / along;
	if (t < 0 || t > 1)
		return draw.to;
	const v3f p = s + (e - s) * t;
	v3s32 w = g;
	for (int b = 0; b < 3; b++) {
		if (b == a)
			continue;
		const s32 at = s32(std::floor((p[b] / BS + 0.5f) * shape::STEPS));
		// no wider than the largest footprint
		w[b] = std::clamp(at, g[b] - (sculpt::MAX_SIZE - 1), g[b] + (sculpt::MAX_SIZE - 1));
	}
	if (w == g)
		return std::nullopt;
	return sculpt::local(w);
}

f32 Game::pieceRadius(u16 id, v3f middle) const
{
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id));
	if (!obj || !obj->getMachine())
		throw BaseException("pieceRadius: no object " + std::to_string(id));
	const std::optional<aabb3f> box = obj->getMachine()->designed();
	if (!box)
		return 0.5f;
	f32 radius = 0.5f;
	for (int c = 0; c < 8; c++) {
		const v3f corner(c & 1 ? box->MaxEdge.X : box->MinEdge.X, c & 2 ? box->MaxEdge.Y : box->MinEdge.Y,
				c & 4 ? box->MaxEdge.Z : box->MinEdge.Z);
		radius = std::max(radius, (corner - middle).getLength());
	}
	return radius;
}

Game::View Game::view(v3f look)
{
	// the view's right, up and forward (left-handed: up × forward is right)
	const v3f forward = look.normalize();
	v3f right = v3f(0, 1, 0).crossProduct(forward);
	if (right.getLengthSQ() < 1e-6f)
		right = v3f(1, 0, 0); // looking straight up or down
	right.normalize();
	return { right, forward.crossProduct(right), forward };
}

void Game::handleDrive(const PointedThing &pointed, f32 dtime)
{
	constexpr f32 SEND = 0.05f; // seconds between controls sent
	Body &body = client->getBody();
	if (!m_driving) {
		if (!body.sit(pointed.object_id))
			return;
		m_driving = Driving{ pointed.object_id, SEND };
		m_game_ui->showTranslatedStatusText("Driving · W/S: go and back · A/D: steer · Space: brake · F: get up");
	}
	const u16 object = m_driving->object;
	const bool gone = body.driving() != object; // its machine went
	const bool up = gone || wasKeyPressed(KeyType::FOCUS);
	if (up) {
		body.getUp(object);
		if (auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object)))
			obj->setPosed(std::nullopt);
		client->sendDrive(object, std::nullopt);
		m_driving.reset();
		m_game_ui->showTranslatedStatusText("Got up");
		return;
	}
	const auto key = [&](KeyType::T k) { return isKeyDown(k) ? 1.f : 0.f; };
	const physics::Drive d{ key(KeyType::FORWARD) - key(KeyType::BACKWARD), key(KeyType::RIGHT) - key(KeyType::LEFT),
		key(KeyType::JUMP) };
	body.steer(object, d);
	// drawn where it is here, as the player in its seat is
	if (auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object)))
		if (const std::optional<std::pair<v3f, v3f>> here = body.grabbed(object))
			obj->applyPosed(*here);
	m_driving->age += dtime;
	if (m_driving->age < SEND)
		return;
	m_driving->age = 0;
	client->sendDrive(object, d);
}

void Game::handleGrab(const PointedThing &pointed, f32 dtime)
{
	constexpr f32 SEND = 0.05f; // seconds between grips sent
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	const v3f eye = player->getEyePosition(), look = camera->getDirection();
	if (!m_grab) {
		// the point pressed on, in the object's own frame: it's that point
		// that's pulled, wherever it's turned to
		auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(pointed.object_id));
		// on the sixteenth pressed on, not its box's face (sprint 71)
		v3f hit = pointed.intersection_point;
		if (const std::optional<Aim> aim = aimAtPiece(pointed.object_id)) {
			hit = aim->point;
			aim->frame.transformVect(hit);
		}
		core::matrix4 back;
		obj->getFrame().getInverse(back);
		v3f point = hit;
		back.transformVect(point);
		m_grab = Grab{ pointed.object_id, point, eye.getDistanceFrom(hit), SEND };
		client->getBody().grab(m_grab->object, point, workshop::STRENGTH);
		m_game_ui->showTranslatedStatusText("Pulling · scroll: nearer or further · let go to drop it");
	}
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_grab->object));
	if (!obj) { // gone (the server's let it go too)
		client->getBody().release(m_grab->object);
		m_grab.reset();
		return;
	}
	// the hand stops at the first solid block before it (looking down, it's
	// on the ground, not in it)
	v3f hand = eye + look * m_grab->distance;
	{
		const NodeDefManager *ndef = client->ndef();
		Map &map = client->getEnv().getMap();
		for (f32 t = 0; t < m_grab->distance; t += 0.1f * BS) {
			const v3f at = eye + look * t;
			if (ndef->get(map.getNode(floatToInt(at, BS))).walkable) {
				hand = eye + look * std::max(0.f, t - 0.1f * BS);
				break;
			}
		}
	}
	client->getBody().grabTo(m_grab->object, hand);
	client->getBody().stepObjects(*player, dtime);
	// drawn where it is here, a step ahead of the server's word
	if (const std::optional<std::pair<v3f, v3f>> here = client->getBody().grabbed(m_grab->object))
		obj->applyPosed(*here);
	// the grip drawn: a mark where it's held, a line to the hand, from
	// green to red as it stretches (what's heavy lags the hand: sprint 69)
	{
		constexpr f32 MARK = 0.08f * BS, TAUT = 1.f * BS;
		// where it's held now: on the part held, a wheel's as it turns
		v3f at = m_grab->point;
		obj->getFrame().transformVect(at);
		if (const std::optional<v3f> here = client->getBody().gripAt(m_grab->object))
			at = *here;
		const v3f offset = intToFloat(client->getEnv().getCameraOffset(), BS);
		const f32 strain = std::min(1.f, at.getDistanceFrom(hand) / TAUT);
		const video::SColor colour(255, u32(80 + 175 * strain), u32(220 - 160 * strain), 60);
		std::vector<Hud::Guide> &guides = *hud->getGuides();
		guides.clear();
		for (const v3f &d : { v3f(MARK, 0, 0), v3f(0, MARK, 0), v3f(0, 0, MARK) })
			guides.push_back({ at - d - offset, at + d - offset, colour });
		guides.push_back({ at - offset, hand - offset, colour });
	}
	m_grab->age += dtime;
	// what they stand on they can't lift: they'd lift themselves
	const bool on_it = client->getBody().standingOn() == m_grab->object;
	if (on_it)
		m_game_ui->showTranslatedStatusText("You can't lift what you're standing on");
	const bool held = isKeyDown(KeyType::DIG) && !on_it;
	if (held && m_grab->age < SEND)
		return;
	m_grab->age = 0;
	client->sendGrab(m_grab->object, held ? std::optional(std::make_pair(m_grab->point, hand)) : std::nullopt);
	if (held)
		return;
	client->getBody().release(m_grab->object);
	obj->setPosed(std::nullopt);
	hud->getGuides()->clear();
	m_grab.reset();
}

void Game::startCarry(u16 object, v3f hit, bool sticky)
{
	// grabbed where it's pointed at, held as far off as it was; its
	// origin from there kept in the view's own terms, so it turns with it
	constexpr f32 SEND = 0.05f;
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object));
	if (!obj)
		throw BaseException("Game::startCarry: no object " + std::to_string(object));
	const View v = view(camera->getDirection());
	const v3f off = obj->getPosition() - hit;
	m_carry = Carry{ object, player->getEyePosition().getDistanceFrom(hit),
		v3f(off.dotProduct(v.right), off.dotProduct(v.up), off.dotProduct(v.forward)), obj->getRotation(),
		player->getYaw(), SEND, sticky };
	client->sendHold(object, workshop::Grip::Carried);
	// pulled toward the hands here as the server pulls it there
	client->getBody().carry(object, client->checkLocalPrivilege("creative")
			? workshop::STRENGTH_CREATIVE : workshop::STRENGTH);
}

void Game::handleCarry(const PointedThing &pointed, f32 dtime)
{
	constexpr f32 SEND = 0.05f; // seconds between poses sent
	constexpr f32 REACH = 4.f;  // nodes past where it's held a face is looked for
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	const v3f eye = player->getEyePosition(), look = camera->getDirection();
	if (!m_carry)
		startCarry(pointed.object_id, pointed.intersection_point, false);
	client->getBody().stepObjects(*player, dtime);
	auto *carried = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_carry->object));
	if (!carried || !carried->getMachine()) { // it's gone
		m_carry.reset();
		hud->getGuides()->clear();
		return;
	}

	// what's been asked of it since (processKeyInput): it's turned in the
	// hands, snapped or not, and the snap follows (it's what's nearest)
	const int turns = std::exchange(m_carry->turn_asked, 0), tilts = std::exchange(m_carry->tilt_asked, 0);
	if (turns || tilts) {
		m_carry->rotation += v3f(0, player->getYaw() - m_carry->yaw, 0);
		m_carry->yaw = player->getYaw();
		core::matrix4 q;
		q.setRotationAxisRadians(core::PI / 2 * turns, v3f(0, 1, 0));
		core::matrix4 r;
		r.setRotationAxisRadians(core::PI / 2 * tilts, view(look).right);
		const core::matrix4 now = piece::frame(v3f(0), m_carry->rotation);
		m_carry->rotation = -getPitchYawRoll(r * q * now);
		// turned about its middle, not its origin (a corner: it swung off
		// to the side and back): its origin moved so its middle stays put
		if (const std::optional<aabb3f> box = carried->getMachine()->designed()) {
			const View w = view(look);
			const v3f off = w.right * m_carry->offset.X + w.up * m_carry->offset.Y + w.forward * m_carry->offset.Z;
			v3f was = box->getCenter() * BS, will = was;
			now.transformVect(was);
			piece::frame(v3f(0), m_carry->rotation).transformVect(will);
			const v3f moved = off + was - will;
			m_carry->offset = v3f(moved.dotProduct(w.right), moved.dotProduct(w.up), moved.dotProduct(w.forward));
		}
	}
	// where it's carried: as far along the look as it was grabbed, turned
	// as the player's turned since; drawn there now, the server told so often
	const View v = view(look);
	const v3f at = eye + look * m_carry->distance + v.right * m_carry->offset.X + v.up * m_carry->offset.Y
			+ v.forward * m_carry->offset.Z;
	const v3f turned = m_carry->rotation + v3f(0, player->getYaw() - m_carry->yaw, 0);
	// where it's to be: the hands; where it's drawn: where it is (it's
	// heavy, or in the way of something), and now, as this frame's camera sees it
	// fitted (last frame), it's pulled to where it'd go, not into what
	// it's fitted to (sprint 77: the workpiece shoved it about)
	const std::pair<v3f, v3f> to = m_carry->fitted.value_or(std::make_pair(at, turned));
	client->getBody().pull(m_carry->object, to.first, to.second);
	carried->applyPosed(client->getBody().carried(m_carry->object).value_or(to));
	m_carry->age += dtime;
	if (m_carry->age >= SEND) {
		client->sendPose(m_carry->object, to.first, to.second);
		m_carry->age = 0;
	}

	// a socket near where it's carried (along the look, within a node of
	// it) first; else a face of something else the look meets
	m_carry->snap.reset();
	std::vector<Hud::Guide> *guides = hud->getGuides();
	guides->clear();
	std::vector<DistanceSortedActiveObject> near;
	const f32 far_as = m_carry->distance + REACH * BS;
	client->getEnv().getActiveObjects(eye, far_as, near);
	f32 nearest = 0;
	for (const DistanceSortedActiveObject &d : near) {
		auto *other = dynamic_cast<GenericCAO *>(d.obj);
		if (!other || other->getId() == m_carry->object || !other->getMachine())
			continue;
		showSockets(*other, false);
		for (const piece::Machine::Socket &k : other->getMachine()->sockets()) {
			v3f w = (k.point / shape::STEPS - v3f(0.5f)) * BS;
			other->getFrame().transformVect(w);
			const f32 along = (w - eye).dotProduct(look);
			const f32 off = (eye + look * along).getDistanceFrom(w);
			if (along < 0 || along > far_as || off > BS)
				continue;
			if (m_carry->snap && m_carry->snap->socket && off >= nearest)
				continue;
			nearest = off;
			m_carry->snap = Carry::Snap{ other->getId(), {}, {}, k.id };
		}
	}
	for (const DistanceSortedActiveObject &d : near) {
		if (m_carry->snap && m_carry->snap->socket)
			break; // a socket's nearer than any face
		auto *other = dynamic_cast<GenericCAO *>(d.obj);
		if (!other || other->getId() == m_carry->object || !other->getMachine())
			continue;
		const piece::Machine &theirs = *other->getMachine();
		for (const piece::Machine::Part &part : theirs.parts()) {
			// its whole box and a little round it, not its voxels (sprint
			// 77): a frame's top is met over its hole, a side anywhere on it
			constexpr f32 MARGIN = 0.25f; // nodes
			const std::optional<aabb3f> filled = theirs.filled(part.object.id());
			if (!filled)
				continue;
			const core::matrix4 frame = other->getFrame() * part.pose.matrix();
			core::matrix4 into;
			if (!frame.getInverse(into))
				throw BaseException("a part's frame has no inverse");
			v3f s = eye, e = eye + look * far_as;
			into.transformVect(s);
			into.transformVect(e);
			const v3f lo = (filled->MinEdge - v3f(MARGIN)) * BS, hi = (filled->MaxEdge + v3f(MARGIN)) * BS;
			const v3f dir = e - s;
			f32 enter = 0, leave = 1;
			int axis = -1; // the side it enters by (none: it starts inside)
			bool miss = false;
			for (int k = 0; k < 3 && !miss; k++) {
				if (std::fabs(dir[k]) < 1e-6f) {
					miss = s[k] < lo[k] || s[k] > hi[k];
					continue;
				}
				f32 t0 = (lo[k] - s[k]) / dir[k], t1 = (hi[k] - s[k]) / dir[k];
				if (t0 > t1)
					std::swap(t0, t1);
				if (t0 > enter) {
					enter = t0;
					axis = k;
				}
				leave = std::min(leave, t1);
			}
			if (miss || axis < 0 || enter > leave)
				continue;
			const f32 far = (dir * enter).getLengthSQ();
			if (m_carry->snap && far >= nearest)
				continue;
			// on its face, in its sixteenths, then the machine's
			v3s16 normal(0, 0, 0);
			normal[axis] = dir[axis] > 0 ? -1 : 1;
			v3f hit = (s + dir * enter) / BS;
			for (int k = 0; k < 3; k++)
				hit[k] = std::clamp(hit[k], filled->MinEdge[k], filled->MaxEdge[k]);
			hit[axis] = normal[axis] > 0 ? filled->MaxEdge[axis] : filled->MinEdge[axis];
			const v3f aim = part.pose.apply((hit + v3f(0.5f)) * f32(shape::STEPS));
			const v3s16 out = theirs.normal({ part.object.id(), {}, normal });
			// a sixteenth's middle across, as faces are named
			v3f at = aim;
			for (int k = 0; k < 3; k++)
				if (!out[k])
					at[k] = std::floor(at[k]) + 0.5f;
			nearest = far;
			m_carry->snap = Carry::Snap{ other->getId(), theirs.faceAt(part.object.id(), at, out), {}, std::nullopt, aim };
		}
	}
	if (m_carry->snap) {
		auto *target = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_carry->snap->object));
		const piece::Machine &into = *target->getMachine();
		const piece::Machine &mine_m = *carried->getMachine();
		const v3s16 n = m_carry->snap->socket
				? std::find_if(into.sockets().begin(), into.sockets().end(),
						[&](const piece::Machine::Socket &s) { return s.id == *m_carry->snap->socket; })->axis
				: into.normal(m_carry->snap->on);
		// it goes on the way it's held (sprint 77): of the square turns
		// that set a face of it flat on theirs, the nearest how it's turned
		// in the hands, seen in their frame. A copy meets by the face it
		// hung by; only its turn about the joint is chosen so
		core::matrix4 theirs_turn = target->getFrame();
		theirs_turn.setTranslation(v3f(0));
		core::matrix4 into_theirs;
		theirs_turn.getTransposed(into_theirs);
		const core::matrix4 held = into_theirs * piece::frame(v3f(0), turned);
		const std::optional<piece::Machine::Face> plugged = mine_m.plugFace();
		const piece::Fit fit = piece::nearestFit(n, plugged ? std::optional(mine_m.normal(*plugged)) : std::nullopt, held);
		const v3s16 side = fit.side;
		m_carry->spin = fit.spin;
		const std::optional<piece::Machine::Face> mine = plugged ? plugged : mine_m.faceToward(side);
		if (!mine) {
			m_carry->snap.reset();
		} else {
			m_carry->snap->mine = *mine;
			// both made one, as the server will: where what's carried would
			// sit is where it's drawn, its outline round it
			const size_t had = into.parts().size();
			auto joinedAt = [&](const piece::Machine::Face &on) {
				piece::Machine joined(client), theirs(client);
				joined.read(into.write());
				theirs.read(mine_m.write());
				if (m_carry->snap->socket)
					joined.plug(*m_carry->snap->socket, std::move(theirs), *mine, m_carry->spin);
				else
					joined.join(on, std::move(theirs), *mine, piece::Joint::Fixed, m_carry->spin);
				return joined;
			};
			// a box of parts, in the machine's sixteenths
			auto boxOf = [](const piece::Machine &m, size_t from, size_t to) {
				std::optional<aabb3f> out;
				for (size_t i = from; i < to; i++) {
					const piece::Machine::Part &p = m.parts()[i];
					const std::optional<aabb3f> box = m.filled(p.object.id());
					if (!box)
						continue;
					for (const v3f &c : { box->MinEdge, box->MaxEdge }) {
						const v3f q = p.pose.apply((c + v3f(0.5f)) * f32(shape::STEPS));
						if (out)
							out->addInternalPoint(q);
						else
							out = aabb3f(q, q);
					}
				}
				return out;
			};
			piece::Machine joined = joinedAt(m_carry->snap->on);
			if (!m_carry->snap->socket) {
				// lined up with their part (sprint 77): along the face it
				// stays on it, flush with an edge or centred when it's near
				// that, else to the sixteenth, as it's aimed (sprint 78: by
				// whole nodes lost a seat's place)
				constexpr f32 STICK = 4; // sixteenths
				const piece::Machine::Face &on = m_carry->snap->on;
				size_t at = 0;
				while (into.parts()[at].object.id() != on.part)
					at++;
				const std::optional<aabb3f> them = boxOf(into, at, at + 1), us = boxOf(joined, had, joined.parts().size());
				if (them && us) {
					v3f shift(0, 0, 0);
					for (int k = 0; k < 3; k++) {
						if (n[k])
							continue;
						const f32 lo = them->MinEdge[k], hi = them->MaxEdge[k];
						const f32 size = us->MaxEdge[k] - us->MinEdge[k];
						// its low edge's range on their face: on it, or over it if it's bigger
						const f32 least = std::min(lo, hi - size), most = std::max(lo, hi - size);
						const f32 want = m_carry->snap->aim[k] - size / 2;
						f32 best = std::clamp(std::round(want), least, most);
						f32 best_off = STICK;
						for (const f32 c : { lo, hi - size, std::round((lo + hi - size) / 2) })
							if (c >= least && c <= most && std::fabs(c - want) < best_off) {
								best = c;
								best_off = std::fabs(c - want);
							}
						shift[k] = best - us->MinEdge[k];
					}
					if (shift != v3f(0, 0, 0)) {
						m_carry->snap->on = into.faceAt(on.part, into.point(on) + shift, n);
						joined = joinedAt(m_carry->snap->on);
					}
				}
			}
			const core::matrix4 frame = target->getFrame();
			// its frame there: theirs, then its first part's in the joined,
			// back from that part's in its own
			core::matrix4 own_inv;
			if (!mine_m.parts()[0].pose.matrix().getInverse(own_inv))
				throw BaseException("a part's pose has no inverse");
			const core::matrix4 there = frame * joined.parts()[had].pose.matrix() * own_inv;
			m_carry->fitted = std::make_pair(there.getTranslation(), -getPitchYawRoll(there));
			const v3f offset = intToFloat(client->getEnv().getCameraOffset(), BS);
			const video::SColor colour = m_carry->snap->socket ? video::SColor(255, 255, 255, 120)
					: video::SColor(255, 150, 230, 255);
			for (size_t i = had; i < joined.parts().size(); i++) {
				const piece::Machine::Part &p = joined.parts()[i];
				// what's filled, not its nodes: a beam's outline is the beam
				const std::optional<aabb3f> box = joined.filled(p.object.id());
				if (!box)
					continue;
				v3f c[8];
				for (int k = 0; k < 8; k++) {
					v3f q = (p.pose.apply((v3f(k & 1 ? box->MaxEdge.X : box->MinEdge.X, k & 2 ? box->MaxEdge.Y : box->MinEdge.Y,
							k & 4 ? box->MaxEdge.Z : box->MinEdge.Z) + v3f(0.5f)) * f32(shape::STEPS)) / shape::STEPS - v3f(0.5f)) * BS;
					frame.transformVect(q);
					c[k] = q - offset;
				}
				for (int k = 0; k < 8; k++)
					for (const int bit : { 1, 2, 4 })
						if (!(k & bit))
							guides->push_back({ c[k], c[k | bit], colour });
			}
			// a copy goes on as it hung (sprint 76: a corner on its spring)
			const std::optional<piece::Machine::Plug> &plug = carried->getMachine()->plug();
			const std::string how = m_carry->snap->socket ? "put it on"
					: plug && plug->joint != piece::Joint::Fixed
					? std::string("put it on ") + piece::JOINTS[size_t(plug->joint)].label : "weld it on";
			m_game_ui->showStatusText(utf8_to_wide((m_carry->sticky ? "Click to " : "Let go to ") + how
					+ " · E: turn it · Q: tip it · Shift: back"));
		}
	}
	if (!m_carry->snap)
		m_carry->fitted.reset();
	// fitted, it's drawn where it'd go (the hands still pull it, unseen)
	if (m_carry->fitted)
		carried->applyPosed(*m_carry->fitted);

	if (m_carry->sticky ? !wasKeyPressed(KeyType::DIG) : isKeyDown(KeyType::DIG))
		return;
	// let go: on what it's pointed at, joined; else dropped (where it's
	// been drawn, told the server first)
	carried->setPosed(std::nullopt);
	client->getBody().letGo(m_carry->object); // the server's to move again
	client->sendPose(m_carry->object, at, turned);
	if (m_carry->snap && m_carry->snap->socket) {
		client->sendPlug(m_carry->snap->object, *m_carry->snap->socket, m_carry->object, m_carry->snap->mine,
				m_carry->spin);
		m_game_ui->showTranslatedStatusText("On · Shift-click another socket for another of it");
	} else if (m_carry->snap) {
		client->sendJoin(m_carry->snap->object, m_carry->snap->on, m_carry->object, m_carry->snap->mine,
				piece::Joint::Fixed, m_carry->spin);
	} else {
		client->sendHold(m_carry->object, workshop::Grip::Free);
	}
	m_carry.reset();
	guides->clear();
}

std::optional<std::pair<u16, u64>> Game::socketAt() const
{
	// the socket nearest the look (within half a node of it), of what's in reach
	constexpr f32 REACH = 6, NEAR = 0.5f; // nodes
	const v3f eye = m_pointed_from.start, look = (m_pointed_from.end - m_pointed_from.start).normalize();
	std::vector<DistanceSortedActiveObject> near;
	client->getEnv().getActiveObjects(eye, REACH * BS, near);
	std::optional<std::pair<u16, u64>> best;
	f32 nearest = NEAR * BS;
	for (const DistanceSortedActiveObject &d : near) {
		auto *obj = dynamic_cast<GenericCAO *>(d.obj);
		if (!obj || !obj->getMachine())
			continue;
		for (const piece::Machine::Socket &k : obj->getMachine()->sockets()) {
			v3f w = (k.point / shape::STEPS - v3f(0.5f)) * BS;
			obj->getFrame().transformVect(w);
			const f32 along = (w - eye).dotProduct(look);
			const f32 off = (eye + look * along).getDistanceFrom(w);
			if (along > 0 && off < nearest) {
				nearest = off;
				best = std::make_pair(obj->getId(), k.id);
			}
		}
	}
	return best;
}

void Game::labelPart(GenericCAO &obj, u64 id)
{
	const piece::Machine &m = *obj.getMachine();
	const piece::Machine::Part *part = m.part(id);
	if (!part)
		return;
	const NodeDefManager *ndef = client->ndef();
	IItemDefManager *idef = client->idef();
	auto material = [&](const piece::Machine::Part &of) {
		std::string name;
		of.object.design().each([&](v3s16, MapNode n, const shape::Shape *) {
			if (name.empty())
				name = idef->get(ndef->get(n).name).description;
		});
		return name.empty() ? std::string("Empty") : name;
	};
	// what the whole of it weighs (sprint 66): what a person can push is
	// a cart, not a house, and this says which it is
	float kg = 0;
	std::vector<aabb3f> boxes;
	for (const piece::Machine::Part &p : m.parts())
		p.object.built().each([&](v3s16, MapNode n, const shape::Shape *s) {
			const workshop::Material *mat = client->getMaterials().get(n.getContent());
			if (!mat)
				return;
			boxes.clear();
			if (s)
				s->boxes(boxes);
			else
				boxes.push_back(aabb3f(v3f(-BS / 2), v3f(BS / 2)));
			for (const aabb3f &b : boxes)
				kg += b.getVolume() / (BS * BS * BS) * mat->density;
		});
	char weighs[32];
	if (kg >= 1000)
		snprintf(weighs, sizeof weighs, "%.1f t", kg / 1000);
	else
		snprintf(weighs, sizeof weighs, "%.0f kg", kg);
	std::string text = std::string(weighs) + " · " + material(*part);
	for (const piece::Machine::Link &l : m.links())
		if (l.a == id || l.b == id)
			if (const piece::Machine::Part *other = m.part(l.a == id ? l.b : l.a))
				text += std::string(" · ") + piece::JOINTS[size_t(l.joint)].label + " to " + material(*other);
	// over its middle
	const std::optional<aabb3f> box = piece::boxOf(part->object.design().write());
	if (!box)
		return;
	v3f at = (part->pose.apply((box->getCenter() + v3f(0.5f, box->getExtent().Y / 2 + 0.9f, 0.5f)) * f32(shape::STEPS))
			/ shape::STEPS - v3f(0.5f)) * BS;
	obj.getFrame().transformVect(at);
	hud->getLabels()->push_back({ at, utf8_to_wide(text), video::SColor(255, 235, 240, 245) });
}

void Game::showSockets(GenericCAO &obj, bool lit) const
{
	std::vector<Hud::Guide> &guides = *hud->getGuides();
	// a socket: a stub out of its face along its axis, a ring round its foot
	const core::matrix4 frame = obj.getFrame();
	const v3f offset = intToFloat(client->getEnv().getCameraOffset(), BS);
	auto at = [&](v3f sixteenths) {
		v3f p = (sixteenths / shape::STEPS - v3f(0.5f)) * BS;
		frame.transformVect(p);
		return p - offset;
	};
	const video::SColor colour = lit ? video::SColor(255, 255, 255, 120) : video::SColor(255, 110, 230, 130);
	// by its joint (sprint 64): fixed a square plate, rotary a whole ring
	// round an axle, a hinge half a ring
	for (const piece::Machine::Socket &k : obj.getMachine()->sockets()) {
		const v3f axis = v3f::from(k.axis);
		const v3f u = std::abs(axis.Y) > 0.9f ? v3f(1, 0, 0) : v3f(0, 1, 0).crossProduct(axis).normalize();
		const v3f w = axis.crossProduct(u);
		constexpr f32 R = 5; // sixteenths
		const piece::JointKind &kind = piece::JOINTS[size_t(k.joint)];
		if (!kind.turns) {
			const v3f c[4] = { k.point + (u + w) * R, k.point + (u - w) * R, k.point - (u + w) * R, k.point - (u - w) * R };
			for (int i = 0; i < 4; i++)
				guides.push_back({ at(c[i]), at(c[(i + 1) % 4]), colour });
			continue;
		}
		guides.push_back({ at(k.point), at(k.point + axis * 10), colour });
		constexpr int SIDES = 16;
		const int sides = kind.limit > 0 ? SIDES / 2 : SIDES;
		for (int i = 0; i < sides; i++) {
			const f32 a = core::PI * 2 * i / SIDES, b = core::PI * 2 * (i + 1) / SIDES;
			guides.push_back({ at(k.point + (u * std::cos(a) + w * std::sin(a)) * R),
				at(k.point + (u * std::cos(b) + w * std::sin(b)) * R), colour });
		}
	}
}

void Game::benchStep(f32 dtime)
{
	constexpr f32 SEND = 0.05f; // seconds between poses sent
	const std::optional<v3f> middle = pieceMiddle(m_hold->object());
	if (!middle) { // the object is gone
		m_hold.reset();
		closeMenu();
		hud->getGuides()->clear();
		return;
	}
	hud->getLabels()->clear(); // said again below, and by the menu
	// walked away from, it's put down (it floats while you're about it)
	constexpr f32 AWAY = 10; // nodes
	const v3f player = client->getEnv().getLocalPlayer()->getPosition();
	if (player.getDistanceFrom(m_hold->pose().position) > AWAY * BS) {
		closeFocus();
		return;
	}
	// it's held by its middle, however it's grown; its ring faces the player
	const f32 radius = pieceRadius(m_hold->object(), *middle);
	if (*middle != m_hold->middle() || radius != m_hold->radius())
		m_hold->reshape(*middle, radius);
	m_hold->watch(client->getEnv().getLocalPlayer()->getEyePosition() / BS, camera->getDirection());

	// pressed on the disc or the ring, it turns as they're looked round
	// (or pointed round) till it's let go, in steps (Shift: quarters)
	const core::line3d<f32> ray = m_pointed_from;
	const f32 step = (isKeyDown(KeyType::AUX1) ? 90.f : 15.f) * core::DEGTORAD;
	// the other button that stopped a stroke is spent till it's let go
	if (m_place_spent && !isKeyDown(KeyType::PLACE))
		m_place_spent = false;
	const bool down = !m_menu && m_hand.out() && m_hand.creating()
			&& (isKeyDown(KeyType::DIG) || (isKeyDown(KeyType::PLACE) && !m_place_spent));
	if (!down) {
		// the other button clicked on a part (not turned): its menu
		if (m_drag.held && m_drag.place && !m_drag.turned)
			if (const std::optional<Aim> aim = aimAt(PointedThing()))
				openMenu(*aim);
		m_hold->end();
		m_drag = {};
	} else if (!m_draw) {
		if (!m_drag.held) {
			m_drag = { true, v2s32(), m_hold->grip(ray), false, false, isKeyDown(KeyType::PLACE) };
			if (m_drag.grip)
				m_hold->begin(m_drag.grip->spin);
		}
		if (m_drag.grip)
			if (const std::optional<f32> at = m_hold->angle(m_drag.grip->spin, ray)) {
				m_hold->spin(*at - m_drag.grip->angle, step);
				m_drag.turned = true;
			}
	}
	showHandle(client->getEnv().getCameraOffset());

	// where it is: drawn there now; to the server, as it changes, so often
	m_pose_age += dtime;
	const Hold::Pose pose = m_hold->pose();
	if (auto *held = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_hold->object())))
		held->setPosed(std::make_pair(pose.position, pose.rotation));
	const bool moved = !m_pose_sent || m_pose_sent->position != pose.position || m_pose_sent->rotation != pose.rotation;
	if (moved && m_pose_age >= SEND) {
		client->sendPose(m_hold->object(), pose.position, pose.rotation);
		m_pose_sent = pose;
		m_pose_age = 0;
	}
}

void Game::showHandle(v3s16 camera_offset)
{
	// the disc under it with a cross through it, and the ring round it
	std::vector<Hud::Guide> *guides = hud->getGuides();
	guides->clear();
	if (!m_hold)
		return;
	const v3f offset = intToFloat(camera_offset, BS);
	const bool on = m_drag.grip.has_value();
	auto circle = [&](const Hold::Ring &r, video::SColor colour) {
		const v3f u = std::abs(r.axis.Y) > 0.9f ? v3f(1, 0, 0) : v3f(0, 1, 0).crossProduct(r.axis).normalize();
		const v3f w = r.axis.crossProduct(u);
		constexpr int SIDES = 48;
		for (int i = 0; i < SIDES; i++) {
			const f32 a = core::PI * 2 * i / SIDES, b = core::PI * 2 * (i + 1) / SIDES;
			guides->push_back({ (r.centre + (u * std::cos(a) + w * std::sin(a)) * r.radius) * BS - offset,
				(r.centre + (u * std::cos(b) + w * std::sin(b)) * r.radius) * BS - offset, colour });
		}
	};
	const video::SColor idle(255, 120, 200, 255), lit(255, 255, 210, 80);
	const Hold::Ring disc = m_hold->disc();
	circle(disc, on && m_drag.grip->spin == Hold::Spin::Yaw ? lit : idle);
	for (f32 d = -disc.radius; d <= disc.radius + 0.01f; d += disc.radius / 2) { // its grid
		const f32 half = std::sqrt(std::max(0.f, disc.radius * disc.radius - d * d));
		const video::SColor faint(255, 70, 110, 140);
		guides->push_back({ (disc.centre + v3f(d, 0, -half)) * BS - offset, (disc.centre + v3f(d, 0, half)) * BS - offset, faint });
		guides->push_back({ (disc.centre + v3f(-half, 0, d)) * BS - offset, (disc.centre + v3f(half, 0, d)) * BS - offset, faint });
	}
	circle(m_hold->ring(), on && m_drag.grip->spin == Hold::Spin::Roll ? lit : idle);

	// its joints: a hinge its axis through the anchor, a weld a small cross
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_hold->object()));
	if (!obj || !obj->getMachine())
		return;
	const core::matrix4 frame = obj->getFrame();
	// its front (sprint 70): an arrow out across the disc
	v3f front = v3f::from(obj->getMachine()->front());
	front = frame.rotateAndScaleVect(front);
	front.Y = 0;
	if (front.getLengthSQ() > 1e-4f) {
		front.normalize();
		const v3f side = v3f(0, 1, 0).crossProduct(front);
		const video::SColor arrow(255, 0, 148, 210);
		const v3f tip = disc.centre + front * disc.radius, tail = disc.centre + front * (disc.radius * 0.4f);
		guides->push_back({ tail * BS - offset, tip * BS - offset, arrow });
		guides->push_back({ tip * BS - offset, (tip - front * 0.35f + side * 0.25f) * BS - offset, arrow });
		guides->push_back({ tip * BS - offset, (tip - front * 0.35f - side * 0.25f) * BS - offset, arrow });
	}
	auto at = [&](v3f sixteenths) { // the machine's, to where it's drawn
		v3f p = (sixteenths / shape::STEPS - v3f(0.5f)) * BS;
		frame.transformVect(p);
		return p - offset;
	};
	// how it's drawn: selecting, see-through but for the part under the
	// cursor; the part selected tinted
	// (its menu open: see-through, its joints shown, sprint 71)
	const bool selecting = m_menu && m_menu->object == m_hold->object();
	// with the tools away, the part looked at's lit too (sprint 64)
	const std::optional<Aim> under = selecting ? aimAt(PointedThing())
			: !m_hand.out() ? aimAtPiece(m_hold->object()) : std::nullopt;
	obj->setLook({ m_part, under ? std::optional<u64>(under->part) : std::nullopt, selecting });
	// the part selected (else looked at) said by it: what it is, how it's joined (sprint 65)
	if (const std::optional<u64> said = m_part ? m_part : under ? std::optional<u64>(under->part) : std::nullopt)
		labelPart(*obj, *said);
	showSockets(*obj, false);
	hoverJoints(*obj);
	// its joints, selecting
	if (!selecting)
		return;
	const video::SColor hinge(255, 255, 140, 60), weld(255, 230, 230, 230);
	for (const piece::Machine::Link &l : obj->getMachine()->links()) {
		const v3f axis = v3f::from(l.axis);
		// two ways across the axis
		const v3f u = std::abs(axis.Y) > 0.9f ? v3f(1, 0, 0) : v3f(0, 1, 0).crossProduct(axis).normalize();
		const v3f w = axis.crossProduct(u);
		if (piece::JOINTS[size_t(l.joint)].turns) {
			// an axle through it, and a ring round it to turn by
			guides->push_back({ at(l.anchor - axis * 12), at(l.anchor + axis * 12), hinge });
			constexpr int SIDES = 16;
			constexpr f32 R = 6; // sixteenths
			for (int i = 0; i < SIDES; i++) {
				const f32 a = core::PI * 2 * i / SIDES, b = core::PI * 2 * (i + 1) / SIDES;
				guides->push_back({ at(l.anchor + (u * std::cos(a) + w * std::sin(a)) * R),
					at(l.anchor + (u * std::cos(b) + w * std::sin(b)) * R), hinge });
			}
			continue;
		}
		// a plate, flat on the face
		constexpr f32 H = 4;
		const v3f c[4] = { l.anchor + (u + w) * H, l.anchor + (u - w) * H, l.anchor - (u + w) * H, l.anchor - (u - w) * H };
		for (int i = 0; i < 4; i++)
			guides->push_back({ at(c[i]), at(c[(i + 1) % 4]), weld });
		guides->push_back({ at(c[0]), at(c[2]), weld });
	}
}

piece::Machine::Face Game::faceOf(const Aim &aim) const
{
	return { aim.part, shape::hit(aim.node, aim.point, aim.normal), aim.normal };
}

int Game::mirrorAxis(u16 object, int now) const
{
	if (!m_mirror_plane || m_mirror_plane->object != object)
		m_mirror_plane = MirrorPlane{ object, now };
	return m_mirror_plane->axis;
}

int Game::sideAxis(u16 object) const
{
	// left to right of its front (sprint 70): across x if it faces along z
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object));
	if (!obj || !obj->getMachine())
		throw BaseException("Game::sideAxis: not a machine");
	return obj->getMachine()->front().X != 0 ? 2 : 0;
}

void Game::stepMirror()
{
	// a workpiece's (sprint 70): off → left to right of its front → front
	// to back → off, whichever way it's looked at; the world's: across
	// the screen's right, then kept, then each other upright plane
	const u16 held = m_hold ? m_hold->object() : 0;
	if (!m_hand.mirrored()) {
		m_hand.toggleMirror();
		m_mirror_plane.reset();
		if (held)
			m_mirror_plane = MirrorPlane{ held, sideAxis(held) };
	} else if (!m_mirror_plane) {
		m_hand.toggleMirror();
	} else if (const u16 object = m_mirror_plane->object) {
		if (m_mirror_plane->axis == sideAxis(object)) {
			m_mirror_plane->axis = 2 - m_mirror_plane->axis; // x ↔ z: front to back
		} else {
			m_hand.toggleMirror();
			m_mirror_plane.reset();
		}
	} else {
		const int next = m_mirror_plane->axis == 0 ? 2 : 0;
		if (next == 0) {
			m_hand.toggleMirror();
			m_mirror_plane.reset();
		} else {
			m_mirror_plane->axis = next;
		}
	}
	std::string across;
	if (m_hand.mirrored() && m_mirror_plane) {
		static const char *const AXES[] = { "x", "y", "z" };
		across = m_mirror_plane->object ? (m_mirror_plane->axis == sideAxis(m_mirror_plane->object)
				? " left to right" : " front to back") : std::string(" across ") + AXES[m_mirror_plane->axis];
	}
	m_game_ui->showStatusText(utf8_to_wide(m_hand.describe() + across));
}

std::optional<sculpt::Stroke::Mirror> Game::machineMirror() const
{
	if (!m_hand.mirrored() || !m_hold)
		return std::nullopt;
	// across the machine's middle, in its sixteenths from its corner (the
	// plane kept since mirroring began, if it's this machine's)
	const int axis = m_mirror_plane && m_mirror_plane->object == m_hold->object() ? m_mirror_plane->axis
			: sideAxis(m_hold->object());
	const f32 plane = (m_hold->middle()[axis] + 0.5f) * shape::STEPS;
	return sculpt::Stroke::Mirror{ axis, s32(std::lround(plane * 2)) };
}

// The menus in the world (sprints 63, 65, 71): rows of buttons drawn as
// the page's are, standing up at the face right-clicked, facing the player
// as it was opened. Looked at and clicked.
static constexpr f32 MENU_SIZE = 0.36f * BS, MENU_GAP = 0.1f * BS, MENU_OUT = 0.5f * BS;

void Game::openMenu(const Aim &aim)
{
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(aim.piece));
	if (!obj || !obj->getMachine())
		return;
	const piece::Machine &machine = *obj->getMachine();
	Menu m;
	m.object = aim.piece;
	m.part = aim.part;
	m.face = faceOf(aim);
	m.at = aim.point;
	aim.frame.transformVect(m.at);
	core::matrix4 turn = aim.frame;
	turn.setTranslation(v3f(0));
	m.normal = v3f::from(aim.normal);
	turn.transformVect(m.normal);
	m.normal.normalize();
	const bool benched = m_hold && m_hold->object() == aim.piece;
	using Do = Menu::Do;
	if (benched) {
		// joints, on the workpiece: the one between two parts by where it's
		// clicked, else the socket on the face (its kind lit)
		const v3f at = partPose(aim.piece, aim.part).apply((aim.point / BS + v3f(0.5f)) * shape::STEPS);
		constexpr f32 NEAR = 8; // sixteenths
		f32 best = NEAR * NEAR;
		for (const piece::Machine::Link &l : machine.links())
			if ((l.a == aim.part || l.b == aim.part) && (l.anchor - at).getLengthSQ() <= best) {
				m.link = l;
				best = (l.anchor - at).getLengthSQ();
			}
		std::optional<piece::Joint> socket;
		best = NEAR * NEAR;
		for (const piece::Machine::Socket &k : machine.sockets())
			if (k.part == aim.part && (k.point - at).getLengthSQ() <= best) {
				m.socket = k.id;
				socket = k.joint;
				best = (k.point - at).getLengthSQ();
			}
		for (const auto &[j, row] : JOINT_ROWS)
			m.buttons.push_back({ piece::JOINTS[size_t(j)].label, m.link ? Do::Relink : Do::Socket, j, row,
				m.link ? m.link->joint == j : socket == j });
	}
	const int row = benched ? 2 : 0;
	if (client->getBody().hasSeat(aim.piece))
		m.buttons.push_back({ "sit", Do::Sit, piece::Joint::Fixed, row, false });
	m.buttons.push_back({ benched ? "put down" : "work on", Do::Focus, piece::Joint::Fixed, row, false });
	if (!benched) {
		m.buttons.push_back({ "carry (hold)", Do::Carry, piece::Joint::Fixed, row, false });
		m.buttons.push_back({ "set upright", Do::Upright, piece::Joint::Fixed, row, false });
	}
	if (benched) {
		m.buttons.push_back({ "edit", Do::Edit, piece::Joint::Fixed, 3, m_part == aim.part });
		if (machine.parts().size() > 1)
			m.buttons.push_back({ "detach", Do::Detach, piece::Joint::Fixed, 3, false });
		if (std::any_of(machine.sockets().begin(), machine.sockets().end(),
				[&](const piece::Machine::Socket &k) { return k.part == aim.part; }))
			m.buttons.push_back({ "no sockets", Do::Bare, piece::Joint::Fixed, 3, false });
		// its only part's delete would be the whole object's: not here
		if (machine.parts().size() > 1)
			m.buttons.push_back({ "delete", Do::Delete, piece::Joint::Fixed, 3, false });
	}
	faceMenu(m);
	m_menu = std::move(m);
	m_game_ui->showTranslatedStatusText(benched ? "Look at a button and click · hover a joint to see it · right-click or Esc: close"
			: "Look at a button and click · right-click or Esc: close");
}

// The joints a menu offers, by row: how parts meet, then how a wheel's
// axle works (sprint 73)
const std::pair<piece::Joint, int> Game::JOINT_ROWS[size_t(piece::Joint::Count)] = {
	{ piece::Joint::Fixed, 0 }, { piece::Joint::Rotary, 0 }, { piece::Joint::Hinge, 0 }, { piece::Joint::Sprung, 0 },
	{ piece::Joint::Steered, 1 }, { piece::Joint::Driven, 1 }, { piece::Joint::SteerDriven, 1 },
};

void Game::springRows(Menu &m, int row)
{
	if (!m.link || m.link->joint != piece::Joint::Sprung)
		return;
	for (u8 i = 0; i < piece::SPRING_STEPS; i++) {
		m.buttons.push_back({ piece::TRAVELS[i].label, Menu::Do::Travel, piece::Joint::Sprung, row,
			m.link->spring.travel == i, i });
		m.buttons.push_back({ piece::FIRMNESSES[i].label, Menu::Do::Firmness, piece::Joint::Sprung, row + 1,
			m.link->spring.firmness == i, i });
	}
}

void Game::gridStep()
{
	// building (sprint 73): the floor's block edges in a bubble round the
	// player, fading out, to measure a drawing by (two blocks wide)
	std::vector<Hud::Guide> &grid = *hud->getGrid();
	grid.clear();
	if (!m_hand.out())
		return;
	constexpr int RADIUS = 5; // nodes
	constexpr int DOWN = 4;   // nodes below the feet the floor's looked for
	const v3f foot = client->getEnv().getLocalPlayer()->getPosition() / BS;
	const v3s16 at = floatToInt(foot * BS, BS);
	const v3f offset = intToFloat(client->getEnv().getCameraOffset(), BS);
	const NodeDefManager *ndef = client->ndef();
	Map &map = client->getEnv().getMap();
	for (s16 x = -RADIUS; x <= RADIUS; x++)
		for (s16 z = -RADIUS; z <= RADIUS; z++) {
			const f32 fade = 1 - std::sqrt(f32(x * x + z * z)) / RADIUS;
			if (fade <= 0)
				continue;
			// the top of the first solid block down from the feet, open above
			for (s16 y = at.Y; y >= at.Y - DOWN; y--) {
				const v3s16 n(at.X + x, y, at.Z + z);
				if (!ndef->get(map.getNode(n)).walkable)
					continue;
				if (ndef->get(map.getNode(n + v3s16(0, 1, 0))).walkable)
					break;
				const f32 top = (y + 0.5f + 0.01f) * BS; // a hair over it
				const video::SColor colour(u32(170 * fade), 120, 200, 240);
				auto p = [&](f32 dx, f32 dz) { return v3f((n.X + dx) * BS, top, (n.Z + dz) * BS) - offset; };
				grid.push_back({ p(-0.5f, -0.5f), p(0.5f, -0.5f), colour });
				grid.push_back({ p(0.5f, -0.5f), p(0.5f, 0.5f), colour });
				grid.push_back({ p(0.5f, 0.5f), p(-0.5f, 0.5f), colour });
				grid.push_back({ p(-0.5f, 0.5f), p(-0.5f, -0.5f), colour });
				break;
			}
		}
}

bool Game::fitStep(u16 lit)
{
	// a fitting held with the tools away (sprint 73): the block it'd take
	// on the object looked at outlined, right-click fits it there (as a
	// block's placed in the world)
	const bool was = m_fit_shown;
	m_fit_shown = false;
	const auto done = [&] {
		if (was && !m_hold) // the bench redraws its own each frame
			hud->getGuides()->clear();
		return false;
	};
	if (m_hand.out())
		return done();
	ItemStack selected, hand;
	const ItemStack &held = client->getEnv().getLocalPlayer()->getWieldedItem(&selected, &hand);
	content_t c;
	if (held.empty() || !client->ndef()->getId(held.name, c))
		return done();
	const workshop::Material *mat = client->getMaterials().get(c);
	if (!mat || mat->role == workshop::Role::None)
		return done();
	const u16 object = lit ? lit : m_hold && aimAtPiece(m_hold->object()) ? m_hold->object() : 0;
	const std::optional<Aim> aim = object ? aimAtPiece(object) : std::nullopt;
	const std::optional<sculpt::Stroke> s = aim ? strokeAt(*aim, sculpt::Act::Add, std::nullopt) : std::nullopt;
	if (!s)
		return done();
	// a block set flat on the face, in its part's grid (as the server fits
	// it: Workshop::fit)
	const v3s32 least = workshop::fittingCorner(sculpt::world(s->at), s->normal);
	const v3f centre = (v3f(least.X, least.Y, least.Z) / shape::STEPS) * BS;
	const v3f offset = intToFloat(client->getEnv().getCameraOffset(), BS);
	auto corner = [&](int i) {
		v3f p = centre + v3f(i & 1 ? 0.5f : -0.5f, i & 2 ? 0.5f : -0.5f, i & 4 ? 0.5f : -0.5f) * BS;
		aim->frame.transformVect(p);
		return p - offset;
	};
	std::vector<Hud::Guide> &guides = *hud->getGuides();
	if (!m_hold)
		guides.clear();
	const video::SColor colour(255, 0, 148, 210);
	for (int i = 0; i < 8; i++)
		for (const int bit : { 1, 2, 4 })
			if (!(i & bit))
				guides.push_back({ corner(i), corner(i | bit), colour });
	m_fit_shown = true;
	if (!was)
		m_game_ui->showTranslatedStatusText("Right-click: fit it here, facing the way you look");
	if (wasKeyPressed(KeyType::PLACE))
		client->sendStroke(object, aim->part, *s, std::nullopt);
	return true;
}

void Game::faceMenu(Menu &m) const
{
	// standing up, facing the player: across is their right, level
	const View v = view(camera->getDirection());
	m.u = v3f(v.right.X, 0, v.right.Z);
	if (m.u.getLengthSQ() < 1e-4f)
		m.u = v3f(1, 0, 0);
	m.u.normalize();
	m.w = v3f(0, 1, 0);
	m.facing = m.u.crossProduct(m.w); // toward the player
}

void Game::hoverJoints(GenericCAO &obj)
{
	// with the tools away (sprint 72): each joint and socket of the
	// workpiece marked, the one nearest the look lit, to right-click
	m_hover_joint.reset();
	if (m_hand.out() || m_menu || !obj.getMachine())
		return;
	const piece::Machine &machine = *obj.getMachine();
	const core::matrix4 frame = obj.getFrame();
	auto world = [&](v3f sixteenths, v3s16 axis) {
		v3f at = (sixteenths / shape::STEPS - v3f(0.5f)) * BS;
		frame.transformVect(at);
		return std::make_pair(at, frame.rotateAndScaleVect(v3f::from(axis)).normalize());
	};
	std::vector<JointAt> all;
	for (const piece::Machine::Link &l : machine.links()) {
		const auto [at, axis] = world(l.anchor, l.axis);
		all.push_back({ l, 0, at, axis, l.joint });
	}
	for (const piece::Machine::Socket &k : machine.sockets()) {
		const auto [at, axis] = world(k.point, k.axis);
		all.push_back({ std::nullopt, k.id, at, axis, k.joint });
	}
	// the nearest to the look's line, near enough, in front
	constexpr f32 NEAR = 0.3f * BS;
	const v3f from = m_pointed_from.start, dir = (m_pointed_from.end - m_pointed_from.start).normalize();
	f32 best = std::numeric_limits<f32>::infinity();
	for (const JointAt &j : all) {
		const f32 along = (j.at - from).dotProduct(dir);
		const f32 off = ((j.at - from) - dir * along).getLength();
		if (along > 0 && off < NEAR && along < best) {
			best = along;
			m_hover_joint = j;
		}
	}
	const video::SColor idle(255, 0, 148, 210), lit(255, 255, 220, 90);
	for (const JointAt &j : all) {
		const bool on = m_hover_joint && m_hover_joint->link.has_value() == j.link.has_value()
				&& (j.link ? j.link->a == m_hover_joint->link->a && j.link->b == m_hover_joint->link->b
				: j.socket == m_hover_joint->socket);
		jointSign(j.at, j.axis, j.joint, on ? lit : idle, on ? 0.8f : 0.35f);
	}
	if (!(m_hover_joint == m_hover_said)) {
		if (m_hover_joint)
			m_game_ui->showTranslatedStatusText(m_hover_joint->link ? "A joint · right-click to change it"
					: "A socket · right-click to change it");
		m_hover_said = m_hover_joint;
	}
}

void Game::openJointMenu(const JointAt &j)
{
	// a joint's own (sprint 72): its kind, or taken away
	using Do = Menu::Do;
	Menu m;
	m.object = m_hold->object();
	m.at = j.at;
	m.normal = j.axis;
	if (j.link) {
		m.link = j.link;
		m.part = j.link->a;
	} else {
		const std::optional<Aim> aim = socketAim(m.object, j.socket);
		if (!aim)
			return;
		m.socket = j.socket;
		m.part = aim->part;
		m.face = faceOf(*aim);
	}
	for (const auto &[k, row] : JOINT_ROWS)
		m.buttons.push_back({ piece::JOINTS[size_t(k)].label, j.link ? Do::Relink : Do::Socket, k, row, j.joint == k });
	springRows(m, 2);
	m.buttons.push_back({ "remove", Do::Unjoint, piece::Joint::Fixed, m.link && m.link->joint == piece::Joint::Sprung ? 4 : 2,
		false });
	faceMenu(m);
	m_menu = std::move(m);
	m_game_ui->showTranslatedStatusText("Look at a kind and click · hover to see it · right-click or Esc: close");
}

void Game::closeMenu()
{
	m_menu.reset();
	hud->getTiles()->clear();
	hud->getLabels()->clear();
}

void Game::jointSign(v3f at, v3f axis, piece::Joint joint, video::SColor colour, f32 scale)
{
	// as the workpiece's joints are drawn: an axle and a ring, half a ring
	// between stops, a plate (BS, the world's)
	const v3f offset = intToFloat(client->getEnv().getCameraOffset(), BS);
	std::vector<Hud::Guide> &guides = *hud->getGuides();
	axis.normalize();
	const v3f u = std::abs(axis.Y) > 0.9f ? v3f(1, 0, 0) : v3f(0, 1, 0).crossProduct(axis).normalize();
	const v3f w = axis.crossProduct(u);
	const f32 R = 0.4f * BS * scale, AXLE = 0.75f * BS * scale;
	if (!piece::JOINTS[size_t(joint)].turns) {
		const v3f c[4] = { at + (u + w) * R, at + (u - w) * R, at - (u + w) * R, at - (u - w) * R };
		for (int i = 0; i < 4; i++)
			guides.push_back({ c[i] - offset, c[(i + 1) % 4] - offset, colour });
		guides.push_back({ c[0] - offset, c[2] - offset, colour });
		guides.push_back({ c[1] - offset, c[3] - offset, colour });
		return;
	}
	guides.push_back({ at - axis * AXLE - offset, at + axis * AXLE - offset, colour });
	constexpr int SIDES = 24;
	const int sides = piece::JOINTS[size_t(joint)].limit > 0 ? SIDES / 2 : SIDES;
	for (int i = 0; i < sides; i++) {
		const f32 a = core::PI * 2 * i / SIDES, b = core::PI * 2 * (i + 1) / SIDES;
		guides.push_back({ at + (u * std::cos(a) + w * std::sin(a)) * R - offset,
			at + (u * std::cos(b) + w * std::sin(b)) * R - offset, colour });
	}
}

void Game::menuStep(f32 dtime)
{
	Menu &m = *m_menu;
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m.object));
	if (!obj || !obj->getMachine() || wasKeyPressed(KeyType::PLACE)) {
		closeMenu();
		return;
	}
	// rows up from out in front of the face, each centred across; the
	// first (joints) on top
	const v3f anchor = m.at + m.facing * MENU_OUT;
	int rows = 0;
	for (const Menu::Button &b : m.buttons)
		rows = std::max(rows, b.row + 1);
	auto middle = [&](size_t i) {
		const int row = m.buttons[i].row;
		size_t in_row = 0, before = 0;
		for (size_t j = 0; j < m.buttons.size(); j++)
			if (m.buttons[j].row == row) {
				before += j < i;
				in_row++;
			}
		return anchor + m.u * ((before - (in_row - 1) / 2.f) * (MENU_SIZE + MENU_GAP))
				+ m.w * ((rows - 1 - row) * (MENU_SIZE + MENU_GAP * 3) + MENU_SIZE);
	};
	const v3f from = m_pointed_from.start, dir = (m_pointed_from.end - m_pointed_from.start).normalize();
	const f32 facing = dir.dotProduct(m.facing);
	int lit = -1;
	if (std::abs(facing) > 1e-4f) {
		const f32 t = (anchor - from).dotProduct(m.facing) / facing;
		const v3f hit = from + dir * t;
		for (size_t i = 0; t > 0 && i < m.buttons.size(); i++) {
			const v3f d = hit - middle(i);
			if (std::abs(d.dotProduct(m.u)) <= MENU_SIZE / 2 && std::abs(d.dotProduct(m.w)) <= MENU_SIZE / 2)
				lit = int(i);
		}
	}
	m.lit = lit;

	// drawn: the page's buttons, a sign on each, its name under it
	const v3f offset = intToFloat(client->getEnv().getCameraOffset(), BS);
	std::vector<Hud::Tile> &tiles = *hud->getTiles();
	std::vector<Hud::Label> &labels = *hud->getLabels();
	tiles.clear();
	labels.clear();
	const f32 h = MENU_SIZE / 2;
	for (size_t i = 0; i < m.buttons.size(); i++) {
		const Menu::Button &button = m.buttons[i];
		Hud::Tile t{ middle(i) - offset, m.u * h, m.w * h, int(i) == lit || button.on, {} };
		auto line = [&](v2f a, v2f b) { t.strokes.push_back({ a * 0.7f, b * 0.7f }); };
		auto arc = [&](f32 a0, f32 a1, f32 r) {
			constexpr int STEPS = 12;
			for (int k = 0; k < STEPS; k++) {
				const f32 a = a0 + (a1 - a0) * k / STEPS, b = a0 + (a1 - a0) * (k + 1) / STEPS;
				line({ std::cos(a) * r, std::sin(a) * r }, { std::cos(b) * r, std::sin(b) * r });
			}
		};
		// a coil of `turns` between end plates
		auto spring = [&](int turns) {
			line({ -0.5f, 0.85f }, { 0.5f, 0.85f });
			line({ -0.5f, -0.85f }, { 0.5f, -0.85f });
			for (int k = 0; k < turns; k++) {
				const f32 y0 = 0.85f - 1.7f * k / turns, y1 = 0.85f - 1.7f * (k + 0.5f) / turns, y2 = 0.85f - 1.7f * (k + 1) / turns;
				line({ -0.4f, y0 }, { 0.4f, y1 });
				line({ 0.4f, y1 }, { -0.4f, y2 });
			}
		};
		switch (button.does) {
		case Menu::Do::Travel: { // how far: an arrow up and down, longer for more
			const f32 r = 0.35f + 0.25f * button.step;
			line({ 0, -r }, { 0, r });
			for (const f32 s : { -1.f, 1.f }) {
				line({ 0, s * r }, { -0.25f, s * (r - 0.25f) });
				line({ 0, s * r }, { 0.25f, s * (r - 0.25f) });
			}
			break;
		}
		case Menu::Do::Firmness: // how firm: more turns of a stiffer coil
			spring(3 + 2 * button.step);
			break;
		case Menu::Do::Socket:
		case Menu::Do::Relink:
			if (button.joint == piece::Joint::Fixed) { // a plate bolted: a cross
				line({ -0.6f, -0.6f }, { 0.6f, 0.6f });
				line({ -0.6f, 0.6f }, { 0.6f, -0.6f });
			} else if (piece::JOINTS[size_t(button.joint)].steers || piece::JOINTS[size_t(button.joint)].driven) {
				// a wheel; steering: turned on a pin above it; driven: a bolt in it
				const piece::JointKind &k = piece::JOINTS[size_t(button.joint)];
				arc(0, core::PI * 2, 0.55f);
				if (k.steers) {
					line({ 0, 0.55f }, { 0, 0.95f });
					line({ -0.35f, 0.95f }, { 0.35f, 0.95f });
				}
				if (k.driven) {
					line({ 0.1f, 0.4f }, { -0.15f, 0 });
					line({ -0.15f, 0 }, { 0.15f, 0 });
					line({ 0.15f, 0 }, { -0.1f, -0.4f });
				}
			} else if (button.joint == piece::Joint::Sprung) { // a coil spring
				spring(5);
			} else if (button.joint == piece::Joint::Rotary) { // round all the way
				arc(0.4f, core::PI * 2, 0.65f);
				line({ 0.65f, 0 }, { 0.45f, 0.25f });
				line({ 0.65f, 0 }, { 0.85f, 0.25f });
			} else { // a swing between stops
				arc(core::PI / 4, core::PI * 3 / 4, 0.8f);
				line({ 0, -0.5f }, { 0.57f, 0.57f });
				line({ 0, -0.5f }, { -0.57f, 0.57f });
			}
			break;
		case Menu::Do::Upright: // an arrow up off the ground
			line({ -0.8f, -0.8f }, { 0.8f, -0.8f });
			line({ 0, -0.6f }, { 0, 0.8f });
			line({ 0, 0.8f }, { -0.4f, 0.4f });
			line({ 0, 0.8f }, { 0.4f, 0.4f });
			break;
		case Menu::Do::Sit: // a seat and its back
			line({ -0.6f, 0 }, { 0.4f, 0 });
			line({ 0.4f, 0 }, { 0.4f, 0.8f });
			line({ -0.6f, 0 }, { -0.6f, -0.7f });
			line({ 0.4f, 0 }, { 0.4f, -0.7f });
			break;
		case Menu::Do::Focus: // the bench's corners
			for (const v2f c : { v2f(-1, -1), v2f(1, -1), v2f(1, 1), v2f(-1, 1) }) {
				line(c * 0.8f, c * 0.8f - v2f(c.X * 0.4f, 0));
				line(c * 0.8f, c * 0.8f - v2f(0, c.Y * 0.4f));
			}
			break;
		case Menu::Do::Carry: // up
			line({ 0, -0.8f }, { 0, 0.8f });
			line({ 0, 0.8f }, { -0.5f, 0.3f });
			line({ 0, 0.8f }, { 0.5f, 0.3f });
			break;
		case Menu::Do::Edit: // a pencil
			line({ -0.7f, -0.7f }, { 0.6f, 0.6f });
			line({ -0.5f, -0.8f }, { 0.8f, 0.5f });
			line({ -0.7f, -0.7f }, { -0.5f, -0.8f });
			break;
		case Menu::Do::Detach: // two apart
			for (const f32 x : { -0.75f, 0.15f }) {
				line({ x, -0.3f }, { x + 0.6f, -0.3f });
				line({ x + 0.6f, -0.3f }, { x + 0.6f, 0.3f });
				line({ x + 0.6f, 0.3f }, { x, 0.3f });
				line({ x, 0.3f }, { x, -0.3f });
			}
			break;
		case Menu::Do::Unjoint:
		case Menu::Do::Bare: // none: a ring struck through
			arc(0, core::PI * 2, 0.6f);
			line({ -0.6f, -0.6f }, { 0.6f, 0.6f });
			break;
		case Menu::Do::Delete: // a bin
			line({ -0.7f, 0.6f }, { 0.7f, 0.6f });
			line({ -0.5f, 0.6f }, { -0.4f, -0.8f });
			line({ 0.5f, 0.6f }, { 0.4f, -0.8f });
			line({ -0.4f, -0.8f }, { 0.4f, -0.8f });
			break;
		}
		tiles.push_back(std::move(t));
		labels.push_back({ middle(i) - m.w * (h + 0.12f * BS), utf8_to_wide(button.label),
			int(i) == lit ? video::SColor(255, 255, 255, 255) : video::SColor(255, 200, 210, 220) });
	}
	// a stem from the face to the buttons
	hud->getGuides()->push_back({ m.at - offset, anchor - offset, video::SColor(255, 0, 148, 210) });
	// a joint looked at: shown where it'd be, before it's made
	if (lit >= 0 && (m.buttons[lit].does == Menu::Do::Socket || m.buttons[lit].does == Menu::Do::Relink)) {
		const video::SColor ghost(255, 255, 200, 90);
		if (m.link) {
			v3f at = (m.link->anchor / shape::STEPS - v3f(0.5f)) * BS, axis = v3f::from(m.link->axis);
			obj->getFrame().transformVect(at);
			axis = obj->getFrame().rotateAndScaleVect(axis);
			jointSign(at, axis, m.buttons[lit].joint, ghost);
		} else {
			jointSign(m.at, m.normal, m.buttons[lit].joint, ghost);
		}
	}

	if (!wasKeyPressed(KeyType::DIG))
		return;
	if (lit < 0) {
		closeMenu();
		return;
	}
	// delete asks again: the first click arms it (sprint 72)
	if (m.buttons[lit].does == Menu::Do::Delete && !m.armed) {
		m.armed = true;
		m.buttons[lit].label = "sure? click";
		m_game_ui->showTranslatedStatusText("Click delete again to delete this part");
		return;
	}
	const Menu::Button button = m.buttons[lit];
	const Menu done = m;
	closeMenu();
	PointedThing at; // as if it were pressed where the menu's opened
	at.type = POINTEDTHING_OBJECT;
	at.object_id = done.object;
	at.intersection_point = done.at;
	switch (button.does) {
	case Menu::Do::Socket: {
		const std::optional<sculpt::Stroke::Mirror> mirror = machineMirror();
		if (done.socket) { // the one there made again as this
			if (const piece::Machine::Socket *k = socketOf(done.object, *done.socket)) {
				if (mirror)
					if (const piece::Machine::Socket *across = mirrorSocket(done.object, *k, *mirror))
						client->sendUnsocket(done.object, across->id);
				client->sendUnsocket(done.object, k->id);
			}
		}
		client->sendSocket(done.object, done.face, button.joint, mirror);
		m_game_ui->showStatusText(utf8_to_wide(std::string("Socket: ") + button.label
				+ " · carry a part onto it, or draw on it"));
		return;
	}
	case Menu::Do::Relink:
		client->sendLink(done.object, done.link->a, done.link->b, button.joint, std::nullopt);
		m_game_ui->showStatusText(utf8_to_wide(std::string("Joint: ") + button.label
				+ (button.joint == piece::Joint::Sprung ? " · right-click it again for its travel and firmness"
				" · a wheel turning on the part it holds rides it" : "")));
		return;
	case Menu::Do::Travel:
	case Menu::Do::Firmness: {
		piece::Spring spring = done.link->spring;
		(button.does == Menu::Do::Travel ? spring.travel : spring.firmness) = button.step;
		client->sendLink(done.object, done.link->a, done.link->b, piece::Joint::Sprung, std::nullopt, spring);
		m_game_ui->showStatusText(utf8_to_wide(std::string("Spring: ") + piece::TRAVELS[spring.travel].label + ", "
				+ piece::FIRMNESSES[spring.firmness].label));
		return;
	}
	case Menu::Do::Sit:
		handleDrive(at, dtime);
		return;
	case Menu::Do::Focus:
		if (m_hold && m_hold->object() == done.object)
			closeFocus();
		else
			focusOn(done.object);
		return;
	case Menu::Do::Carry:
		handleCarry(at, dtime);
		return;
	case Menu::Do::Upright:
		client->sendUpright(done.object);
		return;
	case Menu::Do::Edit:
		pickPart(done.part == m_part ? std::nullopt : std::optional<u64>(done.part));
		return;
	case Menu::Do::Detach:
	case Menu::Do::Delete:
		client->sendPart(done.object, done.part, button.does == Menu::Do::Delete);
		pickPart(std::nullopt);
		return;
	case Menu::Do::Unjoint:
		if (done.link) {
			client->sendLink(done.object, done.link->a, done.link->b, std::nullopt, std::nullopt);
		} else if (const piece::Machine::Socket *k = done.socket ? socketOf(done.object, *done.socket) : nullptr) {
			if (const std::optional<sculpt::Stroke::Mirror> mirror = machineMirror())
				if (const piece::Machine::Socket *across = mirrorSocket(done.object, *k, *mirror))
					client->sendUnsocket(done.object, across->id);
			client->sendUnsocket(done.object, k->id);
		}
		return;
	case Menu::Do::Bare:
		for (const piece::Machine::Socket &k : obj->getMachine()->sockets())
			if (k.part == done.part)
				client->sendUnsocket(done.object, k.id);
		return;
	}
	throw BaseException("Game::menuStep: not a button");
}

void Game::selectPart(const Aim &aim)
{
	// this one selected, or unselected clicked again (sprint 65; joints
	// are the joint tool's, sprint 70)
	pickPart(aim.part == m_part ? std::nullopt : std::optional<u64>(aim.part));
}

void Game::pickPart(std::optional<u64> part)
{
	m_part = part;
	if (!part)
		closeMenu();
	m_game_ui->showTranslatedStatusText(part ? "Working on one part (Esc: all of it)" : "Working on all of it");
}

void Game::handleAsk(const Hand::Ask &ask, const PointedThing &pointed)
{
	// the object it's for: the one focused, else the one pointed at
	const u16 object = m_hold ? m_hold->object()
			: pointed.type == POINTEDTHING_OBJECT && pieceMiddle(pointed.object_id) ? pointed.object_id : 0;
	// the part it's for: the one picked, else the one pointed at
	std::optional<u64> part = m_part;
	if (!part)
		if (const std::optional<Aim> aim = aimAt(pointed); aim && aim->piece)
			part = aim->part;
	switch (ask.verb) {
	case Hand::Verb::Focus:
		toggleFocus();
		return;
	case Hand::Verb::Mirror:
		m_hand.toggleMirror();
		m_mirror_plane.reset(); // chosen afresh at the next stroke, then kept
		showTool();
		return;
	case Hand::Verb::Undo:
		client->sendUndo();
		return;
	case Hand::Verb::Start: {
		// of the block held: said here, not only refused
		ItemStack selected, hand;
		const ItemStack &item = client->getEnv().getLocalPlayer()->getWieldedItem(&selected, &hand);
		content_t id;
		if (item.empty() || !client->ndef()->getId(item.name, id) || client->ndef()->get(id).drawtype != NDT_NORMAL) {
			m_game_ui->showTranslatedStatusText("Hold a block in your hotbar to make it of");
			return;
		}
	}
		// holding a machine, a block attached to the face pointed at; else a new one
		if (!m_hold) {
			client->sendPieceNew();
			return;
		}
		// a block of a part of its own on it (and its mirror's)
		if (const std::optional<Aim> aim = aimAt(pointed)) {
			if (std::optional<sculpt::Stroke> s = strokeAt(*aim, sculpt::Act::Add)) {
				s->tool = sculpt::Tool::Box;
				s->size = shape::STEPS;
				client->sendStroke(aim->piece, aim->part, *s, piece::Joint::Fixed);
			}
		} else {
			m_game_ui->showTranslatedStatusText("Point at a face of it to add a part to");
		}
		return;
	case Hand::Verb::Place:
		if (pointed.type != POINTEDTHING_NODE) {
			m_game_ui->showTranslatedStatusText("Point at the ground to put it on");
			return;
		}
		client->sendBlueprintPlace(ask.name, pointed.node_abovesurface);
		return;
	case Hand::Verb::Level:
	case Hand::Verb::Reset:
		if (!m_hold) {
			m_game_ui->showTranslatedStatusText("Pick up an object first");
			return;
		}
		if (ask.verb == Hand::Verb::Level)
			m_hold->level();
		else
			m_hold->reset();
		return;
	case Hand::Verb::Detach:
	case Hand::Verb::Delete:
		if (!m_hold || !m_part) {
			m_game_ui->showTranslatedStatusText("Select a part first");
			return;
		}
		client->sendPart(m_hold->object(), *m_part, ask.verb == Hand::Verb::Delete);
		pickPart(std::nullopt);
		return;
	case Hand::Verb::Relink:
		if (!m_hold || !m_part) {
			m_game_ui->showTranslatedStatusText("Select a part first");
			return;
		}
		client->sendLink(m_hold->object(), *m_part, ask.other, ask.unlink ? std::nullopt
				: std::optional<piece::Joint>(ask.joint), std::nullopt);
		return;
	case Hand::Verb::Socket:
		if (!m_hold || !m_menu) {
			m_game_ui->showTranslatedStatusText("Right-click a face of what's on the workbench first");
			return;
		}
		client->sendSocket(m_hold->object(), m_menu->face, ask.joint, machineMirror());
		closeMenu();
		return;
	case Hand::Verb::Unsocket:
		if (!m_hold) {
			m_game_ui->showTranslatedStatusText("Put it on the workbench first");
			return;
		}
		client->sendUnsocket(m_hold->object(), ask.other);
		return;
	case Hand::Verb::Hollow:
	case Hand::Verb::Round:
		if (!object) {
			m_game_ui->showTranslatedStatusText("Pick up an object, or point at one");
			return;
		}
		if (!part) {
			m_game_ui->showTranslatedStatusText("Point at the part to hollow or round");
			return;
		}
		client->sendHollow(object, *part, m_hand.size(), ask.verb == Hand::Verb::Round);
		return;
	case Hand::Verb::Save: {
		// what was pointed at as it was named (Ctrl+S), else what's pointed at now
		const std::optional<std::pair<u16, u64>> what = std::exchange(m_naming, std::nullopt);
		if (what) {
			client->sendBlueprintSave(what->first, what->second, ask.name);
		} else if (object && part) {
			client->sendBlueprintSave(object, *part, ask.name);
		} else if (auto *obj = object ? dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(object)) : nullptr;
				obj && obj->getMachine() && !obj->getMachine()->parts().empty()) {
			client->sendBlueprintSave(object, obj->getMachine()->parts().front().object.id(), ask.name); // all of it
		} else {
			m_game_ui->showTranslatedStatusText("Pick up an object, or point at one");
			return;
		}
		if (what)
			m_cursor = false;
		return;
	}
	case Hand::Verb::Copy:
	case Hand::Verb::Name:
		if (!object || !part) {
			m_game_ui->showTranslatedStatusText("Point at a part of an object");
			return;
		}
		if (ask.verb == Hand::Verb::Copy) {
			client->sendCopy(object, *part);
			return;
		}
		// named on the page, the cursor freed to type it
		m_naming = std::make_pair(object, *part);
		m_cursor = true;
		closeMenu();
		m_game_ui->showTranslatedStatusText("Name the part to save it (Esc: don't)");
		return;
	case Hand::Verb::Paste:
		client->sendPaste();
		return;
	case Hand::Verb::Take:
		client->sendTake(ask.name);
		return;
	case Hand::Verb::Forget:
		client->sendForget(ask.name);
		return;
	case Hand::Verb::Count:
		break;
	}
	throw BaseException("Game::handleAsk: no such verb");
}

void Game::showGhost(const Aim &aim, const sculpt::Stroke &stroke, v3s16 camera_offset)
{
	if (!(m_ghost_of == stroke)) {
		m_ghost = sculpt::outline(stroke.cells());
		m_ghost_of = stroke;
	}
	// in the aim's frame (BS), then the world's
	std::vector<std::pair<v3f, v3f>> world;
	for (const auto &[a, b] : m_ghost) {
		auto at = [&](const v3s32 &w) {
			v3f p = (v3f(w.X, w.Y, w.Z) / shape::STEPS - v3f(0.5f)) * BS;
			aim.frame.transformVect(p);
			return p;
		};
		world.emplace_back(at(a), at(b));
	}
	std::vector<aabb3f> *boxes = hud->getSelectionBoxes();
	boxes->clear();
	aabb3f cell = shape::box(stroke.at.cell);
	// adding, the sixteenth it starts from is the one on the face, not in it
	const v3f out = stroke.act == sculpt::Act::Add ? v3f::from(stroke.normal) * (BS / shape::STEPS) : v3f(0);
	cell.MinEdge += intToFloat(stroke.at.node, BS) + out;
	cell.MaxEdge += intToFloat(stroke.at.node, BS) + out;
	boxes->push_back(cell);
	hud->setSelectionPos(aim.frame.getTranslation(), camera_offset);
	hud->setSelectionRotationRadians(aim.frame.getRotationRadians());

	std::vector<std::pair<v3f, v3f>> *lines = hud->getSelectionLines();
	lines->clear();
	m_ghost_shown = true;
	if (AtlasView *view = client->getEnv().getClientMap().atlasView()) {
		// on a planet, placed as the node they start in is drawn (the
		// world's frame is at its origin: the node pointed at, not that)
		v3f start = intToFloat(stroke.at.node, BS);
		aim.frame.transformVect(start);
		const v3s16 o = floatToInt(start, BS);
		std::vector<std::pair<v3f, v3f>> rel;
		for (const auto &[a, b] : world)
			rel.emplace_back(a / BS - v3f::from(o), b / BS - v3f::from(o));
		if (auto placed = view->place(o, rel))
			*lines = std::move(*placed);
	} else {
		const v3f offset = intToFloat(camera_offset, BS);
		for (const auto &[a, b] : world)
			lines->emplace_back(a - offset, b - offset);
	}
}

void Game::showTool()
{
	m_game_ui->showStatusText(utf8_to_wide(m_hand.describe()));
}

std::optional<v3f> Game::pieceMiddle(u16 id) const
{
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id));
	if (!obj || !obj->getMachine())
		return std::nullopt;
	const std::optional<aabb3f> box = obj->getMachine()->designed();
	if (!box)
		return std::nullopt;
	return box->getCenter();
}

const piece::Pose &Game::partPose(u16 id, u64 part) const
{
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id));
	const piece::Machine::Part *p = obj && obj->getMachine() ? obj->getMachine()->part(part) : nullptr;
	if (!p)
		throw BaseException("partPose: no part " + std::to_string(part));
	return p->pose;
}


void Game::toggleFocus()
{
	// pointed at another object, it's the workpiece instead (one at a time)
	const PointedThing &at = runData.pointed_old;
	const bool other = at.type == POINTEDTHING_OBJECT && pieceMiddle(at.object_id)
			&& !(m_hold && m_hold->object() == at.object_id);
	if (other) {
		focusOn(at.object_id);
		return;
	}
	if (m_hold) {
		closeFocus();
		return;
	}
	m_game_ui->showTranslatedStatusText("Point at an object to work on it (C: create one)");
}

void Game::focusOn(u16 id)
{
	if (m_hold)
		closeFocus();
	const std::optional<v3f> middle = pieceMiddle(id);
	if (!middle)
		throw BaseException("Game::focusOn: not an object");
	// lifted where it is, a little, and left floating there
	constexpr f32 LIFT = 0.5f; // nodes
	auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(id));
	v3f at = *middle * BS;
	obj->getFrame().transformVect(at);
	m_hold.emplace(id, *middle, pieceRadius(id, *middle), obj->getFrame(), at / BS + v3f(0, LIFT, 0));
	m_hold->level();
	m_pose_sent.reset();
	m_pose_age = 0;
	client->sendHold(id, workshop::Grip::Still);
	// build mode is Tab's alone (sprint 72): F only puts it on the bench
	m_game_ui->showTranslatedStatusText("On the workbench: hold left on a part to pull it off · disc and ring turn it · F to put it down");
}

void Game::closeFocus()
{
	if (pieceMiddle(m_hold->object()))
		client->sendHold(m_hold->object(), workshop::Grip::Free);
	pickPart(std::nullopt);
	if (auto *obj = dynamic_cast<GenericCAO *>(client->getEnv().getActiveObject(m_hold->object()))) {
		obj->setLook({});
		obj->setPosed(std::nullopt);
		const Hold::Pose pose = m_hold->pose();
		client->sendPose(m_hold->object(), pose.position, pose.rotation); // where it was drawn
	}
	m_hold.reset();
	closeMenu();
	hud->getGuides()->clear();
	m_game_ui->showTranslatedStatusText("Put down");
}

bool Game::nodePlacement(const ItemDefinition &selected_def,
	const ItemStack &selected_item, const v3s16 &nodepos, const v3s16 &neighborpos,
	const PointedThing &pointed, const NodeMetadata *meta)
{
	const auto &prediction = selected_def.node_placement_prediction;

	const NodeDefManager *nodedef = client->ndef();
	ClientMap &map = client->getEnv().getClientMap();
	MapNode node;
	bool is_valid_position;

	node = map.getNode(nodepos, &is_valid_position);
	if (!is_valid_position) {
		soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
		return false;
	}

	// formspec in meta
	if (meta && !meta->getString("formspec").empty() && !input->isRandom()
			&& !isKeyDown(KeyType::AUX1)) {
		// on_rightclick callbacks are called anyway
		if (nodedef_manager->get(map.getNode(nodepos)).rightclickable)
			client->interact(INTERACT_PLACE, pointed);

		m_game_formspec.showNodeFormspec(meta->getString("formspec"), nodepos);
		return false;
	}

	// on_rightclick callback
	if (prediction.empty() || (nodedef->get(node).rightclickable &&
			!isKeyDown(KeyType::AUX1))) {
		// Report to server
		client->interact(INTERACT_PLACE, pointed);
		return false;
	}

	verbosestream << "Node placement prediction for "
		<< selected_def.name << " is " << prediction << std::endl;
	v3s16 p = neighborpos;

	// Place inside node itself if buildable_to
	MapNode n_under = map.getNode(nodepos, &is_valid_position);
	if (is_valid_position) {
		if (nodedef->get(n_under).buildable_to) {
			p = nodepos;
		} else {
			node = map.getNode(p, &is_valid_position);
			if (is_valid_position && !nodedef->get(node).buildable_to) {
				soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
				// Report to server
				client->interact(INTERACT_PLACE, pointed);
				return false;
			}
		}
	}

	// Find id of predicted node
	content_t id;
	bool found = nodedef->getId(prediction, id);

	if (!found) {
		errorstream << "Node placement prediction failed for "
			<< selected_def.name << " (places " << prediction
			<< ") - Name not known" << std::endl;
		// Handle this as if prediction was empty
		// Report to server
		client->interact(INTERACT_PLACE, pointed);
		return false;
	}

	const ContentFeatures &predicted_f = nodedef->get(id);

	// Compare core.item_place_node() for what the server does with param2
	MapNode predicted_node(id, 0, 0);

	const auto place_param2 = selected_def.place_param2;

	if (place_param2) {
		predicted_node.setParam2(*place_param2);
	} else if (predicted_f.param_type_2 == CPT2_WALLMOUNTED ||
			predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
		v3s16 dir = nodepos - neighborpos;

		if (abs(dir.Y) > MYMAX(abs(dir.X), abs(dir.Z))) {
			// If you change this code, also change builtin/game/item.lua
			u8 predicted_param2 = dir.Y < 0 ? 1 : 0;
			if (selected_def.wallmounted_rotate_vertical) {
				bool rotate90 = false;
				v3f ppos = client->getEnv().getLocalPlayer()->getPosition() / BS;
				v3f pdir = v3f::from(neighborpos) - ppos;
				switch (predicted_f.drawtype) {
					case NDT_TORCHLIKE: {
						rotate90 = !((pdir.X < 0 && pdir.Z > 0) ||
								(pdir.X > 0 && pdir.Z < 0));
						if (dir.Y > 0) {
							rotate90 = !rotate90;
						}
						break;
					};
					case NDT_SIGNLIKE: {
						rotate90 = std::abs(pdir.X) < std::abs(pdir.Z);
						break;
					}
					default: {
						rotate90 = std::abs(pdir.X) > std::abs(pdir.Z);
						break;
					}
				}
				if (rotate90) {
					predicted_param2 += 6;
				}
			}
			predicted_node.setParam2(predicted_param2);
		} else if (abs(dir.X) > abs(dir.Z)) {
			predicted_node.setParam2(dir.X < 0 ? 3 : 2);
		} else {
			predicted_node.setParam2(dir.Z < 0 ? 5 : 4);
		}
	} else if (predicted_f.param_type_2 == CPT2_FACEDIR ||
			predicted_f.param_type_2 == CPT2_COLORED_FACEDIR ||
			predicted_f.param_type_2 == CPT2_4DIR ||
			predicted_f.param_type_2 == CPT2_COLORED_4DIR) {
		v3s16 dir = nodepos - floatToInt(client->getEnv().getLocalPlayer()->getPosition(), BS);

		if (abs(dir.X) > abs(dir.Z)) {
			predicted_node.setParam2(dir.X < 0 ? 3 : 1);
		} else {
			predicted_node.setParam2(dir.Z < 0 ? 2 : 0);
		}
	}

	// Check attachment if node is in group attached_node
	int an = itemgroup_get(predicted_f.groups, "attached_node");
	if (an != 0) {
		v3s16 pp;

		if (an == 3) {
			pp = p + v3s16(0, -1, 0);
		} else if (an == 4) {
			pp = p + v3s16(0, 1, 0);
		} else if (an == 2) {
			if (predicted_f.param_type_2 == CPT2_FACEDIR ||
					predicted_f.param_type_2 == CPT2_COLORED_FACEDIR ||
					predicted_f.param_type_2 == CPT2_4DIR ||
					predicted_f.param_type_2 == CPT2_COLORED_4DIR) {
				pp = p + facedir_dirs[predicted_node.getFaceDir(nodedef)];
			} else {
				pp = p;
			}
		} else if (predicted_f.param_type_2 == CPT2_WALLMOUNTED ||
				predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
			pp = p + predicted_node.getWallMountedDir(nodedef);
		} else {
			pp = p + v3s16(0, -1, 0);
		}

		if (!nodedef->get(map.getNode(pp)).walkable) {
			soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
			// Report to server
			client->interact(INTERACT_PLACE, pointed);
			return false;
		}
	}

	// Apply color
	if (!place_param2 && (predicted_f.param_type_2 == CPT2_COLOR
			|| predicted_f.param_type_2 == CPT2_COLORED_FACEDIR
			|| predicted_f.param_type_2 == CPT2_COLORED_4DIR
			|| predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED)) {
		const auto &indexstr = selected_item.metadata.
			getString("palette_index", 0);
		if (!indexstr.empty()) {
			s32 index = mystoi(indexstr);
			if (predicted_f.param_type_2 == CPT2_COLOR) {
				predicted_node.setParam2(index);
			} else if (predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
				// param2 = pure palette index + other
				predicted_node.setParam2((index & 0xf8) | (predicted_node.getParam2() & 0x07));
			} else if (predicted_f.param_type_2 == CPT2_COLORED_FACEDIR) {
				// param2 = pure palette index + other
				predicted_node.setParam2((index & 0xe0) | (predicted_node.getParam2() & 0x1f));
			} else if (predicted_f.param_type_2 == CPT2_COLORED_4DIR) {
				// param2 = pure palette index + other
				predicted_node.setParam2((index & 0xfc) | (predicted_node.getParam2() & 0x03));
			}
		}
	}

	// Add node to client map
	try {
		LocalPlayer *player = client->getEnv().getLocalPlayer();

		bool build_where_you_stand = g_settings->getBool("enable_build_where_you_stand");
		bool stands_inside = false;
		if (!build_where_you_stand) {
			// Check if attempting to place a node that player stands inside,
			// depending on the player's height
			aabb3f collisionbox = player->getCollisionbox();
			int player_height = ceilf((collisionbox.MaxEdge.Y - collisionbox.MinEdge.Y) / BS);
			player_height = MYMAX(player_height, 1);
			v3s16 diff = neighborpos - player->getStandingNodePos();
			stands_inside = (diff.X == 0 && diff.Z == 0 && diff.Y >= 1 && diff.Y <= player_height);
		}

		// Don't place node when player would be inside new node
		// NOTE: This is to be eventually implemented by a mod as client-side Lua
		if (!predicted_f.walkable ||
				build_where_you_stand ||
				(client->checkPrivilege("noclip") && g_settings->getBool("noclip")) ||
				(predicted_f.walkable && !stands_inside)) {
			// This triggers the required mesh update too
			client->addNode(p, predicted_node);
			// Report to server
			client->interact(INTERACT_PLACE, pointed);
			// A node is predicted, also play a sound
			soundmaker->m_player_rightpunch_sound = selected_def.sound_place;
			return true;
		} else {
			soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
			return false;
		}
	} catch (const InvalidPositionException &e) {
		errorstream << "Node placement prediction failed for "
			<< selected_def.name << " (places "
			<< prediction << ") - Position not loaded" << std::endl;
		soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
		return false;
	}
}

void Game::handlePointingAtObject(const PointedThing &pointed, const ItemStack &tool_item,
		const ItemStack &hand_item, const v3f &player_position, bool show_debug)
{
	// Terra Firma: an object that's gone since it was pointed at
	if (!runData.selected_object)
		return;
	std::wstring infotext = unescape_translate(
		utf8_to_wide(runData.selected_object->infoText()));

	if (show_debug) {
		if (!infotext.empty()) {
			infotext += L"\n";
		}
		infotext += utf8_to_wide(runData.selected_object->debugInfoText());
	}

	m_game_ui->setInfoText(infotext);

	// Terra Firma: a machine isn't punched: it's grabbed, or struck (above)
	const auto *punched = dynamic_cast<GenericCAO *>(runData.selected_object);
	if (isKeyDown(KeyType::DIG) && !(punched && punched->getMachine())) {
		bool do_punch = false;
		bool do_punch_damage = false;

		if (runData.object_hit_delay_timer <= 0.0) {
			do_punch = true;
			do_punch_damage = true;
			runData.object_hit_delay_timer = object_hit_delay;
		}

		if (wasKeyPressed(KeyType::DIG))
			do_punch = true;

		if (do_punch) {
			infostream << "Punched object" << std::endl;
			runData.punching = true;
			runData.nodig_delay_timer = std::max(0.15f, m_repeat_dig_time);
		}

		if (do_punch_damage) {
			// Report direct punch
			v3f objpos = runData.selected_object->getPosition();
			v3f dir = (objpos - player_position).normalize();

			bool disable_send = runData.selected_object->directReportPunch(
					dir, &tool_item, &hand_item, runData.time_from_last_punch);
			runData.time_from_last_punch = 0;

			if (!disable_send)
				client->interact(INTERACT_START_DIGGING, pointed);
		}
	} else if (wasKeyDown(KeyType::PLACE)) {
		infostream << "Pressed place button while pointing at object" << std::endl;
		client->interact(INTERACT_PLACE, pointed);  // place
	}
}


void Game::handleDigging(const PointedThing &pointed, const v3s16 &nodepos,
		const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime)
{
	// See also: serverpackethandle.cpp, action == 2
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	ClientMap &map = client->getEnv().getClientMap();
	MapNode n = map.getNode(nodepos);
	const auto &features = nodedef_manager->get(n);
	const ItemStack &tool_item = selected_item.name.empty() ? hand_item : selected_item;

	// NOTE: Similar piece of code exists on the server side for
	// cheat detection.
	// Get digging parameters
	DigParams params = getDigParams(features.groups,
			&tool_item.getToolCapabilities(itemdef_manager, &hand_item),
			tool_item.wear);

	// If can't dig, try hand
	if (!params.diggable) {
		params = getDigParams(features.groups,
				&hand_item.getToolCapabilities(itemdef_manager));
	}

	if (!params.diggable) {
		// I guess nobody will wait for this long
		runData.dig_time_complete = 10000000.0;
	} else {
		runData.dig_time_complete = params.time;
		if (g_settings->getBool("enable_dig_particles")) {
			client->getParticleManager()->addNodeParticle(player, nodepos, n);
		}
	}

	if (!runData.digging) {
		infostream << "Started digging" << std::endl;
		runData.dig_instantly = runData.dig_time_complete == 0;
		if (client->modsLoaded() && client->getScript()->on_punchnode(nodepos, n))
			return;

		client->interact(INTERACT_START_DIGGING, pointed);
		runData.digging = true;
		runData.btn_down_for_dig = true;
	}

	if (!runData.dig_instantly) {
		runData.dig_index = (float)crack_animation_length
				* runData.dig_time
				/ runData.dig_time_complete;
	} else {
		// This is for e.g. torches
		runData.dig_index = crack_animation_length;
	}

	const auto &sound_dig = features.sound_dig;

	if (sound_dig.exists() && params.diggable) {
		if (sound_dig.name == "__group") {
			if (!params.main_group.empty()) {
				soundmaker->m_player_leftpunch_sound.gain = 0.5;
				soundmaker->m_player_leftpunch_sound.name =
						std::string("default_dig_") +
						params.main_group;
			}
		} else {
			soundmaker->m_player_leftpunch_sound = sound_dig;
		}
	}

	// Don't show cracks if not diggable
	if (runData.dig_time_complete >= 100000.0) {
	} else if (runData.dig_index < crack_animation_length) {
		client->setCrack(runData.dig_index, nodepos);
	} else {
		infostream << "Digging completed" << std::endl;
		client->setCrack(-1, v3s16(0, 0, 0));

		runData.dig_time = 0;
		runData.digging = false;
		// we successfully dug, now block it from repeating if we want to be safe
		if (g_settings->getBool("safe_dig_and_place"))
			runData.digging_blocked = true;

		runData.nodig_delay_timer =
				runData.dig_time_complete / (float)crack_animation_length;

		// We don't want a corresponding delay to very time consuming nodes
		// and nodes without digging time (e.g. torches) get a fixed delay.
		if (runData.nodig_delay_timer > 0.3f)
			runData.nodig_delay_timer = 0.3f;
		else if (runData.dig_instantly)
			runData.nodig_delay_timer = 0.15f;

		// Ensure that the delay between breaking nodes
		// (dig_time_complete + nodig_delay_timer) is at least the
		// value of the repeat_dig_time setting.
		runData.nodig_delay_timer = std::max(runData.nodig_delay_timer,
				m_repeat_dig_time - runData.dig_time_complete);

		if (client->modsLoaded() &&
				client->getScript()->on_dignode(nodepos, n)) {
			return;
		}

		if (features.node_dig_prediction == "air") {
			client->removeNode(nodepos);
		} else if (!features.node_dig_prediction.empty()) {
			content_t id;
			bool found = nodedef_manager->getId(features.node_dig_prediction, id);
			if (found)
				client->addNode(nodepos, id, true);
		}
		// implicit else: no prediction

		client->interact(INTERACT_DIGGING_COMPLETED, pointed);

		if (g_settings->getBool("enable_dig_particles")) {
			client->getParticleManager()->addDiggingParticles(player, nodepos, n);
		}

		// Send event to trigger sound
		client->getEventManager()->put(new NodeDugEvent(nodepos, n));
	}

	if (runData.dig_time_complete < 100000.0) {
		runData.dig_time += dtime;
	} else {
		runData.dig_time = 0;
		client->setCrack(-1, nodepos);
	}

	camera->setDigging(0);  // Dig animation
}

void Game::updateFrame(ProfilerGraph *graph, RunStats *stats, f32 dtime,
		const CameraOrientation &cam)
{
	ZoneScoped;
	TimeTaker tt_update("Game::updateFrame()");
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	/*
		Frame time
	*/

	client->getEnv().updateFrameTime(m_is_paused);

	/*
		Fog range
	*/

	if (sky->getFogDistance() >= 0) {
		draw_control->wanted_range = MYMIN(draw_control->wanted_range, sky->getFogDistance());
	}
	if (draw_control->range_all && sky->getFogDistance() < 0) {
		runData.fog_range = FOG_RANGE_ALL;
	} else {
		runData.fog_range = draw_control->wanted_range * BS;
	}

	// Terra Firma: the far view, once the server says this is a terra world.
	// Created after the sky, so it draws right after it. The fog moves out to
	// what it reaches (from high up, the planet's horizon).
	// The planet comes before the horizon (Server::SendPlanet). On an atlas
	// world it's the body the player stands on: made again when that's
	// another (kept while they're in seam, between tiles).
	if (client->getHorizon() && client->getAtlas()) {
		const v3f feet = player->getPosition() / BS;
		const std::optional<int> on = client->getAtlas()->bodyAt(planet::Atlas::moverNode(feet.X, feet.Y, feet.Z));
		if (far_view && on && *on != far_body) {
			warningstream << "sky: the eye changed body, " << far_body << " to " << *on << std::endl;
			space->remove(); // the scene manager holds them too
			space.reset();
			far_view->remove();
			far_view.reset();
		}
		if (!far_view && on) {
			// the sky first: the pass draws them in the order they're made
			space = make_irr<planetarium::Sky>(smgr, shader_src, *client->getAtlas(), *client->getHorizon(), *on);
			far_view = make_irr<far::View>(smgr, shader_src, texture_src, client->ndef(), *client->getHorizon(),
					*client->getAtlas(), *on);
			space->palette(far_view->palette());
			far_body = *on;
		}
	}
	if (space)
		space->update(*camera, client->getEnv().getClientMap(),
				client->getEnv().getDayCount() + client->getEnv().getTimeOfDayF());
	// Terra Firma: land fades into the air's own horizon
	if (space)
		sky->setFogColor(space->fog().value_or(video::SColor(0, 0, 0, 0))); // (no alpha: the sky's own)
	// Terra Firma: liquid lying in the world (sprint 81), drawn on its own
	if (!liquid_layer)
		liquid_layer = make_irr<liquids::Layer>(smgr, client->ndef(), shader_src);
	{
		// what pushes water aside (sprint 82): the player and the objects near
		std::vector<liquids::Mover> movers;
		LocalPlayer *me = client->getEnv().getLocalPlayer();
		movers.push_back({ 0, me->getPosition() / BS, 0.35f, 1.7f });
		std::vector<DistanceSortedActiveObject> near;
		client->getEnv().getActiveObjects(me->getPosition(), 32 * BS, near);
		for (const DistanceSortedActiveObject &o : near) {
			aabb3f box{ v3f(0) };
			if (o.obj->isLocalPlayer() || !o.obj->getSelectionBox(&box))
				continue;
			const v3f at = o.obj->getPosition() / BS;
			movers.push_back({ u32(o.obj->getId()) + 1, at + v3f(0, box.MinEdge.Y / BS, 0),
					std::max(box.getExtent().X, box.getExtent().Z) / (2 * BS), box.getExtent().Y / BS });
		}
		ScopeProfiler water_timed(g_profiler, "Water all [us]", SPT_GRAPH_ADD, PRECISION_MICRO);
		liquid_layer->update(client->takeLiquidsChanged(), client->takeLiquidGround(), client->takeStreams(), movers,
				client->getEnv().getClientMap(), camera->getOffset(), camera->getPosition() / BS,
				client->getHorizon() ? std::optional<s16>(client->getHorizon()->water_level) : std::nullopt,
				client->getEnv().getDayNightRatio(), dtime, player->getSpeed().getLength() / BS > 30);
	}
	if (far_view) {
		std::optional<float> fog_limit;
		if (sky->getFogDistance() >= 0)
			fog_limit = sky->getFogDistance();
		far_view->update(*camera, client->getEnv().getClientMap(), draw_control->wanted_range,
				draw_control->range_all, fog_limit, player->getSpeed() / BS);
		if (far_view->showing())
			runData.fog_range = far_view->reach();
	}

	/*
		Calculate general brightness
	*/
	u32 daynight_ratio = client->getEnv().getDayNightRatio();
	float time_brightness = decode_light_f((float)daynight_ratio / 1000.0f);
	float direct_brightness;
	bool sunlight_seen;

	// When in noclip mode force same sky brightness as above ground so you
	// can see properly
	bool noclip_fly = draw_control->allow_noclip &&
			m_cache_enable_free_move &&
			client->checkPrivilege("fly");
	if (!sky->getAutoCaveBrightness() || noclip_fly) {
		direct_brightness = time_brightness;
		sunlight_seen = true;
	} else {
		float old_brightness = sky->getBrightness();
		direct_brightness = client->getEnv().getClientMap()
				.getBackgroundBrightness(MYMIN(runData.fog_range * 1.2, 60 * BS),
						daynight_ratio, (int)(old_brightness * 255.5), &sunlight_seen)
				/ 255.0;
	}

	float time_of_day_smooth = runData.time_of_day_smooth;
	float time_of_day = client->getEnv().getTimeOfDayF();

	static const float maxsm = 0.05f;
	static const float todsm = 0.05f;

	if (std::fabs(time_of_day - time_of_day_smooth) > maxsm &&
			std::fabs(time_of_day - time_of_day_smooth + 1.0) > maxsm &&
			std::fabs(time_of_day - time_of_day_smooth - 1.0) > maxsm)
		time_of_day_smooth = time_of_day;

	if (time_of_day_smooth > 0.8 && time_of_day < 0.2)
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm)
				+ (time_of_day + 1.0) * todsm;
	else
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm)
				+ time_of_day * todsm;

	runData.time_of_day_smooth = time_of_day_smooth;

	// Terra Firma: the date the smoothed time belongs to, which lags across midnight
	const float smooth_lag = time_of_day_smooth - time_of_day;
	sky->setDay(client->getEnv().getDayCount() + (smooth_lag > 0.5f ? -1 : smooth_lag < -0.5f ? 1 : 0));
	sky->update(time_of_day_smooth, time_brightness, direct_brightness,
			sunlight_seen, camera->getCameraMode(), player->getYaw(),
			player->getPitch());

	/*
		Update clouds
	*/
	updateClouds(dtime);

	/*
		Update particles
	*/
	client->getParticleManager()->step(dtime);

	/*
		Damage camera tilt
	*/
	if (player->hurt_tilt_timer > 0.0f) {
		player->hurt_tilt_timer -= dtime * 6.0f;

		if (player->hurt_tilt_timer < 0.0f)
			player->hurt_tilt_strength = 0.0f;
	}

	/*
		Update minimap pos and rotation
	*/
	if (mapper && m_game_ui->m_flags.show_hud) {
		mapper->setPos(floatToInt(player->getPosition(), BS));
		mapper->setAngle(player->getYaw());
	}

	/*
		Get chat messages from client
	*/

	updateChat(dtime);

	/*
		Inventory
	*/

	if (player->getWieldIndex() != runData.new_playeritem)
		client->setPlayerItem(runData.new_playeritem);

	if (client->updateWieldedItem()) {
		// Update wielded tool
		ItemStack selected_item, hand_item;
		ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

		bool skip_anim = client->consumeSkipNextWieldAnimation();
		camera->wield(tool_item, !skip_anim);
	}

	/*
		Update block draw list every 200ms or when camera direction has
		changed much
	*/
	runData.update_draw_list_timer += dtime;
	runData.touch_blocks_timer += dtime;

	constexpr float update_draw_list_delta = 0.2f;
	constexpr float touch_mapblock_delta = 4.0f;

	v3f camera_direction = camera->getDirection();

	auto &client_map = client->getEnv().getClientMap();

	// call only one of updateDrawList, touchMapBlocks, or updateShadow per frame
	// (the else-ifs below are intentional)
	if (runData.update_draw_list_timer >= update_draw_list_delta
			|| runData.update_draw_list_last_cam_dir.getDistanceFrom(camera_direction) > 0.2f
			|| m_camera_offset_changed
			|| client_map.needsUpdateDrawList()) {
		client_map.updateDrawList();
		runData.update_draw_list_timer = 0;
		runData.update_draw_list_last_cam_dir = camera_direction;
	} else if (runData.touch_blocks_timer > touch_mapblock_delta) {
		client_map.touchMapBlocks();
		runData.touch_blocks_timer = 0;
	} else if (RenderingEngine::get_shadow_renderer()) {
		updateShadows();
	}

	m_game_ui->update(*stats, client, draw_control, cam, runData.pointed_old,
			gui_chat_console.get(), dtime);

	m_game_formspec.update();

	/*
		==================== Drawing begins ====================
	*/
	if (device->isWindowVisible())
		drawScene(graph, stats);
	/*
		==================== End scene ====================
	*/

	// Damage flash is drawn in drawScene, but the timing update is done here to
	// keep dtime out of the drawing code.
	if (runData.damage_flash > 0.0f) {
		runData.damage_flash -= 384.0f * dtime;
	}

	g_profiler->avg("Game::updateFrame(): update frame [ms]", tt_update.stop(true));
}

void Game::updateClouds(float dtime)
{
	if (this->sky->getCloudsVisible()) {
		this->clouds->setVisible(true);
		this->clouds->step(dtime);
		// this->camera->getPosition is not enough for third-person camera.
		v3f camera_node_position = this->camera->getCameraNode()->getPosition();
		v3s16 camera_offset      = this->camera->getOffset();
		camera_node_position.X   = camera_node_position.X + camera_offset.X * BS;
		camera_node_position.Y   = camera_node_position.Y + camera_offset.Y * BS;
		camera_node_position.Z   = camera_node_position.Z + camera_offset.Z * BS;
		this->clouds->update(camera_node_position, this->sky->getCloudColor());
		if (this->clouds->isCameraInsideCloud() && this->fogEnabled()) {
			// If camera is inside cloud and fog is enabled, use cloud's colors as sky colors.
			video::SColor clouds_dark = this->clouds->getColor().getInterpolated(
					video::SColor(255, 0, 0, 0), 0.9);
			this->sky->overrideColors(clouds_dark, this->clouds->getColor());
			this->sky->setInClouds(true);
			this->runData.fog_range = std::fmin(this->runData.fog_range * 0.5f, 32.0f * BS);
			// Clouds are not drawn in this case.
			this->clouds->setVisible(false);
		}
	} else {
		this->clouds->setVisible(false);
	}
}

/* Log times and stuff for visualization */
inline void Game::updateProfilerGraphs(ProfilerGraph *graph)
{
	Profiler::GraphValues values;
	g_profiler->graphPop(values);
	graph->put(values);
}

/****************************************************************************
 * Shadows
 *****************************************************************************/
void Game::updateShadows()
{
	ShadowRenderer *shadow = RenderingEngine::get_shadow_renderer();
	if (!shadow)
		return;

	const auto &lighting = client->getEnv().getLocalPlayer()->getLighting();
	shadow->setShadowTint(lighting.shadow_tint);

	const float offset_constant = 10000.0f;

	v3f light;
	if (const std::optional<planetarium::Sky::Light> real = space ? space->light() : std::nullopt) {
		// Terra Firma: the stock sun's hidden on a planet; the real star casts them
		shadow->setShadowIntensity(real->strength);
		light = real->way;
	} else if (lighting.shadow_direction.getLengthSQ() > 0.0f) {
		// Custom shadow direction: bypass sun/moon visibility check
		shadow->setShadowIntensity(lighting.shadow_intensity);
		light = lighting.shadow_direction;
	} else {
		bool is_day = sky->getSunDirection().Y > 0.0f;
		bool is_shadow_visible = is_day ? sky->getSunVisible() : sky->getMoonVisible();
		shadow->setShadowIntensity(is_shadow_visible ? lighting.shadow_intensity : 0.0f);
		light = is_day ? sky->getSunDirection() : sky->getMoonDirection();
	}

	v3f sun_pos = light * offset_constant;
	shadow->getDirectionalLight().setDirection(sun_pos);
	shadow->setTimeOfDay(std::fmod(sky->getLightTimeOfDay(), 1.0f));

	shadow->getDirectionalLight().updateFrustum(camera, client);
}

void Game::drawScene(ProfilerGraph *graph, RunStats *stats)
{
	ZoneScoped;

	const video::SColor fog_color = this->sky->getFogColor();
	const video::SColor sky_color = this->sky->getSkyColor();

	/*
		Fog
	*/
	if (this->fogEnabled()) {
		this->driver->setFog(
				fog_color,
				video::EFT_FOG_LINEAR,
				this->runData.fog_range * this->sky->getFogStart(),
				this->runData.fog_range * 1.0f,
				0.f, // unused
				false, // pixel fog
				true // range fog
		);
	} else {
		this->driver->setFog(
				fog_color,
				video::EFT_FOG_LINEAR,
				FOG_RANGE_ALL,
				FOG_RANGE_ALL + 100 * BS,
				0.f, // unused
				false, // pixel fog
				false // range fog
		);
	}

	/*
		Drawing
	*/
	TimeTaker tt_draw("Draw scene", nullptr, PRECISION_MICRO);
	this->driver->beginScene(true, true, sky_color);

	const LocalPlayer *player = this->client->getEnv().getLocalPlayer();
	bool draw_wield_tool = (this->m_game_ui->m_flags.show_hud &&
			(player->hud_flags & HUD_FLAG_WIELDITEM_VISIBLE) &&
			(this->camera->getCameraMode() == CAMERA_MODE_FIRST) &&
			!this->camera->hasShot());
	bool draw_crosshair = (
			(player->hud_flags & HUD_FLAG_CROSSHAIR_VISIBLE) &&
			(this->camera->getCameraMode() != CAMERA_MODE_THIRD_FRONT));

	if (isTouchShootlineUsed())
		draw_crosshair = false;

#if USE_SHELL
	m_shell_session->update({ this->m_game_ui->m_flags.show_hud, draw_crosshair && !m_cursor,
		m_hold.has_value(), m_cursor, m_naming.has_value() });
	if (m_online_host) {
		m_online_host->step();
		if (std::optional<online::Endpoint> endpoint = m_online_host->changed())
			g_shell->social().hosting(online::toJson(*endpoint));
	}
#endif

	if (m_planetarium_closing)
		closePlanetarium();
#if USE_SHELL
	if (planetariumShown())
		m_shell_session->planetarium().draw(this->driver); // Terra Firma: over the whole window
	else
#endif
		this->m_rendering_engine->draw_scene(sky_color, this->m_game_ui->m_flags.show_hud,
				draw_wield_tool, draw_crosshair);

	/*
		Profiler graph
	*/
	v2u32 screensize = this->driver->getScreenSize();

	if (this->m_game_ui->m_flags.show_profiler_graph) {
		auto font = g_fontengine->getFont(
			g_fontengine->getDefaultFontSize() * 0.9f, FM_Mono);
		graph->draw(10, screensize.Y - 10, driver, font);
	}

	/*
		Damage flash
	*/
	if (this->runData.damage_flash > 0.0f) {
		video::SColor color(this->runData.damage_flash, 180, 0, 0);
		this->driver->draw2DRectangle(color,
					core::rect<s32>(0, 0, screensize.X, screensize.Y),
					NULL);
	}

#if USE_SHELL
	if (g_shell)
		g_shell->frame();
#endif

	// Must be called immediately before endScene() to capture the rendered frame
	this->client->takeScreenshotIfRequested();

	this->driver->endScene();

	stats->drawtime = tt_draw.stop(true);
	g_profiler->graphAdd("Draw scene [us]", stats->drawtime);

}

/****************************************************************************
 Misc
 ****************************************************************************/

void Game::showOverlayMessage(const char *msg, float dtime, int percent, float *indef_pos)
{
	m_rendering_engine->draw_load_screen(wstrgettext(msg), guienv, texture_src,
			dtime, percent, indef_pos);
}

void Game::settingChangedCallback(const std::string &setting_name, void *data)
{
	((Game *)data)->readSettings();
}

void Game::readSettings()
{
	// Terra Firma: what the world controls change (only what did: F3 has its say too)
	for (const physics::TuningKnob &knob : physics::TUNING_KNOBS)
		physics::tuning().*knob.field = g_settings->getFloat(knob.setting);
	if (hud && draw_control)
		applyView();

	LogLevel chat_log_level = Logger::stringToLevel(g_settings->get("chat_log_level"));
	if (chat_log_level == LL_MAX) {
		warningstream << "Supplied unrecognized chat_log_level; showing none." << std::endl;
		chat_log_level = LL_NONE;
	}
	m_chat_log_buf.setLogLevel(chat_log_level);

	m_cache_doubletap_jump               = g_settings->getBool("doubletap_jump");
	m_cache_toggle_sneak_key             = g_settings->getBool("toggle_sneak_key");
	m_cache_toggle_aux1_key              = g_settings->getBool("toggle_aux1_key");
	m_cache_enable_fog                   = g_settings->getBool("enable_fog");
	m_cache_mouse_sensitivity            = g_settings->getFloat("mouse_sensitivity", 0.001f, 10.0f);
	m_repeat_place_time                  = g_settings->getFloat("repeat_place_time", 0.16f, 2.0f);
	m_repeat_dig_time                    = g_settings->getFloat("repeat_dig_time", 0.0f, 2.0f);

	m_cache_enable_noclip                = g_settings->getBool("noclip");
	m_cache_enable_free_move             = g_settings->getBool("free_move");

	m_cache_cam_smoothing = 0;
	if (g_settings->getBool("cinematic"))
		m_cache_cam_smoothing = g_settings->getFloat("cinematic_camera_smoothing");
	else
		m_cache_cam_smoothing = g_settings->getFloat("camera_smoothing");

	m_cache_cam_smoothing = std::max(0.0f, m_cache_cam_smoothing);
	m_cache_mouse_sensitivity = rangelim(m_cache_mouse_sensitivity, 0.001, 100.0);

	m_invert_mouse = g_settings->getBool("invert_mouse");
	m_enable_hotbar_mouse_wheel = g_settings->getBool("enable_hotbar_mouse_wheel");
	m_invert_hotbar_mouse_wheel = g_settings->getBool("invert_hotbar_mouse_wheel");

	m_does_lost_focus_pause_game = g_settings->getBool("pause_on_lost_focus");
}

/****************************************************************************/
/****************************************************************************
 extern function for launching the game
 ****************************************************************************/
/****************************************************************************/

void the_game(volatile std::sig_atomic_t *kill,
		InputHandler *input,
		RenderingEngine *rendering_engine,
		GameStartData &start_data,
		GameErrorData &errordata,
		ChatBackend &chat_backend)
{
	Game game;
	std::string error_message;

	try {

		if (game.startup(kill, input, rendering_engine, start_data,
				errordata, &chat_backend)) {
			game.run();
		}

	} catch (SerializationError &e) {
		const std::string ver_err = fmtgettext("The server is probably running a different version of %s.", PROJECT_NAME_C);
		error_message = strgettext("A serialization error occurred:") +"\n"
				+ e.what() + "\n\n" + ver_err;
	} catch (ServerError &e) {
		error_message = "ServerError: ";
		error_message.append(e.what());
	} catch (ModError &e) {
		// DO NOT TRANSLATE the `ModError`, it's used by `ui.lua`
		error_message = std::string("ModError: ") + e.what() +
				strgettext("\nCheck debug.txt for details.");
	} catch (con::PeerNotFoundException &e) {
		error_message = gettext("Connection error (timed out?)");
	} catch (ShaderException &e) {
		error_message = e.what();
	} catch (SocketException &e) {
		error_message = e.what();
	}

	if (!error_message.empty()) {
		errorstream << error_message << std::endl;
		errordata.message = std::move(error_message);
	}

	game.shutdown();
}
