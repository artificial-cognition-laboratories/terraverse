// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/app/headless/eye.h"
#include "src/client/app/headless/link.h"
#include "src/client/app/headless/motor.h"
#include "src/client/app/headless/senses.h"
#include <csignal>
#include <memory>
#include <string>

class Camera;
class Client;
class GameGlobalShaderUniformSetterFactory;
class EventManager;
class ISoundManager;
class IWritableItemDefManager;
class IWritableShaderSource;
class IWritableTextureSource;
class MyEventReceiver;
class NodeDefManager;
class RenderingEngine;
struct ItemVisualsManager;
struct MapDrawControl;

namespace headless {

/*
	A body (sprint 126): the game's client with no window, worn by a mind through terra. It joins
	a server on this machine as a player, by the run's secret (online/local.h), and moves by the
	same code a person's player does: the agent's commands become a PlayerControl, as keys and a
	mouse do in Game. Nothing is drawn: the video driver is the NULL one, there's no Chromium and
	no sound. What it senses and what it was told go over the Link, a JSON line each.

	`luanti --body --address <host> --port <n> --name <name> [--vision WxH]`, the secret in
	TERRA_LOCAL_SECRET. With vision it draws as the game does, into a window nobody sees (Eye).
*/
class Headless
{
public:
	struct Opts
	{
		std::string address;
		unsigned short port;
		std::string name;
		// The run's secret: what the server's gate admits a body by.
		std::string secret;
		// Its eye's picture size, when it sees; none for a blind body (the NULL driver, nothing drawn).
		std::optional<Eye::Size> vision;
	};

	// Connected, its content received and its player in the world; or a thrown reason.
	static std::unique_ptr<Headless> boot(const Opts &o, volatile std::sig_atomic_t *kill);
	~Headless();

	// Worn until terra lets it go, the server drops it or the process is told to stop: its exit code.
	int run(volatile std::sig_atomic_t *kill);

private:
	explicit Headless(const Opts &o);
	void connect(const Opts &o, volatile std::sig_atomic_t *kill);
	// One message from terra; false when it says to leave.
	bool heard(const Json::Value &message, unsigned long long now);
	// What the client said since last step: a teleport turns the view, death respawns it.
	void events();
	// What the body is, said once it's in the world.
	Json::Value hello() const;

	std::unique_ptr<MyEventReceiver> m_receiver;
	std::unique_ptr<RenderingEngine> m_engine;
	std::unique_ptr<IWritableTextureSource> m_textures;
	std::unique_ptr<IWritableShaderSource> m_shaders;
	std::unique_ptr<IWritableItemDefManager> m_items;
	std::unique_ptr<NodeDefManager> m_nodes;
	std::unique_ptr<ItemVisualsManager> m_item_visuals;
	std::unique_ptr<EventManager> m_events;
	std::unique_ptr<ISoundManager> m_sound;
	std::unique_ptr<MapDrawControl> m_draw;
	std::unique_ptr<Client> m_client;
	std::unique_ptr<Camera> m_camera;
	std::unique_ptr<Eye> m_eye;
	Link m_link;
	Motor m_motor;
	std::unique_ptr<Senses> m_senses;
};

// --body: boot a body from the command line's address, port and name (and its eye's size, `WxH`,
// when it sees), run it, and its exit code.
int runBody(const std::string &address, unsigned short port, const std::string &name, const std::string &vision);

} // namespace headless
