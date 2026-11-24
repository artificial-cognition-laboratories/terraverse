// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "src/client/world/session/client.h"
#include "src/client/world/player/body.h" // Terra Firma
#include "src/server/world/run/ground/ground.h" // Terra Firma

#include "src/shared/game/chat/chatmessage.h"
#include "src/client/app/clientevent.h"
#include "src/client/world/session/clientdynamicinfo.h"
#include "src/client/render/fonts/fontengine.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/app/mods/clientmedia.h"
#include "src/client/render/map/mesh_generator_thread.h"
#include "src/client/views/game/sky/particles.h"
#include "src/client/render/renderingengine.h"
#include "src/client/app/output/sound/sound.h"
#include "src/client/render/textures/texturesource.h"
#include "src/client/views/game/camera.h"
#include "src/shared/platform/system/filesys.h"
#include "src/client/app/game.h"
#include "src/shared/platform/system/gettext.h"
#include "src/shared/base/gettime.h"
#include "src/client/render/textures/guiscalingfilter.h"
#include "src/client/render/items/item_visuals_manager.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/map/block/mapblock.h"
#include "src/client/render/map/mesh/mapblock_mesh.h"
#include "src/shared/map/block/mapnode.h"
#include "src/shared/map/block/mapsector.h"
#include "src/client/views/game/minimap/minimap.h"
#include "src/client/render/map/mesh/node_visuals.h"
#include "src/shared/platform/system/profiler.h"
#include "src/client/render/shaders/shader.h"
#include "src/shared/platform/system/translation.h"
#include "src/shared/game/pointing/pointedthing.h"
#include "src/shared/util/image/screenshot.h"
#include "src/shared/util/data/serialize.h"
#include "src/shared/util/text/string.h"
#include "src/shared/base/version.h"

// CPCSM

// Network
#include "src/client/app/network/clientopcodes.h"
#include "src/server/app/network/serveropcodes.h"
#include "library/trace/net.h"
#include "library/trace/trace.h"
#include "src/shared/platform/network/connection.h"
#include "src/shared/platform/network/networkexceptions.h"
#include "src/shared/platform/network/networkpacket.h"
#include "src/shared/base/serialization.h"

// Database
#include "src/server/world/load/database/database-files.h"
#include "src/server/world/load/database/database-sqlite3.h"

#include <IAnimatedMesh.h>
#include <IFileSystem.h>
#include <IReadFile.h>
#include <IMeshCache.h>
#include <json/json.h>

#include <iostream>
#include <algorithm>
#include "src/server/world/run/pile/piles.h"
#include "src/client/world/player/body.h"
#include <sstream>
#include <cmath>

extern gui::IGUIEnvironment* guienv;

/*
	Utility classes
*/

u32 PacketCounter::sum() const
{
	u32 n = 0;
	for (const auto &it : m_packets)
		n += it.second;
	return n;
}

void PacketCounter::print(std::ostream &o) const
{
	for (const auto &it : m_packets) {
		auto name = it.first >= TOCLIENT_NUM_MSG_TYPES ? nullptr
			: toClientCommandTable[it.first].name;
		if (!name)
			name = "?";
		o << "cmd " << it.first << " (" << name << ") count "
			<< it.second << std::endl;
	}
}

[[maybe_unused]]
static void enrich_exception(BaseException &e, const NetworkPacket &pkt, bool include_pos)
{
	const u16 cmd = pkt.getCommand();
	std::ostringstream oss;
	if (cmd < TOCLIENT_NUM_MSG_TYPES)
		oss << " name=" << toClientCommandTable[cmd].name;

	if (include_pos) {
		// (not necessary for PacketError: already in e.what())

		oss << " cmd=" << cmd
			<< " offset=" << pkt.getOffset()
			<< " size=" << pkt.getSize();
	}

	e.append(" @").append(oss.str());
}

/*
	Client
*/

Client::Client(
		const char *playername,
		const std::string &password,
		MapDrawControl &control,
		IWritableTextureSource *tsrc,
		IWritableShaderSource *shsrc,
		IWritableItemDefManager *itemdef,
		NodeDefManager *nodedef,
		ISoundManager *sound,
		MtEventManager *event,
		RenderingEngine *rendering_engine,
		ItemVisualsManager *item_visuals_manager
):
	m_tsrc(tsrc),
	m_shsrc(shsrc),
	m_itemdef(itemdef),
	m_nodedef(nodedef),
	m_sound(sound),
	m_event(event),
	m_rendering_engine(rendering_engine),
	m_item_visuals_manager(item_visuals_manager),
	m_mesh_update_manager(std::make_unique<MeshUpdateManager>(this)),
	m_env(
		make_irr<ClientMap>(this, rendering_engine, control, 666),
		tsrc, this
	),
	m_particle_manager(std::make_unique<ParticleManager>(&m_env)),
	m_server_ser_ver(SER_FMT_VER_INVALID),
	m_last_chat_message_sent(time(NULL)),
	m_password(password),
	m_chosen_auth_mech(AUTH_MECHANISM_NONE),
	m_media_downloader(std::make_unique<ClientMediaDownloader>()),
	m_state(LC_Created)
{
	// Add local player
	m_env.setLocalPlayer(new LocalPlayer(this, playername));

	if (g_settings->getBool("enable_minimap")) {
		m_minimap = std::make_unique<Minimap>(this);
	}

	m_cache_save_interval = g_settings->getU16("server_map_save_interval");
	m_mesh_grid = { g_settings->getU16("client_mesh_chunk") };

}

void Client::Stop()
{
	m_shutdown = true;
	//request all client managed threads to stop
	m_mesh_update_manager->stop();
	// Save local server map
	if (m_localdb) {
		infostream << "Local map saving ended." << std::endl;
		m_localdb->endSave();
		m_localdb.reset();
	}
}

bool Client::isShutdown()
{
	return m_shutdown || !m_mesh_update_manager->isRunning();
}

void Client::meshDetail(v3s16 camera)
{
	ClientMap &map = m_env.getClientMap();
	if (camera != m_detail_camera) {
		m_detail_camera = camera;
		m_mesh_update_manager->setCamera(camera);
		// the meshes whose ring it crossed
		m_detail_pending.clear();
		for (auto it = m_detail.begin(); it != m_detail.end();) {
			if (!map.getBlockNoCreateNoEx(it->first)) {
				it = m_detail.erase(it);
				continue;
			}
			if (m_mesh_update_manager->detailAt(it->first) != it->second)
				m_detail_pending.push_back(it->first);
			++it;
		}
		// nearest last: taken from the back first
		const v3s16 at = getNodeBlockPos(camera);
		std::sort(m_detail_pending.begin(), m_detail_pending.end(), [&](v3s16 a, v3s16 b) {
			return (a - at).getLengthSQ() > (b - at).getLengthSQ();
		});
	}
	// made again (not urgent, alone: their neighbours' detail is their own),
	// a few a frame: a ring crossed all at once was a burst of meshes to swap in
	constexpr int EACH_FRAME = 6;
	for (int n = 0; n < EACH_FRAME && !m_detail_pending.empty(); n++) {
		const v3s16 p = m_detail_pending.back();
		m_detail_pending.pop_back();
		auto it = m_detail.find(p);
		if (it == m_detail.end() || !map.getBlockNoCreateNoEx(p))
			continue;
		it->second = m_mesh_update_manager->detailAt(p); // asked: not again till it changes again
		m_mesh_update_manager->updateBlock(&map, p, false, false);
	}
}

void Client::flushGround()
{
	if (m_ground_nodes.empty() && m_ground_blocks.empty())
		return;
	// every block a node's change reaches (as addUpdateMeshTaskForNode), once
	const s16 up = ground::REACH + 1;
	std::unordered_set<v3s16> blocks, bases;
	blocks.swap(m_ground_blocks); // relit by a loose node's change
	for (v3s16 p : m_ground_nodes) {
		bases.insert(getNodeBlockPos(p));
		const v3s16 low = getNodeBlockPos(p - v3s16(2, up, 2)), high = getNodeBlockPos(p + v3s16(1, up, 1));
		for (s16 x = low.X; x <= high.X; x++)
			for (s16 y = low.Y; y <= high.Y; y++)
				for (s16 z = low.Z; z <= high.Z; z++)
					blocks.insert(v3s16(x, y, z));
	}
	m_ground_nodes.clear();
	for (v3s16 b : blocks)
		addUpdateMeshTask(b, false, true);
	for (v3s16 b : bases) {
		m_liquid_ground.insert(b);
		updateMeshReaders(b, true);
		m_body_ground.insert(b);
	}
}

void Client::flushBodyGround(float dtime)
{
	// the physics' ground built again at most a few times a second while
	// ground churns: a block's ground is a hull per sloped node (dear), and
	// a sixteenth's change a quarter second late isn't felt
	constexpr float EVERY = 0.25f;
	m_body_ground_wait -= dtime;
	if (m_body_ground.empty() || m_body_ground_wait > 0)
		return;
	m_body_ground_wait = EVERY;
	if (m_body)
		for (v3s16 b : m_body_ground)
			m_body->groundChanged(b);
	m_body_ground.clear();
}

