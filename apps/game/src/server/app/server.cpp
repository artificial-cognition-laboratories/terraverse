// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "src/shared/platform/release/release.h" // Terra Firma
#include "src/shared/base/version.h" // Terra Firma
#include "src/server/app/server.h"
#include "src/server/app/world_lock.h"
#include "library/trace/marks.h"
#include "src/server/world/run/machines/workshop/workshop.h"
#include "library/space/orbit/field.h"
#include "library/world/terragen/src/system/planet.h"
#include "src/server/game/command_book.h"
#include "src/server/app/access/access.h"
#include "src/server/world/state/objects/entity_sao.h"
#include "src/server/world/run/pile/piles.h"

#include "src/shared/game/objects/activeobject.h"
#include "src/shared/game/chat/chat_interface.h"
#include "src/shared/game/chat/chatmessage.h"
#include "src/shared/base/config.h"
#include "src/shared/base/constants.h"
#include "src/shared/game/content/content_nodemeta.h"
#include "src/shared/game/items/craftdef.h"
#include "src/shared/game/environment/environment.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/base/gameparams.h"
#include "src/shared/platform/system/gettext.h"
#include "src/shared/base/irr_v2d.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/content/particles.h"
#include "src/shared/platform/system/profiler.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/server/app/access/ban.h"
#include "src/shared/platform/online/gate.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/server/world/state/servermap.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/rollback.h"
#include "src/server/world/state/objects/serveractiveobject.h"
#include "src/server/world/state/objects/serverinventorymgr.h"
#include "src/server/app/serverlist.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/platform/system/translation.h"
#include "src/shared/util/text/base64.h"
#include "src/shared/util/hash/hashing.h"
#include "src/shared/util/text/hex.h"
#include "src/shared/util/data/serialize.h"
#include "src/shared/util/text/string.h"
#include "src/shared/util/data/thread.h"
#include "src/shared/util/timing/tracy_wrapper.h"
#include "src/shared/base/version.h"

// Mapgen
#include "src/server/world/load/emerge.h"
#include "src/server/world/generate/mapgen.h"
#include "src/server/world/generate/luanti/mg_biome.h"
#include "src/server/world/generate/mapgen_terra.h"
#include "src/client/views/game/far/land/horizon.h"

// Modding
#include "src/server/app/mods/sidecar/sidecar.h"
#include "src/server/app/mods/sidecar/terra.h"
#include "src/server/app/mods/sidecar/ledger.h"
#include "src/server/game/registry.h" // Terra Firma
#include "tests/probe/probe.h" // Terra Firma
#include "src/server/game/interact.h" // Terra Firma

// Network
#include "src/shared/platform/network/connection.h"
#include "src/shared/platform/network/networkexceptions.h"
#include "src/shared/platform/network/networkpacket.h"
#include "src/shared/platform/network/networkprotocol.h"
#include "src/server/app/network/serveropcodes.h"
#include "src/shared/base/serialization.h" // SER_FMT_VER_INVALID

// Database
#include "src/server/world/load/database/database.h"
#include "src/server/world/load/database/database-sqlite3.h"
#if USE_POSTGRESQL
#include "src/server/world/load/database/database-postgresql.h"
#endif
#include "src/server/world/load/database/database-files.h"
#include "src/server/world/load/database/database-dummy.h"

#include "src/server/world/terra.h" // Terra Firma
#include "src/server/world/run/rules/matter.h" // Terra Firma
#include <iostream>
#include <queue>
#include <algorithm>
#include <sstream>
#include <csignal>

class ClientNotFoundException : public BaseException
{
public:
	ClientNotFoundException(const char *s):
		BaseException(s)
	{}
};

class ServerThread : public Thread
{
public:

	ServerThread(Server *server):
		Thread("Server"),
		m_server(server)
	{}

	void *run();

private:
	Server *m_server;
};

void *ServerThread::run()
{
	ZoneScoped;

	BEGIN_DEBUG_EXCEPTION_HANDLER

	/*
	 * The real business of the server happens on the ServerThread.
	 * How this works:
	 * AsyncRunStep() (which runs the actual server step) is called at the
	 * server-step frequency. Receive() is used for waiting between the steps.
	 */

	auto framemarker = FrameMarker("ServerThread::run()-frame").started();
	try {
		m_server->AsyncRunStep(0.0f, true);
	} catch (BaseException &e) {
		// Terra Firma: what the engine refuses stops the server, said (was Lua's errors)
		m_server->setAsyncFatalError(e.what());
	}
	framemarker.end();

	float dtime = 0.0f;

	while (!stopRequested()) {
		framemarker.start();
		ScopeProfiler spm(g_profiler, "Server::RunStep() (max)", SPT_MAX);

		u64 t0 = porting::getTimeUs();

		const auto step_settings = m_server->getStepSettings();

		try {
			// see explanation inside
			// (+1 ms, because we don't sleep more fine-grained)
			if (dtime > step_settings.steplen + 0.001f)
				m_server->yieldToOtherThreads(dtime);

			m_server->AsyncRunStep(step_settings.pause ? 0.0f : dtime);

			const float remaining_time = step_settings.steplen
					- 1e-6f * (porting::getTimeUs() - t0);
			m_server->Receive(remaining_time);

		} catch (con::PeerNotFoundException &e) {
			infostream<<"Server: PeerNotFoundException"<<std::endl;
		} catch (ClientNotFoundException &e) {
			infostream<<"Server: ClientNotFoundException"<<std::endl;
		} catch (BaseException &e) {
			m_server->setAsyncFatalError(e.what());
		}

		dtime = 1e-6f * (porting::getTimeUs() - t0);
		framemarker.end();
	}

	END_DEBUG_EXCEPTION_HANDLER

	return nullptr;
}

v3f ServerPlayingSound::getPos(ServerEnvironment *env, bool *pos_exists) const
{
	if (pos_exists)
		*pos_exists = false;

	switch (type ){
	case SoundLocation::Local:
		return v3f(0,0,0);
	case SoundLocation::Position:
		if (pos_exists)
			*pos_exists = true;
		return pos;
	case SoundLocation::Object:
		{
			if (object == 0)
				return v3f(0,0,0);
			ServerActiveObject *sao = env->getActiveObject(object);
			if (!sao)
				return v3f(0,0,0);
			if (pos_exists)
				*pos_exists = true;
			return sao->getBasePosition();
		}
	}

	return v3f(0,0,0);
}

void Server::ShutdownState::reset()
{
	m_timer = 0.0f;
	message.clear();
	should_reconnect = false;
	is_requested = false;
}

void Server::ShutdownState::trigger(float delay, const std::string &msg, bool reconnect)
{
	m_timer = delay;
	message = msg;
	should_reconnect = reconnect;
}

void Server::ShutdownState::tick(float dtime, Server *server)
{
	if (m_timer <= 0.0f)
		return;

	// Timed shutdown
	static const float shutdown_msg_times[] =
	{
		1, 2, 3, 4, 5, 10, 20, 40, 60, 120, 180, 300, 600, 1200, 1800, 3600
	};

	// Automated messages
	if (m_timer < shutdown_msg_times[ARRLEN(shutdown_msg_times) - 1]) {
		for (float t : shutdown_msg_times) {
			// If shutdown timer matches an automessage, shot it
			if (m_timer > t && m_timer - dtime < t) {
				std::wstring periodicMsg = getShutdownTimerMessage();

				infostream << wide_to_utf8(periodicMsg).c_str() << std::endl;
				server->SendChatMessage(PEER_ID_INEXISTENT, periodicMsg);
				break;
			}
		}
	}

	m_timer -= dtime;
	if (m_timer < 0.0f) {
		m_timer = 0.0f;
		is_requested = true;
	}
}

std::wstring Server::ShutdownState::getShutdownTimerMessage() const
{
	std::wstringstream ws;
	ws << L"*** Server shutting down in "
		<< duration_to_string(myround(m_timer)).c_str() << ".";
	return ws.str();
}

static void enrich_exception(BaseException &e, const NetworkPacket &pkt, bool include_pos)
{
	const u16 cmd = pkt.getCommand();
	std::ostringstream oss;
	if (cmd < TOSERVER_NUM_MSG_TYPES)
		oss << " name=" << toServerCommandTable[cmd].name;

	if (include_pos) {
		// (not necessary for PacketError: already in e.what())

		oss << " cmd=" << cmd
			<< " offset=" << pkt.getOffset()
			<< " size=" << pkt.getSize();
	}

	e.append(" @").append(oss.str());
}

/*
	Server
*/

// Terra Firma: the map blocks of a slot of the planet's window, low and
// high corners (every layer)
static std::pair<v3s16, v3s16> slotBlocks(int slot)
{
	const s32 x = planet::Atlas::ORIGIN + (slot % planet::Atlas::SLOTS) * planet::Atlas::SLOT;
	const s32 z = planet::Atlas::ORIGIN + (slot / planet::Atlas::SLOTS) * planet::Atlas::SLOT;
	const v3s16 low = getNodeBlockPos(v3s16(x, -MAX_MAP_GENERATION_LIMIT, z));
	const v3s16 high = getNodeBlockPos(v3s16(x + planet::Atlas::TILE - 1, MAX_MAP_GENERATION_LIMIT, z + planet::Atlas::TILE - 1));
	return { low, high };
}

Server::Server(
		const std::string &path_world,
		const SubgameSpec &gamespec,
		bool simple_singleplayer_mode,
		UDPSocket &&socket,
		bool dedicated,
		ChatInterface *iface,
		std::string *shutdown_errmsg
	):
	m_bind_addr(socket.GetLocalAddress()),
	m_path_world(path_world),
	m_gamespec(gamespec),
	m_simple_singleplayer_mode(simple_singleplayer_mode),
	m_dedicated(dedicated),
	m_con(con::createMTP(/*is_server=*/true, std::move(socket), this)),
	m_itemdef(createItemDefManager()),
	m_nodedef(createNodeDefManager()),
	m_craftdef(createCraftDefManager()),
	m_thread(new ServerThread(this)),
	m_clients(m_con),
	m_admin_chat(iface),
	m_shutdown_errmsg(shutdown_errmsg),
	m_terra(std::make_unique<ServerTerra>()),
	m_commands(std::make_unique<CommandBook>(CommandBook::Opts{ /* server */ *this }))
{
	if (m_path_world.empty())
		throw ServerError("Supplied empty world path");

	if (!gamespec.isValid())
		throw ServerError("Supplied invalid gamespec");

#if USE_PROMETHEUS
	if (!simple_singleplayer_mode) {
		// Note: may return null
		m_metrics_backend.reset(createPrometheusMetricsBackend());
	}
#endif
	if (!m_metrics_backend)
		m_metrics_backend = std::make_unique<MetricsBackend>();

	m_uptime_counter = m_metrics_backend->addCounter("minetest_core_server_uptime", "Server uptime (in seconds)");
	m_player_gauge = m_metrics_backend->addGauge("minetest_core_player_number", "Number of connected players");

	m_timeofday_gauge = m_metrics_backend->addGauge(
			"minetest_core_timeofday",
			"Time of day value");

	m_lag_gauge = m_metrics_backend->addGauge(
			"minetest_core_latency",
			"Latency value (in seconds)");


	const std::string aom_types[] = {"reliable", "unreliable"};
	for (u32 i = 0; i < ARRLEN(aom_types); i++) {
		std::string help_str("Number of active object messages generated (");
		help_str.append(aom_types[i]).append(")");
		m_aom_buffer_counter[i] = m_metrics_backend->addCounter(
				"minetest_core_aom_generated_count", help_str,
				{{"type", aom_types[i]}});
	}

	m_packet_recv_counter = m_metrics_backend->addCounter(
			"minetest_core_server_packet_recv",
			"Processable packets received");

	m_packet_recv_processed_counter = m_metrics_backend->addCounter(
			"minetest_core_server_packet_recv_processed",
			"Valid received packets processed");

	m_map_edit_event_counter = m_metrics_backend->addCounter(
			"minetest_core_map_edit_events",
			"Number of map edit events");

	m_lag_gauge->set(g_settings->getFloat("dedicated_server_step"));

	m_path_mod_data = porting::path_user + DIR_DELIM "mod_data";
	if (!fs::CreateDir(m_path_mod_data))
		throw ServerError("Failed to create mod data dir");
}

Server::~Server()
{
	// Send shutdown message
	SendChatMessage(PEER_ID_INEXISTENT, ChatMessage(CHATMESSAGE_TYPE_ANNOUNCE,
			L"*** Server shutting down"));

	actionstream << "Server: Shutting down" << std::endl;
#if USE_SHELL
	// Terra Firma: each slow part of closing is named, so a slow leave says why
	u64 closing = porting::getTimeMs();
	const auto took = [&](const char *what) {
		const u64 now = porting::getTimeMs();
		if (now - closing > 200)
			actionstream << "Server: " << what << " took " << (now - closing) << " ms" << std::endl;
		closing = now;
	};
#else
	const auto took = [](const char *) {};
#endif

	auto old_async_fatal_error = m_async_fatal_error.get();

	// Stop server step from happening
	if (m_thread) {
		stop();
		// (Do not delete yet. Accessed by setAsyncFatalError().)
	}
	took("stopping its thread");

	// Stop all emerge activity and finish off mapgen callbacks. Do this before
	// shutdown callbacks since there may be state that is finalized in a
	// callback.
	// Note: The emerge manager is not deleted yet because further code can
	//       still interact with map loading.
	if (m_emerge)
		m_emerge->stopThreads();
	took("stopping map generation");

	if (m_env) {
		EnvAutoLock envlock(this);

		infostream << "Server: Saving players" << std::endl;
		m_env->saveLoadedPlayers();

		infostream << "Server: Kicking players" << std::endl;
		std::string kick_msg;
		bool reconnect = false;
		if (isShutdownRequested()) {
			reconnect = m_shutdown_state.should_reconnect;
			kick_msg = m_shutdown_state.message;
		}
		if (kick_msg.empty()) {
			kick_msg = g_settings->get("kick_msg_shutdown");
		}
		m_env->saveLoadedPlayers(true);
		kickAllPlayers(SERVER_ACCESSDENIED_SHUTDOWN,
			kick_msg, reconnect);

		m_env->deactivateBlocksAndObjects();
		took("saving players and emptying the world");

		infostream << "Server: Saving environment metadata" << std::endl;
		m_env->saveMeta();
		// Terra Firma: what's awake in the world's matter (sprint 119: was tf_matter's on_shutdown)
		try {
			serverMatterSave();
		} catch (const BaseException &e) {
			errorstream << e.what() << std::endl;
		}

		// Delete classes that depend on the environment
		m_piles.reset();
		m_workshop.reset();
		m_inventory_mgr.reset();
		m_sidecar.reset();
		serverMatterShutdown(); // Terra Firma: it points at this server
		took("closing its systems");

		// Note that this also deletes and saves the map.
		delete m_env;
		m_env = nullptr;
		took("saving the map");
	}

	// Clean up files
	for (auto &it : m_media) {
		if (it.second.delete_at_shutdown) {
			fs::DeleteSingleFileOrEmptyDirectory(it.second.path, true);
		}
	}

	// emerge may depend on definition managers, so destroy first
	m_emerge.reset();
	m_probe.reset(); // (after: what it asked emerged is answered to it)
	took("closing map generation");

	// Catch one async error that just happened while this dtor is running
	auto async_fatal_error = m_async_fatal_error.get();
	if (old_async_fatal_error != async_fatal_error) {
		errorstream << "Server: new AsyncErr during shutdown: " << async_fatal_error;
	}

	// Delete the rest in the reverse order of creation
	delete m_game_settings;
	delete m_banmanager;
	delete m_rollback;
	delete m_itemdef;
	delete m_nodedef;
	delete m_craftdef;
	delete m_thread;

	while (!m_unsent_map_edit_queue.empty()) {
		delete m_unsent_map_edit_queue.front();
		m_unsent_map_edit_queue.pop();
	}
}

