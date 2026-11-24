// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/headless/headless.h"
#include "src/client/app/headless/rig.h"
#include "src/client/views/game/view/world_view.h"
#include "src/client/app/clientevent.h"
#include "src/client/app/input/event_manager.h"
#include "src/client/app/input/inputhandler.h"
#include "src/client/app/output/sound/sound.h"
#include "src/client/render/items/item_visuals_manager.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/render/renderingengine.h"
#include "src/client/render/shaders/shader.h"
#include "src/client/render/shaders/world_uniforms.h"
#include "src/shared/platform/system/filesys.h"
#include "src/client/render/textures/texturesource.h"
#include "src/client/views/game/camera.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/world/session/client.h"
#include "src/client/world/session/clientenvironment.h"
#include "src/shared/base/constants.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/content/particles.h"
#include "src/shared/game/environment/skyparams.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/platform/network/address.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/util/text/string.h"
#include "src/shared/platform/system/porting.h"
#include <chrono>
#include <cstdio>
#ifdef _WIN32
	#include <process.h>
	#define getpid _getpid
#else
	#include <unistd.h>
#endif
#include <cstdlib>
#include <thread>

namespace headless {

namespace {

// How often a body steps, s: a client's frame, at a person's 60.
constexpr float STEP = 1.0f / 60;
// How far round it a body has the world, nodes: what it senses, and room to move into.
constexpr float REACHED = 32;
// How far a seeing body has the world, nodes.
constexpr float SEEN = 96;
// The crack texture's frames, for the shaders (Game counts crack_anylength.png's; a body digs unseen).
constexpr int CRACK_FRAMES = 5;
// How long the server has to let it in, and then to send its content, s.
constexpr float CONNECTING = 10, RECEIVING = 120;

void discard(ClientEvent &event);
std::string pictures(const std::string &name);

} // namespace

std::unique_ptr<Headless> Headless::boot(const Opts &o, volatile std::sig_atomic_t *kill)
{
	if (o.vision) {
		// a GL context by EGL, with no display: SDL's offscreen driver, a window nobody sees
#ifdef _WIN32
		_putenv_s("SDL_VIDEODRIVER", "offscreen");
#else
		setenv("SDL_VIDEODRIVER", "offscreen", 1);
#endif
		g_settings->set("video_driver", "opengl3");
		g_settings->setU16("screen_w", o.vision->width);
		g_settings->setU16("screen_h", o.vision->height);
		g_settings->setBool("fullscreen", false);
		g_settings->setBool("autosave_screensize", false);
	} else {
		// blind: the NULL video driver composes what it must on the CPU, and draws nothing
		g_settings->set("video_driver", "null");
	}
	g_settings->setBool("enable_minimap", false);
	g_settings->setBool("enable_sound", false);
	std::unique_ptr<Headless> body(new Headless(o));
	body->connect(o, kill);
	return body;
}

Headless::Headless(const Opts &o) :
	m_receiver(std::make_unique<MyEventReceiver>()),
	m_engine(std::make_unique<RenderingEngine>(m_receiver.get())),
	m_textures(createTextureSource()),
	m_shaders(createShaderSource()),
	m_items(createItemDefManager()),
	m_nodes(createNodeDefManager()),
	m_item_visuals(std::make_unique<ItemVisualsManager>()),
	m_events(std::make_unique<EventManager>()),
	m_sound(std::make_unique<DummySoundManager>()),
	m_draw(std::make_unique<MapDrawControl>())
{
	// what it asks the server for: the world near it, not a person's view to the horizon
	m_draw->wanted_range = REACHED;
}

Headless::~Headless()
{
	if (m_client) {
		m_client->Stop();
		while (!m_client->isShutdown())
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	// the client before what it was handed, as Game has it
	m_senses.reset();
	m_eye.reset();
	m_client.reset();
	m_camera.reset();
	if (m_engine)
		m_engine->finalize();
}

void Headless::connect(const Opts &o, volatile std::sig_atomic_t *kill)
{
	Address address(0, 0, 0, 0, o.port);
	address.Resolve(o.address.c_str());
	m_client = std::make_unique<Client>(o.name.c_str(), o.secret, *m_draw, m_textures.get(), m_shaders.get(),
			m_items.get(), m_nodes.get(), m_sound.get(), m_events.get(), m_engine.get(), m_item_visuals.get());
	m_client->connect(address, o.address);

	// let in, or refused with the gate's reason
	float waited = 0;
	while (m_client->getState() < LC_Init) {
		if (*kill)
			throw BaseException("body: stopped while connecting");
		if (m_client->accessDenied())
			throw BaseException("body: refused: " + m_client->accessDeniedReason());
		if (waited > CONNECTING)
			throw BaseException("body: " + o.address + ":" + std::to_string(o.port) + " didn't answer");
		m_client->step(STEP);
		std::this_thread::sleep_for(std::chrono::duration<float>(STEP));
		waited += STEP;
	}
	waited = 0;
	while (!(m_client->mediaReceived() && m_client->itemdefReceived() && m_client->nodedefReceived())) {
		if (*kill)
			throw BaseException("body: stopped while receiving the server's content");
		if (m_client->accessDenied() || m_client->getState() < LC_Init)
			throw BaseException("body: the server let it go: " + m_client->accessDeniedReason());
		if (waited > RECEIVING)
			throw BaseException("body: the server's content didn't come");
		m_client->step(STEP);
		std::this_thread::sleep_for(std::chrono::duration<float>(STEP));
		waited += STEP;
	}
	// the world's shaders, as Game adds them (render/shaders/world_uniforms.h)
	GameGlobalShaderUniformSetterFactory *uniforms = nullptr;
	if (o.vision) {
		m_shaders->addShaderConstantSetter(createNodeShaderConstantSetter());
		auto made = std::make_unique<GameGlobalShaderUniformSetterFactory>(m_client.get(), CRACK_FRAMES);
		uniforms = made.get();
		m_shaders->addShaderUniformSetterFactory(std::move(made));
		m_shaders->addShaderUniformSetterFactory(std::make_unique<FogShaderUniformSetterFactory>());
		ShadowRenderer::preInit(m_shaders.get()); // as Game, before the content's shaders are made
		// a seeing body has the world as far as it can see, not only round it
		m_draw->wanted_range = SEEN;
	}
	m_client->afterContentReceived();
	m_camera = std::make_unique<Camera>(*m_draw, m_client.get(), m_engine.get());
	m_client->setCamera(m_camera.get());
	if (o.vision)
		m_eye = std::make_unique<Eye>(Eye::Opts{ /* client */ *m_client, /* engine */ *m_engine, /* shaders */ *m_shaders,
			/* textures */ *m_textures, /* camera */ *m_camera, /* uniforms */ *uniforms,
			/* dir */ pictures(o.name), /* size */ *o.vision, /* draw */ *m_draw });
	m_senses = std::make_unique<Senses>(Senses::Opts{ /* client */ *m_client });
}

int Headless::run(volatile std::sig_atomic_t *kill)
{
	using clock = std::chrono::steady_clock;
	const auto started = clock::now();
	auto next = started;
	bool said_hello = false;
	for (;;) {
		next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(STEP));
		std::this_thread::sleep_until(next);
		const unsigned long long now = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - started).count();
		if (*kill || m_link.closed())
			return 0;
		while (std::optional<Json::Value> message = m_link.take())
			if (!heard(*message, now))
				return 0;
		if (m_client->accessDenied() || m_client->getState() < LC_Init) {
			Json::Value gone;
			gone["t"] = "gone";
			gone["why"] = m_client->accessDeniedReason().empty() ? "the server dropped it" : m_client->accessDeniedReason();
			m_link.say(gone);
			return 1;
		}
		events();
		LocalPlayer *player = m_client->getEnv().getLocalPlayer();
		m_motor.turn(player->takePlanetTurn());
		// what the client's threads asked of the main thread: textures and shaders made here (Game::processQueues)
		m_textures->processQueue();
		m_shaders->processQueue();
		const Motor::Step step = m_motor.step(STEP, now);
		PlayerControl control = step.control;
		m_client->setPlayerControl(control);
		m_client->step(STEP);

		// in the world once the server's placed it and sent its own entity: then it's somebody
		if (!said_hello) {
			if (m_client->getState() != LC_Ready || !player->getCAO())
				continue;
			m_link.say(hello());
			said_hello = true;
		}
		for (const Motor::Applied &a : step.applied) {
			Json::Value applied;
			applied["t"] = "applied";
			applied["ch"] = a.channel;
			applied["seq"] = Json::UInt64(a.seq);
			applied["at"] = Json::UInt64(now);
			for (float v : a.values)
				applied["v"].append(v);
			m_link.say(applied);
		}
		for (Json::Value &reading : m_senses->read(STEP, now, m_motor.yaw(), m_motor.pitch())) {
			reading["t"] = "read";
			m_link.say(reading);
		}
		if (m_eye)
			if (std::optional<Json::Value> seen = m_eye->see(now, STEP)) {
				(*seen)["t"] = "read";
				m_link.say(*seen);
			}
	}
}