void Client::pressGround(float dtime)
{
	constexpr float FOOT = 0.12f;   // nodes, as the server's (Server::pressGround)
	constexpr float PERSON = 800.f; // N
	constexpr float PI = 3.14159265f;
	if (!m_piles)
		m_piles = std::make_unique<pile::Piles>(m_env.getMap(), *m_nodedef, [this](v3s16 p) {
			m_ground_nodes.insert(p);
		}, true);
	ScopeProfiler sp(g_profiler, "Client: piles [us]", SPT_AVG, PRECISION_MICRO);
	if (m_body && m_body->driving()) {
		for (const physics::World::Contact &c : m_body->drivenWheels()) {
			const float r = c.width / 2;
			m_piles->press((u64(*m_body->driving()) << 8) | c.wheel, v3f(c.at.X, c.at.Y, c.at.Z), r,
					c.load / (PI * r * r) / 1000);
		}
	} else if (LocalPlayer *me = m_env.getLocalPlayer()) {
		m_piles->press(u64(1) << 40, me->getPosition() / BS, FOOT, PERSON / (PI * FOOT * FOOT) / 1000);
	}
	m_piles->step(dtime);
}

Client::~Client()
{
	m_shutdown = true;
	if (m_con)
		m_con->Disconnect();

	m_chosen_auth_mech = AUTH_MECHANISM_NONE;

	m_mesh_update_manager->stop();
	m_mesh_update_manager->wait();
	m_mesh_update_manager->clearAllQueues(true);

	// Delete detached inventories
	for (auto &it : m_detached_inventories)
		delete it.second;
	m_detached_inventories.clear();

	// cleanup 3d model meshes on client shutdown
	m_rendering_engine->cleanupMeshCache();

	m_item_visuals_manager->clear();

	guiScalingCacheClear();

	m_minimap.reset();
	m_media_downloader.reset();

	// Write the changes and delete

	// Free sound ids
	for (auto &csp : m_sounds_client_to_server)
		m_sound->freeId(csp.first);
	m_sounds_client_to_server.clear();
}

void Client::connect(const Address &address, const std::string &address_name)
{
	connect(address, address_name, UDPSocket::CreateEphemeral(address.isIPv6()));
}

void Client::connect(const Address &address, const std::string &address_name, UDPSocket &&socket)
{
	trace::net::event("Connecting");
	if (m_con) {
		// can't do this if the connection has entered auth phase
		sanity_check(m_state == LC_Created && m_proto_ver == 0);
		infostream << "Client connection will be recreated" << std::endl;

		m_access_denied = false;
		m_access_denied_reconnect = false;
		m_access_denied_reason.clear();
	}

	m_address_name = address_name;
	m_con.reset(con::createMTP(/*is_server=*/false, std::move(socket), this));

	infostream << "Connecting to server at ";
	address.print(infostream);
	infostream << std::endl;

	m_con->Connect(address);

	initLocalMapSaving(address, m_address_name);
}