void Server::init()
{
	trace::Marking marking("startup", "Server init"); // Terra Firma: the debugger's Startup waterfall
	infostream << "Server created for gameid \"" << m_gamespec.id << "\"";
	if (m_simple_singleplayer_mode)
		infostream << " in simple singleplayer mode" << std::endl;
	else
		infostream << std::endl;
	infostream << "- world:  " << m_path_world << std::endl;
	infostream << "- game:   " << m_gamespec.path << std::endl;
	// Terra Firma: one engine to a world, held from here, where it's opened (world_lock.h)
	m_world_lock = std::make_unique<WorldLock>(WorldLock::Opts{ /* world */ m_path_world });

	m_game_settings = Settings::createLayer(SL_GAME);

	// Create world if it doesn't exist
	try {
		loadGameConfAndInitWorld(m_path_world,
				fs::GetFilenameFromPath(m_path_world.c_str()),
				m_gamespec, false);
	} catch (const BaseException &e) {
		throw ServerError(std::string("Failed to initialize world: ") + e.what());
	}
	// Terra Firma: which release may open this world, and a copy before a new one does
	try {
		release::openWorld(m_path_world, g_version_hash, porting::path_user + DIR_DELIM "backups");
	} catch (const BaseException &e) {
		throw ServerError(e.what());
	}

	// Create emerge manager
	m_emerge = std::make_unique<EmergeManager>(this, m_metrics_backend.get());

	// Terra Firma: who may do what (sprint 119): the one playing alone, or the host, owns it
	m_access = std::make_unique<Access>(m_path_world,
			m_simple_singleplayer_mode ? "singleplayer" : g_settings->get("name"));

	// Create ban manager
	std::string ban_path = m_path_world + DIR_DELIM "ipban.txt";
	m_banmanager = new BanManager(ban_path);

	//lock environment
	EnvAutoLock envlock(this);

	// Create the Map (loads map_meta.txt, overriding configured mapgen params)
	std::unique_ptr<ServerMap> startup_server_map;
	try {
		startup_server_map = std::make_unique<ServerMap>(m_path_world, this,
				m_emerge.get(), m_metrics_backend.get());
	} catch (DatabaseException &e) {
		throw ServerError(std::string(
			"Failed to initialize the map database. The world may be "
			"corrupted or in an unsupported format.\n") + e.what());
	}

	// Terra Firma: whether the world is a planet is the world's (map_meta.txt,
	// or the settings a new world starts from), known before any mod runs:
	// core.planet answers while mods load. One made on the cube-sphere net
	// (planet_half), before the atlas, is refused.
	if (std::string half; startup_server_map->settings_mgr.getMapSetting("planet_half", &half) && stoi(half) != 0)
		throw ServerError("This world was made on the old cube planet, and can't be played any more. "
			"Make a new world: planets are laid out by the atlas now.");
	// The world's atlas (planet/atlas.h): its home's mapping code is the
	// world's, and every other body it holds comes from the world code's
	// system (terragen/worlds.h).
	if (std::string code; startup_server_map->settings_mgr.getMapSetting("planet_atlas", &code) && !code.empty()) {
		std::string world;
		if (!startup_server_map->settings_mgr.getMapSetting("mgterra_code", &world) || world.empty())
			throw ServerError("This world is a planet without a world code (mgterra_code)");
		try {
			m_body_worlds = terragen::bodyWorlds(terragen::WorldCode::decode(world), terragen::Atlas::parse(code));
			std::vector<terragen::Atlas::Params> bodies;
			std::vector<std::string> ids;
			for (const terragen::BodyWorld &b : m_body_worlds) {
				bodies.push_back(b.atlas);
				ids.push_back(b.id);
			}
			m_atlas.emplace(bodies);
			// which tiles were in the map's window when the world was last saved
			const std::string windowFile = m_path_world + DIR_DELIM + "planet_window";
			if (std::string window; fs::PathExists(windowFile)) {
				if (!fs::ReadFile(windowFile, window, true))
					throw ServerError("This world's planet_window can't be read");
				m_atlas->setWindow(window);
			}
			startup_server_map->layOn(*m_atlas, ids);
		} catch (const std::invalid_argument &e) {
			throw ServerError(std::string("This world's planet_atlas can't be used: ") + e.what());
		} catch (const terragen::CodeError &e) {
			throw ServerError(std::string("This world's world code can't be read: ") + e.what());
		}
	}
	if (m_atlas) {
		planet::Pager::Hooks hooks;
		ServerMap *map = startup_server_map.get(); // the environment's, once it's made
		hooks.empty = [this, map](int slot) {
			const auto [low, high] = slotBlocks(slot);
			const bool emptied = map->unloadArea(low, high, true);
			if (!emptied)
				infostream << "planet window: slot " << slot << " has blocks in use; it stays for now" << std::endl;
			return emptied;
		};
		hooks.moved = [this](int slot) {
			const auto [low, high] = slotBlocks(slot);
			m_clients.forgetArea(low, high);
			SendWindow(PEER_ID_INEXISTENT, { slot });
			if (!fs::safeWriteToFile(m_path_world + DIR_DELIM + "planet_window", m_atlas->windowCode()))
				throw ServerError("The planet's window can't be saved (planet_window)");
		};
		m_pager.emplace(*m_atlas, hooks);
		// a new world (no planet_window yet): a small planet is all in the map
		if (m_atlas->tiles().empty())
			m_pager->all();
	}
	// TEMP: 2026-10-28 the home body's sky and orbit; sprint 40 gives each body its own
	const double radius = m_atlas ? m_atlas->body(planet::Atlas::HOME).params().radius : 0;
	m_flight = std::make_unique<orbit::Flight>(radius > 0 ? terragen::sphere(radius) : terragen::home());

	// Must be created before content, as the world's inventories are
	m_inventory_mgr = std::make_unique<ServerInventoryManager>();

	m_gamespec.checkAndLog();
	m_materials = std::make_unique<workshop::Materials>();
	// Terra Firma (sprint 119): what the world's made of, natively (server/registry): the
	// engine's own blocks, then the TypeScript mods'. A world given no server folder runs the
	// game's own (apps/game/server, staged prepared into the share): the official mods.
	registerContent(this);
	if (m_server_folder.empty()) {
		const std::string game = porting::path_share + DIR_DELIM "server";
		if (fs::PathExists(game + DIR_DELIM ".terra" DIR_DELIM "manifest.json"))
			m_server_folder = game;
	}
	if (!m_server_folder.empty())
		startSidecar();
	// what every block's made of, into the world's matter and machines
	terraRegisterMatter(this);

	// Read Textures and calculate sha1 sums
	fillMediaCache();

	// Apply item aliases in the node definition manager
	m_nodedef->updateAliases(m_itemdef);

	// Apply texture overrides from texturepack/override.txt
	std::vector<std::string> paths = getTexturePathsReversed();
	fs::GetRecursiveDirs(paths, m_gamespec.path + DIR_DELIM + "textures");
	for (const std::string &path : paths) {
		TextureOverrideSource override_source(path + DIR_DELIM + "override.txt");
		m_nodedef->applyTextureOverrides(override_source.getNodeTileOverrides());
		m_itemdef->applyTextureOverrides(override_source.getItemTextureOverrides());
	}

	m_nodedef->setNodeRegistrationStatus(true);

	// Perform pending node name resolutions
	m_nodedef->runNodeResolveCallbacks();

	// unmap node names in cross-references
	m_nodedef->resolveCrossrefs();

	// init the recipe hashes to speed up crafting
	m_craftdef->initHashes(this);

	// Initialize Environment
	m_env = new ServerEnvironment(std::move(startup_server_map),
		this, m_metrics_backend.get());
	m_env->init();
	// what pulls on things: the planet's, or the home card's straight down (sprint 59)
	if (m_atlas) {
		std::vector<terragen::Planet> planets;
		for (const terragen::BodyWorld &b : m_body_worlds)
			planets.push_back(terragen::planetOf(b));
		m_field = std::make_unique<orbit::Field>(*m_atlas, planets);
	} else {
		m_field = std::make_unique<orbit::Field>(terragen::home());
	}
	m_workshop = std::make_unique<workshop::Workshop>(workshop::Workshop::Opts{ *m_env, this, m_path_world, *m_materials, *m_field,
		[this](const std::string &player) { return checkPriv(player, "core:creative"); } });
	m_piles = std::make_unique<pile::Piles>(m_env->getMap(), *m_nodedef, nullptr, false,
		[this](const std::vector<v3s16> &nodes) {
			// each loose node's shape, as the server has it (a client skips what it has)
			Map &map = m_env->getMap();
			for (v3s16 p : nodes)
				if (map.getShape(p) || m_nodedef->get(map.getNode(p)).loose)
					sendShape(p, nullptr, 30);
		});

	m_inventory_mgr->setEnv(m_env);
	m_clients.setEnv(m_env);

	// Initialize mapgens
	auto &servermap = m_env->getServerMap();
	if (!servermap.settings_mgr.makeMapgenParams())
		FATAL_ERROR("Couldn't create any mapgen type");
	servermap.getMapgenParams()->planet = m_atlas ? &*m_atlas : nullptr;
	m_emerge->initMapgens(servermap.getMapgenParams());
	// every terra world is a planet; one made before planets were is refused,
	// not played flat
	if (m_emerge->mgparams->mgtype == MAPGEN_TERRA && !m_atlas)
		throw ServerError("This world was made before worlds were planets, and can't be played any more. "
			"Make a new world: every world is a planet now.");
	// the same planet, however the code was written
	const std::string &mgAtlas = m_emerge->mgparams->planet_atlas;
	if (mgAtlas.empty() != !m_atlas ||
			(m_atlas && terragen::Atlas(terragen::Atlas::parse(mgAtlas)).code() != m_atlas->body(planet::Atlas::HOME).code()))
		throw ServerError("planet_atlas changed while mods loaded: the planet is the world's, not a mod's");
	if (m_atlas) {
		// every node the atlas stores must be one the generator can make
		const auto [emin, emax] = get_mapgen_edges(m_emerge->mgparams->mapgen_limit, m_emerge->mgparams->chunksize);
		for (const planet::Atlas::Tile &tile : m_atlas->tiles())
			if (const planet::Atlas::Rect &r = tile.rect; r.x < emin.X || r.z < emin.Z || r.x + r.width - 1 > emax.X || r.z + r.depth - 1 > emax.Z)
				throw ServerError("the atlas's tiles reach past what the map generator makes (chunksize, mapgen_limit)");
	}

	if (g_settings->getBool("enable_rollback_recording")) {
		// Create rollback manager
		m_rollback = new RollbackManager(m_path_world, this);
	}

	// Terra Firma: snow, ice and thaw (sprint 119: were tf_weather's)
	terraAddWeatherModifiers(this);

	// Register us to receive map edit events
	servermap.addEventReceiver(this);

	m_env->loadMeta();
	m_sidecar_now = m_env->getGameTime();
	if (!m_probe_name.empty())
		m_probe = probe::start(m_probe_name, this);

	// Those settings can be overwritten in world.mt, they are
	// intended to be cached after environment loading.
	m_liquid_transform_every = g_settings->getFloat("liquid_update");
	m_max_chatmessage_length = g_settings->getU16("chat_message_max_size");
}

void Server::start()
{
	init();

	infostream << "Starting server thread..." << std::endl;

	// Stop thread if already running
	m_thread->stop();

	// Start thread
	m_thread->start();

	// ASCII art for the win!
	const char *art[] = {
		R"( _                   _   _ )",
		R"(| |_   _  __ _ _ __ | |_(_))",
		R"(| | | | |/ _` | '_ \| __| |)",
		R"(| | |_| | (_| | | | | |_| |)",
		R"(|_|\__,_|\__,_|_| |_|\__|_|)",
	};
	if (!m_admin_chat) {
		// we're not printing to rawstream to avoid it showing up in the logs.
		// however it would then mess up the ncurses terminal (m_admin_chat),
		// so we skip it in that case.
		for (size_t i = 0; i < ARRLEN(art); ++i)
			std::cerr << art[i] << (i == ARRLEN(art) - 1 ? "" : "\n");
		// add a "tail" with the engine version
		std::cerr << "  ___ " << g_version_hash << std::endl;
	}

	actionstream << "World at [" << m_path_world << "]" << std::endl;
	actionstream << "Server for gameid=\"" << m_gamespec.id
			<< "\" listening on ";
	m_bind_addr.print(actionstream);
	actionstream << "." << std::endl;
}

void Server::stop()
{
	infostream<<"Server: Stopping and waiting for threads"<<std::endl;

	// Stop threads (set run=false first so both start stopping)
	m_thread->stop();
	m_thread->wait();

	infostream<<"Server: Threads stopped"<<std::endl;
}

void Server::step()
{
	// Throw if fatal error occurred in thread
	std::string async_err = m_async_fatal_error.get();
	if (!async_err.empty()) {
		if (!m_simple_singleplayer_mode) {
			kickAllPlayers(SERVER_ACCESSDENIED_CRASH,
				g_settings->get("kick_msg_crash"),
				g_settings->getBool("ask_reconnect_on_crash"));
		}
		throw ServerError("AsyncErr: " + async_err);
	}
}

void Server::AsyncRunStep(float dtime, bool initial_step)
{
	ZoneScoped;
	auto framemarker = FrameMarker("Server::AsyncRunStep()-frame").started();

	if (!m_async_fatal_error.get().empty()) {
		infostream << "Refusing server step in error state" << std::endl;
		return;
	}

	{
		// Send blocks to clients
		SendBlocks(dtime);
	}
	m_client_count.store(m_clients.getClientIDs(CS_Created).size(), std::memory_order_relaxed); // Terra Firma: clientCount()

	// Terra Firma: answers on joiners' tickets, admitted under the environment lock, as
	// packet handling is.
	if (m_gate) {
		std::vector<online::Gate::Decision> decisions = m_gate->poll();
		if (!decisions.empty()) {
			EnvAutoLock envlock(this);
			for (const online::Gate::Decision &d : decisions)
				admit(d.peer, d.admit, d.reason);
		}
	}

	// Terra Firma: the players' report (reportPlayers), each half second
	m_players_report_age += dtime;
	if (m_players_report_age >= 0.5f) {
		m_players_report_age = 0;
		std::vector<PlayerReport> report;
		{
			EnvAutoLock envlock(this);
			report = collectPlayers();
		}
		std::lock_guard<std::mutex> hold(m_players_report_lock);
		m_players_report = std::move(report);
	}

	// If paused, this function is called with a 0.0f literal
	if ((dtime == 0.0f) && !initial_step)
		return;

	ScopeProfiler sp(g_profiler, "Server::AsyncRunStep()", SPT_AVG);

	/*
		Update uptime
	*/
	m_uptime_counter->increment(dtime);

	/*
		Update time of day and overall game time
	*/
	m_env->setTimeOfDaySpeed(getTimeSpeed());

	/*
		Send to clients at constant intervals
	*/

	static const float time_send_interval = 5.0f;
	m_time_of_day_send_timer -= dtime;
	if (m_time_of_day_send_timer < 0) {
		m_time_of_day_send_timer = time_send_interval;
		u16 time = m_env->getTimeOfDay();
		SendTimeOfDay(PEER_ID_INEXISTENT, time, getTimeSpeed());

		m_timeofday_gauge->set(time);
	}

	{
		EnvAutoLock lock(this);
		float max_lag = m_env->getMaxLagEstimate();
		constexpr float lag_warn_threshold = 1.0f;

		// Decrease value gradually, halve it every minute.
		if (m_max_lag_decrease.step(dtime, 0.5f)) {
			// To reproduce this number enter "solve (x)**(60/0.5) = 0.5"
			// into Wolfram Alpha.
			max_lag *= 0.99425f;
		}

		// Report a 20% change to the log but only if we're >10% off target
		// also report if we crossed into the warning boundary
		if (dtime >= max_lag * 1.2f ||
			(max_lag < lag_warn_threshold && dtime >= lag_warn_threshold)) {
			const float steplen = getStepSettings().steplen;
			if (dtime > steplen * 1.1f) {
				auto &to = dtime >= lag_warn_threshold ? warningstream : infostream;
				to << "Server: Maximum lag peaked at " << dtime
					<< " (steplen=" << steplen << ")" << std::endl;
			}
		}
		max_lag = std::max(max_lag, dtime),

		m_env->reportMaxLagEstimate(max_lag);

		// Terra Firma: machines move by physics, then the environment steps (sprint 57)
		if (m_workshop)
			{ TF_ZONE("Server: machines"); m_workshop->step(dtime); }
		// Terra Firma: what that pressed into loose ground, and how it slides (sprint 84)
		if (m_piles)
			pressGround(dtime);

		// Step environment
		{ TF_ZONE("Server: environment"); m_env->step(dtime); }

		// Terra Firma: a window for TypeScript mods (sprint 119)
		if (m_sidecar) {
			m_sidecar_now += dtime;
			{ TF_ZONE("Server: mods (sidecar)"); m_sidecar->step(dtime, m_sidecar_now); }
			// new content can't be swapped into a running universe: stop, to be started again
			if (m_sidecar->needsRestart() && !isShutdownRequested()) {
				m_restart_for_mods = true;
				requestShutdown("The mods changed what's in the universe: restarting it.", true);
			}
		}

		// Terra Firma: a check's probe, driving the world (src/probe, sprint 119)
		if (m_probe)
			m_probe->step(dtime);

		// Terra Firma: each player's sky, from the real sun where they stand (sprint 119: was tf_sky)
		{ TF_ZONE("Server: sky"); terraStepSkies(this, dtime); }
		// Terra Firma: each player's weight, and standing on the planet (sprint 119: were
		// tf_flight's and tf_planet's)
		{ TF_ZONE("Server: gravity"); terraStepGravity(this); }
		{ TF_ZONE("Server: planet"); terraStepPlanet(this, dtime); }
		{ TF_ZONE("Server: weather"); terraStepWeather(this, dtime); }
		{ TF_ZONE("Server: players"); terraStepPlayers(this, dtime); }
		{ TF_ZONE("Server: matter"); terraStepMatter(this, dtime); }
		// Terra Firma: the shore's smoothed ground holds water as a fill (sprint 81; sprint
		// 119: was tf_world's on_generated)
		{
			std::vector<v3s16> shore;
			{
				MutexAutoLock lock(m_shore_mutex);
				shore.swap(m_shore);
			}
			terraFillShore(this, shore);
		}

		// Terra Firma: the rivers round each player, as the weather fills them (sprint 115)
		if ((m_rivers_timer -= dtime) <= 0) {
			m_rivers_timer = 2;
			terraSendRivers(this);
		}
		// Terra Firma: dammed rivers' lakes rise (sprint 115)
		m_lakes_dtime += dtime;
		if (m_lakes_dtime >= 0.5f) {
			{ TF_ZONE("Server: lakes"); terraStepLakes(this, m_lakes_dtime); }
			m_lakes_dtime = 0;
		}

		// Terra Firma: the planet's window follows the players
		if (m_pager && (m_window_timer -= dtime) <= 0) {
			m_window_timer = 1;
			keepPlanetWindow();
		}

		// Run Map's timers and unload unused data
		constexpr float map_timer_and_unload_dtime = 2.95f;
		auto &map = m_env->getServerMap();
		if (m_map_timer_and_unload_interval.step(dtime, map_timer_and_unload_dtime)) {
			ScopeProfiler sp(g_profiler, "Server: map timer and unload");
			map.timerUpdate(map_timer_and_unload_dtime,
				std::max(g_settings->getFloat("server_unload_unused_data_timeout"), 0.0f),
				-1);
		}

		// Note: Orphan MapBlock pointers become dangling after this call
		map.step();
	}

	/*
		Listen to the admin chat, if available
	*/
	if (m_admin_chat) {
		if (!m_admin_chat->command_queue.empty()) {
			EnvAutoLock lock(this);
			while (!m_admin_chat->command_queue.empty()) {
				ChatEvent *evt = m_admin_chat->command_queue.pop_frontNoEx();
				handleChatInterfaceEvent(evt);
				delete evt;
			}
		}
		m_admin_chat->outgoing_queue.push_back(
			new ChatEventTimeInfo(m_env->getGameTime(), m_env->getTimeOfDay()));
	}

	/*
		Do background stuff
	*/

	/* Transform liquids */
	m_liquid_transform_timer += dtime;
	if(m_liquid_transform_timer >= m_liquid_transform_every)
	{
		m_liquid_transform_timer -= m_liquid_transform_every;

		EnvAutoLock lock(this);

		ScopeProfiler sp(g_profiler, "Server: liquid transform");

		std::map<v3s16, MapBlock*> modified_blocks;
		m_env->getServerMap().transformLiquids(modified_blocks, m_env);

		if (!modified_blocks.empty()) {
			MapEditEvent event;
			event.type = MEET_OTHER;
			event.low_priority = true;
			event.setModifiedBlocks(modified_blocks);
			m_env->getMap().dispatchEvent(event);
		}
	}
	m_clients.step(dtime);

	// increase/decrease lag gauge gradually
	if (m_lag_gauge->get() > dtime) {
		m_lag_gauge->decrement(dtime/100);
	} else {
		m_lag_gauge->increment(dtime/100);
	}

	{
		float &counter = m_step_pending_dyn_media_timer;
		counter += dtime;
		if (counter >= 5.0f) {
			stepPendingDynMediaCallbacks(counter);
			counter = 0;
		}
	}


#if USE_CURL
	// send masterserver announce
	{
		float &counter = m_masterserver_timer;
		if (!isSingleplayer() && (!counter || counter >= 300.0) &&
				g_settings->getBool("server_announce")) {
			ServerList::sendAnnounce(counter ? ServerList::AA_UPDATE :
						ServerList::AA_START,
					m_bind_addr.getPort(),
					m_clients.getPlayerNames(),
					m_uptime_counter->get(),
					m_env->getGameTime(),
					m_lag_gauge->get(),
					m_gamespec.id,
					Mapgen::getMapgenName(m_emerge->mgparams->mgtype),
					modNames(),
					m_dedicated);
			counter = 0.01;
		}
		counter += dtime;
	}
#endif

	// Send queued particles
	{
		EnvAutoLock envlock(this);
		SendSpawnParticles();
	}

	/*
		Check added and deleted active objects
	*/
	{
		//infostream<<"Server: Checking added and deleted active objects"<<std::endl;
		EnvAutoLock envlock(this);

		// This guarantees that each object recomputes its cache only once per server step,
		// unless get_effective_observers is called.
		// If we were to update observer sets eagerly in set_observers instead,
		// the total costs of calls to set_observers could theoretically be higher.
		m_env->invalidateActiveObjectObserverCaches();

		{
			ClientInterface::AutoLock clientlock(m_clients);
			const RemoteClientMap &clients = m_clients.getClientList();
			ScopeProfiler sp(g_profiler, "Server: update objects within range");

			m_player_gauge->set(clients.size());
			for (const auto &client_it : clients) {
				RemoteClient *client = client_it.second;

				if (client->getState() < CS_DefinitionsSent)
					continue;

				// This can happen if the client times out somehow
				if (!m_env->getPlayer(client->peer_id))
					continue;

				PlayerSAO *playersao = getPlayerSAO(client->peer_id);
				if (!playersao)
					continue;

				SendActiveObjectRemoveAdd(client, playersao);
			}
		}
	}

	/*
		Send object messages
	*/
	{
		EnvAutoLock envlock(this);
		ScopeProfiler sp(g_profiler, "Server: send SAO messages");

		// Key = object id
		// Value = data sent by object
		std::unordered_map<u16, std::vector<ActiveObjectMessage>*> buffered_messages;

		// Get active object messages from environment
		ActiveObjectMessage aom(0);
		u32 count_reliable = 0, count_unreliable = 0;
		for(;;) {
			if (!m_env->getActiveObjectMessage(&aom))
				break;
			if (aom.reliable)
				count_reliable++;
			else
				count_unreliable++;

			std::vector<ActiveObjectMessage>* message_list = nullptr;
			auto n = buffered_messages.find(aom.id);
			if (n == buffered_messages.end()) {
				message_list = new std::vector<ActiveObjectMessage>;
				buffered_messages[aom.id] = message_list;
			} else {
				message_list = n->second;
			}
			message_list->push_back(std::move(aom));
		}

		m_aom_buffer_counter[0]->increment(count_reliable);
		m_aom_buffer_counter[1]->increment(count_unreliable);

		{
			ClientInterface::AutoLock clientlock(m_clients);
			const RemoteClientMap &clients = m_clients.getClientList();
			// Route data to every client
			std::string reliable_data, unreliable_data;
			for (const auto &client_it : clients) {
				reliable_data.clear();
				unreliable_data.clear();
				RemoteClient *client = client_it.second;
				PlayerSAO *player = getPlayerSAO(client->peer_id);
				// Go through all objects in message buffer
				for (const auto &buffered_message : buffered_messages) {
					// If object does not exist or is not known by client, skip it
					u16 id = buffered_message.first;
					ServerActiveObject *sao = m_env->getActiveObject(id);
					if (!sao || client->m_known_objects.find(id) == client->m_known_objects.end())
						continue;

					// Get message list of object
					std::vector<ActiveObjectMessage>* list = buffered_message.second;
					// Go through every message
					for (const ActiveObjectMessage &aom : *list) {
						const auto cmd = static_cast<ActiveObjectCommand>(aom.datastring[0]);

						// Send position updates to players who do not see the attachment
						if (cmd == AO_CMD_UPDATE_POSITION) {
							if (sao->getId() == player->getId())
								continue;

							// Do not send position updates for attached players
							// as long the parent is known to the client
							ServerActiveObject *parent = sao->getParent();
							if (parent && client->m_known_objects.find(parent->getId()) !=
									client->m_known_objects.end())
								continue;
						}

						if (cmd >= AO_CMD_STOP_ANIMATION && client->net_proto_version < 52)
							continue; // AO_CMD_STOP_ANIMATION added in protocol version 52
						if (cmd >= AO_CMD_SET_PARTS && client->net_proto_version < 70)
							continue; // Terra Firma: AO_CMD_SET_PARTS added in protocol version 70

						// Add full new data to appropriate buffer
						std::string &buffer = aom.reliable ? reliable_data : unreliable_data;
						aom.appendTo(buffer);
					}
				}
				/*
					reliable_data and unreliable_data are now ready.
					Send them.
				*/
				if (!reliable_data.empty()) {
					SendActiveObjectMessages(client->peer_id, reliable_data);
				}

				if (!unreliable_data.empty()) {
					SendActiveObjectMessages(client->peer_id, unreliable_data, false);
				}
			}
		}

		// Clear buffered_messages
		for (auto &buffered_message : buffered_messages) {
			delete buffered_message.second;
		}
	}

	/*
		Send queued-for-sending map edit events.
	*/
	{
		// We will be accessing the environment
		EnvAutoLock lock(this);

		// Single change sending is disabled if queue size is big
		bool disable_single_change_sending = false;
		if(m_unsent_map_edit_queue.size() >= 4)
			disable_single_change_sending = true;

		const auto event_count = m_unsent_map_edit_queue.size();
		m_map_edit_event_counter->increment(event_count);

		// We'll log the amount of each
		Profiler prof;

		size_t block_count = 0;
		std::unordered_set<v3s16> node_meta_updates;

		while (!m_unsent_map_edit_queue.empty()) {
			MapEditEvent* event = m_unsent_map_edit_queue.front();
			m_unsent_map_edit_queue.pop();

			// Players far away from the change are stored here.
			// Instead of sending the changes, MapBlocks are set not sent
			// for them.
			std::unordered_set<u16> far_players;

			switch (event->type) {
			case MEET_ADDNODE:
			case MEET_SWAPNODE:
				prof.add("MEET_ADDNODE", 1);
				sendAddNode(event->p, event->n, &far_players,
						disable_single_change_sending ? 5 : 30,
						event->type == MEET_ADDNODE);
				break;
			case MEET_REMOVENODE:
				prof.add("MEET_REMOVENODE", 1);
				sendRemoveNode(event->p, &far_players,
						disable_single_change_sending ? 5 : 30);
				break;
			case MEET_BLOCK_NODE_METADATA_CHANGED: {
				prof.add("MEET_BLOCK_NODE_METADATA_CHANGED", 1);
				if (!event->is_private_change) {
					node_meta_updates.emplace(event->p);
				}

				if (MapBlock *block = m_env->getMap().getBlockNoCreateNoEx(
						getNodeBlockPos(event->p))) {
					block->raiseModified(MOD_STATE_WRITE_NEEDED,
						MOD_REASON_REPORT_META_CHANGE);
				}
				break;
			}
			case MEET_SHAPE:
				prof.add("MEET_SHAPE", 1);
				sendShape(event->p, &far_players, disable_single_change_sending ? 5 : 30);
				break;
			case MEET_FILL:
				prof.add("MEET_FILL", 1);
				sendFill(event->p, &far_players, disable_single_change_sending ? 5 : 30);
				break;
			case MEET_OTHER:
				prof.add("MEET_OTHER", 1);
				m_clients.markBlocksNotSent(event->modified_blocks, event->low_priority);
				break;
			default:
				prof.add("unknown", 1);
				warningstream << "Server: Unknown MapEditEvent "
						<< ((u32)event->type) << std::endl;
				break;
			}

			block_count += event->modified_blocks.size();

			/*
				Set blocks not sent to far players
			*/
			for (const u16 far_player : far_players) {
				if (RemoteClient *client = getClient(far_player))
					client->SetBlocksNotSent(event->modified_blocks, event->low_priority);
			}

			delete event;
		}

		if (event_count != 0) {
			verbosestream << "Server: MapEditEvents modified total "
				<< block_count << " blocks:" << std::endl;
			prof.print(verbosestream);
		}

		// Send all metadata updates
		if (!node_meta_updates.empty())
			sendMetadataChanged(node_meta_updates);
	}

	/*
		Trigger emerge thread
		Doing this every 2s is left over from old code, unclear if this is still needed.
	*/
	{
		float &counter = m_emergethread_trigger_timer;
		counter -= dtime;
		if (counter <= 0.0f) {
			counter = 2.0f;

			m_emerge->startThreads();
		}
	}

	// Save map, players and auth stuff
	{
		float &counter = m_savemap_timer;
		counter += dtime;
		static thread_local const float save_interval =
			g_settings->getFloat("server_map_save_interval");
		if (counter >= save_interval) {
			counter = 0.0;
			EnvAutoLock lock(this);

			ScopeProfiler sp(g_profiler, "Server: map saving (sum)");

			// Save ban file
			if (m_banmanager->isModified()) {
				m_banmanager->save();
			}

			// Save changed parts of map
			m_env->getMap().save(MOD_STATE_WRITE_NEEDED);

			// Save players
			m_env->saveLoadedPlayers();

			// Save environment metadata
			m_env->saveMeta();
		}
	}

	m_shutdown_state.tick(dtime, this);
}