bool Headless::heard(const Json::Value &message, unsigned long long now)
{
	const std::string kind = message["t"].asString();
	if (kind == "leave")
		return false;
	if (kind == "say") {
		m_client->sendChatMessage(utf8_to_wide(message["text"].asString()));
		return true;
	}
	if (kind == "cmd") {
		std::vector<float> values;
		for (const Json::Value &v : message["v"])
			values.push_back(v.asFloat());
		const std::string channel = message["ch"].asString();
		if (const std::optional<std::string> why = m_motor.accept(channel, message["seq"].asUInt64(), values, now)) {
			Json::Value refused;
			refused["t"] = "refused";
			refused["ch"] = channel;
			refused["seq"] = message["seq"];
			refused["why"] = *why;
			m_link.say(refused);
		}
		return true;
	}
	throw BaseException("body: terra sent a message it has no word for: " + kind);
}

void Headless::events()
{
	while (m_client->hasClientEvents()) {
		std::unique_ptr<ClientEvent> event(m_client->getClientEvent());
		switch (event->type) {
		case CE_PLAYER_FORCE_MOVE:
			m_motor.face(event->player_force_move.yaw, event->player_force_move.pitch);
			break;
		case CE_DEATHSCREEN_LEGACY:
			// a body gets up again at once: its death is in /interoception, and the respawn is the server's
			m_client->sendRespawnLegacy();
			break;
		default:
			// the sky's events: drawn by a seeing body's eye; for a blind one only the light they set
			if (m_eye && m_eye->heard(*event))
				break;
			WorldView::light(*m_client, *event);
			discard(*event);
			break;
		}
	}
}