void Client::step(float dtime)
{
	// Limit a bit
	if (dtime > DTIME_LIMIT)
		dtime = DTIME_LIMIT;

	m_animation_time = fmodf(m_animation_time + dtime, 60.0f);

	// Terra Firma: the connection as the debugger's Network tab charts it
	// (rates in KiB/s, the way the connection counts them; times in seconds)
	if (m_con) {
		const float rtt = m_con->getPeerStat(PEER_ID_SERVER, con::AVG_RTT);
		// no server peer (refused, or gone): nothing to measure, and getLocalStat would abort
		if (rtt >= 0) {
			trace::count("Net: RTT [ms]", rtt * 1000, trace::Fold::Last);
			trace::count("Net: jitter [ms]", m_con->getPeerStat(PEER_ID_SERVER, con::AVG_JITTER) * 1000, trace::Fold::Last);
			trace::count("Net: receive [KiB/s]", m_con->getLocalStat(con::CUR_INC_RATE), trace::Fold::Last);
			trace::count("Net: send [KiB/s]", m_con->getLocalStat(con::CUR_DL_RATE), trace::Fold::Last);
			trace::count("Net: lost [KiB/s]", m_con->getLocalStat(con::CUR_LOSS_RATE), trace::Fold::Last);
		}
	}

	{
		TF_ZONE("Client: receive");
		ReceiveAll();
	}

	// Terra Firma: what this client's wheels and feet press, at once (sprint 84)
	if (m_nodedef_received) {
		TF_ZONE("Client: press ground");
		pressGround(dtime);
	}
	// Terra Firma: ground drawn coarser further off (sprint 85)
	if (m_camera) {
		TF_ZONE("Client: mesh detail");
		meshDetail(floatToInt(m_camera->getPosition(), BS));
	}
	{
		TF_ZONE("Client: flush ground");
		flushGround();
		flushBodyGround(dtime);
	}

	/*
		Packet counter
	*/
	{
		float &counter = m_packetcounter_timer;
		counter -= dtime;
		if(counter <= 0.0f)
		{
			counter = 30.0f;
			u32 sum = m_packetcounter.sum();
			float avg = sum / counter;

			infostream << "Client packetcounter (" << counter << "s): "
					<< "sum=" << sum << " avg=" << avg << "/s" << std::endl;
			m_packetcounter.print(infostream);
			m_packetcounter.clear();
		}
	}

	// The issue that made this workaround necessary was fixed in August 2024, but
	// it's not like we can remove this code - ever.
	if (m_state == LC_Created) {
		float &counter = m_connection_reinit_timer;
		counter -= dtime;
		if (counter <= 0) {
			counter = 1.5f;

			LocalPlayer *myplayer = m_env.getLocalPlayer();
			FATAL_ERROR_IF(!myplayer, "Local player not found in environment");

			sendInit(myplayer->getName());
		}

		// Not connected, return
		return;
	}

	/*
		Do stuff if connected
	*/

	/*
		Run Map's timers and unload unused data
	*/
	constexpr float map_timer_and_unload_dtime = 5.25f;
	constexpr s32 mapblock_limit_enforce_distance = 200;
	if(m_map_timer_and_unload_interval.step(dtime, map_timer_and_unload_dtime)) {
		TF_ZONE("Client: unload blocks");
		std::vector<v3s16> deleted_blocks;

		// Determine actual block limit to use
		const s32 configured_limit = g_settings->getS32("client_mapblock_limit");
		s32 mapblock_limit;
		if (configured_limit < 0) {
			mapblock_limit = -1;
		} else {
			s32 view_range = g_settings->getS16("viewing_range");
			// Up to a certain limit we want to guarantee that the client can keep
			// a full 360° view loaded in memory without blocks vanishing behind
			// the players back.
			// We use a sphere volume to approximate this. In practice far less
			// blocks will be needed due to occlusion/culling.
			float blocks_range = ceilf(std::min(mapblock_limit_enforce_distance, view_range)
				/ (float) MAP_BLOCKSIZE);
			mapblock_limit = (4.f/3.f) * M_PI * powf(blocks_range, 3);
			assert(mapblock_limit > 0);
			mapblock_limit = std::max(mapblock_limit, configured_limit);
			if (mapblock_limit > std::max(configured_limit, m_mapblock_limit_logged)) {
				infostream << "Client: using block limit of " << mapblock_limit
					<< " rather than configured " << configured_limit
					<< " due to view range." << std::endl;
				m_mapblock_limit_logged = mapblock_limit;
			}
		}

		m_env.getMap().timerUpdate(map_timer_and_unload_dtime,
			std::max(g_settings->getFloat("client_unload_unused_data_timeout"), 0.0f),
			mapblock_limit, &deleted_blocks);

		// Send info to server

		auto i = deleted_blocks.begin();
		std::vector<v3s16> sendlist;
		for(;;) {
			if(sendlist.size() == 255 || i == deleted_blocks.end()) {
				if(sendlist.empty())
					break;
				/*
					[0] u16 command
					[2] u8 count
					[3] v3s16 pos_0
					[3+6] v3s16 pos_1
					...
				*/

				sendDeletedBlocks(sendlist);

				if(i == deleted_blocks.end())
					break;

				sendlist.clear();
			}

			sendlist.push_back(*i);
			++i;
		}
	}

	/*
		Send pending messages on out chat queue
	*/
	if (!m_out_chat_queue.empty() && canSendChatMessage()) {
		sendChatMessage(m_out_chat_queue.front());
		m_out_chat_queue.pop();
	}

	/*
		Handle environment
	*/
	LocalPlayer *player = m_env.getLocalPlayer();

	// Step environment (also handles player controls)
	{
		TF_ZONE("Client: environment");
		m_env.step(dtime);
	}
	m_sound->step(dtime);

	/*
		Get events
	*/
	while (m_env.hasClientEnvEvents()) {
		ClientEnvEvent envEvent = m_env.getClientEnvEvent();

		if (envEvent.type == CEE_PLAYER_DAMAGE) {
			u16 damage = envEvent.player_damage.amount;

			if (envEvent.player_damage.send_to_server)
				sendDamage(damage);

			// Add to ClientEvent queue
			ClientEvent *event = new ClientEvent();
			event->type = CE_PLAYER_DAMAGE;
			event->player_damage.amount = damage;
			event->player_damage.effect = true;
			m_client_event_queue.push(event);
		}
	}

	/*
		Print some info
	*/
	float &counter = m_avg_rtt_timer;
	counter += dtime;
	if(counter >= 10) {
		counter = 0.0;
		// connectedAndInitialized() is true, peer exists.
		float avg_rtt = getRTT();
		infostream << "Client: avg_rtt=" << avg_rtt << std::endl;
	}

	/*
		Send player position to server
	*/
	{
		float &counter = m_playerpos_send_timer;
		counter += dtime;
		if (m_state == LC_Ready && counter >= m_recommended_send_interval) {
			counter = 0;
			sendPlayerPos();
		}
	}

	/*
		Replace updated meshes
	*/
	{
		TF_ZONE("Client: adopt meshes");
		// This can cause pending mesh updates to lose their validity, so do it
		// before processing them.
		updateStaticLighting(player->getLighting().static_);

		m_mesh_update_manager->takePrepared(&m_env.getMap()); // Terra Firma
		int num_processed_meshes = 0;
		std::vector<v3s16> blocks_to_ack;
		bool force_update_shadows = false;
		MeshUpdateResult r;
		// Terra Firma: only so much new mesh a frame. Each is uploaded to the
		// GPU when it's first drawn, and a burst of them (a ring of detail
		// crossed, a world arriving) was a frame of tens of ms; the rest wait
		// a frame or two. By vertices, not meshes: a forest block's plants are
		// hundreds of thousands, and twelve of them was 7 million vertices in
		// one frame, 1.5 s. One mesh always goes, however big.
		constexpr int MESHES_A_FRAME = 12;
		constexpr u32 VERTICES_A_FRAME = 400'000;
		u32 vertices_taken = 0;
		while (num_processed_meshes < MESHES_A_FRAME && (num_processed_meshes == 0 || vertices_taken < VERTICES_A_FRAME)
				&& m_mesh_update_manager->getNextResult(r))
		{
			num_processed_meshes++;
			if (r.mesh)
				vertices_taken += r.mesh->vertexCount();

			std::vector<MinimapMapblock*> minimap_mapblocks;
			bool do_mapper_update = true;

			ClientMap &map = m_env.getClientMap();
			MapSector *sector = map.emergeSector(v2s16(r.p.X, r.p.Z));

			MapBlock *block = sector->getBlockNoCreateNoEx(r.p.Y);

			// The block in question is not visible (perhaps it is culled at the server),
			// create a blank block just to hold the chunk's mesh.
			// If the block becomes visible later it will replace the blank block.
			if (!block && r.mesh)
				block = sector->createBlankBlock(r.p.Y);

			if (block) {
				// Delete the old mesh
				if (block->mesh)
					map.invalidateMapBlockMesh(block->mesh);
				delete block->mesh;
				block->mesh = nullptr;
				block->solid_sides = r.solid_sides;
				map.meshesChanged(); // Terra Firma: the draw list (and the far view's edge) follow

				m_detail[r.p] = r.detail; // Terra Firma: made again when the camera's moved across its ring
				if (r.mesh) {
					minimap_mapblocks = r.mesh->moveMinimapMapblocks();
					if (minimap_mapblocks.empty())
						do_mapper_update = false;

					if (!r.mesh->isEmpty()) {
						// Replace with the new mesh (Terra Firma: on the GPU now, in this frame's budget)
						r.mesh->upload(m_rendering_engine->get_video_driver());
						block->mesh = r.mesh.release();
						if (r.urgent)
							force_update_shadows = true;
					}
				}
			}

			if (m_minimap && do_mapper_update) {
				v3s16 ofs;

				// See also mapblock_mesh.cpp for the code that creates the array of minimap blocks.
				for (ofs.Z = 0; ofs.Z < m_mesh_grid.cell_size; ofs.Z++)
				for (ofs.Y = 0; ofs.Y < m_mesh_grid.cell_size; ofs.Y++)
				for (ofs.X = 0; ofs.X < m_mesh_grid.cell_size; ofs.X++) {
					size_t i = m_mesh_grid.getOffsetIndex(ofs);
					if (i < minimap_mapblocks.size() && minimap_mapblocks[i])
						m_minimap->addBlock(r.p + ofs, minimap_mapblocks[i]);
				}
			}

			// TODO?: it's really weird and complicates things how we only ack
			// blocks after they've been through the meshgen queue...
			for (auto p : r.ack_list) {
				if (blocks_to_ack.size() == 255) {
					sendGotBlocks(blocks_to_ack);
					blocks_to_ack.clear();
				}

				blocks_to_ack.emplace_back(p);
			}

			r.dropBlocks();
		}
		g_profiler->avg("Client: mesh vertices taken [#]", vertices_taken); // Terra Firma: what the GPU's given this frame
		if (blocks_to_ack.size() > 0) {
				// Acknowledge block(s)
				sendGotBlocks(blocks_to_ack);
		}

		if (num_processed_meshes > 0)
			g_profiler->graphAdd("num_processed_meshes", num_processed_meshes);

		if (force_update_shadows && !g_settings->getFlag("performance_tradeoffs")) {
			auto shadow = RenderingEngine::get_shadow_renderer();
			if (shadow)
				shadow->setForceUpdateShadowMap();
		};
	}

	/*
		Load fetched media
	*/
	if (m_media_downloader && m_media_downloader->isStarted()) {
		m_media_downloader->step(this);
		if (m_media_downloader->isDone()) {
			m_media_downloader.reset();
		}
	}
	{
		// Acknowledge dynamic media downloads to server
		std::vector<u32> done;
		for (auto it = m_pending_media_downloads.begin();
				it != m_pending_media_downloads.end();) {
			assert(it->d->isStarted());
			it->d->step(this);
			if (it->d->isDone()) {
				done.insert(done.end(), it->tokens.begin(), it->tokens.end());

				it = m_pending_media_downloads.erase(it);
			} else {
				it++;
			}
		}
		while (!done.empty()) {
			// this looks stupid, but is a simple and correct way to chunk them
			std::vector<u32> part;
			while (!done.empty() && part.size() < 255) {
				part.push_back(done.back());
				done.pop_back();
			}
			sendHaveMedia(part);
		}
	}

	/*
		If the server didn't update the inventory in a while, revert
		the local inventory (so the player notices the lag problem
		and knows something is wrong).
	*/
	if (m_inventory_from_server && !inhibit_inventory_revert) {
		float interval = 10.0f;
		float count_before = std::floor(m_inventory_from_server_age / interval);

		m_inventory_from_server_age += dtime;

		float count_after = std::floor(m_inventory_from_server_age / interval);

		if (count_after != count_before) {
			// Do this every <interval> seconds after TOCLIENT_INVENTORY
			// Reset the locally changed inventory to the authoritative inventory
			player->inventory = *m_inventory_from_server;
			m_update_wielded_item = true;
		}
	}

	/*
		Update positions of sounds attached to objects
	*/
	{
		for (auto &m_sounds_to_object : m_sounds_to_objects) {
			sound_handle_t client_id = m_sounds_to_object.first;
			u16 object_id = m_sounds_to_object.second;
			ClientActiveObject *cao = m_env.getActiveObject(object_id);
			if (!cao)
				continue;
			m_sound->updateSoundPosVel(client_id, cao->getPosition() * (1.0f/BS),
					cao->getVelocity() * (1.0f/BS));
		}
	}

	/*
		Handle removed remotely initiated sounds
	*/
	m_removed_sounds_check_timer += dtime;
	if(m_removed_sounds_check_timer >= 2.32) {
		m_removed_sounds_check_timer = 0;
		// Find removed sounds and clear references to them
		std::vector<sound_handle_t> removed_client_ids = m_sound->pollRemovedSounds();
		std::vector<s32> removed_server_ids;
		for (sound_handle_t client_id : removed_client_ids) {
			auto client_to_server_id_it = m_sounds_client_to_server.find(client_id);
			if (client_to_server_id_it == m_sounds_client_to_server.end())
				continue;
			s32 server_id = client_to_server_id_it->second;
			m_sound->freeId(client_id);
			m_sounds_client_to_server.erase(client_to_server_id_it);
			if (server_id != -1) {
				m_sounds_server_to_client.erase(server_id);
				removed_server_ids.push_back(server_id);
			}
			m_sounds_to_objects.erase(client_id);
		}

		// Sync to server
		if (!removed_server_ids.empty()) {
			sendRemovedSounds(removed_server_ids);
		}
	}

	// Write server map
	if (m_localdb && m_localdb_save_interval.step(dtime,
			m_cache_save_interval)) {
		TF_ZONE("Client: save map");
		m_localdb->endSave();
		m_localdb->beginSave();
	}
}

bool Client::updateStaticLighting(const StaticLighting &future)
{
	auto &current = m_committed_static_light;

	if (current == future)
		return false;
	current = future;
	infostream << "Static lighting params changed" << std::endl;

	float gamma = g_settings->getFloat("display_gamma");
	if (current.light_curve_set) {
		set_light_table(current.light_curve, gamma);
	} else {
		set_light_curve(gamma);
	}

	// Force a re-mesh of all blocks
	ClientMap &map = m_env.getClientMap();
	std::vector<v3s16> to_update;
	map.getBlocksWithMeshes(to_update);
	m_mesh_update_manager->forceRegenerate(&map, to_update);

	return true;
}