void Server::Receive(float min_time)
{
	// (no zone of its own: it's mostly waiting, shown as Wait, and the packets
	// it handles are their own zone)
	auto framemarker = FrameMarker("Server::Receive()-frame").started();

	const u64 t0 = porting::getTimeUs();
	const float min_time_us = min_time * 1e6f;
	auto remaining_time_us = [&]() -> float {
		return std::max(0.0f, min_time_us - (porting::getTimeUs() - t0));
	};

	NetworkPacket pkt;
	session_t peer_id;
	for (;;) {
		pkt.clear();
		peer_id = 0;
		try {
			// Round up since the target step length is the minimum step length,
			// we only have millisecond precision and we don't want to busy-wait
			// by calling ReceiveTimeoutMs(.., 0) repeatedly.
			const u32 cur_timeout_ms = std::ceil(remaining_time_us() / 1000.0f);

			bool got;
			{
				TF_ZONE("Wait"); // the step's rest, idle till a packet comes
				got = m_con->ReceiveTimeoutMs(&pkt, cur_timeout_ms);
			}
			if (!got) {
				// No incoming data.
				if (remaining_time_us() > 0.0f)
					continue;
				else
					break;
			}

			peer_id = pkt.getPeerId();
			m_packet_recv_counter->increment();
			ProcessData(&pkt);
			m_packet_recv_processed_counter->increment();
		} catch (const con::InvalidIncomingDataException &e) {
			infostream << "Server::Receive(): InvalidIncomingDataException: what()="
					<< e.what() << std::endl;
		} catch (SerializationError &e) {
			enrich_exception(e, pkt, true);
			infostream << "Server::Receive(): SerializationError: what()="
					<< e.what() << std::endl;
		} catch (PacketError &e) {
			enrich_exception(e, pkt, false);
			actionstream << "Server::Receive(): PacketError: what()="
					<< e.what() << std::endl;
		} catch (const ClientStateError &e) {
			errorstream << "ClientStateError: peer=" << peer_id << " what()="
					 << e.what() << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		} catch (con::PeerNotFoundException &e) {
			infostream << "Server: PeerNotFoundException" << std::endl;
		} catch (ClientNotFoundException &e) {
			infostream << "Server: ClientNotFoundException" << std::endl;
		}
	}
}

void Server::yieldToOtherThreads(float dtime)
{
	/*
	 * Problem: the server thread and emerge thread compete for the envlock.
	 * While the emerge thread needs it just once or twice for every processed item
	 * the server thread uses it much more generously.
	 * This is usually not a problem as the server sleeps between steps, which leaves
	 * enough chance. But if the server is overloaded it's busy all the time and
	 * - even with a fair envlock - the emerge thread can't get up to speed.
	 * This generally has a much worse impact on gameplay than server lag itself
	 * ever would.
	 *
	 * Workaround: If we detect that the server is overloaded, introduce some careful
	 * artificial sleeps to leave the emerge threads enough chance to do their job.
	 *
	 * In the future the emerge code should be reworked to exclusively use a result
	 * queue, thereby avoiding this problem (and terrible workaround).
	 */

	// don't activate workaround too quickly
	constexpr size_t MIN_EMERGE_QUEUE_SIZE = 32;
	const size_t qs_initial = m_emerge->getQueueSize();
	if (qs_initial < MIN_EMERGE_QUEUE_SIZE)
		return;

	// give the thread a chance to run for every 28ms (on average)
	// this was experimentally determined
	const float QUANTUM = 28.0f / 1000;
	// put an upper limit to not cause too much lag, also so this doesn't become self-sustaining
	const int SLEEP_MAX = 10;

	int sleep_count = std::clamp<int>(dtime / QUANTUM, 1, SLEEP_MAX);

	ScopeProfiler sp(g_profiler, "Server::yieldTo...() sleep", SPT_AVG);
	size_t qs = qs_initial;
	while (sleep_count-- > 0) {
		sleep_ms(1);
		// abort if we don't make progress
		size_t qs2 = m_emerge->getQueueSize();
		if (qs2 >= qs || qs2 == 0)
			break;
		qs = qs2;
	}
	g_profiler->avg("Server::yieldTo...() progress [#]", qs_initial - qs);
}

PlayerSAO *Server::StageTwoClientInit(session_t peer_id)
{
	std::string playername;
	std::unique_ptr<PlayerSAO> sao;
	{
		ClientInterface::AutoLock clientlock(m_clients);
		RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_InitDone);
		if (client) {
			playername = client->getName();
			sao = emergePlayer(playername.c_str(), peer_id, client->net_proto_version);
		}
	}

	RemotePlayer *player = sao ? sao->getPlayer() : nullptr;

	// If failed, cancel
	if (!player) {
		RemotePlayer *joined = m_env->getPlayer(playername.c_str(), true);
		if (joined) {
			actionstream << "Server: Failed to emerge player \"" << playername
					<< "\" (player allocated to another client)" << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
		} else {
			errorstream << "Server: " << playername << ": Failed to emerge player"
					<< std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
		}
		return nullptr;
	}

	// Add player to environment
	m_env->addPlayer(player);

	/* Clean up old HUD elements from previous sessions */
	player->hud.clear();

	/* Add object to environment */
	PlayerSAO *playersao = sao.get();
	if (m_env->addActiveObject(std::move(sao)) == 0) {
		actionstream << "Server: Can't add SAO for player \"" << playername
			<< "\"." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
		m_env->removePlayer(player);
		return nullptr;
	}

	/*
		Send complete position information
	*/
	SendMovePlayer(playersao);

	// Send privileges
	SendPlayerPrivileges(peer_id);

	// Send inventory formspec
	SendPlayerInventoryFormspec(peer_id);

	// Send inventories
	SendInventory(player, false);
	sendDetachedInventories(peer_id);

	// Send HP
	SendPlayerHP(playersao, false);

	// Send Breath
	SendPlayerBreath(playersao);

	/*
		Update player list and print action
	*/
	{
		NetworkPacket notice_pkt(TOCLIENT_UPDATE_PLAYER_LIST, 0, PEER_ID_INEXISTENT);
		notice_pkt << (u8) PLAYER_LIST_ADD << (u16) 1 << player->getName();
		m_clients.sendToAll(&notice_pkt);
	}
	{
		std::string ip_str = getPeerAddress(player->getPeerId()).serializeString();
		const auto &names = m_clients.getPlayerNames();

		actionstream << player->getName() << " [" << ip_str << "] joins game. List of players: ";
		for (const std::string &name : names)
			actionstream << name << " ";
		actionstream << player->getName() << std::endl;
	}

	return playersao;
}

inline void Server::handleCommand(NetworkPacket *pkt)
{
	const ToServerCommandHandler &opHandle = toServerCommandTable[pkt->getCommand()];
	(this->*opHandle.handler)(pkt);
}

void Server::ProcessData(NetworkPacket *pkt)
{
	// Environment is locked first.
	EnvAutoLock envlock(this);

	ScopeProfiler sp(g_profiler, "Server: Process network packet (sum)");
	u32 peer_id = pkt->getPeerId();

	try {
		ToServerCommand command = (ToServerCommand) pkt->getCommand();

		// Command must be handled into ToServerCommandHandler
		if (command >= TOSERVER_NUM_MSG_TYPES) {
			infostream << "Server: Ignoring unknown command "
					 << static_cast<unsigned>(command) << std::endl;
			return;
		}

		if (toServerCommandTable[command].state == TOSERVER_STATE_NOT_CONNECTED) {
			handleCommand(pkt);
			return;
		}

		u8 peer_ser_ver = getClient(peer_id, CS_InitDone)->serialization_version;

		if(peer_ser_ver == SER_FMT_VER_INVALID) {
			errorstream << "Server: Peer serialization format invalid. "
					"Skipping incoming command "
					<< static_cast<unsigned>(command) << std::endl;
			return;
		}

		/* Handle commands related to client startup */
		if (toServerCommandTable[command].state == TOSERVER_STATE_STARTUP) {
			handleCommand(pkt);
			return;
		}

		if (m_clients.getClientState(peer_id) < CS_Active) {
			warningstream << "Server: Got packet command "
					<< static_cast<unsigned>(command)
					<< " for peer id " << peer_id
					<< " but client isn't active yet. Dropping packet." << std::endl;
			return;
		}

		handleCommand(pkt);
	} catch (SendFailedException &e) {
		errorstream << "Server::ProcessData(): SendFailedException: "
				<< "what=" << e.what()
				<< std::endl;
	}
}

void Server::setTimeOfDay(u32 time)
{
	// (the date only goes forward: what's kept by it, the weather, replays from it)
	if (time < m_env->getTimeOfDay())
		m_env->setDayCount(m_env->getDayCount() + 1);
	m_env->setTimeOfDay(time);
	m_time_of_day_send_timer = 0;
}

void Server::setDate(u32 days, u32 time)
{
	m_env->setDayCount(days);
	m_env->setTimeOfDay(time);
	m_time_of_day_send_timer = 0;
}

void Server::onMapEditEvent(const MapEditEvent &event)
{
	// Terra Firma: the physics' ground follows every edit (sprint 57)
	if (m_workshop)
		m_workshop->mapEdited(event);
	// Terra Firma: a river dammed (sprint 115)
	terraEdited(this, event);

	if (m_ignore_map_edit_events_area.contains(event.getArea()))
		return;

	m_unsent_map_edit_queue.push(new MapEditEvent(event));
}

void Server::peerAdded(con::IPeer *peer)
{
	verbosestream << "Server::peerAdded(): id=" << peer->id << std::endl;

	m_clients.CreateClient(peer->id);
}

void Server::deletingPeer(con::IPeer *peer, bool timeout)
{
	verbosestream << "Server::deletingPeer(): id=" << peer->id
		<< ", timeout=" << timeout << std::endl;

	m_clients.event(peer->id, CSE_Disconnect);
	DeleteClient(peer->id, timeout ? CDR_TIMEOUT : CDR_LEAVE);
}

std::vector<Server::PlayerReport> Server::reportPlayers()
{
	std::lock_guard<std::mutex> hold(m_players_report_lock);
	return m_players_report;
}

std::vector<Server::PlayerReport> Server::collectPlayers()
{
	std::vector<PlayerReport> out;
	for (RemotePlayer *player : m_env->getPlayers()) {
		if (player->getPeerId() == PEER_ID_INEXISTENT)
			continue;
		PlayerReport r{ player->getName(), player->getPeerId(), v3f(), 0, -1, 0, 0 };
		if (PlayerSAO *sao = player->getPlayerSAO()) {
			r.position = sao->getBasePosition() / BS;
			r.hp = sao->getHP();
		}
		getClientConInfo(r.peer, con::AVG_RTT, &r.rtt);
		ClientInfo info;
		if (getClientInfo(r.peer, info)) {
			r.connected = info.uptime;
			r.protocol = info.prot_vers;
		}
		out.push_back(std::move(r));
	}
	return out;
}

bool Server::getClientConInfo(session_t peer_id, con::rtt_stat_type type, float* retval)
{
	*retval = m_con->getPeerStat(peer_id,type);
	return *retval != -1;
}

bool Server::getClientInfo(session_t peer_id, ClientInfo &ret)
{
	ClientInterface::AutoLock clientlock(m_clients);
	RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_Invalid);

	if (!client)
		return false;

	ret.state = client->getState();
	ret.addr = client->getAddress();
	ret.uptime = client->uptime();
	ret.ser_vers = client->serialization_version;
	ret.prot_vers = client->net_proto_version;

	ret.major = client->getMajor();
	ret.minor = client->getMinor();
	ret.patch = client->getPatch();
	ret.vers_string = client->getFullVer();

	ret.lang_code = client->getLangCode();

	return true;
}

const ClientDynamicInfo *Server::getClientDynamicInfo(session_t peer_id)
{
	ClientInterface::AutoLock clientlock(m_clients);
	RemoteClient *client = m_clients.lockedGetClientNoEx(peer_id, CS_Invalid);

	if (!client)
		return nullptr;

	return &client->getDynamicInfo();
}

void Server::printToConsoleOnly(const std::string &text)
{
	if (m_admin_chat) {
		m_admin_chat->outgoing_queue.push_back(
			new ChatEventChat("", utf8_to_wide(text)));
	} else {
		std::cout << text << std::endl;
	}
}

void Server::Send(NetworkPacket *pkt)
{
	FATAL_ERROR_IF(pkt->getPeerId() == 0, "Server::Send() missing peer ID");
	Send(pkt->getPeerId(), pkt);
}

void Server::Send(session_t peer_id, NetworkPacket *pkt)
{
	m_clients.send(peer_id, pkt);
}

void Server::SendMovement(session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_MOVEMENT, 12 * sizeof(float), peer_id);

	pkt << g_settings->getFloat("movement_acceleration_default");
	pkt << g_settings->getFloat("movement_acceleration_air");
	pkt << g_settings->getFloat("movement_acceleration_fast");
	pkt << g_settings->getFloat("movement_speed_walk");
	pkt << g_settings->getFloat("movement_speed_crouch");
	pkt << g_settings->getFloat("movement_speed_fast");
	pkt << g_settings->getFloat("movement_speed_climb");
	pkt << g_settings->getFloat("movement_speed_jump");
	pkt << g_settings->getFloat("movement_liquid_fluidity");
	pkt << g_settings->getFloat("movement_liquid_fluidity_smooth");
	pkt << g_settings->getFloat("movement_liquid_sink");
	pkt << g_settings->getFloat("movement_gravity");

	Send(&pkt);
}