Json::Value Headless::hello() const
{
	const LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	Json::Value hello;
	hello["t"] = "hello";
	hello["name"] = player->getName();
	hello["step"] = STEP * 1000;
	// its skeleton: the figure the game draws, resting (rig.h); how it's posed comes on /joints
	hello["frames"] = rig::frames(physics::Skeleton{}.place(0, physics::Joints{}, 0, nullptr));
	hello["senses"] = Senses::declared();
	if (m_eye)
		hello["senses"].append(m_eye->declared());
	hello["controls"] = Motor::declared();
	return hello;
}

int runBody(const std::string &address, unsigned short port, const std::string &name, const std::string &vision)
{
	std::optional<Eye::Size> size;
	if (!vision.empty()) {
		unsigned w = 0, h = 0;
		if (std::sscanf(vision.c_str(), "%ux%u", &w, &h) != 2 || w < 16 || h < 16 || w > 4096 || h > 4096)
			throw BaseException("body: --vision is WxH, 16 to 4096 each, not " + vision);
		size = Eye::Size{ w, h };
	}
	const char *secret = std::getenv("TERRA_LOCAL_SECRET");
	if (!secret || !*secret)
		throw BaseException("body: no run secret (TERRA_LOCAL_SECRET): only terra starts a body");
	volatile std::sig_atomic_t *kill = porting::signal_handler_killstatus();
	std::unique_ptr<Headless> body = Headless::boot({ address, port, name, secret, size }, kill);
	return body->run(kill);
}

namespace {

// Its own folder for its pictures: shared memory where there is some, so they're never on a disk.
std::string pictures(const std::string &name)
{
	const std::string base = fs::IsDir("/dev/shm") ? "/dev/shm" : fs::TempPath();
	return base + DIR_DELIM "terraverse-bodies" DIR_DELIM + name + "-" + std::to_string(getpid());
}

// What a ClientEvent owns: a body acts on none of these, so each is let go.
void discard(ClientEvent &event)
{
	switch (event.type) {
	case CE_SHOW_FORMSPEC:
		delete event.show_formspec.formspec;
		delete event.show_formspec.formname;
		break;
	case CE_SPAWN_PARTICLE:
		delete event.spawn_particle;
		break;
	case CE_ADD_PARTICLESPAWNER:
		delete event.add_particlespawner.p;
		break;
	case CE_HUDADD:
		delete event.hudadd;
		break;
	case CE_HUDCHANGE:
		delete event.hudchange;
		break;
	case CE_SET_SKY:
		delete event.set_sky;
		break;
	case CE_SET_SUN:
		delete event.sun_params;
		break;
	case CE_SET_MOON:
		delete event.moon_params;
		break;
	case CE_SET_STARS:
		delete event.star_params;
		break;
	default:
		break;
	}
}

} // namespace

} // namespace headless