bool Client::loadMedia(const std::string &data, const std::string &filename,
	bool from_media_push)
{
	std::string name;

	// Consider updating LuantiDocumentsProvider.java if new file types are added

	const char *image_ext[] = {
		".png", ".jpg", ".tga",
		NULL
	};
	name = removeStringEnd(filename, image_ext);
	if (!name.empty()) {
		TRACESTREAM(<< "Client: Attempting to load image "
			<< "file \"" << filename << "\"" << std::endl);

		io::IFileSystem *irrfs = m_rendering_engine->get_filesystem();
		video::IVideoDriver *vdrv = m_rendering_engine->get_video_driver();

		io::IReadFile *rfile = irrfs->createMemoryReadFile(
				data.c_str(), data.size(), filename.c_str());

		FATAL_ERROR_IF(!rfile, "Could not create irrlicht memory file.");

		// Read image
		video::IImage *img = vdrv->createImageFromFile(rfile);
		if (!img) {
			errorstream<<"Client: Cannot create image from data of "
					<<"file \""<<filename<<"\""<<std::endl;
			rfile->drop();
			return false;
		}

		m_tsrc->insertSourceImage(filename, img);
		img->drop();
		rfile->drop();
		return true;
	}

	const char *sound_ext[] = {
		".0.ogg", ".1.ogg", ".2.ogg", ".3.ogg", ".4.ogg",
		".5.ogg", ".6.ogg", ".7.ogg", ".8.ogg", ".9.ogg",
		".ogg", NULL
	};
	name = removeStringEnd(filename, sound_ext);
	if (!name.empty()) {
		TRACESTREAM(<< "Client: Attempting to load sound file \""
				<< filename << "\"" << std::endl);
		if (!m_sound->loadSoundData(filename, std::string(data)))
			return false;
		// "name[.num].ogg" is in group "name"
		m_sound->addSoundToGroup(filename, name);
		return true;
	}

	const char *model_ext[] = {
		".x", ".b3d", ".obj", ".gltf", ".glb",
		NULL
	};
	name = removeStringEnd(filename, model_ext);
	if (!name.empty()) {
		TRACESTREAM(<<"Client: Storing model into memory "
				"\""<<filename<<"\""<<std::endl);
		if(m_mesh_data.count(filename))
			errorstream<<"Multiple models with name \""<<filename
					<<"\" found; replacing previous model"<<std::endl;
		m_mesh_data[filename] = data;
		return true;
	}

	if (Translations::isTranslationFile(filename)) {
		if (from_media_push)
			return false;
		TRACESTREAM(<< "Client: Loading translation: "
				<< "\"" << filename << "\"" << std::endl);

		g_client_translations->setTargetLanguage(gettext_get_lang_code());
		g_client_translations->loadTranslation(filename, data);
		return true;
	}

	const char *font_ext[] = {".ttf", ".woff", NULL};
	name = removeStringEnd(filename, font_ext);
	if (!name.empty()) {
		verbosestream<<"Client: Loading file as font: \""
				<< filename << "\"" << std::endl;
		g_fontengine->setMediaFont(name, data);
		return true;
	}

	errorstream << "Client: Don't know how to load file \""
		<< filename << "\"" << std::endl;
	return false;
}

// Virtual methods from con::PeerHandler
void Client::peerAdded(con::IPeer *peer)
{
	infostream << "Client::peerAdded(): peer->id="
			<< peer->id << std::endl;
}

void Client::deletingPeer(con::IPeer *peer, bool timeout)
{
	infostream << "Client::deletingPeer(): "
			"Server Peer is getting deleted "
			<< "(timeout=" << timeout << ")" << std::endl;

	m_access_denied = true;
	if (timeout)
		m_access_denied_reason = gettext("Connection timed out.");
	else if (m_access_denied_reason.empty())
		m_access_denied_reason = gettext("Connection aborted (protocol error?).");
}

void Client::requestMedia(const std::vector<std::string> &file_requests)
{
	std::ostringstream os(std::ios_base::binary);
	writeU16(os, TOSERVER_REQUEST_MEDIA);
	size_t file_requests_size = file_requests.size();

	FATAL_ERROR_IF(file_requests_size > 0xFFFF, "Unsupported number of file requests");

	NetworkPacket pkt(TOSERVER_REQUEST_MEDIA, 2 + 0);

	pkt << (u16) (file_requests_size & 0xFFFF);

	for (const std::string &file_request : file_requests) {
		pkt << file_request;
	}

	Send(&pkt);

	infostream << "Client: Sending media request list to server ("
			<< file_requests.size() << " files, packet size "
			<< pkt.getSize() << ")" << std::endl;
}

void Client::initLocalMapSaving(const Address &address, const std::string &hostname)
{
	if (!g_settings->getBool("enable_local_map_saving") || m_internal_server) {
		return;
	}
	if (m_localdb) {
		infostream << "Local map saving already initialized" << std::endl;
		return;
	}

	std::string world_path;
#define set_world_path(hostname) \
	world_path = porting::path_user \
		+ DIR_DELIM + "worlds" \
		+ DIR_DELIM + "server_" \
		+ hostname + "_" + std::to_string(address.getPort());

	set_world_path(hostname);
	if (!fs::IsDir(world_path)) {
		std::string hostname_escaped = hostname;
		str_replace(hostname_escaped, ':', '_');
		set_world_path(hostname_escaped);
	}
#undef set_world_path
	fs::CreateAllDirs(world_path);

	m_localdb = std::make_unique<MapDatabaseSQLite3>(world_path);
	m_localdb->beginSave();
	actionstream << "Local map saving started, map will be saved at '" << world_path << "'" << std::endl;
}

void Client::ReceiveAll()
{
	NetworkPacket pkt;
	u64 start_ms = porting::getTimeMs();
	const u64 budget = m_state == LC_Ready ? 10 : 100;

	FATAL_ERROR_IF(!m_con, "Networking not initialized");
	for (;;) {
		// Limit time even if there would be huge amounts of data to
		// process
		if (u64 d = porting::getTimeMs() - start_ms; d >= budget) {
			infostream << "Client::ReceiveAll(): "
				"Packet processing budget exceeded, took "
				<< d << "ms" << std::endl;
			break;
		}

		pkt.clear();
		try {
			if (!m_con->TryReceive(&pkt))
				break;
			ProcessData(&pkt);
		} catch (const con::InvalidIncomingDataException &e) {
			infostream << "Client::ReceiveAll(): "
					"InvalidIncomingDataException: what()="
					 << e.what() << std::endl;
#ifdef NDEBUG
		} catch (SerializationError &e) {
			enrich_exception(e, pkt, true);
			throw;
		} catch (PacketError &e) {
			enrich_exception(e, pkt, false);
			throw;
#endif
		}
	}
}

inline void Client::handleCommand(NetworkPacket* pkt)
{
	const ToClientCommandHandler& opHandle = toClientCommandTable[pkt->getCommand()];
	(this->*opHandle.handler)(pkt);
}

/*
	sender_peer_id given to this shall be quaranteed to be a valid peer
*/
void Client::ProcessData(NetworkPacket *pkt)
{
	ToClientCommand command = (ToClientCommand) pkt->getCommand();

	// Terra Firma: the debugger's Network tab. The server's table says which
	// channel, and whether reliably, it sent this kind on.
	if (command < TOCLIENT_NUM_MSG_TYPES) {
		const ClientCommandFactory &sent = clientCommandFactoryTable[command];
		trace::net::message(trace::net::Dir::In, command, sent.channel, sent.reliable, pkt->getSize(), pkt->getRemainingNoCopy());
	}

	m_packetcounter.add(static_cast<u16>(command));
	g_profiler->graphAdd("client_received_packets", 1);

	/*
		If this check is removed, be sure to change the queue
		system to know the ids
	*/
	if (pkt->getPeerId() != PEER_ID_SERVER) {
		infostream << "Client::ProcessData(): Discarding data not "
			"coming from server: peer_id=" << static_cast<int>(pkt->getPeerId())
			<< " command=" << static_cast<unsigned>(command) << std::endl;
		return;
	}

	// Command must be handled into ToClientCommandHandler
	if (command >= TOCLIENT_NUM_MSG_TYPES) {
		infostream << "Client: Ignoring unknown command "
			<< static_cast<unsigned>(command) << std::endl;
		return;
	}

	/*
	 * Those packets are handled before m_server_ser_ver is set, it's normal
	 * But we must use the new ToClientConnectionState in the future,
	 * as a byte mask
	 */
	if (toClientCommandTable[command].state == TOCLIENT_STATE_NOT_CONNECTED) {
		handleCommand(pkt);
		return;
	}

	if (m_server_ser_ver == SER_FMT_VER_INVALID) {
		infostream << "Client: Server serialization"
				" format invalid. Skipping incoming command "
				<< static_cast<unsigned>(command) << std::endl;
		return;
	}

	handleCommand(pkt);
}