void Server::HandlePlayerHPChange(PlayerSAO *playersao, const PlayerHPChangeReason &reason)
{
	SendPlayerHP(playersao, reason.type != PlayerHPChangeReason::SET_HP_MAX);

	// Send to other clients
	playersao->sendPunchCommand();

	if (playersao->isDead())
		HandlePlayerDeath(playersao, reason);
}

void Server::SendPlayerHP(PlayerSAO *playersao, bool effect)
{
	SendHP(playersao->getPeerID(), playersao->getHP(), effect);
}

void Server::SendHP(session_t peer_id, u16 hp, bool effect)
{
	NetworkPacket pkt(TOCLIENT_HP, 3, peer_id);
	pkt << hp << effect;
	Send(&pkt);
}

void Server::SendBreath(session_t peer_id, u16 breath)
{
	NetworkPacket pkt(TOCLIENT_BREATH, 2, peer_id);
	pkt << (u16) breath;
	Send(&pkt);
}

void Server::SendAccessDenied(session_t peer_id, AccessDeniedCode reason,
		std::string_view custom_reason, bool reconnect)
{
	assert(reason < SERVER_ACCESSDENIED_MAX);

	NetworkPacket pkt(TOCLIENT_ACCESS_DENIED, 1, peer_id);
	pkt << (u8)reason << custom_reason << (u8)reconnect;
	Send(&pkt);
}

void Server::SendItemDef(session_t peer_id,
		IItemDefManager *itemdef, u16 protocol_version)
{
	auto *client = m_clients.getClientNoEx(peer_id, CS_Created);
	assert(client);

	NetworkPacket pkt(TOCLIENT_ITEMDEF, 0, peer_id);

	std::ostringstream tmp_os2(std::ios::binary);
	{
		std::ostringstream tmp_os(std::ios::binary);
		itemdef->serialize(tmp_os, protocol_version);
		if (client->net_proto_version >= 48)
			compressZstd(tmp_os.str(), tmp_os2);
		else
			compressZlib(tmp_os.str(), tmp_os2);
	}
	pkt.putLongString(tmp_os2.str());

	// Make data buffer
	verbosestream << "Server: Sending item definitions to id(" << peer_id
			<< "): size=" << pkt.getSize() << std::endl;

	Send(&pkt);
}

void Server::SendNodeDef(session_t peer_id,
	const NodeDefManager *nodedef, u16 protocol_version)
{
	auto *client = m_clients.getClientNoEx(peer_id, CS_Created);
	assert(client);

	NetworkPacket pkt(TOCLIENT_NODEDEF, 0, peer_id);

	std::ostringstream tmp_os2(std::ios::binary);
	{
		std::ostringstream tmp_os(std::ios::binary);
		nodedef->serialize(tmp_os, protocol_version);
		if (client->net_proto_version >= 48)
			compressZstd(tmp_os.str(), tmp_os2);
		else
			compressZlib(tmp_os.str(), tmp_os2);
	}
	pkt.putLongString(tmp_os2.str());

	// Make data buffer
	verbosestream << "Server: Sending node definitions to id(" << peer_id
			<< "): size=" << pkt.getSize() << std::endl;

	Send(&pkt);
}

/*
	Non-static send methods
*/

void Server::SendInventory(RemotePlayer *player, bool incremental, bool skip_wield_anim)
{
	// Do not send new format to old clients
	incremental &= player->protocol_version >= 38;

	UpdateCrafting(player);

	/*
		Serialize it
	*/

	NetworkPacket pkt(TOCLIENT_INVENTORY, 0, player->getPeerId());

	std::ostringstream os(std::ios::binary);
	player->inventory.serialize(os, incremental);
	player->inventory.setModified(false);
	player->setModified(true);
	std::string content = os.str();

	if (player->protocol_version >= 52) {
		pkt.putLongString(content);
		pkt << skip_wield_anim;
	} else {
		pkt.putRawString(content);
	}

	Send(&pkt);
}

void Server::SendChatMessage(session_t peer_id, const ChatMessage &message)
{
	NetworkPacket pkt(TOCLIENT_CHAT_MESSAGE, 0, peer_id);
	u8 version = 1;
	u8 type = message.type;
	pkt << version << type << message.sender << message.message
		<< static_cast<u64>(message.timestamp);

	if (peer_id != PEER_ID_INEXISTENT) {
		Send(&pkt);
	} else {
		// If a client has completed auth but is still joining, still send chat
		m_clients.sendToAll(&pkt, CS_InitDone);
	}
}

void Server::SendShowFormspecMessage(session_t peer_id, const std::string &formspec,
	const std::string &formname)
{
	NetworkPacket pkt(TOCLIENT_SHOW_FORMSPEC, 0, peer_id);
	if (formspec.empty()){
		// The client should close the formspec
		// But make sure there wasn't another one open in meantime
		// If the formname is empty, any open formspec will be closed so the
		// form name should always be erased from the state.
		const auto it = m_formspec_state_data.find(peer_id);
		if (it != m_formspec_state_data.end() &&
				(it->second == formname || formname.empty())) {
			m_formspec_state_data.erase(peer_id);
		}
	} else {
		m_formspec_state_data[peer_id] = formname;
	}
	pkt.putLongString(formspec);
	pkt << formname;

	Send(&pkt);
}

void Server::SendSpawnParticles(RemotePlayer *player,
		const std::vector<ParticleParameters> &particles)
{
	static thread_local const float radius =
		g_settings->getS16("max_block_send_distance") * MAP_BLOCKSIZE * BS;
	const float radius_sq = radius * radius;

	PlayerSAO *sao = player->getPlayerSAO();
	if (!sao || sao->isGone())
		return;

	std::ostringstream particle_batch_data(std::ios_base::binary);
	for (const auto &particle : particles) {
		if (sao->getBasePosition().getDistanceFromSQ(particle.pos * BS) > radius_sq)
			continue; // out of range

		std::ostringstream particle_data(std::ios_base::binary);
		particle.serialize(particle_data, player->protocol_version);
		std::string particle_data_str = particle_data.str();
		SANITY_CHECK(particle_data_str.size() < U32_MAX);
		if (player->protocol_version < 50) {
			// Client only supports TOCLIENT_SPAWN_PARTICLE,
			// so turn the written particle into a packet immediately
			NetworkPacket pkt(TOCLIENT_SPAWN_PARTICLE, particle_data_str.size(), player->getPeerId());
			pkt.putRawString(particle_data_str);
			Send(&pkt);
		} else {
			particle_batch_data << serializeString32(particle_data_str);
		}
	}

	if (particle_batch_data.tellp() == 0)
		return; // no batch to send

	// Client supports TOCLIENT_SPAWN_PARTICLE_BATCH
	assert(player->protocol_version >= 50);
	std::ostringstream compressed(std::ios_base::binary);
	compressZstd(particle_batch_data.str(), compressed);

	NetworkPacket pkt(TOCLIENT_SPAWN_PARTICLE_BATCH,
			4 + compressed.tellp(), player->getPeerId());
	pkt.putLongString(compressed.str());
	Send(&pkt);
}

void Server::SendSpawnParticles()
{
	for (const auto &[pname, particles] : m_particles_to_send) {
		if (pname.empty())
			continue; // sent to all clients

		RemotePlayer *player = m_env->getPlayer(pname.c_str());
		if (!player)
			continue;

		SendSpawnParticles(player, particles);
	}

	for (auto *player : m_env->getPlayers()) {
		SendSpawnParticles(player, m_particles_to_send[""]);
	}

	m_particles_to_send.clear();
}

void Server::SendAddParticleSpawner(const std::string &to_player,
	const std::string &exclude_player,
	const ParticleSpawnerParameters &p, u16 attached_id, u32 id)
{
	static thread_local const float radius =
			g_settings->getS16("max_block_send_distance") * MAP_BLOCKSIZE * BS;
	const float radius_sq = radius * radius;

	// Average position where particles would spawn (approximate)
	const v3f pos = (
		p.pos.start.min.val +
		p.pos.start.max.val +
		p.pos.end.min.val +
		p.pos.end.max.val
	) / 4.0f * BS;
	/* Don't send short-lived spawners to distant players.
	 * This could be replaced with proper tracking at some point.
	 * A lifetime of 0 means that the spawner exists forever. */
	const bool distance_check = !attached_id && p.time <= 1.0f && p.time != 0.0f;

	const auto &consider_player = [&] (RemotePlayer *player) {
		if (distance_check) {
			PlayerSAO *sao = player->getPlayerSAO();
			if (!sao)
				return;
			if (sao->getBasePosition().getDistanceFromSQ(pos) > radius_sq)
				return;
		}

		SendAddParticleSpawner(player->getPeerId(), player->protocol_version,
			p, attached_id, id);
	};

	// Send to one -or- all (except one)
	if (!to_player.empty()) {
		RemotePlayer *player = m_env->getPlayer(to_player);
		if (player)
			consider_player(player);
		return;
	}
	std::vector<session_t> clients = m_clients.getClientIDs();
	for (const session_t client_id : clients) {
		RemotePlayer *player = m_env->getPlayer(client_id);
		if (!player)
			continue;
		if (!exclude_player.empty() && exclude_player == player->getName())
			continue;
		consider_player(player);
	}
}

void Server::SendAddParticleSpawner(session_t peer_id, u16 protocol_version,
	const ParticleSpawnerParameters &p, u16 attached_id, u32 id)
{
	assert(peer_id != PEER_ID_INEXISTENT);
	assert(protocol_version != 0);

	NetworkPacket pkt(TOCLIENT_ADD_PARTICLESPAWNER, 100, peer_id);

	pkt << p.amount << p.time;

	std::ostringstream os(std::ios_base::binary);
	if (protocol_version >= 42) {
		// Serialize entire thing
		p.pos.serialize(os);
		p.vel.serialize(os);
		p.acc.serialize(os);
		p.exptime.serialize(os);
		p.size.serialize(os);
	} else {
		// serialize legacy fields only (compatibility)
		p.pos.start.legacySerialize(os);
		p.vel.start.legacySerialize(os);
		p.acc.start.legacySerialize(os);
		p.exptime.start.legacySerialize(os);
		p.size.start.legacySerialize(os);
	}
	pkt.putRawString(os.str());
	pkt << p.collisiondetection;

	pkt.putLongString(p.texture.string);

	pkt << id << p.vertical << p.collision_removal << attached_id;
	{
		os.str("");
		p.animation.serialize(os, protocol_version);
		pkt.putRawString(os.str());
	}
	pkt << p.glow << p.object_collision;
	pkt << p.node.param0 << p.node.param2 << p.node_tile;

	{ // serialize new fields
		os.str("");
		if (protocol_version < 42) {
			// initial bias for older properties
			pkt << p.pos.start.bias
				<< p.vel.start.bias
				<< p.acc.start.bias
				<< p.exptime.start.bias
				<< p.size.start.bias;

			// final tween frames of older properties
			p.pos.end.serialize(os);
			p.vel.end.serialize(os);
			p.acc.end.serialize(os);
			p.exptime.end.serialize(os);
			p.size.end.serialize(os);
		}
		// else: fields are already written by serialize() very early

		// properties for legacy texture field
		p.texture.serialize(os, protocol_version, true);

		// new properties
		p.drag.serialize(os);
		p.jitter.serialize(os);
		p.bounce.serialize(os);
		ParticleParamTypes::serializeParameterValue(os, p.attractor_kind);
		if (p.attractor_kind != ParticleParamTypes::AttractorKind::none) {
			p.attract.serialize(os);
			p.attractor_origin.serialize(os);
			writeU16(os, p.attractor_attachment); /* object ID */
			writeU8(os, p.attractor_kill);
			if (p.attractor_kind != ParticleParamTypes::AttractorKind::point) {
				p.attractor_direction.serialize(os);
				writeU16(os, p.attractor_direction_attachment);
			}
		}
		p.radius.serialize(os);

		ParticleParamTypes::serializeParameterValue(os, (u16)p.texpool.size());
		for (const auto& tex : p.texpool) {
			tex.serialize(os, protocol_version);
		}

		pkt.putRawString(os.str());
	}

	Send(&pkt);
}

void Server::SendDeleteParticleSpawner(session_t peer_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_DELETE_PARTICLESPAWNER, 4, peer_id);

	pkt << id;

	if (peer_id != PEER_ID_INEXISTENT)
		Send(&pkt);
	else
		m_clients.sendToAll(&pkt);

}

void Server::SendHUDAdd(session_t peer_id, u32 id, HudElement *form)
{
	NetworkPacket pkt(TOCLIENT_HUDADD, 0 , peer_id);

	pkt << id << (u8) form->type << form->pos << form->name << form->scale
			<< form->text << form->number << form->item << form->dir
			<< form->align << form->offset << form->world_pos;

	if (m_clients.getProtocolVersion(peer_id) >= 52)
		pkt << form->size;
	else
		pkt << v2s32::from(form->size);

	/// Bit 0: hideable
	/// Bits 1 ... 8: unused (set to 0)
	u8 flags = form->hideable ? 1 : 0;

	pkt << form->z_index << form->text2 << form->style << flags;

	Send(&pkt);
}

void Server::SendHUDRemove(session_t peer_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_HUDRM, 4, peer_id);
	pkt << id;
	Send(&pkt);
}

void Server::SendHUDChange(session_t peer_id, u32 id, HudElementStat stat, void *value)
{
	NetworkPacket pkt(TOCLIENT_HUDCHANGE, 0, peer_id);
	pkt << id << (u8) stat;

	switch (stat) {
		case HUD_STAT_POS:
		case HUD_STAT_SCALE:
		case HUD_STAT_ALIGN:
		case HUD_STAT_OFFSET:
			pkt << *(v2f *) value;
			break;
		case HUD_STAT_NAME:
		case HUD_STAT_TEXT:
		case HUD_STAT_TEXT2:
			pkt << *(std::string *) value;
			break;
		case HUD_STAT_WORLD_POS:
			pkt << *(v3f *) value;
			break;
		case HUD_STAT_SIZE: {
			v2f *v = (v2f *) value;
			if (m_clients.getProtocolVersion(peer_id) >= 52)
				pkt << *v;
			else
				pkt << v2s32::from(*v);
			break;
		}
		case HUD_STAT_HIDEABLE:
			pkt << u32{*(bool *) value};
			break;
		default: // all other types
			pkt << *(u32 *) value;
			break;
	}

	Send(&pkt);
}

void Server::SendHUDSetFlags(session_t peer_id, u32 flags, u32 mask)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_FLAGS, 4 + 4, peer_id);

	pkt << flags << mask;

	Send(&pkt);
}

void Server::SendHUDSetParam(session_t peer_id, u16 param, std::string_view value)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_PARAM, 0, peer_id);
	pkt << param << value;
	Send(&pkt);
}

void Server::SendSetSky(session_t peer_id, const SkyboxParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_SKY, 0, peer_id);

	// Handle prior clients here
	if (m_clients.getProtocolVersion(peer_id) < 39) {
		pkt << params.bgcolor << params.type << (u16) params.textures.size();

		for (const std::string& texture : params.textures)
			pkt << texture;

		pkt << params.clouds;
	} else { // Handle current clients and future clients
		pkt << params.bgcolor << params.type
			<< params.clouds << params.fog_sun_tint
			<< params.fog_moon_tint << params.fog_tint_type;

		if (params.type == "skybox") {
			pkt << (u16) params.textures.size();
			for (const std::string &texture : params.textures)
				pkt << texture;
		} else if (params.type == "regular") {
			auto &c = params.sky_color;
			pkt << c.day_sky << c.day_horizon << c.dawn_sky << c.dawn_horizon
				<< c.night_sky << c.night_horizon << c.indoors;
		}

		pkt << params.body_orbit_tilt << params.fog_distance << params.fog_start
			<< params.fog_color;

		pkt << params.auto_dim_skybox;
		// Terra Firma
		pkt << params.solar.on << params.solar.latitude << params.solar.declination << params.solar.turns << params.solar.at;
	}

	Send(&pkt);
}

void Server::SendSetSun(session_t peer_id, const SunParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_SUN, 0, peer_id);
	pkt << params.visible << params.texture
		<< params.tonemap << params.sunrise
		<< params.sunrise_visible << params.scale;

	Send(&pkt);
}
void Server::SendSetMoon(session_t peer_id, const MoonParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_MOON, 0, peer_id);

	pkt << params.visible << params.texture
		<< params.tonemap << params.scale;

	Send(&pkt);
}
void Server::SendSetStars(session_t peer_id, const StarParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_STARS, 0, peer_id);

	pkt << params.visible << params.count
		<< params.starcolor << params.scale
		<< params.day_opacity << params.star_seed;

	Send(&pkt);
}

void Server::SendCloudParams(session_t peer_id, const CloudParams &params)
{
	NetworkPacket pkt(TOCLIENT_CLOUD_PARAMS, 0, peer_id);
	pkt << params.density << params.color_bright << params.color_ambient
		<< params.height << params.thickness << params.speed << params.color_shadow;
	Send(&pkt);
}

void Server::SendOverrideDayNightRatio(session_t peer_id, bool do_override,
		float ratio)
{
	NetworkPacket pkt(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO,
			1 + 2, peer_id);

	pkt << do_override << (u16) (ratio * 65535);

	Send(&pkt);
}

void Server::SendSetLighting(session_t peer_id, const Lighting &lighting)
{
	NetworkPacket pkt(TOCLIENT_SET_LIGHTING, 70, peer_id);

	pkt << lighting.shadow_intensity << lighting.saturation;

	pkt << lighting.exposure.luminance_min
			<< lighting.exposure.luminance_max
			<< lighting.exposure.exposure_correction
			<< lighting.exposure.speed_dark_bright
			<< lighting.exposure.speed_bright_dark
			<< lighting.exposure.center_weight_power;

	pkt << lighting.volumetric_light_strength << lighting.shadow_tint;
	pkt << lighting.bloom_intensity << lighting.bloom_strength_factor <<
			lighting.bloom_radius;

	pkt << lighting.shadow_direction;

	const auto &s = lighting.static_;
	pkt << s.light_curve_set;
	if (s.light_curve_set)
		pkt.putRawString(reinterpret_cast<const char*>(s.light_curve), sizeof(s.light_curve));
	pkt << s.ao_gamma;

	Send(&pkt);
}

void Server::SendCamera(session_t peer_id, Player *player)
{
	NetworkPacket pkt(TOCLIENT_CAMERA, 1, peer_id);

	pkt << static_cast<u8>(player->allowed_camera_mode);

	Send(&pkt);
}

void Server::SendTimeOfDay(session_t peer_id, u16 time, f32 time_speed)
{
	NetworkPacket pkt(TOCLIENT_TIME_OF_DAY, 0, peer_id);
	pkt << time << time_speed << m_env->getDayCount(); // Terra Firma: the client never counts days itself

	if (peer_id == PEER_ID_INEXISTENT) {
		m_clients.sendToAll(&pkt);
	}
	else {
		Send(&pkt);
	}
}

void Server::SendPlayerBreath(PlayerSAO *sao)
{
	assert(sao);

	SendBreath(sao->getPeerID(), sao->getBreath());
}

void Server::SendMovePlayer(PlayerSAO *sao)
{
	// Send attachment updates instantly to the client prior updating position.
	if (sao->isAttachmentOutdated() && !sao->isAttached()) {
		std::string data;
		ActiveObjectMessage aom(
				sao->getId(), true, sao->generateUpdateAttachmentCommand());
		aom.appendTo(data);
		SendActiveObjectMessages(sao->getPeerID(), data);
	}

	NetworkPacket pkt(TOCLIENT_MOVE_PLAYER, sizeof(v3f) + sizeof(f32) * 2, sao->getPeerID());
	pkt << sao->getBasePosition() << sao->getLookPitch() << sao->getRotation().Y;

	{
		verbosestream << "Server: Sending TOCLIENT_MOVE_PLAYER"
				<< " pos=" << sao->getBasePosition()
				<< " pitch=" << sao->getLookPitch()
				<< " yaw=" << sao->getRotation().Y
				<< std::endl;
	}

	Send(&pkt);
}

void Server::SendMovePlayerRel(session_t peer_id, const v3f &added_pos)
{
	NetworkPacket pkt(TOCLIENT_MOVE_PLAYER_REL, 0, peer_id);
	pkt << added_pos;
	Send(&pkt);
}

void Server::SendPlayerFov(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_FOV, 4 + 1 + 4, peer_id);

	PlayerFovSpec fov_spec = player->getFov();
	pkt << fov_spec.fov << fov_spec.is_multiplier << fov_spec.transition_time;

	Send(&pkt);
}

void Server::SendLocalPlayerAnimations(session_t peer_id, v2f animation_frames[4],
		f32 animation_speed)
{
	NetworkPacket pkt(TOCLIENT_LOCAL_PLAYER_ANIMATIONS, 0,
		peer_id);

	for (int i = 0; i < 4; ++i) {
		if (m_clients.getProtocolVersion(peer_id) >= 46) {
			pkt << animation_frames[i];
		} else {
			pkt << v2s32::from(animation_frames[i]);
		}
	}

	pkt  << animation_speed;

	Send(&pkt);
}

void Server::SendEyeOffset(session_t peer_id, v3f first, v3f third, v3f third_front)
{
	NetworkPacket pkt(TOCLIENT_EYE_OFFSET, 0, peer_id);
	pkt << first << third << third_front;
	Send(&pkt);
}

void Server::SendPlayerPrivileges(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	std::set<std::string> privs = getPlayerEffectivePrivs(player->getName());

	NetworkPacket pkt(TOCLIENT_PRIVILEGES, 0, peer_id);
	pkt << (u16) privs.size();

	for (const std::string &priv : privs) {
		pkt << priv;
	}

	Send(&pkt);
}

void Server::SendPlayerInventoryFormspec(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_INVENTORY_FORMSPEC, 0, peer_id);
	pkt.putLongString(player->inventory_formspec);

	Send(&pkt);
}

void Server::SendPlayerFormspecPrepend(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_FORMSPEC_PREPEND, 0, peer_id);
	pkt << player->formspec_prepend;
	Send(&pkt);
}

void Server::SendActiveObjectRemoveAdd(RemoteClient *client, PlayerSAO *playersao)
{
	// Radius inside which objects are active
	static thread_local const s16 radius =
		g_settings->getS16("active_object_send_range_blocks") * MAP_BLOCKSIZE;

	// Radius inside which players are active
	static thread_local const bool is_transfer_limited =
		g_settings->exists("unlimited_player_transfer_distance") &&
		!g_settings->getBool("unlimited_player_transfer_distance");

	static thread_local const s16 player_transfer_dist =
		g_settings->getS16("player_transfer_distance") * MAP_BLOCKSIZE;

	s16 player_radius = player_transfer_dist == 0 && is_transfer_limited ?
		radius : player_transfer_dist;

	s16 my_radius = MYMIN(radius, playersao->getWantedRange() * MAP_BLOCKSIZE);
	if (my_radius <= 0)
		my_radius = radius;

	std::vector<std::pair<bool, u16>> removed_objects;
	std::vector<u16> added_objects;
	m_env->getRemovedActiveObjects(playersao, my_radius, player_radius,
		client->m_known_objects, removed_objects);
	m_env->getAddedActiveObjects(playersao, my_radius, player_radius,
		client->m_known_objects, added_objects);

	if (removed_objects.empty() && added_objects.empty())
		return;

	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD,
		2 * removed_objects.size() + 32 * added_objects.size(), client->peer_id);

	// Removed objects
	pkt << static_cast<u16>(removed_objects.size());

	for (auto &it : removed_objects) {
		const auto [gone, id] = it;
		ServerActiveObject *obj = m_env->getActiveObject(id);

		pkt << id;

		// Remove from known objects
		client->m_known_objects.erase(id);
		if (obj && obj->m_known_by_count > 0)
			obj->m_known_by_count--;
	}

	// Note: Do yet NOT stop or remove object-attached sounds where the object goes out
	// of range (client side). Such sounds would need to be re-sent when coming into range.
	// Currently, the client will initiate m_playing_sounds clean ups indirectly by
	// "Server::handleCommand_RemovedSounds".

	// Added objects
	pkt << static_cast<u16>(added_objects.size());

	for (u16 id : added_objects) {
		ServerActiveObject *obj = m_env->getActiveObject(id);
		if (!obj) {
			warningstream << FUNCTION_NAME << ": found NULL object id="
				<< (int)id << std::endl;
			continue;
		}

		u8 type = obj->getSendType();

		pkt << id << type;
		pkt.putLongString(obj->getClientInitializationData(client->net_proto_version));

		// Add to known objects
		client->m_known_objects.insert(id);
		obj->m_known_by_count++;
	}

	Send(&pkt);
}

void Server::SendActiveObjectMessages(session_t peer_id, const std::string &datas,
		bool reliable)
{
	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_MESSAGES,
			datas.size(), peer_id);

	pkt.putRawString(datas);

	auto &ccf = clientCommandFactoryTable[pkt.getCommand()];
	m_clients.sendCustom(pkt.getPeerId(), reliable ? ccf.channel : 1, &pkt, reliable);
}

void Server::SendPlayerSpeed(session_t peer_id, const v3f &added_vel)
{
	NetworkPacket pkt(TOCLIENT_PLAYER_SPEED, 0, peer_id);
	pkt << added_vel;
	Send(&pkt);
}

inline s32 Server::nextSoundId()
{
	s32 free_id = m_playing_sounds_id_last_used;
	do {
		if (free_id == INT32_MAX)
			free_id = 0; // signed overflow is undefined
		else
			free_id++;

		if (free_id == m_playing_sounds_id_last_used)
			return 0;
	} while (free_id == 0 || m_playing_sounds.find(free_id) != m_playing_sounds.end());

	m_playing_sounds_id_last_used = free_id;
	return free_id;
}

s32 Server::playSound(ServerPlayingSound &params, bool ephemeral)
{
	// Find out initial position of sound
	bool pos_exists = false;
	const v3f pos = params.getPos(m_env, &pos_exists);
	// If position is not found while it should be, cancel sound
	if(pos_exists != (params.type != SoundLocation::Local))
		return -1;

	// Filter destination clients
	std::vector<session_t> dst_clients;
	if (!params.to_player.empty()) {
		RemotePlayer *player = m_env->getPlayer(params.to_player.c_str());
		if(!player){
			infostream<<"Server::playSound: Player \""<<params.to_player
					<<"\" not found"<<std::endl;
			return -1;
		}
		dst_clients.push_back(player->getPeerId());
	} else {
		std::vector<session_t> clients = m_clients.getClientIDs();

		for (const session_t client_id : clients) {
			RemotePlayer *player = m_env->getPlayer(client_id);
			if (!player)
				continue;
			if (!params.exclude_player.empty() &&
					params.exclude_player == player->getName())
				continue;

			PlayerSAO *sao = player->getPlayerSAO();
			if (!sao)
				continue;

			if (pos_exists) {
				if(sao->getBasePosition().getDistanceFrom(pos) >
						params.max_hear_distance)
					continue;
			}
			dst_clients.push_back(client_id);
		}
	}

	if(dst_clients.empty())
		return -1;

	// old clients will still use this, so pick a reserved ID (-1)
	const s32 id = ephemeral ? -1 : nextSoundId();
	if (id == 0)
		return 0;

	float gain = params.gain * params.spec.gain;
	NetworkPacket pkt(TOCLIENT_PLAY_SOUND, 0);
	pkt << id << params.spec.name << gain
			<< (u8) params.type << pos << params.object
			<< params.spec.loop << params.spec.fade << params.spec.pitch
			<< ephemeral << params.spec.start_time;

	const bool as_reliable = !ephemeral;

	for (const session_t peer_id : dst_clients) {
		if (!ephemeral)
			params.clients.insert(peer_id);
		m_clients.sendCustom(peer_id, 0, &pkt, as_reliable);
	}

	if (!ephemeral)
		m_playing_sounds[id] = std::move(params);
	return id;
}
void Server::stopSound(s32 handle)
{
	auto it = m_playing_sounds.find(handle);
	if (it == m_playing_sounds.end())
		return;

	ServerPlayingSound &psound = it->second;

	NetworkPacket pkt(TOCLIENT_STOP_SOUND, 4);
	pkt << handle;

	for (session_t peer_id : psound.clients) {
		Send(peer_id, &pkt);
	}

	// Remove sound reference
	m_playing_sounds.erase(it);
}

void Server::fadeSound(s32 handle, float step, float gain)
{
	auto it = m_playing_sounds.find(handle);
	if (it == m_playing_sounds.end())
		return;

	ServerPlayingSound &psound = it->second;
	psound.gain = gain; // destination gain

	NetworkPacket pkt(TOCLIENT_FADE_SOUND, 4);
	pkt << handle << step << gain;

	for (session_t peer_id : psound.clients) {
		Send(peer_id, &pkt);
	}

	// Remove sound reference
	if (gain <= 0 || psound.clients.empty())
		m_playing_sounds.erase(it);
}

void Server::sendRemoveNode(v3s16 p, std::unordered_set<u16> *far_players,
		float far_d_nodes)
{
	v3f p_f = intToFloat(p, BS);
	v3s16 block_pos = getNodeBlockPos(p);

	NetworkPacket pkt(TOCLIENT_REMOVENODE, 6);
	pkt << p;

	sendNodeChangePkt(pkt, block_pos, p_f, far_d_nodes, far_players);
}

void Server::sendAddNode(v3s16 p, MapNode n, std::unordered_set<u16> *far_players,
		float far_d_nodes, bool remove_metadata)
{
	v3f p_f = intToFloat(p, BS);
	v3s16 block_pos = getNodeBlockPos(p);

	NetworkPacket pkt(TOCLIENT_ADDNODE, 6 + 2 + 1 + 1 + 1);
	pkt << p << n.param0 << n.param1 << n.param2
			<< (u8) (remove_metadata ? 0 : 1);
	sendNodeChangePkt(pkt, block_pos, p_f, far_d_nodes, far_players);
}

void Server::SendBlueprints(session_t peer_id, const std::string &player)
{
	const std::vector<std::string> names = m_workshop->blueprints(player);
	NetworkPacket pkt(TOCLIENT_BLUEPRINTS, 2, peer_id);
	pkt << u16(names.size());
	for (const std::string &name : names)
		pkt << name;
	Send(&pkt);
}

void Server::SendMaterials(session_t peer_id)
{
	const auto all = m_materials->all();
	NetworkPacket pkt(TOCLIENT_MATERIALS, 2 + all.size() * 19, peer_id);
	pkt << u16(all.size());
	for (const auto &[c, m] : all)
		pkt << u16(c) << m.density << m.friction << u8(m.role) << m.power << m.rpm;
	Send(&pkt);
}

void Server::sendShape(v3s16 p, std::unordered_set<u16> *far_players, float far_d_nodes)
{
	NetworkPacket pkt(TOCLIENT_SHAPE, 6 + 1 + shape::CELLS / 8);
	const shape::Shape *s = m_env->getMap().getShape(p);
	pkt << p << u8(s ? 0 : 1);
	if (s) {
		std::ostringstream os(std::ios::binary);
		s->serialize(os);
		const std::string bytes = os.str();
		pkt.putRawString(bytes.data(), bytes.size());
	}
	sendNodeChangePkt(pkt, getNodeBlockPos(p), intToFloat(p, BS), far_d_nodes, far_players);
}

// Terra Firma: a node's liquid (sprint 81), drawn by the client's liquid layer, not its mesh
void Server::sendFill(v3s16 p, std::unordered_set<u16> *far_players, float far_d_nodes)
{
	NetworkPacket pkt(TOCLIENT_FILL, 6 + 64 + 4);
	const fill::Fill *f = m_env->getMap().getFill(p);
	pkt << p;
	if (f)
		pkt << fill::name(f->liquid) << f->amount << f->r << f->g << f->b;
	else
		pkt << std::string() << u8(0) << u8(0) << u8(0) << u8(0);
	sendNodeChangePkt(pkt, getNodeBlockPos(p), intToFloat(p, BS), far_d_nodes, far_players);
}

// Terra Firma: liquid poured (sprint 82), to the players near enough to see it fall
void Server::sendStream(v3f from, v3f to, video::SColor colour, u16 cups)
{
	static const float SEEN = 48; // nodes
	NetworkPacket pkt(TOCLIENT_STREAM, 12 + 12 + 3 + 2);
	pkt << from << to << u8(colour.getRed()) << u8(colour.getGreen()) << u8(colour.getBlue()) << cups;
	std::unordered_set<u16> far; // the far just don't see it
	sendNodeChangePkt(pkt, getNodeBlockPos(floatToInt(from * BS, BS)), from * BS, SEEN, &far);
}

// Terra Firma: the rivers round a player, as the weather fills them (sprint 115)
void Server::sendRivers(session_t peer, const std::vector<std::tuple<v3s16, s16, u8, u8>> &blocks)
{
	NetworkPacket pkt(TOCLIENT_RIVERS, 2 + blocks.size() * 10, peer);
	pkt << u16(blocks.size());
	for (const auto &[pos, rise, speed, way] : blocks)
		pkt << pos << rise << speed << way;
	Send(&pkt);
}

void Server::sendNodeChangePkt(NetworkPacket &pkt, v3s16 block_pos,
		v3f p, float far_d_nodes, std::unordered_set<u16> *far_players)
{
	float maxd = far_d_nodes * BS;
	std::vector<session_t> clients = m_clients.getClientIDs();
	ClientInterface::AutoLock clientlock(m_clients);

	for (session_t client_id : clients) {
		RemoteClient *client = m_clients.lockedGetClientNoEx(client_id);
		if (!client)
			continue;

		RemotePlayer *player = m_env->getPlayer(client_id);
		PlayerSAO *sao = player ? player->getPlayerSAO() : nullptr;

		// If player is far away, only set modified blocks not sent
		if (!client->isBlockSent(block_pos) || (sao &&
				sao->getBasePosition().getDistanceFrom(p) > maxd)) {
			if (far_players)
				far_players->emplace(client_id);
			else
				client->SetBlockNotSent(block_pos);
			continue;
		}

		Send(client_id, &pkt);
	}
}

void Server::sendMetadataChanged(const std::unordered_set<v3s16> &positions, float far_d_nodes)
{
	NodeMetadataList meta_updates_list(false);
	std::ostringstream os(std::ios::binary);

	std::vector<session_t> clients = m_clients.getClientIDs();
	ClientInterface::AutoLock clientlock(m_clients);

	for (session_t i : clients) {
		RemoteClient *client = m_clients.lockedGetClientNoEx(i);
		if (!client)
			continue;

		ServerActiveObject *player = getPlayerSAO(i);
		v3s16 player_pos;
		if (player)
			player_pos = floatToInt(player->getBasePosition(), BS);

		for (const v3s16 pos : positions) {
			NodeMetadata *meta = m_env->getMap().getNodeMetadata(pos);

			if (!meta)
				continue;

			v3s16 block_pos = getNodeBlockPos(pos);
			if (!client->isBlockSent(block_pos) ||
					player_pos.getDistanceFrom(pos) > far_d_nodes) {
				client->SetBlockNotSent(block_pos);
				continue;
			}

			// Add the change to send list
			meta_updates_list.set(pos, meta);
		}
		if (meta_updates_list.size() == 0)
			continue;

		// Send the meta changes
		os.str("");
		meta_updates_list.serialize(os, client->serialization_version, false, true, true);
		std::string raw = os.str();
		os.str("");
		compressZlib(raw, os);

		NetworkPacket pkt(TOCLIENT_NODEMETA_CHANGED, 0, i);
		pkt.putLongString(os.str());
		Send(&pkt);

		meta_updates_list.clear();
	}
}

void Server::SendBlockNoLock(session_t peer_id, MapBlock *block, u8 ver,
		u16 net_proto_version, SerializedBlockCache *cache)
{
	thread_local const int net_compression_level = rangelim(g_settings->getS16("map_compression_level_net"), -1, 9);
	std::string s, *sptr = nullptr;

	if (cache) {
		auto it = cache->find({block->getPos(), ver});
		if (it != cache->end())
			sptr = &it->second;
	}

	// Serialize the block in the right format
	if (!sptr) {
		std::ostringstream os(std::ios_base::binary);
		block->serialize(os, ver, false, net_compression_level);
		block->serializeNetworkSpecific(os);
		s = os.str();
		sptr = &s;
	}

	NetworkPacket pkt(TOCLIENT_BLOCKDATA, 2 + 2 + 2 + sptr->size(), peer_id);
	pkt << block->getPos();
	pkt.putRawString(*sptr);
	Send(&pkt);

	// Store away in cache
	if (cache && sptr == &s)
		(*cache)[{block->getPos(), ver}] = std::move(s);
}

void Server::SendBlocks(float dtime)
{
	EnvAutoLock envlock(this);

	std::vector<PrioritySortedBlockTransfer> queue;

	u32 total_sending = 0, unique_clients = 0;

	{
		ScopeProfiler sp2(g_profiler, "Server::SendBlocks(): Collect list");

		std::vector<session_t> clients = m_clients.getClientIDs();

		ClientInterface::AutoLock clientlock(m_clients);
		for (const session_t client_id : clients) {
			RemoteClient *client = m_clients.lockedGetClientNoEx(client_id, CS_Active);

			if (!client)
				continue;

			total_sending += client->getSendingCount();
			const auto old_count = queue.size();
			client->GetNextBlocks(m_env, m_emerge.get(), dtime, queue);
			unique_clients += queue.size() > old_count ? 1 : 0;
		}
	}

	// Sort.
	// Lowest priority number comes first.
	// Lowest is most important.
	std::sort(queue.begin(), queue.end());

	ClientInterface::AutoLock clientlock(m_clients);

	// Maximal total count calculation
	// The per-client block sends is halved with the maximal online users
	u32 max_blocks_to_send = (m_env->getPlayerCount() + g_settings->getU32("max_users")) *
		g_settings->getU32("max_simultaneous_block_sends_per_client") / 4 + 1;

	ScopeProfiler sp(g_profiler, "Server::SendBlocks(): Send to clients");
	Map &map = m_env->getMap();

	SerializedBlockCache cache, *cache_ptr = nullptr;
	if (unique_clients > 1) {
		// caching is pointless with a single client
		cache_ptr = &cache;
	}

	for (const PrioritySortedBlockTransfer &block_to_send : queue) {
		if (total_sending >= max_blocks_to_send)
			break;

		MapBlock *block = map.getBlockNoCreateNoEx(block_to_send.pos);
		if (!block)
			continue;

		RemoteClient *client = m_clients.lockedGetClientNoEx(block_to_send.peer_id,
				CS_Active);
		if (!client)
			continue;

		SendBlockNoLock(block_to_send.peer_id, block, client->serialization_version,
				client->net_proto_version, cache_ptr);

		client->SentBlock(block_to_send.pos);
		total_sending++;
	}
}

bool Server::SendBlock(session_t peer_id, const v3s16 &blockpos)
{
	MapBlock *block = m_env->getMap().getBlockNoCreateNoEx(blockpos);
	if (!block)
		return false;

	ClientInterface::AutoLock clientlock(m_clients);
	RemoteClient *client = m_clients.lockedGetClientNoEx(peer_id, CS_Active);
	if (!client || client->isBlockSent(blockpos))
		return false;
	SendBlockNoLock(peer_id, block, client->serialization_version,
			client->net_proto_version);

	return true;
}