void Client::Send(NetworkPacket* pkt)
{
	auto &scf = serverCommandFactoryTable[pkt->getCommand()];
	FATAL_ERROR_IF(!scf.name, "packet type missing in table");
	trace::net::message(trace::net::Dir::Out, pkt->getCommand(), scf.channel, scf.reliable, pkt->getSize());
	m_con->Send(PEER_ID_SERVER, scf.channel, pkt, scf.reliable);
}

// Will fill up 12 + 12 + 4 + 4 + 4 + 1 + 1 + 1 + 4 + 4 bytes
void writePlayerPos(LocalPlayer *myplayer, ClientMap *clientMap, NetworkPacket *pkt, bool camera_inverted)
{
	v3s32 position   = v3s32::from(myplayer->getPosition() * 100);
	v3s32 speed      = v3s32::from(myplayer->getSpeed() * 100);
	s32 pitch        = myplayer->getPitch() * 100;
	s32 yaw          = myplayer->getYaw() * 100;
	u32 keyPressed   = myplayer->control.getKeysPressed();
	// scaled by 80, so that pi can fit into a u8
	u8 fov           = std::fmin(255.0f, clientMap->getCameraFov() * 80.0f);
	u8 wanted_range  = std::fmin(255.0f,
			std::ceil(clientMap->getWantedRange() * (1.0f / MAP_BLOCKSIZE)));
	f32 movement_speed = myplayer->control.movement_speed;
	f32 movement_dir = myplayer->control.movement_direction;

	/*
		Format:
		[0] v3s32 position*100
		[12] v3s32 speed*100
		[12+12] s32 pitch*100
		[12+12+4] s32 yaw*100
		[12+12+4+4] u32 keyPressed
		[12+12+4+4+4] u8 fov*80
		[12+12+4+4+4+1] u8 ceil(wanted_range / MAP_BLOCKSIZE)
		[12+12+4+4+4+1+1] u8 camera_inverted (bool)
		[12+12+4+4+4+1+1+1] f32 movement_speed
		[12+12+4+4+4+1+1+1+4] f32 movement_direction
	*/
	*pkt << position << speed << pitch << yaw << keyPressed;
	*pkt << fov << wanted_range;
	*pkt << camera_inverted;
	*pkt << movement_speed << movement_dir;
}

void Client::interact(InteractAction action, const PointedThing& pointed)
{
	if(m_state != LC_Ready) {
		errorstream << "Client::interact() "
				"Canceled (not connected)"
				<< std::endl;
		return;
	}

	LocalPlayer *myplayer = m_env.getLocalPlayer();
	if (myplayer == NULL)
		return;

	/*
		[0] u16 command
		[2] u8 action
		[3] u16 item
		[5] u32 length of the next item (plen)
		[9] serialized PointedThing
		[9 + plen] player position information
	*/

	NetworkPacket pkt(TOSERVER_INTERACT, 1 + 2 + 0);

	pkt << (u8)action;
	pkt << myplayer->getWieldIndex();

	std::ostringstream tmp_os(std::ios::binary);
	pointed.serialize(tmp_os);

	pkt.putLongString(tmp_os.str());

	writePlayerPos(myplayer, &m_env.getClientMap(), &pkt, m_camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT);

	Send(&pkt);
}


AuthMechanism Client::choseAuthMech(const u32 mechs)
{
	if (mechs & AUTH_MECHANISM_TICKET) // Terra Firma: a world hosted for friends
		return AUTH_MECHANISM_TICKET;
	if (mechs & AUTH_MECHANISM_LOCAL) // a world played alone
		return AUTH_MECHANISM_LOCAL;
	return AUTH_MECHANISM_NONE;
}

void Client::sendInit(const std::string &playerName)
{
	NetworkPacket pkt(TOSERVER_INIT, 1 + 2 + 2 + (1 + playerName.size()));

	pkt << SER_FMT_VER_HIGHEST_READ << (u16) 0 /* unused */;
	pkt << CLIENT_PROTOCOL_VERSION_MIN << LATEST_PROTOCOL_VERSION;
	pkt << playerName;

	Send(&pkt);
}

void Client::startAuth(AuthMechanism chosen_auth_mechanism)
{
	m_chosen_auth_mech = chosen_auth_mechanism;

	switch (chosen_auth_mechanism) {
		case AUTH_MECHANISM_TICKET: { // Terra Firma: the ticket is the password field
			NetworkPacket resp_pkt(TOSERVER_TICKET, 0);
			resp_pkt << m_password << std::string(g_version_hash);
			Send(&resp_pkt);
			break;
		}
		case AUTH_MECHANISM_LOCAL:
			break; // the server accepts by itself
		case AUTH_MECHANISM_NONE:
			break; // not handled in this method
	}
}

void Client::sendDeletedBlocks(std::vector<v3s16> &blocks)
{
	NetworkPacket pkt(TOSERVER_DELETEDBLOCKS, 1 + sizeof(v3s16) * blocks.size());

	pkt << (u8) blocks.size();

	for (const v3s16 &block : blocks) {
		pkt << block;
	}

	Send(&pkt);
}

void Client::sendGotBlocks(const std::vector<v3s16> &blocks)
{
	NetworkPacket pkt(TOSERVER_GOTBLOCKS, 1 + 6 * blocks.size());
	pkt << (u8) blocks.size();
	for (const v3s16 &block : blocks)
		pkt << block;

	Send(&pkt);
}

void Client::sendRemovedSounds(const std::vector<s32> &soundList)
{
	size_t server_ids = soundList.size();
	assert(server_ids <= 0xFFFF);

	NetworkPacket pkt(TOSERVER_REMOVED_SOUNDS, 2 + server_ids * 4);

	pkt << (u16) (server_ids & 0xFFFF);

	for (s32 sound_id : soundList)
		pkt << sound_id;

	Send(&pkt);
}

void Client::sendNodemetaFields(v3s16 p, const std::string &formname,
		const StringMap &fields)
{
	size_t fields_size = fields.size();

	FATAL_ERROR_IF(fields_size > 0xFFFF, "Unsupported number of nodemeta fields");

	NetworkPacket pkt(TOSERVER_NODEMETA_FIELDS, 0);

	pkt << p << formname << (u16) (fields_size & 0xFFFF);

	StringMap::const_iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		const std::string &name = it->first;
		const std::string &value = it->second;
		pkt << name;
		pkt.putLongString(value);
	}

	Send(&pkt);
}

void Client::sendInventoryFields(const std::string &formname,
		const StringMap &fields)
{
	size_t fields_size = fields.size();
	FATAL_ERROR_IF(fields_size > 0xFFFF, "Unsupported number of inventory fields");

	NetworkPacket pkt(TOSERVER_INVENTORY_FIELDS, 0);
	pkt << formname << (u16) (fields_size & 0xFFFF);

	StringMap::const_iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		const std::string &name  = it->first;
		const std::string &value = it->second;
		pkt << name;
		pkt.putLongString(value);
	}

	Send(&pkt);
}

void Client::sendPieceNew()
{
	NetworkPacket pkt(TOSERVER_PIECE_NEW, 0);
	Send(&pkt);
}

void Client::sendHold(u16 object, workshop::Grip grip)
{
	NetworkPacket pkt(TOSERVER_OBJECT_HOLD, 3);
	pkt << object << u8(grip);
	Send(&pkt);
}

void Client::sendHollow(u16 object, u64 part, int thickness, bool rounded)
{
	NetworkPacket pkt(TOSERVER_HOLLOW, 12);
	pkt << object << part << u8(thickness) << u8(rounded);
	Send(&pkt);
}

static void writeFace(NetworkPacket &pkt, const piece::Machine::Face &f)
{
	pkt << f.part << f.at.node << u8(f.at.cell.X) << u8(f.at.cell.Y) << u8(f.at.cell.Z) << f.normal;
}

void Client::sendLink(u16 object, u64 a, u64 b, std::optional<piece::Joint> joint,
		const std::optional<piece::Machine::Face> &at, std::optional<piece::Spring> spring)
{
	NetworkPacket pkt(TOSERVER_LINK, 2 + 16 + 2 + 23 + 2);
	pkt << object << a << b << u8(joint ? u8(*joint) : 255) << u8(at.has_value());
	if (at)
		writeFace(pkt, *at);
	pkt << u8(spring ? spring->travel : 255) << u8(spring ? spring->firmness : 255);
	Send(&pkt);
}

void Client::sendJoin(u16 a, const piece::Machine::Face &on_a, u16 b, const piece::Machine::Face &on_b, piece::Joint joint,
		u8 spin)
{
	NetworkPacket pkt(TOSERVER_JOIN, 2 + 23 + 2 + 23 + 2);
	pkt << a;
	writeFace(pkt, on_a);
	pkt << b;
	writeFace(pkt, on_b);
	pkt << u8(joint) << spin;
	Send(&pkt);
}

void Client::sendSocket(u16 object, const piece::Machine::Face &face, piece::Joint joint,
		std::optional<sculpt::Stroke::Mirror> mirror)
{
	NetworkPacket pkt(TOSERVER_SOCKET, 3 + 23 + 6);
	pkt << object << u8(0);
	writeFace(pkt, face);
	pkt << u8(joint) << u8(mirror ? mirror->axis : 255) << s32(mirror ? mirror->twice : 0);
	Send(&pkt);
}