bool Server::addMediaFile(const std::string &filename,
	const std::string &filepath, std::string *filedata_to,
	std::string *digest_to)
{
	// If name contains illegal characters, ignore the file
	if (!string_allowed(filename, TEXTURENAME_ALLOWED_CHARS)) {
		warningstream << "Server: ignoring file as it has disallowed characters: \""
				<< filename << "\"" << std::endl;
		return false;
	}

	// If name is not in a supported format, ignore it
	const char *supported_ext[] = {
		".png", ".jpg", ".tga",
		".ogg",
		".x", ".b3d", ".obj", ".gltf", ".glb",
		// Translation file formats
		".tr", ".po", ".mo",
		// Fonts
		".ttf", ".woff",
		nullptr
	};
	if (removeStringEnd(filename, supported_ext).empty()) {
		infostream << "Server: ignoring unsupported file extension: \""
				<< filename << "\"" << std::endl;
		return false;
	}
	// Ok, attempt to load the file and add to cache

	// Read data
	std::string filedata;
	if (!fs::ReadFile(filepath, filedata, true)) {
		return false;
	}

	if (filedata.empty()) {
		errorstream << "Server::addMediaFile(): Empty file \""
				<< filepath << "\"" << std::endl;
		return false;
	}
	if (filedata.size() > MEDIAFILE_MAX_SIZE) {
		errorstream << "Server::addMediaFile(): \""
				<< filepath << "\" is too big (" << (filedata.size() >> 10)
				<< "KiB). The internal limit is " << (MEDIAFILE_MAX_SIZE >> 10) << "KiB." << std::endl;
		return false;
	}

	std::string sha1 = hashing::sha1(filedata);
	std::string sha1_hex = hex_encode(sha1);
	if (digest_to)
		*digest_to = sha1;

	// Put in list
	m_media.insert_or_assign(filename, MediaInfo(filepath, sha1));
	verbosestream << "Server: " << sha1_hex << " is " << filename
			<< " (" << (filedata.size() >> 10) << "KiB)" << std::endl;

	// Invalidate cached translations if we just added a translation file
	if (Translations::isTranslationFile(filename)) {
		// (could be optimized to clear only the relevant one, but not critical here)
		server_translations.clear();
	}

	if (filedata_to)
		*filedata_to = std::move(filedata);
	return true;
}

void Server::queueShore(std::vector<v3s16> &&shore)
{
	if (shore.empty())
		return;
	MutexAutoLock lock(m_shore_mutex);
	m_shore.insert(m_shore.end(), shore.begin(), shore.end());
}

bool Server::sidecarCommand(const std::string &player, const std::string &name, const std::string &text)
{
	return m_sidecar && m_sidecar->command(player, name, text);
}

// Terra Firma: start `terra runtime` on the server folder, and make what its mods
// declared into items and nodes, before registration closes (sprint 119).
void Server::startSidecar()
{
	std::string terra;
	try {
		terra = sidecar::Terra::locate().path();
	} catch (const sidecar::Error &e) {
		throw ServerError(std::string("TypeScript mods: ") + e.what());
	}

	sidecar::Host host;
	host.tell = [this](const std::string &name, const std::string &text) {
		RemotePlayer *player = m_env->getPlayer(name);
		if (player && player->getPeerId() != PEER_ID_INEXISTENT)
			SendChatMessage(player->getPeerId(), ChatMessage(utf8_to_wide(text)));
	};
	host.announce = [this](const std::string &text) {
		SendChatMessage(PEER_ID_INEXISTENT, ChatMessage(CHATMESSAGE_TYPE_ANNOUNCE, utf8_to_wide(text)));
	};
	host.players = [this]() { return m_clients.getPlayerNames(); };
	// the mods' commands: each line typed under one goes to the next window (CommandBook::run)
	host.commands = [this](const std::vector<sidecar::Host::Command> &commands) {
		m_commands->setMods(commands);
	};
	// by their roles and grants (server/access): an admin may do anything
	host.can = [this](const std::string &name, const std::string &permission) { return checkPriv(name, permission); };
	host.kick = [this](const std::string &name, const std::string &reason) {
		RemotePlayer *player = m_env->getPlayer(name);
		if (player && player->getPeerId() != PEER_ID_INEXISTENT)
			DenyAccess(player->getPeerId(), SERVER_ACCESSDENIED_CUSTOM_STRING, reason);
	};

	try {
		m_sidecar = std::make_unique<sidecar::Sidecar>(terra, m_server_folder, std::move(host));
	} catch (const sidecar::Error &e) {
		throw ServerError(std::string("TypeScript mods: ") + e.what());
	}

	const sidecar::Content &content = m_sidecar->content();
	for (const sidecar::Content::Media &m : content.media)
		if (!addMediaFile(m.name, m.path))
			throw ServerError("TypeScript mods: " + m.path + " can't be sent to players (a PNG, 16 × 16 for a block face)");

	// each a block of its substance, which the world's matter and machines then take up
	registerModContent(this, content);
	// what each mod put in this world, so the menu can tell what removing it would leave behind
	try {
		sidecar::Ledger({ /* .world = */ m_path_world }).record(content);
	} catch (const sidecar::Error &e) {
		throw ServerError(std::string("TypeScript mods: ") + e.what());
	}
	actionstream << "mods: " << content.blocks.size() << " blocks and " << content.items.size() << " items from " << m_server_folder << std::endl;
}

void Server::fillMediaCache()
{
	infostream << "Server: Calculating media file checksums" << std::endl;

	// Collect all media file paths
	std::vector<std::string> paths;

	// ordered in descending priority
	fs::GetRecursiveDirs(paths,
		porting::path_user + DIR_DELIM "textures" DIR_DELIM "server");
	fs::GetRecursiveDirs(paths,
		m_gamespec.path + DIR_DELIM "textures");

	// Collect media file information from paths into cache
	for (const std::string &mediapath : paths) {
		std::vector<fs::DirListNode> dirlist = fs::GetDirListing(mediapath);
		for (const auto &dln : dirlist) {
			if (dln.dir) // Ignore dirs (already in paths)
				continue;

			const std::string &filename = dln.name;
			if (m_media.count(filename) > 0) // Do not override
				continue;

			std::string filepath = mediapath;
			filepath.append(DIR_DELIM).append(filename);
			addMediaFile(filename, filepath);
		}
	}

	infostream << "Server: " << m_media.size() << " media files collected" << std::endl;
}

void Server::sendMediaAnnouncement(session_t peer_id, const std::string &lang_code)
{
	auto include = [&] (const std::string &name, const MediaInfo &info) -> bool {
		if (info.no_announce)
			return false;
		if (Translations::isTranslationFileType(name)) {
			auto this_lang_code = Translations::getFileLanguage(name);
			if (this_lang_code.empty())
				return false;

			if (this_lang_code == lang_code)
				return true;

			// Only clients 5.18 and older can correctly handle fallbacks with English language.
			if (this_lang_code == "en") {
				return m_clients.getProtocolVersion(peer_id) >= 54;
			}

			return false;
		}
		return true;
	};

	// Make packet
	auto *client = m_clients.getClientNoEx(peer_id, CS_Created);
	assert(client);
	NetworkPacket pkt(TOCLIENT_ANNOUNCE_MEDIA, 0, peer_id);

	size_t media_sent = 0;
	if (client->net_proto_version < 48) {
		for (const auto &i : m_media) {
			if (include(i.first, i.second))
				media_sent++;
		}
		assert(media_sent < U16_MAX);
		pkt << static_cast<u16>(media_sent);
		for (const auto &i : m_media) {
			if (include(i.first, i.second))
				pkt << i.first << base64_encode(i.second.sha1_digest);
		}
	} else {
		std::vector<std::string> names;
		for (const auto &i : m_media) {
			if (include(i.first, i.second))
				names.emplace_back(i.first);
		}
		media_sent = names.size();

		// compressed table of media names
		{
			std::ostringstream oss(std::ios::binary);
			auto tmp = serializeString16Array(names);
			compressZstd(tmp, oss);
			pkt.putLongString(oss.str());
		}

		// then the raw hash for each file
		for (const auto &i : m_media) {
			if (include(i.first, i.second)) {
				assert(i.second.sha1_digest.size() == 20);
				pkt.putRawString(i.second.sha1_digest);
			}
		}
	}

	// and the remote media server(s)
	pkt << g_settings->get("remote_media");
	Send(&pkt);

	verbosestream << "Server: Announcing files to id(" << peer_id
		<< "): count=" << media_sent << " size=" << pkt.getSize() << std::endl;
}

namespace {

struct SendableMedia
{
	const std::string &name;
	const std::string &path;
	std::string data;

	SendableMedia(const std::string &name, const std::string &path,
			std::string &&data):
		name(name), path(path), data(std::move(data))
	{}
};

}

void Server::sendRequestedMedia(session_t peer_id,
		const std::unordered_set<std::string> &tosend)
{
	auto *client = getClient(peer_id, CS_DefinitionsSent);
	assert(client);

	const bool compress = client->net_proto_version >= 48;

	infostream << "Server::sendRequestedMedia(): Sending "
		<< tosend.size() << " files to " << client->getName()
		<< (compress ? " (compressed)" : "") << std::endl;

	/* Read files and prepare bunches */

	// Put 5KB in one bunch (this is not accurate)
	// This is a tradeoff between burdening the network with too many packets
	// and burdening it with too large split packets.
	const u32 bytes_per_bunch = 5000;

	std::vector<std::vector<SendableMedia>> file_bunches;
	file_bunches.emplace_back();

	// Note that applying a "real" bin packing algorithm here is not necessarily
	// an improvement (might even perform worse) since games usually have lots
	// of files larger than 5KB and the current algorithm already minimizes
	// the amount of bunches quite well (at the expense of overshooting).

	u32 file_size_bunch_total = 0;
	size_t bytes_compressed = 0, bytes_uncompressed = 0;
	for (const std::string &name : tosend) {
		auto it = m_media.find(name);

		if (it == m_media.end()) {
			warningstream << "Server::sendRequestedMedia(): Client asked for "
					"unknown file \"" << name << "\"" << std::endl;
			continue;
		}
		const auto &m = it->second;

		// Ephemeral dynamic media may have duplicate filenames. So we can't check it.
		if (!m.ephemeral) {
			if (!client->markMediaSent(name)) {
				warningstream << "Server::sendRequestedMedia(): Client has "
					"requested \"" << name << "\" before, not sending it again."
					<< std::endl;
				continue;
			}
		}

		// Read data
		std::string data;
		if (!fs::ReadFile(m.path, data, true)) {
			continue;
		}
		bytes_uncompressed += data.size();
		if (compress) {
			// Zstd is very fast and can handle non-compressible data efficiently
			// so we can just throw it at every file. Still we don't want to
			// spend too much here, so we use the lowest compression level.
			std::ostringstream oss(std::ios::binary);
			compressZstd(data, oss, 1);
			data = oss.str();
		}
		bytes_compressed += data.size();

		// Put in list
		file_size_bunch_total += data.size();
		file_bunches.back().emplace_back(name, m.path, std::move(data));

		// Start next bunch if got enough data
		if(file_size_bunch_total >= bytes_per_bunch) {
			file_bunches.emplace_back();
			file_size_bunch_total = 0;
		}
	}

	/* Create and send packets */

	const u16 num_bunches = file_bunches.size();
	for (u16 i = 0; i < num_bunches; i++) {
		auto &bunch = file_bunches[i];
		NetworkPacket pkt(TOCLIENT_MEDIA, 4 + 0, peer_id);

		const u32 bunch_size = bunch.size();
		pkt << num_bunches << i << bunch_size;

		for (auto &j : bunch) {
			pkt << j.name;
			pkt.putLongString(j.data);
		}
		bunch.clear(); // free memory early

		verbosestream << "Server::sendRequestedMedia(): bunch "
				<< i << "/" << num_bunches
				<< " files=" << bunch_size
				<< " size="  << pkt.getSize() << std::endl;
		Send(&pkt);
	}

	if (compress && bytes_uncompressed != 0) {
		int percent = bytes_compressed / (float)bytes_uncompressed * 100;
		int diff = (int)bytes_compressed - (int)bytes_uncompressed;
		infostream << "Server::sendRequestedMedia(): size after compression "
			<< percent << "% (" << (diff > 0 ? '+' : '-') << std::abs(diff)
			<< " byte)" << std::endl;
	}
}

namespace {
	// unordered_map erase_if is only C++20
	template <typename C, typename F>
	void erase_if(C &container, F predicate) {
		for (auto it = container.begin(); it != container.end(); ) {
			if (predicate(*it))
				it = container.erase(it);
			else
				++it;
		}
	}
}

void Server::stepPendingDynMediaCallbacks(float dtime)
{
	EnvAutoLock lock(this);

	erase_if(m_pending_dyn_media, [&] (decltype(m_pending_dyn_media)::value_type &it) {
		auto &[token, state] = it;

		state.expiry_timer -= dtime;
		if (!state.waiting_players.empty() && state.expiry_timer >= 0)
			return false;

		const auto &name = state.filename;
		if (!name.empty()) {
			auto it = m_media.find(name);
			sanity_check(it != m_media.end());
			sanity_check(it->second.ephemeral);

			fs::DeleteSingleFileOrEmptyDirectory(it->second.path, true);
			m_media.erase(it);
		}
		return true;
	});
}

void Server::SendMinimapModes(session_t peer_id,
		std::vector<MinimapMode> &modes, size_t wanted_mode)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_MINIMAP_MODES, 0, peer_id);
	pkt << (u16)modes.size() << (u16)wanted_mode;

	for (auto &mode : modes)
		pkt << (u16)mode.type << mode.label << mode.size << mode.texture << mode.scale;

	Send(&pkt);
}

void Server::sendDetachedInventory(Inventory *inventory, const std::string &name, session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_DETACHED_INVENTORY, 0, peer_id);
	pkt << name;

	if (!inventory) {
		pkt << false; // Remove inventory
	} else {
		pkt << true; // Update inventory

		// Serialization & NetworkPacket isn't a love story
		std::ostringstream os(std::ios_base::binary);
		inventory->serialize(os);
		inventory->setModified(false);

		const std::string &os_str = os.str();
		pkt << static_cast<u16>(os_str.size()); // HACK: to keep compatibility with 5.0.0 clients
		pkt.putRawString(os_str);
	}

	// every client that has the atlas (SendPlanet, with its definitions), not
	// only active ones: a joining player's spawn puts its tile before they are
	if (peer_id == PEER_ID_INEXISTENT)
		m_clients.sendToAll(&pkt, CS_DefinitionsSent);
	else
		Send(&pkt);
}

void Server::sendDetachedInventories(session_t peer_id)
{
	if (peer_id == PEER_ID_INEXISTENT) {
		assert(false);
		return;
	}

	std::string peer_name = getClient(peer_id, CS_Created)->getName();

	auto send_cb = [&] (const std::string &name, Inventory *inv, const std::string &owner) {
		// don't send what we don't own
		if (!owner.empty() && owner != peer_name)
			return;

		assert(inv);
		sendDetachedInventory(inv, name, peer_id);
	};

	m_inventory_mgr->sendDetachedInventories(peer_name, false, send_cb);
}

void Server::sendDetachedInventories()
{
	auto send_cb = [this] (const std::string &name, Inventory *inv, const std::string &owner) {
		session_t peer_id = PEER_ID_INEXISTENT;
		if (!owner.empty()) {
			auto *player = m_env->getPlayer(owner);
			peer_id = player ? player->getPeerId() : PEER_ID_INEXISTENT;
			if (peer_id == PEER_ID_INEXISTENT)
				return; // owner is not online; skip
		}

		assert(inv);
		sendDetachedInventory(inv, name, peer_id);
	};

	m_inventory_mgr->sendDetachedInventories("", true, send_cb);
}

/*
	Something random
*/

void Server::HandlePlayerDeath(PlayerSAO *playersao, const PlayerHPChangeReason &reason)
{
	infostream << "Server::DiePlayer(): Player "
			<< playersao->getPlayer()->getName()
			<< " dies" << std::endl;

	playersao->clearParentAttachment();

	// Terra Firma (sprint 119: was builtin's death_screen.lua): shown they died; they
	// respawn when they choose (handleCommand_Respawn)
	SendDeathscreen(playersao->getPeerID());
}

void Server::SendDeathscreen(session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_DEATHSCREEN_LEGACY, 0, peer_id);
	Send(&pkt);
}

void Server::DenyAccess(session_t peer_id, AccessDeniedCode reason,
		std::string_view custom_reason, bool reconnect)
{
	SendAccessDenied(peer_id, reason, custom_reason, reconnect);
	m_clients.event(peer_id, CSE_SetDenied);
	DisconnectPeer(peer_id);
}

void Server::kickAllPlayers(AccessDeniedCode reason,
		const std::string &str_reason, bool reconnect)
{
	std::vector<session_t> clients = m_clients.getClientIDs();
	for (const session_t client_id : clients) {
		DenyAccess(client_id, reason, str_reason, reconnect);
	}
}

void Server::DisconnectPeer(session_t peer_id)
{
	m_con->DisconnectPeer(peer_id);
}

void Server::setGate(std::unique_ptr<online::Gate> gate)
{
	m_gate = std::move(gate);
}

void Server::setStrayHandler(con::IConnection::StrayHandler handler)
{
	m_con->setStrayHandler(std::move(handler));
}

void Server::admit(session_t peer_id, bool ok, const std::string &reason)
{
	RemoteClient *client = getClientNoEx(peer_id, CS_Invalid);
	if (!client || client->getState() != CS_HelloSent)
		return; // gone while their ticket was checked
	const std::string name = client->getName();
	if (!ok) {
		actionstream << "Server: " << name << " was refused: " << reason << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING, reason);
		return;
	}
	// A ticket is the account: what they may do is the world's access (server/access)
	acceptAuth(peer_id);
}

void Server::acceptAuth(session_t peer_id)
{
	NetworkPacket resp_pkt(TOCLIENT_AUTH_ACCEPT, 12 + 8 + 4, peer_id);
	resp_pkt << v3f() << (u64) m_env->getServerMap().getSeed()
			<< g_settings->getFloat("dedicated_server_step");
	Send(&resp_pkt);
	m_clients.event(peer_id, CSE_AuthAccept);
}

void Server::DeleteClient(session_t peer_id, ClientDeletionReason reason)
{
	EnvAutoLock envlock(this);
	std::wstring message;
	{
		/*
			Clear references to playing sounds
		*/
		for (auto i = m_playing_sounds.begin(); i != m_playing_sounds.end();) {
			ServerPlayingSound &psound = i->second;
			psound.clients.erase(peer_id);
			if (psound.clients.empty())
				i = m_playing_sounds.erase(i);
			else
				++i;
		}

		// clear formspec info so the next client can't abuse the current state
		m_formspec_state_data.erase(peer_id);

		RemotePlayer *player = m_env->getPlayer(peer_id);

		/* Run scripts and remove from environment */
		if (player) {
			PlayerSAO *playersao = player->getPlayerSAO();
			assert(playersao);

			// inform connected clients
			const std::string &player_name = player->getName();
			NetworkPacket notice(TOCLIENT_UPDATE_PLAYER_LIST, 0, PEER_ID_INEXISTENT);
			// (u16) 1 + std::string represents a vector serialization representation
			notice << (u8) PLAYER_LIST_REMOVE  << (u16) 1 << player_name;
			m_clients.sendToAll(&notice);
			// Terra Firma: was builtin's (sprint 119)
			SendChatMessage(PEER_ID_INEXISTENT, ChatMessage(CHATMESSAGE_TYPE_ANNOUNCE, utf8_to_wide("*** " +
					player_name + (reason == CDR_TIMEOUT ? " left the game (timed out)." : " left the game."))));
			if (m_sidecar)
				m_sidecar->left(player_name);
			m_access->left(player_name); // grants last while they're here

			playersao->disconnected();
		}

		/*
			Print out action
		*/
		{
			if (player && reason != CDR_DENY) {
				std::ostringstream os(std::ios_base::binary);
				std::vector<session_t> clients = m_clients.getClientIDs();

				for (const session_t client_id : clients) {
					// Get player
					RemotePlayer *player = m_env->getPlayer(client_id);
					if (!player)
						continue;

					// Get name of player
					os << player->getName() << " ";
				}

				std::string name = player->getName();
				actionstream << name << " "
						<< (reason == CDR_TIMEOUT ? "times out." : "leaves game.")
						<< " List of players: " << os.str() << std::endl;
				if (m_admin_chat)
					m_admin_chat->outgoing_queue.push_back(
						new ChatEventNick(CET_NICK_REMOVE, name));
			}
		}

		m_clients.DeleteClient(peer_id);
	}

	// Send leave chat message to all remaining clients
	if (!message.empty()) {
		SendChatMessage(PEER_ID_INEXISTENT,
				ChatMessage(CHATMESSAGE_TYPE_ANNOUNCE, message));
	}
}

void Server::UpdateCrafting(RemotePlayer *player)
{
	InventoryList *clist = player->inventory.getList("craft");
	if (!clist || clist->getSize() == 0)
		return;

	if (!clist->checkModified())
		return;

	// Get a preview for crafting
	ItemStack preview;
	std::vector<ItemStack> output_replacements;
	getCraftingResult(&player->inventory, preview, output_replacements, false, this);

	InventoryList *plist = player->inventory.getList("craftpreview");
	if (plist && plist->getSize() >= 1) {
		// Put the new preview in
		plist->changeItem(0, preview);
	}
}

void Server::handleChatInterfaceEvent(ChatEvent *evt)
{
	if (evt->type == CET_NICK_ADD) {
		// The terminal informed us of its nick choice
		m_admin_nick = ((ChatEventNick *)evt)->nick;
	} else {
		assert(evt->type == CET_CHAT);
		handleAdminChat((ChatEventChat *)evt);
	}
}

std::wstring Server::handleChat(const std::string &name,
	std::wstring wmessage, bool check_shout_priv, RemotePlayer *player)
{
	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			std::string("player:") + name);

	if (g_settings->getBool("strip_color_codes"))
		wmessage = unescape_enriched(wmessage);

	wmessage = trim(wmessage);

	if (player) {
		switch (player->canSendChatMessage()) {
		case RPLAYER_CHATRESULT_FLOODING: {
			std::wstringstream ws;
			ws << L"You cannot send more messages. You are limited to "
					<< g_settings->getFloat("chat_message_limit_per_10sec")
					<< L" messages per 10 seconds.";
			return ws.str();
		}
		case RPLAYER_CHATRESULT_KICK:
			DenyAccess(player->getPeerId(), SERVER_ACCESSDENIED_CUSTOM_STRING,
				"You have been kicked due to message flooding.");
			return L"";
		case RPLAYER_CHATRESULT_OK:
			break;
		default:
			FATAL_ERROR("Unhandled chat filtering result found.");
		}
	}

	if (m_max_chatmessage_length > 0
			&& wmessage.length() > m_max_chatmessage_length) {
		return L"Your message exceed the maximum chat message limit set on the server. "
				L"It was refused. Send a shorter message";
	}

	auto message = wide_to_utf8(wmessage);
	if (message.empty())
		return L"";

	if (message.find_first_of("\r\n") != std::string::npos)
		return L"Newlines are not permitted in chat messages";

	// Terra Firma: a command is the engine's or a mod's (server/commands, sprint 119)
	if (message[0] == '/')
		return utf8_to_wide(m_commands->run(name, player, message.substr(1)));

	// Line to send
	std::wstring line;
	// Whether to send line to the player that sent the message, or to all players
	bool broadcast_line = true;

	if (check_shout_priv && !checkPriv(name, "core:shout")) {
		line += L"-!- You don't have permission to shout.";
		broadcast_line = false;
	} else {
		line += L"<" + utf8_to_wide(name) + L"> " + wmessage;
	}

	/*
		Tell calling method to send the message to sender
	*/
	if (!broadcast_line)
		return line;

	/*
		Send the message to others
	*/
	actionstream << "CHAT: " << wide_to_utf8(unescape_enriched(line)) << std::endl;

	// who said it, apart from what: the page sets the name itself (client/app/shell/bridge/chat.h)
	ChatMessage chatmsg(CHATMESSAGE_TYPE_NORMAL, wmessage, utf8_to_wide(name));

	SendChatMessage(PEER_ID_INEXISTENT, chatmsg);

	return L"";
}

void Server::handleAdminChat(const ChatEventChat *evt)
{
	std::string name = evt->nick;
	std::wstring wmessage = evt->evt_msg;

	std::wstring answer = handleChat(name, wmessage);

	// If asked to send answer to sender
	if (!answer.empty()) {
		m_admin_chat->outgoing_queue.push_back(new ChatEventChat("", answer));
	}
}

RemoteClient *Server::getClient(session_t peer_id, ClientState state_min)
{
	RemoteClient *client = getClientNoEx(peer_id,state_min);
	if(!client)
		throw ClientNotFoundException("Client not found");

	return client;
}
RemoteClient *Server::getClientNoEx(session_t peer_id, ClientState state_min)
{
	return m_clients.getClientNoEx(peer_id, state_min);
}

std::string Server::getPlayerName(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return "[id="+itos(peer_id)+"]";
	return player->getName();
}

PlayerSAO *Server::getPlayerSAO(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return NULL;
	return player->getPlayerSAO();
}

std::string Server::getStatusString()
{
	std::ostringstream os(std::ios_base::binary);
	os << "# Server: ";
	// Version
	os << "version: " << g_version_string;
	// Game
	os << " | game: " << (m_gamespec.title.empty() ? m_gamespec.id : m_gamespec.title);
	// Uptime
	os << " | uptime: " << duration_to_string((int) m_uptime_counter->get());
	// Max lag estimate
	os << " | max lag: " << std::setprecision(3);
	os << (m_env ? m_env->getMaxLagEstimate() : 0) << "s";

	// Information about clients
	bool first = true;
	os << " | clients: ";
	if (m_env) {
		std::vector<std::string> player_names = m_clients.getPlayerNames();

		std::sort(player_names.begin(), player_names.end());

		for (const std::string& name : player_names) {
			if (!first)
				os << ", ";
			else
				first = false;
			os << name;
		}
	}

	if (m_env && !((ServerMap*)(&m_env->getMap()))->isSavingEnabled())
		os << std::endl << "# Server: " << " WARNING: Map saving is disabled.";

	if (!g_settings->get("motd").empty())
		os << std::endl << "# Server: " << g_settings->get("motd");

	return os.str();
}

std::set<std::string> Server::getPlayerEffectivePrivs(const std::string &name)
{
	// Terra Firma: their roles' and grants' (server/access), and creative's (sprint 119)
	std::set<std::string> privs = m_access->permissions(name);
	terraCreativePrivs(privs);
	return privs;
}

bool Server::checkPriv(const std::string &name, const std::string &priv)
{
	// an admin may do anything, a mod's permissions too: so ask, rather than look in a list
	return m_access->can(name, priv) || getPlayerEffectivePrivs(name).count(priv) != 0;
}

void Server::reportPrivsModified(const std::string &name)
{
	if (name.empty()) {
		std::vector<session_t> clients = m_clients.getClientIDs();
		for (const session_t client_id : clients) {
			RemotePlayer *player = m_env->getPlayer(client_id);
			reportPrivsModified(player->getName());
		}
	} else {
		RemotePlayer *player = m_env->getPlayer(name.c_str());
		if (!player)
			return;
		SendPlayerPrivileges(player->getPeerId());
		PlayerSAO *sao = player->getPlayerSAO();
		if(!sao)
			return;
		sao->updatePrivileges(getPlayerEffectivePrivs(name));
	}
}

void Server::reportInventoryFormspecModified(const std::string &name)
{
	RemotePlayer *player = m_env->getPlayer(name.c_str());
	if (!player)
		return;
	SendPlayerInventoryFormspec(player->getPeerId());
}

void Server::reportFormspecPrependModified(const std::string &name)
{
	RemotePlayer *player = m_env->getPlayer(name.c_str());
	if (!player)
		return;
	SendPlayerFormspecPrepend(player->getPeerId());
}

void Server::setIpBanned(const std::string &ip, const std::string &name)
{
	m_banmanager->add(ip, name);

	auto clients = m_clients.getClientIDs(CS_Created);
	for (const auto peer_id : clients) {
		denyIfBanned(peer_id);
	}
}

void Server::unsetIpBanned(const std::string &ip_or_name)
{
	m_banmanager->remove(ip_or_name);
}

std::string Server::getBanDescription(const std::string &ip_or_name)
{
	return m_banmanager->getBanDescription(ip_or_name);
}

bool Server::denyIfBanned(session_t peer_id)
{
	Address address = getPeerAddress(peer_id);
	std::string addr_s = address.serializeString();

	if (m_banmanager->isIpBanned(addr_s)) {
		std::string ban_name = m_banmanager->getBanName(addr_s);
		actionstream << "Server: A banned client tried to connect from "
			<< addr_s << "; banned name was " << ban_name << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING,
			"Your IP is banned. Banned name was " + ban_name);
		return true;
	}
	return false;
}

bool Server::checkUserLimit(const std::string &player_name, const std::string &addr_s)
{
	if (!m_clients.isUserLimitReached())
		return false;
	if (player_name == g_settings->get("name")) // admin can always join
		return false;
	return true;
}

void Server::notifyPlayer(const char *name, const std::wstring &msg)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return;

	if (m_admin_nick == name && !m_admin_nick.empty()) {
		m_admin_chat->outgoing_queue.push_back(new ChatEventChat("", msg));
	}

	RemotePlayer *player = m_env->getPlayer(name);
	if (!player)
		return;

	SendChatMessage(player->getPeerId(), ChatMessage(msg));
}

bool Server::showFormspec(const char *playername, const std::string &formspec,
	const std::string &formname)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return false;

	RemotePlayer *player = m_env->getPlayer(playername);
	if (!player)
		return false;

	// To allow re-sending the same inventory formspec.
	player->inventory_formspec_overridden = formname.empty() && !formspec.empty();

	SendShowFormspecMessage(player->getPeerId(), formspec, formname);
	return true;
}

u32 Server::hudAdd(RemotePlayer *player, std::unique_ptr<HudElement> form)
{
	if (!player)
		return -1;

	u32 id = player->hud.add(std::move(form));

	SendHUDAdd(player->getPeerId(), id, player->hud.get(id));

	return id;
}

bool Server::hudRemove(RemotePlayer *player, u32 id) {
	if (!player)
		return false;

	if (!player->hud.remove(id))
		return false;

	SendHUDRemove(player->getPeerId(), id);
	return true;
}

bool Server::hudChange(RemotePlayer *player, u32 id, HudElementStat stat, void *data)
{
	if (!player)
		return false;

	SendHUDChange(player->getPeerId(), id, stat, data);
	return true;
}

bool Server::hudSetFlags(RemotePlayer *player, u32 flags, u32 mask)
{
	if (!player)
		return false;

	u32 new_hud_flags = (player->hud_flags & ~mask) | flags;
	if (new_hud_flags == player->hud_flags) // no change
		return true;

	SendHUDSetFlags(player->getPeerId(), flags, mask);
	player->hud_flags = new_hud_flags;

	PlayerSAO* playersao = player->getPlayerSAO();

	if (!playersao)
		return false;

	return true;
}

bool Server::hudSetHotbarItemcount(RemotePlayer *player, s32 hotbar_itemcount)
{
	if (!player)
		return false;

	if (hotbar_itemcount <= 0 || hotbar_itemcount > HUD_HOTBAR_ITEMCOUNT_MAX)
		return false;

	if (player->getHotbarItemcount() == hotbar_itemcount)
		return true;

	player->setHotbarItemcount(hotbar_itemcount);
	std::ostringstream os(std::ios::binary);
	writeS32(os, hotbar_itemcount);
	SendHUDSetParam(player->getPeerId(), HUD_PARAM_HOTBAR_ITEMCOUNT, os.str());
	return true;
}

void Server::hudSetHotbarImage(RemotePlayer *player, const std::string &name)
{
	if (!player)
		return;

	if (player->getHotbarImage() == name)
		return;

	player->setHotbarImage(name);
	SendHUDSetParam(player->getPeerId(), HUD_PARAM_HOTBAR_IMAGE, name);
}

void Server::hudSetHotbarSelectedImage(RemotePlayer *player, const std::string &name)
{
	if (!player)
		return;

	if (player->getHotbarSelectedImage() == name)
		return;

	player->setHotbarSelectedImage(name);
	SendHUDSetParam(player->getPeerId(), HUD_PARAM_HOTBAR_SELECTED_IMAGE, name);
}

Address Server::getPeerAddress(session_t peer_id)
{
	return getClient(peer_id, CS_Invalid)->getAddress();
}

void Server::setLocalPlayerAnimations(RemotePlayer *player,
		v2f animation_frames[4], f32 frame_speed)
{
	sanity_check(player);
	player->setLocalAnimations(animation_frames, frame_speed);
	SendLocalPlayerAnimations(player->getPeerId(), animation_frames, frame_speed);
}

void Server::setPlayerEyeOffset(RemotePlayer *player, v3f first, v3f third, v3f third_front)
{
	sanity_check(player);
	if (std::tie(player->eye_offset_first, player->eye_offset_third,
			player->eye_offset_third_front) == std::tie(first, third, third_front))
		return; // no change

	player->eye_offset_first = first;
	player->eye_offset_third = third;
	player->eye_offset_third_front = third_front;
	SendEyeOffset(player->getPeerId(), first, third, third_front);
}

void Server::setSky(RemotePlayer *player, const SkyboxParams &params)
{
	sanity_check(player);
	player->setSky(params);
	SendSetSky(player->getPeerId(), params);
}

void Server::setSun(RemotePlayer *player, const SunParams &params)
{
	sanity_check(player);
	player->setSun(params);
	SendSetSun(player->getPeerId(), params);
}

void Server::setMoon(RemotePlayer *player, const MoonParams &params)
{
	sanity_check(player);
	player->setMoon(params);
	SendSetMoon(player->getPeerId(), params);
}

void Server::setStars(RemotePlayer *player, const StarParams &params)
{
	sanity_check(player);
	player->setStars(params);
	SendSetStars(player->getPeerId(), params);
}

void Server::setClouds(RemotePlayer *player, const CloudParams &params)
{
	sanity_check(player);
	player->setCloudParams(params);
	SendCloudParams(player->getPeerId(), params);
}

void Server::overrideDayNightRatio(RemotePlayer *player, bool do_override,
	float ratio)
{
	sanity_check(player);
	player->overrideDayNightRatio(do_override, ratio);
	SendOverrideDayNightRatio(player->getPeerId(), do_override, ratio);
}

void Server::setLighting(RemotePlayer *player, const Lighting &lighting)
{
	sanity_check(player);
	player->setLighting(lighting);
	SendSetLighting(player->getPeerId(), lighting);
}

void Server::notifyPlayers(const std::wstring &msg)
{
	SendChatMessage(PEER_ID_INEXISTENT, ChatMessage(msg));
}

void Server::spawnParticle(const std::string &playername,
	const ParticleParameters &p)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return;

	m_particles_to_send[playername].push_back(p);
}

u32 Server::addParticleSpawner(const ParticleSpawnerParameters &p,
	ServerActiveObject *attached, const std::string &to_player,
	const std::string &exclude_player)
{
	if (!m_env)
		return -1;

	u16 attached_id = attached ? attached->getId() : 0;

	u32 id;
	if (attached_id == 0)
		id = m_env->addParticleSpawner(p.time);
	else
		id = m_env->addParticleSpawner(p.time, attached_id);

	SendAddParticleSpawner(to_player, exclude_player, p, attached_id, id);
	return id;
}

void Server::deleteParticleSpawner(const std::string &playername, u32 id)
{
	if (!m_env)
		throw ServerError("Can't delete particle spawners during initialisation!");

	session_t peer_id = PEER_ID_INEXISTENT;
	if (!playername.empty()) {
		RemotePlayer *player = m_env->getPlayer(playername.c_str());
		if (!player)
			return;
		peer_id = player->getPeerId();
	}

	// FIXME: we don't track which client still knows about this spawner, so
	// just deleting it entirely is problematic!
	// We also don't check if the ID is even in use. FAIL!
	m_env->deleteParticleSpawner(id);

	SendDeleteParticleSpawner(peer_id, id);
}

namespace {
	std::string writeToTempFile(std::string_view content)
	{
		auto filepath = fs::CreateTempFile();
		if (filepath.empty())
			return "";
		auto os = open_ofstream(filepath.c_str(), true);
		if (!os.good())
			return "";
		os << content;
		os.close();
		if (os.fail()) {
			fs::DeleteSingleFileOrEmptyDirectory(filepath);
			return "";
		}
		return filepath;
	}
}

bool Server::dynamicAddMedia(const DynamicMediaArgs &a)
{
	if (!m_env && (!a.to_player.empty() || a.ephemeral)) {
		errorstream << "Server: "
			"adding ephemeral or player-specific media at startup is nonsense"
			<< std::endl;
		return false;
	}

	// Deal with file -or- data, as provided
	// (Note: caller must ensure validity, so sanity_check is okay)
	std::string filename = a.filename;
	std::string filepath;
	if (a.filepath) {
		sanity_check(!a.data);
		filepath = *a.filepath;
		if (filename.empty())
			filename = fs::GetFilenameFromPath(filepath.c_str());
	} else {
		sanity_check(a.data);
		sanity_check(!filename.empty());

		// Write the file to disk. addMediaFile() will read it right back but this
		// is the best way without turning the media loading code into spaghetti.
		filepath = writeToTempFile(*a.data);
		if (filepath.empty()) {
			errorstream << "Server: failed writing media file \""
				<< filename << "\" to disk" << std::endl;
			return false;
		}
		verbosestream << "Server: \"" << filename << "\" temporarily written to "
			<< filepath << std::endl;
	}

	{
		auto it = m_media.find(filename);
		if (it == m_media.end()) {
			// standard case
		} else if (a.ephemeral || it->second.ephemeral || it->second.path != filepath) {
			// If the path is the same we can safely allow adding the same file twice.
			// Note that we already trust mods to not to modify files after the fact.
			// Ephemeral files are excluded too, because currently each
			// PendingDynamicMediaCallback "owns" the matching m_media[] entry
			// so that would mess up.
			errorstream << "Server::dynamicAddMedia(): file \"" << filename
				<< "\" already exists in media list" << std::endl;
			return false;
		}
	}

	// Load the file and add it to our media cache
	std::string filedata, raw_hash;
	bool ok = addMediaFile(filename, filepath, &filedata, &raw_hash);
	if (!ok) {
		if (a.data) // file was temporary
			fs::DeleteSingleFileOrEmptyDirectory(filepath);
		return false;
	}
	assert(!filedata.empty());

	const auto &media_it = m_media.find(filename);
	assert(media_it != m_media.end());

	if (a.ephemeral) {
		if (!a.data) {
			// Create a copy of the file and swap out the path, this removes the
			// requirement that mods keep the file accessible at the original path.
			filepath = writeToTempFile(filedata);
			if (filepath.empty()) {
				errorstream << "Server: failed creating a copy of media file \""
					<< filename << "\"" << std::endl;
				m_media.erase(filename);
				return false;
			}
			verbosestream << "Server: \"" << filename << "\" temporarily copied to "
				<< filepath << std::endl;
			media_it->second.path = filepath;
		}

		media_it->second.no_announce = true;
		media_it->second.ephemeral = true;
		// stepPendingDynMediaCallbacks will clean the file up later
	} else if (a.data) {
		// data is in a temporary file but not ephemeral, so the cleanup point
		// is different.
		media_it->second.delete_at_shutdown = true;
	}
	if (!a.to_player.empty()) {
		// only sent to one player (who must be online), so shouldn't announce.
		media_it->second.no_announce = true;
	}

	std::unordered_set<session_t> delivered, waiting;

	// Push file to existing clients
	if (m_env) {
		NetworkPacket pkt(TOCLIENT_MEDIA_PUSH, 0);
		pkt << raw_hash << filename;
		// NOTE: the meaning of this bit was accidentally inverted between proto 39 and 40,
		// when dynamic_add_media v2 was added. As of 5.12.0 the server sends it correctly again.
		// Compatibility code on the client-side was not added.
		pkt << static_cast<bool>(a.client_cache);

		NetworkPacket legacy_pkt = pkt;

		// Newer clients get asked to fetch the file (asynchronous)
		pkt << a.token;
		// Older clients have an awful hack that just throws the data at them
		legacy_pkt.putLongString(filedata);

		ClientInterface::AutoLock clientlock(m_clients);
		for (auto &pair : m_clients.getClientList()) {
			if (pair.second->getState() == CS_DefinitionsSent && !a.ephemeral) {
				/*
					If a client is in the DefinitionsSent state it is too late to
					transfer the file via sendMediaAnnouncement() but at the same
					time the client cannot accept a media push yet.
					Short of artificially delaying the joining process there is no
					way for the server to resolve this so we (currently) opt not to.
				*/
				warningstream << "The media \"" << filename << "\" (dynamic) could "
					"not be delivered to " << pair.second->getName()
					<< " due to a race condition." << std::endl;
				continue;
			}
			if (pair.second->getState() < CS_Active)
				continue;

			const auto proto_ver = pair.second->net_proto_version;
			if (proto_ver < 39)
				continue;

			const session_t peer_id = pair.second->peer_id;
			if (!a.to_player.empty() && getPlayerName(peer_id) != a.to_player)
				continue;

			if (proto_ver < 40) {
				delivered.emplace(peer_id);
				/*
					The network layer only guarantees ordered delivery inside a channel.
					Since the very next packet could be one that uses the media, we have
					to push the media over ALL channels to ensure it is processed before
					it is used. In practice this means channels 1 and 0.
				*/
				m_clients.sendCustom(peer_id, 1, &legacy_pkt, true);
				m_clients.sendCustom(peer_id, 0, &legacy_pkt, true);
			} else {
				waiting.emplace(peer_id);
				Send(peer_id, &pkt);
			}
		}
	}

	// Save all others in our pending state
	auto &state = m_pending_dyn_media[a.token];
	state.waiting_players = std::move(waiting);
	// regardless of success throw away the callback after a while
	state.expiry_timer = 60.0f;
	if (a.ephemeral)
		state.filename = filename;

	return true;
}