void Client::sendUnsocket(u16 object, u64 socket)
{
	NetworkPacket pkt(TOSERVER_SOCKET, 11);
	pkt << object << u8(1) << socket;
	Send(&pkt);
}

void Client::sendPlug(u16 a, u64 socket, u16 b, const piece::Machine::Face &on_b, u8 spin)
{
	NetworkPacket pkt(TOSERVER_PLUG, 12 + 23 + 1);
	pkt << a << socket << b;
	writeFace(pkt, on_b);
	pkt << spin;
	Send(&pkt);
}

void Client::sendPart(u16 object, u64 part, bool remove)
{
	NetworkPacket pkt(TOSERVER_PART, 11);
	pkt << object << part << u8(remove);
	Send(&pkt);
}

void Client::sendPlugAgain(u16 object, u64 socket)
{
	NetworkPacket pkt(TOSERVER_PLUG_AGAIN, 10);
	pkt << object << socket;
	Send(&pkt);
}

void Client::sendPush(u16 object, v3f impulse)
{
	NetworkPacket pkt(TOSERVER_OBJECT_PUSH, 2 + 12);
	pkt << object << impulse;
	Send(&pkt);
}

void Client::sendGrab(u16 object, std::optional<std::pair<v3f, v3f>> grip)
{
	NetworkPacket pkt(TOSERVER_OBJECT_GRAB, 2 + 1 + 24);
	pkt << object << u8(grip.has_value());
	if (grip)
		pkt << grip->first << grip->second;
	Send(&pkt);
}

void Client::sendDrive(u16 object, std::optional<physics::Drive> drive)
{
	NetworkPacket pkt(TOSERVER_DRIVE, 2 + 1 + 13);
	pkt << object << u8(drive.has_value());
	if (drive)
		pkt << drive->forward << drive->right << drive->brake;
	Send(&pkt);
}

void Client::sendPose(u16 object, v3f position, v3f rotation)
{
	NetworkPacket pkt(TOSERVER_OBJECT_POSE, 2 + 12 + 12);
	pkt << object << position << rotation;
	Send(&pkt);
}

void Client::sendStroke(u16 piece, u64 part, const sculpt::Stroke &s, std::optional<piece::Joint> apart)
{
	NetworkPacket pkt(TOSERVER_SCULPT, 2 + 8 + 2 + 6 + 3 + 6 + 1 + 1 + 4 + 1);
	pkt << piece << part << u8(s.tool) << u8(s.size) << s.at.node << u8(s.at.cell.X) << u8(s.at.cell.Y)
			<< u8(s.at.cell.Z) << s.normal << u8(s.act)
			<< u8(s.mirror ? s.mirror->axis : 255) << s32(s.mirror ? s.mirror->twice : 0) << u8(s.to.has_value());
	if (s.to)
		pkt << s.to->node << u8(s.to->cell.X) << u8(s.to->cell.Y) << u8(s.to->cell.Z);
	pkt << u8(apart ? u8(*apart) + 1 : 0) << u8(s.deep.has_value());
	if (s.deep)
		pkt << u16(*s.deep);
	Send(&pkt);
}

void Client::sendUndo()
{
	NetworkPacket pkt(TOSERVER_UNDO, 0);
	Send(&pkt);
}

void Client::sendBlueprintSave(u16 piece, u64 part, const std::string &name)
{
	NetworkPacket pkt(TOSERVER_BLUEPRINT_SAVE, 2 + 8 + 2 + name.size());
	pkt << piece << part << name;
	Send(&pkt);
}

void Client::sendCopy(u16 piece, u64 part)
{
	NetworkPacket pkt(TOSERVER_CLIPBOARD, 1 + 2 + 8);
	pkt << u8(workshop::Clip::Copy) << piece << part;
	Send(&pkt);
}

void Client::sendPull(u16 piece, u64 part)
{
	NetworkPacket pkt(TOSERVER_CLIPBOARD, 1 + 2 + 8);
	pkt << u8(workshop::Clip::Pull) << piece << part;
	Send(&pkt);
}

void Client::sendUpright(u16 piece)
{
	NetworkPacket pkt(TOSERVER_UPRIGHT, 2);
	pkt << piece;
	Send(&pkt);
}

void Client::sendPaste()
{
	NetworkPacket pkt(TOSERVER_CLIPBOARD, 1);
	pkt << u8(workshop::Clip::Paste);
	Send(&pkt);
}

void Client::sendTake(const std::string &name)
{
	NetworkPacket pkt(TOSERVER_CLIPBOARD, 1 + 2 + name.size());
	pkt << u8(workshop::Clip::Take) << name;
	Send(&pkt);
}

void Client::sendForget(const std::string &name)
{
	NetworkPacket pkt(TOSERVER_CLIPBOARD, 1 + 2 + name.size());
	pkt << u8(workshop::Clip::Forget) << name;
	Send(&pkt);
}

void Client::sendBlueprintPlace(const std::string &name, v3s16 at)
{
	NetworkPacket pkt(TOSERVER_BLUEPRINT_PLACE, 2 + name.size() + 6);
	pkt << name << at;
	Send(&pkt);
}

void Client::sendInventoryAction(InventoryAction *a)
{
	std::ostringstream os(std::ios_base::binary);

	a->serialize(os);

	// Make data buffer
	std::string s = os.str();

	NetworkPacket pkt(TOSERVER_INVENTORY_ACTION, s.size());
	pkt.putRawString(s.c_str(),s.size());

	Send(&pkt);
}

bool Client::canSendChatMessage() const
{
	u32 now = time(NULL);
	float time_passed = now - m_last_chat_message_sent;

	float virt_chat_message_allowance = m_chat_message_allowance + time_passed *
			(CLIENT_CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);

	if (virt_chat_message_allowance < 1.0f)
		return false;

	return true;
}

void Client::sendChatMessage(const std::wstring &message)
{
	const s16 max_queue_size = g_settings->getS16("max_out_chat_queue_size");
	if (canSendChatMessage()) {
		u32 now = time(NULL);
		float time_passed = now - m_last_chat_message_sent;
		m_last_chat_message_sent = now;

		m_chat_message_allowance += time_passed * (CLIENT_CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);
		if (m_chat_message_allowance > CLIENT_CHAT_MESSAGE_LIMIT_PER_10S)
			m_chat_message_allowance = CLIENT_CHAT_MESSAGE_LIMIT_PER_10S;

		m_chat_message_allowance -= 1.0f;

		NetworkPacket pkt(TOSERVER_CHAT_MESSAGE, 2 + message.size() * sizeof(u16));
		pkt << message;
		Send(&pkt);
	} else if (m_out_chat_queue.size() < (u16) max_queue_size || max_queue_size < 0) {
		m_out_chat_queue.push(message);
	} else {
		infostream << "Could not queue chat message because maximum out chat queue size ("
				<< max_queue_size << ") is reached." << std::endl;
	}
}

void Client::clearOutChatQueue()
{
	m_out_chat_queue = std::queue<std::wstring>();
}


void Client::sendDamage(u16 damage)
{
	NetworkPacket pkt(TOSERVER_DAMAGE, sizeof(u16));
	pkt << damage;
	Send(&pkt);
}

void Client::sendRespawnLegacy()
{
	NetworkPacket pkt(TOSERVER_RESPAWN_LEGACY, 0);
	Send(&pkt);
}

void Client::sendReady()
{
	NetworkPacket pkt(TOSERVER_CLIENT_READY,
			1 + 1 + 1 + 1 + 2 + sizeof(char) * strlen(g_version_hash) + 2);

	pkt << (u8) VERSION_MAJOR << (u8) VERSION_MINOR << (u8) VERSION_PATCH
		<< (u8) 0 << (u16) strlen(g_version_hash);

	pkt.putRawString(g_version_hash, (u16) strlen(g_version_hash));
	pkt << (u16)FORMSPEC_API_VERSION;
	Send(&pkt);
}

void Client::sendPlayerPos()
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;

	// Save bandwidth by only updating position when
	// player is not dead and something changed

	if (m_activeobjects_received && player->isDead())
		return;

	ClientMap &map = m_env.getClientMap();
	u8 camera_fov   = std::fmin(255.0f, map.getCameraFov() * 80.0f);
	u8 wanted_range = std::fmin(255.0f,
			std::ceil(map.getWantedRange() * (1.0f / MAP_BLOCKSIZE)));

	u32 keyPressed = player->control.getKeysPressed();
	bool camera_inverted = m_camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT;
	f32 movement_speed = player->control.movement_speed;
	f32 movement_dir = player->control.movement_direction;

	bool identical = (
			player->last_position        == player->getPosition() &&
			player->last_speed           == player->getSpeed()    &&
			player->last_pitch           == player->getPitch()    &&
			player->last_yaw             == player->getYaw()      &&
			player->last_keyPressed      == keyPressed            &&
			player->last_camera_fov      == camera_fov            &&
			player->last_camera_inverted == camera_inverted       &&
			player->last_wanted_range    == wanted_range          &&
			player->last_movement_speed  == movement_speed        &&
			player->last_movement_dir    == movement_dir);

	if (identical) {
		// Since the movement info is sent non-reliable an unfortunate desync might
		// occur if we stop sending and the last packet gets lost or re-ordered.
		// To make this situation less likely we stop sending duplicate packets
		// only after a delay.
		m_playerpos_repeat_count++;
		if (m_playerpos_repeat_count >= 5)
			return;
	} else {
		m_playerpos_repeat_count = 0;
	}

	player->last_position        = player->getPosition();
	player->last_speed           = player->getSpeed();
	player->last_pitch           = player->getPitch();
	player->last_yaw             = player->getYaw();
	player->last_keyPressed      = keyPressed;
	player->last_camera_fov      = camera_fov;
	player->last_camera_inverted = camera_inverted;
	player->last_wanted_range    = wanted_range;
	player->last_movement_speed  = movement_speed;
	player->last_movement_dir    = movement_dir;

	NetworkPacket pkt(TOSERVER_PLAYERPOS, 12 + 12 + 4 + 4 + 4 + 1 + 1 + 1 + 4 + 4);

	writePlayerPos(player, &map, &pkt, camera_inverted);

	Send(&pkt);
}

void Client::sendHaveMedia(const std::vector<u32> &tokens)
{
	NetworkPacket pkt(TOSERVER_HAVE_MEDIA, 1 + tokens.size() * 4);

	sanity_check(tokens.size() < 256);

	pkt << static_cast<u8>(tokens.size());
	for (u32 token : tokens)
		pkt << token;

	Send(&pkt);
}

void Client::sendUpdateClientInfo(const ClientDynamicInfo& info)
{
	NetworkPacket pkt(TOSERVER_UPDATE_CLIENT_INFO, 4*2 + 4 + 4 + 4*2);
	pkt << (u32)info.render_target_size.X << (u32)info.render_target_size.Y;
	pkt << info.real_gui_scaling;
	pkt << info.real_hud_scaling;
	pkt << (f32)info.max_fs_size.X << (f32)info.max_fs_size.Y;
	pkt << info.touch_controls;

	Send(&pkt);
}

void Client::removeNode(v3s16 p)
{
	std::map<v3s16, MapBlock*> modified_blocks;

	try {
		const bool loose = m_nodedef->get(m_env.getMap().getNode(p)).loose; // Terra Firma: as addNode
		m_env.getMap().removeNodeAndUpdate(p, modified_blocks);
		if (loose) {
			m_ground_nodes.insert(p);
			for (const auto &modified_block : modified_blocks)
				m_ground_blocks.insert(modified_block.first);
			return;
		}
	}
	catch(InvalidPositionException &e) {
	}
	m_liquid_ground.insert(getNodeBlockPos(p)); // Terra Firma: the liquid beside it meets it anew

	for (const auto &modified_block : modified_blocks) {
		addUpdateMeshTaskWithEdge(modified_block.first, false, true);
	}
}

void Client::addNode(v3s16 p, MapNode n, bool remove_metadata)
{
	//TimeTaker timer1("Client::addNode()");

	std::map<v3s16, MapBlock*> modified_blocks;

	try {
		//TimeTaker timer3("Client::addNode(): addNodeAndUpdate");
		// Terra Firma: loose ground's comes with its shape, sent after it: made
		// again with it, once, at the frame's end (sprint 85: drawn between
		// the two it was smoothed, the old ramp, for a frame)
		const bool loose = m_nodedef->get(n).loose || m_nodedef->get(m_env.getMap().getNode(p)).loose;
		m_env.getMap().addNodeAndUpdate(p, n, modified_blocks, remove_metadata);
		if (loose) {
			m_ground_nodes.insert(p);
			for (const auto &modified_block : modified_blocks)
				m_ground_blocks.insert(modified_block.first);
			return;
		}
	}
	catch(InvalidPositionException &e) {
	}
	m_liquid_ground.insert(getNodeBlockPos(p)); // Terra Firma: the liquid beside it meets it anew

	for (const auto &modified_block : modified_blocks) {
		addUpdateMeshTaskWithEdge(modified_block.first, false, true);
	}
}

void Client::setPlayerControl(PlayerControl &control)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player);
	player->control = control;
}

void Client::setPlayerItem(u16 item)
{
	m_env.getLocalPlayer()->setWieldIndex(item);
	m_update_wielded_item = true;

	NetworkPacket pkt(TOSERVER_PLAYERITEM, 2);
	pkt << item;
	Send(&pkt);
}

// Returns true once after the inventory of the local player
// has been updated from the server.
bool Client::updateWieldedItem()
{
	if (!m_update_wielded_item)
		return false;

	m_update_wielded_item = false;

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player);
	if (auto *list = player->inventory.getList("main"))
		list->setModified(false);
	if (auto *list = player->inventory.getList("hand"))
		list->setModified(false);

	return true;
}

bool Client::consumeSkipNextWieldAnimation()
{
	bool v = m_skip_next_wield_animation;
	m_skip_next_wield_animation = false;
	return v;
}

scene::ISceneManager* Client::getSceneManager()
{
	return m_rendering_engine->get_scene_manager();
}

Inventory* Client::getInventory(const InventoryLocation &loc)
{
	switch(loc.type){
	case InventoryLocation::UNDEFINED:
	{}
	break;
	case InventoryLocation::CURRENT_PLAYER:
	{
		LocalPlayer *player = m_env.getLocalPlayer();
		assert(player);
		return &player->inventory;
	}
	break;
	case InventoryLocation::PLAYER:
	{
		// Check if we are working with local player inventory
		LocalPlayer *player = m_env.getLocalPlayer();
		if (!player || player->getName() != loc.name)
			return NULL;
		return &player->inventory;
	}
	break;
	case InventoryLocation::NODEMETA:
	{
		NodeMetadata *meta = m_env.getMap().getNodeMetadata(loc.p);
		if(!meta)
			return NULL;
		return meta->getInventory();
	}
	break;
	case InventoryLocation::DETACHED:
	{
		if (m_detached_inventories.count(loc.name) == 0)
			return NULL;
		return m_detached_inventories[loc.name];
	}
	break;
	default:
		FATAL_ERROR("Invalid inventory location type.");
		break;
	}
	return NULL;
}

void Client::inventoryAction(InventoryAction *a)
{
	/*
		Send it to the server
	*/
	sendInventoryAction(a);

	/*
		Predict some local inventory changes
	*/
	a->clientApply(this, this);

	// Remove it
	delete a;
}

int Client::getCrackLevel()
{
	return m_crack_level;
}

v3s16 Client::getCrackPos()
{
	return m_crack_pos;
}

void Client::setCrack(int level, v3s16 pos)
{
	int old_crack_level = m_crack_level;
	v3s16 old_crack_pos = m_crack_pos;

	m_crack_level = level;
	m_crack_pos = pos;

	if(old_crack_level >= 0 && (level < 0 || pos != old_crack_pos))
	{
		// remove old crack
		addUpdateMeshTaskForNode(old_crack_pos, false, true);
	}
	if(level >= 0 && (old_crack_level < 0 || pos != old_crack_pos))
	{
		// add new crack
		addUpdateMeshTaskForNode(pos, false, true);
	}
}

u16 Client::getHP()
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player);
	return player->hp;
}

std::optional<ChatMessage> Client::takeChatMessage()
{
	if (m_chat_queue.empty())
		return std::nullopt;
	std::unique_ptr<ChatMessage> said(m_chat_queue.front());
	m_chat_queue.pop();
	return *said;
}

void Client::typeChatMessage(const std::wstring &message)
{
	// Discard empty line
	if (message.empty())
		return;

	auto message_utf8 = wide_to_utf8(message);
	infostream << "Typed chat message: \"" << message_utf8 << "\"" << std::endl;

	// Send to others
	sendChatMessage(message);
}

void Client::addUpdateMeshTask(v3s16 p, bool ack_to_server, bool urgent)
{
	// Check if the block exists to begin with. In the case when a non-existing
	// neighbor is automatically added, it may not. In that case we don't want
	// to tell the mesh update thread about it.
	MapBlock *b = m_env.getMap().getBlockNoCreateNoEx(p);
	if (b == NULL)
		return;

	m_mesh_update_manager->updateBlock(&m_env.getMap(), p, ack_to_server, urgent);
}

void Client::addUpdateMeshTaskWithEdge(v3s16 blockpos, bool ack_to_server, bool urgent)
{
	m_mesh_update_manager->updateBlock(&m_env.getMap(), blockpos, ack_to_server, urgent, true);
	updateMeshReaders(blockpos, urgent); // Terra Firma
	// Terra Firma: what the player walks on is as the map is; a smoothed
	// top at a block's edge is shaped by the next block too, so those are
	// built again as well (sprint 72: a block that came before its
	// neighbours kept a wrong edge under one side of a car)
	if (m_body)
		for (const v3s16 &d : { v3s16(0, 0, 0), v3s16(1, 0, 0), v3s16(-1, 0, 0), v3s16(0, 1, 0),
				v3s16(0, -1, 0), v3s16(0, 0, 1), v3s16(0, 0, -1) })
			m_body->groundChanged(blockpos + d);
}

Body &Client::getBody()
{
	if (!m_body)
		m_body = std::make_unique<Body>(this);
	return *m_body;
}