// actions: time-reversed list
// Return value: success/failure
bool Server::rollbackRevertActions(const std::list<RollbackAction> &actions,
		std::list<std::string> *log)
{
	infostream<<"Server::rollbackRevertActions(len="<<actions.size()<<")"<<std::endl;
	auto *map = &m_env->getServerMap();

	// Fail if no actions to handle
	if (actions.empty()) {
		assert(log);
		log->push_back("Nothing to do.");
		return false;
	}

	int num_tried = 0;
	int num_failed = 0;

	for (const RollbackAction &action : actions) {
		num_tried++;
		bool success = action.applyRevert(map, m_inventory_mgr.get(), this);
		if(!success){
			num_failed++;
			std::ostringstream os;
			os<<"Revert of step ("<<num_tried<<") "<<action.toString()<<" failed";
			infostream<<"Map::rollbackRevertActions(): "<<os.str()<<std::endl;
			if (log)
				log->push_back(os.str());
		}else{
			std::ostringstream os;
			os<<"Successfully reverted step ("<<num_tried<<") "<<action.toString();
			infostream<<"Map::rollbackRevertActions(): "<<os.str()<<std::endl;
			if (log)
				log->push_back(os.str());
		}
	}

	infostream<<"Map::rollbackRevertActions(): "<<num_failed<<"/"<<num_tried
			<<" failed"<<std::endl;

	// Call it done if less than half failed
	return num_failed <= num_tried/2;
}

// IGameDef interface
// Under envlock
IItemDefManager *Server::getItemDefManager()
{
	return m_itemdef;
}

const NodeDefManager *Server::getNodeDefManager()
{
	return m_nodedef;
}

ICraftDefManager *Server::getCraftDefManager()
{
	return m_craftdef;
}

u16 Server::allocateUnknownNodeId(const std::string &name)
{
	return m_nodedef->allocateDummy(name);
}

IWritableItemDefManager *Server::getWritableItemDefManager()
{
	return m_itemdef;
}

NodeDefManager *Server::getWritableNodeDefManager()
{
	return m_nodedef;
}

IWritableCraftDefManager *Server::getWritableCraftDefManager()
{
	return m_craftdef;
}

std::vector<std::string> Server::modNames() const
{
	std::vector<std::string> out;
	for (const ModReport &m : modList())
		out.push_back(m.name);
	return out;
}

std::vector<Server::ModReport> Server::modList() const
{
	std::vector<ModReport> out;
	if (m_server_folder.empty())
		return out;
	const std::string path = m_server_folder + DIR_DELIM ".terra" DIR_DELIM "manifest.json";
	std::ifstream in(path);
	Json::Value root;
	std::string errors;
	if (!in || !Json::parseFromStream(Json::CharReaderBuilder(), in, &root, &errors))
		throw BaseException("mods: " + path + " can't be read: " + errors);
	for (const Json::Value &m : root["mods"])
		out.push_back({ m["name"].asString(), m["version"].asString() });
	return out;
}

void Server::setAsyncFatalError(const std::string &error)
{
	// print error right here in the thread that set it, for clearer logging
	infostream << "setAsyncFatalError: " << error << std::endl;

	m_async_fatal_error.set(error);

	// make sure server steps stop happening immediately
	if (m_thread)
		m_thread->stop();
}

Map &Server::getMap()
{
	return m_env->getMap();
}

v3f Server::findSpawnPos()
{
	ServerMap &map = m_env->getServerMap();

	std::optional<v3f> staticSpawnPoint;
	if (g_settings->getV3FNoEx("static_spawnpoint", staticSpawnPoint) && staticSpawnPoint.has_value())
	{
		return *staticSpawnPoint * BS;
	}

	// Terra Firma: on the planet, land nearest the equator (sprint 119: was tf_planet's)
	if (m_atlas)
		return terraSpawn(this);

	v3f nodeposf;

	bool is_good = false;
	// Limit spawn range to mapgen edges (determined by 'mapgen_limit')
	s32 range_max = map.getMapgenParams()->getSpawnRangeMax();

	// Try to find a good place a few times
	for (s32 i = 0; i < 4000 && !is_good; i++) {
		s32 range = MYMIN(1 + i, range_max);
		// We're going to try to throw the player to this position
		v2s16 nodepos2d = v2s16(
			-range + myrand_range(0, range*2),
			-range + myrand_range(0, range*2));
		// Get spawn level at point
		s16 spawn_level = m_emerge->getSpawnLevelAtPoint(nodepos2d);
		// Continue if MAX_MAP_GENERATION_LIMIT was returned by the mapgen to
		// signify an unsuitable spawn position, or if outside limits.
		if (spawn_level >= MAX_MAP_GENERATION_LIMIT ||
				spawn_level <= -MAX_MAP_GENERATION_LIMIT)
			continue;

		v3s16 nodepos(nodepos2d.X, spawn_level, nodepos2d.Y);
		// Consecutive empty nodes
		s32 air_count = 0;

		// Search upwards from 'spawn level' for 2 consecutive empty nodes, to
		// avoid obstructions in already-generated mapblocks.
		// In ungenerated mapblocks consisting of 'ignore' nodes, there will be
		// no obstructions, but mapgen decorations are generated after spawn so
		// the player may end up inside one.
		for (s32 i = 0; i < 8; i++) {
			v3s16 blockpos = getNodeBlockPos(nodepos);
			map.emergeBlock(blockpos, true);
			content_t c = map.getNode(nodepos).getContent();

			// In generated mapblocks allow spawn in all 'airlike' drawtype nodes.
			// In ungenerated mapblocks allow spawn in 'ignore' nodes.
			if (m_nodedef->get(c).drawtype == NDT_AIRLIKE || c == CONTENT_IGNORE) {
				air_count++;
				if (air_count >= 2) {
					// Spawn in lower empty node
					nodepos.Y--;
					nodeposf = intToFloat(nodepos, BS);
					// Don't spawn the player outside map boundaries
					if (objectpos_over_limit(nodeposf))
						// Exit this loop, positions above are probably over limit
						break;

					// Good position found, cause an exit from main loop
					is_good = true;
					break;
				}
			} else {
				air_count = 0;
			}
			nodepos.Y++;
		}
	}

	if (is_good)
		return nodeposf;

	// No suitable spawn point found, return fallback 0,0,0
	return v3f(0.0f, 0.0f, 0.0f);
}

void Server::requestShutdown(const std::string &msg, bool reconnect, float delay)
{
	if (delay == 0.0f) {
	// No delay, shutdown immediately
		m_shutdown_state.is_requested = true;
		// only print to the infostream, a chat message saying
		// "Server Shutting Down" is sent when the server destructs.
		infostream << "*** Immediate Server shutdown requested." << std::endl;
	} else if (delay < 0.0f && m_shutdown_state.isTimerRunning()) {
		// Negative delay, cancel shutdown if requested
		m_shutdown_state.reset();

		const char *s = "*** Server shutdown canceled.";

		infostream << s << std::endl;
		SendChatMessage(PEER_ID_INEXISTENT, utf8_to_wide(s));
		// m_shutdown_state already handled, skip.
		return;
	} else if (delay > 0.0f) {
	// Positive delay, tell the clients when the server will shut down
		std::ostringstream oss;

		oss << "*** Server shutting down in "
			<< duration_to_string(myround(delay)) << ".";

		infostream << oss.str() << std::endl;
		SendChatMessage(PEER_ID_INEXISTENT, utf8_to_wide(oss.str()));
	}

	m_shutdown_state.trigger(delay, msg, reconnect);
}

std::unique_ptr<PlayerSAO> Server::emergePlayer(const char *name, session_t peer_id,
	u16 proto_version)
{
	/*
		Try to get an existing player
	*/
	// Also match a disconnected player that is still pending removal
	RemotePlayer *player = m_env->getPlayer(name, true);

	// If player is already connected, cancel
	if (player) {
		infostream<<"emergePlayer(): Player already connected"<<std::endl;
		return NULL;
	}

	/*
		If player with the wanted peer_id already exists, cancel.
	*/
	if (m_env->getPlayer(peer_id)) {
		infostream<<"emergePlayer(): Player with wrong name but same"
				" peer_id already exists"<<std::endl;
		return NULL;
	}

	/*
		Object construction sequence/hierarchy
		--------------------------------------
		1. RemoteClient (tightly connection-bound)
		2. RemotePlayer (controls, in-game appearance)
		3. PlayerSAO (movable object in-game)
			PlayerSAO controls the peer_id assignment of RemotePlayer,
			indicating whether the player is ready

		Destruction sequence
		--------------------
		1. PlayerSAO pending removal flag
		2. PlayerSAO save data before free
		3. RemotePlayer, then PlayerSAO freed
		4. RemoteClient freed
	*/

	if (!player) {
		player = new RemotePlayer(name, idef());
	}

	// Load player
	auto playersao = m_env->loadPlayer(player, peer_id);
	// Terra Firma: back where they were on the planet, whatever slot of the
	// map's window holds it now (keepPlanetWindow keeps it)
	if (m_pager)
		if (const std::string &at = playersao->getMeta().getString("planet_cell"); !at.empty()) {
			char id[64];
			int region;
			long long i, j, k;
			float ox, oy, oz;
			if (std::sscanf(at.c_str(), "%63s %d %lld %lld %lld %f %f %f", id, &region, &i, &j, &k, &ox, &oy, &oz) != 8)
				throw ServerError("player " + std::string(name) + "'s planet_cell can't be read: " + at);
			const int body = bodyIndex(id);
			const auto [kind, index] = m_atlas->regionOfNumber(body, region);
			const planet::Atlas::Node n = planetNode(planet::Atlas::Cell(body, { kind, index, i, j, k }));
			playersao->setBasePosition(v3f(n.x + ox, n.y + oy, n.z + oz) * BS);
		}

	// Complete init with server parts
	playersao->finalize(player, getPlayerEffectivePrivs(player->getName()));
	player->protocol_version = proto_version;

	return playersao;
}

void dedicated_server_loop(Server &server, volatile std::sig_atomic_t &kill)
{
	verbosestream<<"dedicated_server_loop()"<<std::endl;

	IntervalLimiter m_profiler_interval;

	constexpr float steplen = 0.05f; // always 50 ms
	const float profiler_print_interval = g_settings->getFloat("profiler_print_interval");

	server.setStepSettings(Server::StepSettings{
			g_settings->getFloat("dedicated_server_step"),
			false
		});

	/*
	 * The dedicated server loop only provides a way to main.cpp to kill the
	 * server externally (bool &kill).
	 */

	for(;;) {
		// This is kind of a hack but can be done like this
		// because server.step() is very light
		sleep_ms((int)(steplen*1000.0f));
		server.step();

		if (server.isShutdownRequested() || kill)
			break;

		/*
			Profiler
		*/
		if (profiler_print_interval > 0) {
			if (m_profiler_interval.step(steplen, profiler_print_interval)) {
				infostream << "Profiler:" << std::endl;
				g_profiler->print(infostream);
				g_profiler->clear();
			}
		}
	}

	infostream << "Dedicated server quitting" << std::endl;
#if USE_CURL
	if (g_settings->getBool("server_announce"))
		ServerList::sendAnnounce(ServerList::AA_DELETE,
			server.m_bind_addr.getPort());
#endif

	if (profiler_print_interval > 0) {
		infostream << "Profiler:" << std::endl;
		g_profiler->print(infostream);
		g_profiler->clear();
	}
}

/*
 * Mod channels
 */


Translations *Server::getTranslationLanguage(const std::string &lang_code)
{
	if (lang_code.empty())
		return nullptr;

	auto it = server_translations.find(lang_code);
	if (it != server_translations.end())
		return &it->second; // Already loaded

	// [] will create an entry
	auto *translations = &server_translations[lang_code];
	translations->setTargetLanguage(lang_code);

	for (const auto &i : m_media) {
		auto file_lang = Translations::getFileLanguage(i.first);

		if (file_lang == lang_code || file_lang == "en") {
			std::string data;
			if (fs::ReadFile(i.second.path, data, true)) {
				translations->loadTranslation(i.first, data);
			}
		}
	}

	return translations;
}

std::unordered_map<std::string, std::string> Server::getMediaList()
{
	EnvAutoLock envlock(this);

	std::unordered_map<std::string, std::string> ret;
	for (auto &it : m_media) {
		if (it.second.no_announce)
			continue;
		ret.emplace(it.second.sha1_digest, it.second.path);
	}
	return ret;
}


u16 Server::getProtocolVersionMin()
{
	u16 min_proto = g_settings->getU16("protocol_version_min");
	if (g_settings->getBool("strict_protocol_version_checking"))
		min_proto = LATEST_PROTOCOL_VERSION;
	return rangelim(min_proto,
		SERVER_PROTOCOL_VERSION_MIN,
		LATEST_PROTOCOL_VERSION);
}

u16 Server::getProtocolVersionMax()
{
	return LATEST_PROTOCOL_VERSION;
}

void Server::setUi(RemotePlayer *player, const std::string &key, const std::string &value_json)
{
	sanity_check(player);
	NetworkPacket pkt(TOCLIENT_UI, 0, player->getPeerId());
	pkt << key;
	pkt.putLongString(value_json);
	Send(&pkt);
}

std::optional<far::Horizon> Server::horizon()
{
	const MapgenParams *params = m_emerge->mgparams;
	if (params->mgtype != MAPGEN_TERRA)
		return std::nullopt;
	far::Horizon h;
	h.code = static_cast<const MapgenTerraParams *>(params)->code;
	h.water_level = params->water_level;
	h.content.blocks = terraBlocks(m_emerge->getBiomeManager(), m_nodedef);
	h.content.species = m_terra->resolve(m_nodedef);
	return h;
}

std::unordered_map<std::string, std::string> Server::mediaPaths() const
{
	std::unordered_map<std::string, std::string> out;
	for (const auto &[name, info] : m_media)
		out.emplace(name, info.path);
	return out;
}

void Server::SendHorizon(session_t peer_id)
{
	const std::optional<far::Horizon> h = horizon();
	if (!h)
		return;
	NetworkPacket pkt(TOCLIENT_HORIZON, 0, peer_id);
	far::write(pkt, *h);
	Send(&pkt);
}

// Terra Firma: the world's atlas (every body's), so the client stores and
// draws it as the server does, and its window
void Server::SendPlanet(session_t peer_id)
{
	if (!m_atlas)
		return;
	NetworkPacket pkt(TOCLIENT_ATLAS, 0, peer_id);
	pkt << (u16)m_atlas->bodies();
	for (int b = 0; b < m_atlas->bodies(); b++)
		pkt << m_atlas->body(b).code();
	Send(&pkt);
	std::vector<int> every(planet::Atlas::SLOTS * planet::Atlas::SLOTS);
	for (int s = 0; s < (int)every.size(); s++)
		every[s] = s;
	SendWindow(peer_id, every);
}

void Server::SendWindow(session_t peer_id, const std::vector<int> &slots)
{
	const std::vector<std::optional<planet::Atlas::TileId>> window = m_atlas->window();
	NetworkPacket pkt(TOCLIENT_WINDOW, 0, peer_id);
	pkt << (u16)slots.size();
	for (int s : slots) {
		pkt << (u16)s << (u8)window.at(s).has_value();
		if (window[s])
			pkt << (u8)window[s]->body << (s32)window[s]->region << (s32)window[s]->ti << (s32)window[s]->tj;
	}
	// every client that has the atlas (SendPlanet, with its definitions), not
	// only active ones: a joining player's spawn puts its tile before they are
	if (peer_id == PEER_ID_INEXISTENT)
		m_clients.sendToAll(&pkt, CS_DefinitionsSent);
	else
		Send(&pkt);
}

int Server::bodyIndex(const std::string &id) const
{
	for (size_t b = 0; b < m_body_worlds.size(); b++)
		if (m_body_worlds[b].id == id)
			return (int)b;
	throw ServerError("this world has no body '" + id + "' to stand on");
}

planet::Atlas::Node Server::planetNode(const planet::Atlas::Cell &cell)
{
	if (!m_pager)
		throw ServerError("this world isn't a planet");
	return m_pager->node(cell, getUptime());
}

// Terra Firma (sprint 84): wheels and feet press loose ground. A wheel's
// patch is its tyre's width across, carrying its load; a foot's, a
// person's weight on a sole. The piles keep each pressing once per
// sixteenth it moves, and ignore what's off the ground.
void Server::pressGround(float dtime)
{
	constexpr float FOOT = 0.12f;      // nodes: a sole's patch, as a circle
	constexpr float PERSON = 800.f;    // N
	constexpr float PI = 3.14159265f;
	if (m_workshop)
		for (const workshop::Motion::WheelOn &w : m_workshop->motion().wheelsOn()) {
			const float r = w.contact.width / 2;
			const v3f at(w.contact.at.X, w.contact.at.Y, w.contact.at.Z);
			m_piles->press((u64(w.object) << 8) | w.contact.wheel, at, r, w.contact.load / (PI * r * r) / 1000);
		}
	for (RemotePlayer *player : m_env->getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (!sao || sao->isGone() || sao->getParent())
			continue;
		m_piles->press((u64(1) << 40) | sao->getId(), sao->getBasePosition() / BS, FOOT,
				PERSON / (PI * FOOT * FOOT) / 1000);
	}
	m_piles->step(dtime);
	g_profiler->avg("Server: piles awake", m_piles->awake());
}

void Server::groundChanged(v3s16 p, MapNode was)
{
	if (m_piles)
		m_piles->changed(p, was);
}

void Server::keepPlanetWindow()
{
	std::vector<planet::Pager::Point> points;
	for (RemotePlayer *player : m_env->getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (!sao)
			continue;
		const v3f p = sao->getBasePosition() / BS;
		const planet::Atlas::Node n = planet::Atlas::moverNode(p.X, p.Y, p.Z);
		const std::optional<planet::Atlas::Cell> c = m_atlas->cell(n);
		if (!c) // in seam (between slots, above the sky): no ground to keep
			continue;
		points.push_back({ c->body, m_atlas->body(c->body).centre(*c) });
		// where they are on which body, for when they're back (emergePlayer)
		sao->getMeta().setString("planet_cell", m_body_worlds.at(c->body).id + " " +
				std::to_string(m_atlas->regionNumber(c->body, c->region, c->index)) + " " +
				std::to_string(c->i) + " " + std::to_string(c->j) + " " + std::to_string(c->k) + " " +
				std::to_string(p.X - n.x) + " " + std::to_string(p.Y - n.y) + " " + std::to_string(p.Z - n.z));
	}
	m_pager->keep(points, getUptime());
}

void Server::setTimeSpeed(std::optional<float> speed)
{
	m_time_speed = speed;
	m_time_of_day_send_timer = 0; // clients learn the new speed on the next step
}

float Server::getTimeSpeed() const
{
	return m_time_speed ? *m_time_speed : g_settings->getFloat("time_speed");
}