// Terra Firma, an atlas: the meshes that read a block over a seam (made
// before it came or changed, they show what it was)
void Client::updateMeshReaders(v3s16 blockpos, bool urgent)
{
	if (!getAtlas())
		return;
	for (const v3s16 &reader : m_mesh_update_manager->readersOf(blockpos))
		if (reader != blockpos)
			addUpdateMeshTask(reader, false, urgent);
}

void Client::addUpdateMeshTaskForNode(v3s16 nodepos, bool ack_to_server, bool urgent)
{
	infostream << "Client::addUpdateMeshTaskForNode(): " << nodepos << std::endl;

	v3s16 blockpos = getNodeBlockPos(nodepos);
	m_mesh_update_manager->updateBlock(&m_env.getMap(), blockpos, ack_to_server, urgent, false);
	// Terra Firma: every block a node's change reaches. A cube's faces read
	// a node away (the leading edge); the ground's shape reads the columns
	// round it, up to two across and REACH + 1 up and down (ground::Ground).
	const s16 up = ground::REACH + 1;
	const v3s16 low = getNodeBlockPos(nodepos - v3s16(2, up, 2));
	const v3s16 high = getNodeBlockPos(nodepos + v3s16(1, up, 1));
	for (s16 x = low.X; x <= high.X; x++)
	for (s16 y = low.Y; y <= high.Y; y++)
	for (s16 z = low.Z; z <= high.Z; z++)
		if (v3s16(x, y, z) != blockpos)
			addUpdateMeshTask(v3s16(x, y, z), false, urgent);
	updateMeshReaders(blockpos, urgent); // Terra Firma
}

ClientEvent *Client::getClientEvent()
{
	FATAL_ERROR_IF(m_client_event_queue.empty(),
			"Cannot getClientEvent, queue is empty.");

	ClientEvent *event = m_client_event_queue.front();
	m_client_event_queue.pop();
	return event;
}

const Address Client::getServerAddress()
{
	return m_con ? m_con->GetPeerAddress(PEER_ID_SERVER) : Address();
}

bool Client::mediaReceiveProgress(s32 &received, s32 &total, size_t &received_size) const
{
	if (m_media_downloader && m_media_downloader->isStarted()) {
		m_media_downloader->getProgress(received, total, received_size);
		return true;
	}

	return false;
}

void Client::drawLoadScreen(const std::wstring &text, float dtime, int percent)
{
	m_rendering_engine->run();
	m_rendering_engine->draw_load_screen(text, guienv, m_tsrc, dtime, percent);
}

struct TextureUpdateArgs {
	u64 last_time_ms;
	std::wstring text_base;
};

void Client::showUpdateProgressTexture(void *args, float progress)
{
	auto *targs = reinterpret_cast<TextureUpdateArgs*>(args);
	u16 cur_percent = std::ceil(100 * progress);

	// Throttle menu drawing
	bool do_draw = false;
	u64 time_ms = porting::getTimeMs();
	if (time_ms - targs->last_time_ms > 50) {
		do_draw = true;
		targs->last_time_ms = time_ms;
	}

	if (!do_draw)
		return;

	std::wostringstream strm;
	strm << targs->text_base << L" " << cur_percent << L"%...";
	// 70% -> 99%
	int shown_progress = 70 + std::ceil(0.29f * cur_percent);
	m_rendering_engine->draw_load_screen(strm.str(), guienv, m_tsrc,
		0, shown_progress);
}

void Client::afterContentReceived()
{
	infostream<<"Client::afterContentReceived() started"<<std::endl;
	trace::net::event("Content ready");
	assert(m_itemdef_received); // pre-condition
	assert(m_nodedef_received); // pre-condition
	assert(mediaReceived()); // pre-condition

	// Clear cached pre-scaled 2D GUI images, as this cache
	// might have images with the same name but different
	// content from previous sessions.
	guiScalingCacheClear();

	infostream<<"- Rebuilding images and textures"<<std::endl;
	m_rendering_engine->draw_load_screen(wstrgettext("Loading textures..."),
			guienv, m_tsrc, 0, 66);
	m_tsrc->rebuildImagesAndTextures();

	infostream<<"- Rebuilding shaders"<<std::endl;
	m_rendering_engine->draw_load_screen(wstrgettext("Rebuilding shaders..."),
			guienv, m_tsrc, 0, 68);
	m_shsrc->rebuildShaders();

	infostream<<"- Updating node aliases"<<std::endl;
	m_rendering_engine->draw_load_screen(wstrgettext("Initializing nodes..."),
			guienv, m_tsrc, 0, 70);
	m_nodedef->updateAliases(m_itemdef);
	// Apply texture overrides from texturepack/override.txt
	for (const std::string &path : getTexturePathsReversed()) {
		TextureOverrideSource override_source(path + DIR_DELIM + "override.txt");
		m_nodedef->applyTextureOverrides(override_source.getNodeTileOverrides());
		m_itemdef->applyTextureOverrides(override_source.getItemTextureOverrides());
	}
	m_nodedef->setNodeRegistrationStatus(true);
	m_nodedef->runNodeResolveCallbacks();

	infostream<<"- Updating node visuals"<<std::endl;
	TextureUpdateArgs tu_args;
	tu_args.last_time_ms = porting::getTimeMs();
	tu_args.text_base = wstrgettext("Initializing nodes");
	NodeVisuals::fillNodeVisuals(m_nodedef, this, &tu_args);

	// cf. updateStaticLighting()
	set_light_curve(g_settings->getFloat("display_gamma"));

	infostream<<"- Starting mesh update thread"<<std::endl;
	m_mesh_update_manager->start();

	m_state = LC_Ready;
	sendReady();

	m_rendering_engine->draw_load_screen(wstrgettext("Done!"), guienv, m_tsrc, 0, 100);
	infostream<<"Client::afterContentReceived() done"<<std::endl;
}

float Client::getRTT()
{
	assert(m_con);
	return m_con->getPeerStat(PEER_ID_SERVER,con::AVG_RTT);
}

float Client::getCurRate()
{
	assert(m_con);
	return (m_con->getLocalStat(con::CUR_INC_RATE) +
			m_con->getLocalStat(con::CUR_DL_RATE));
}

void Client::takeScreenshotIfRequested()
{
	if (!m_take_screenshot)
		return;
	m_take_screenshot = false;

	video::IVideoDriver *driver = m_rendering_engine->get_video_driver();
	std::string filename;
	if (takeScreenshot(driver, filename)) {
		std::string msg = fmtgettext("Saved screenshot to \"%s\"", filename.c_str());
		pushToChatQueue(new ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
				utf8_to_wide(msg)));
	}
}

void Client::pushToEventQueue(ClientEvent *event)
{
	m_client_event_queue.push(event);
}

// IGameDef interface
// Under envlock
IItemDefManager* Client::getItemDefManager()
{
	return m_itemdef;
}
const NodeDefManager* Client::getNodeDefManager()
{
	return m_nodedef;
}
ICraftDefManager* Client::getCraftDefManager()
{
	return NULL;
	//return m_craftdef;
}
ITextureSource* Client::getTextureSource()
{
	return m_tsrc;
}
IWritableShaderSource* Client::getShaderSource()
{
	return m_shsrc;
}

u16 Client::allocateUnknownNodeId(const std::string &name)
{
	errorstream << "Client::allocateUnknownNodeId(): "
			<< "Client cannot allocate node IDs" << std::endl;
	FATAL_ERROR("Client allocated unknown node");

	return CONTENT_IGNORE;
}
ISoundManager* Client::getSoundManager()
{
	return m_sound;
}
MtEventManager* Client::getEventManager()
{
	return m_event;
}

ParticleManager* Client::getParticleManager()
{
	return m_particle_manager.get();
}

scene::IAnimatedMesh *Client::getMesh(const std::string &filename, bool *is_shared)
{
	auto it = m_mesh_data.find(filename);
	if (it == m_mesh_data.end())
		return nullptr;

	if (is_shared)
		*is_shared = true; // this is currently always the case

	// Try getting it from cache explicitly
	auto *sm = m_rendering_engine->get_scene_manager();
	auto *mesh = sm->getMeshCache()->getMeshByName(filename.c_str());
	if (mesh) {
		mesh->grab();
		return mesh;
	}

	// Load the mesh from file data
	const std::string &data = it->second;
	io::IReadFile *rfile = m_rendering_engine->get_filesystem()->createMemoryReadFile(
			data.c_str(), data.size(), filename.c_str());
	FATAL_ERROR_IF(!rfile, "Could not create/open RAM file");

	mesh = sm->getMesh(rfile);
	rfile->drop();
	if (!mesh)
		return nullptr;
	mesh->grab();
	return mesh;
}

/*
 * Mod channels
 */

const std::string &Client::getFormspecPrepend() const
{
	return m_env.getLocalPlayer()->formspec_prepend;
}

std::vector<std::pair<std::string, std::string>> Client::takeUiValues()
{
	return std::exchange(m_ui_values, {});
}

void Client::sendUiEvent(const std::string &name, const std::string &payload_json)
{
	NetworkPacket pkt(TOSERVER_UI, 0);
	pkt << name;
	pkt.putLongString(payload_json);
	Send(&pkt);
}
