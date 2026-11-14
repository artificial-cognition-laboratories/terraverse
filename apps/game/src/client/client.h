// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "clientenvironment.h"
#include "piece/machine.h"
#include "sculpt/sculpt.h"
#include "workshop/motion.h" // Terra Firma: Materials, Grip
#include "gamedef.h"
#include "gameparams.h" // ELoginRegister
#include "inventorymanager.h"
#include "irrlichttypes.h"
#include "lighting.h"
#include "network/address.h"
#include "network/networkprotocol.h" // multiple enums
#include "network/peerhandler.h"
#include "util/numeric.h"
#include "util/string.h" // StringMap
#include "far/terrain.h" // Terra Firma: far::Horizon
#include "planet/atlas.h" // Terra Firma

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <unordered_set>
#include <vector>
#include "client/liquids.h" // Terra Firma: liquids::Stream

#if !IS_CLIENT_BUILD
#error Do not include in server builds
#endif

#define CLIENT_CHAT_MESSAGE_LIMIT_PER_10S 10.0f

class Body;
namespace pile { class Piles; }
class Camera;
class ClientMediaDownloader;
class ISoundManager;
class IWritableItemDefManager;
class IWritableShaderSource;
class IWritableTextureSource;
class LuaError;
class MapDatabase;
class MeshUpdateManager;
class Minimap;
class ModChannelMgr;
class MtEventManager;
class NetworkPacket;
class NodeDefManager;
class ParticleManager;
class RenderingEngine;
class SingleMediaDownloader;
class ClientScripting;
class SSCSMController;
struct ChatMessage;
struct ClientDynamicInfo;
struct ClientEvent;
struct MapDrawControl;
struct MapNode;
struct PlayerControl;
struct PointedThing;
struct ItemVisualsManager;
struct ModVFS;

namespace scene {
class IAnimatedMesh;
}

namespace con {
class IConnection;
}
using sound_handle_t = int;

enum LocalClientState {
	LC_Created,
	LC_Init,
	LC_Ready
};

/*
	Packet counter
*/

class PacketCounter
{
public:
	PacketCounter() = default;

	void add(u16 command)
	{
		auto n = m_packets.find(command);
		if (n == m_packets.end())
			m_packets[command] = 1;
		else
			n->second++;
	}

	void clear()
	{
		m_packets.clear();
	}

	u32 sum() const;
	void print(std::ostream &o) const;

private:
	// command, count
	std::map<u16, u32> m_packets;
};

class Client : public con::PeerHandler, public InventoryManager, public IGameDef
{
public:
	/*
		NOTE: Nothing is thread-safe here.
	*/

	Client(
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
			ItemVisualsManager *item_visuals,
			ELoginRegister allow_login_or_register
	);

	~Client();
	DISABLE_CLASS_COPY(Client);

	/*
	 request all threads managed by client to be stopped
	 */
	void Stop();


	bool isShutdown();

	void connect(const Address &address, const std::string &address_name);
	// Terra Firma: connect from this socket — the one STUN mapped, whose
	// public address the host is punching toward (src/online/join.h).
	void connect(const Address &address, const std::string &address_name, UDPSocket &&socket);

	/*
		Stuff that references the environment is valid only as
		long as this is not called. (eg. Players)
		If this throws a PeerNotFoundException, the connection has
		timed out.
	*/
	void step(float dtime);

	/*
	 * Command Handlers
	 */

	void handleCommand(NetworkPacket* pkt);

	void handleCommand_Null(NetworkPacket* pkt) {};
	void handleCommand_Deprecated(NetworkPacket* pkt);
	void handleCommand_Hello(NetworkPacket* pkt);
	void handleCommand_AuthAccept(NetworkPacket* pkt);
	void handleCommand_AcceptSudoMode(NetworkPacket* pkt);
	void handleCommand_DenySudoMode(NetworkPacket* pkt);
	void handleCommand_AccessDenied(NetworkPacket* pkt);
	void handleCommand_RemoveNode(NetworkPacket* pkt);
	void handleCommand_AddNode(NetworkPacket* pkt);
	void handleCommand_NodemetaChanged(NetworkPacket *pkt);
	void handleCommand_BlockData(NetworkPacket* pkt);
	void handleCommand_Inventory(NetworkPacket* pkt);
	void handleCommand_TimeOfDay(NetworkPacket* pkt);
	void handleCommand_ChatMessage(NetworkPacket *pkt);
	void handleCommand_ActiveObjectRemoveAdd(NetworkPacket* pkt);
	void handleCommand_ActiveObjectMessages(NetworkPacket* pkt);
	void handleCommand_Movement(NetworkPacket* pkt);
	void handleCommand_Fov(NetworkPacket *pkt);
	void handleCommand_HP(NetworkPacket* pkt);
	void handleCommand_Breath(NetworkPacket* pkt);
	void handleCommand_MovePlayer(NetworkPacket* pkt);
	void handleCommand_MovePlayerRel(NetworkPacket* pkt);
	void handleCommand_DeathScreenLegacy(NetworkPacket* pkt);
	void handleCommand_AnnounceMedia(NetworkPacket* pkt);
	void handleCommand_Media(NetworkPacket* pkt);
	void handleCommand_NodeDef(NetworkPacket* pkt);
	void handleCommand_ItemDef(NetworkPacket* pkt);
	void handleCommand_PlaySound(NetworkPacket* pkt);
	void handleCommand_StopSound(NetworkPacket* pkt);
	void handleCommand_FadeSound(NetworkPacket *pkt);
	void handleCommand_Privileges(NetworkPacket* pkt);
	void handleCommand_InventoryFormSpec(NetworkPacket* pkt);
	void handleCommand_DetachedInventory(NetworkPacket* pkt);
	void handleCommand_ShowFormSpec(NetworkPacket* pkt);
	void handleCommand_SpawnParticle(NetworkPacket* pkt);
	void handleCommand_SpawnParticleBatch(NetworkPacket *pkt);
	void handleCommand_Ui(NetworkPacket *pkt);
	void handleCommand_Horizon(NetworkPacket *pkt);
	void handleCommand_Atlas(NetworkPacket *pkt);
	void handleCommand_Window(NetworkPacket *pkt);
	void handleCommand_Shape(NetworkPacket *pkt); // Terra Firma
	void handleCommand_Fill(NetworkPacket *pkt); // Terra Firma (sprint 81)
	void handleCommand_Stream(NetworkPacket *pkt); // Terra Firma (sprint 82)
	void handleCommand_Rivers(NetworkPacket *pkt); // Terra Firma (sprint 115)
	// Terra Firma: map blocks whose liquid changed since the liquid layer last looked (sprint 81)
	std::unordered_set<v3s16> takeLiquidsChanged() { std::unordered_set<v3s16> out; out.swap(m_liquids_changed); return out; }
	std::vector<liquids::Stream> takeStreams() { std::vector<liquids::Stream> out; out.swap(m_streams); return out; }
	const liquids::Rivers &rivers() const { return m_rivers; } // Terra Firma (sprint 115)
	// blocks whose ground changed under or beside their liquid (sprint 82: the sea's looked at again)
	std::unordered_set<v3s16> takeLiquidGround() { std::unordered_set<v3s16> out; out.swap(m_liquid_ground); return out; }
	void liquidsChanged(v3s16 blockpos) { m_liquids_changed.insert(blockpos); }
	void handleCommand_Blueprints(NetworkPacket *pkt); // Terra Firma
	void handleCommand_Held(NetworkPacket *pkt); // Terra Firma
	void handleCommand_Materials(NetworkPacket *pkt); // Terra Firma
	void handleCommand_AddParticleSpawner(NetworkPacket* pkt);
	void handleCommand_DeleteParticleSpawner(NetworkPacket* pkt);
	void handleCommand_HudAdd(NetworkPacket* pkt);
	void handleCommand_HudRemove(NetworkPacket* pkt);
	void handleCommand_HudChange(NetworkPacket* pkt);
	void handleCommand_HudSetFlags(NetworkPacket* pkt);
	void handleCommand_HudSetParam(NetworkPacket* pkt);
	void handleCommand_HudSetSky(NetworkPacket* pkt);
	void handleCommand_HudSetSun(NetworkPacket* pkt);
	void handleCommand_HudSetMoon(NetworkPacket* pkt);
	void handleCommand_HudSetStars(NetworkPacket* pkt);
	void handleCommand_CloudParams(NetworkPacket* pkt);
	void handleCommand_OverrideDayNightRatio(NetworkPacket* pkt);
	void handleCommand_LocalPlayerAnimations(NetworkPacket* pkt);
	void handleCommand_EyeOffset(NetworkPacket* pkt);
	void handleCommand_UpdatePlayerList(NetworkPacket* pkt);
	void handleCommand_ModChannelMsg(NetworkPacket *pkt);
	void handleCommand_ModChannelSignal(NetworkPacket *pkt);
	void handleCommand_SrpBytesSandB(NetworkPacket *pkt);
	void handleCommand_FormspecPrepend(NetworkPacket *pkt);
	void handleCommand_CSMRestrictionFlags(NetworkPacket *pkt);
	void handleCommand_PlayerSpeed(NetworkPacket *pkt);
	void handleCommand_MediaPush(NetworkPacket *pkt);
	void handleCommand_MinimapModes(NetworkPacket *pkt);
	void handleCommand_SetLighting(NetworkPacket *pkt);
	void handleCommand_Camera(NetworkPacket* pkt);

	void ProcessData(NetworkPacket *pkt);

	void Send(NetworkPacket* pkt);

	void interact(InteractAction action, const PointedThing &pointed);

	void sendNodemetaFields(v3s16 p, const std::string &formname,
		const StringMap &fields);
	void sendInventoryFields(const std::string &formname,
		const StringMap &fields);
	void sendInventoryAction(InventoryAction *a);
	// Terra Firma: a stroke of the tools in hand (TOSERVER_SCULPT)
	// on the world (piece 0) or a piece (its object id), in its own grid
	// apart: what it adds made a part of its own, joined so
	void sendStroke(u16 piece, u64 part, const sculpt::Stroke &s, std::optional<piece::Joint> apart);
	// Terra Firma: a new object, held up in front (TOSERVER_PIECE_NEW)
	void sendPieceNew();
	// Terra Firma: an object held or let go (sprint 49)
	void sendHold(u16 object, workshop::Grip grip);
	// where a held object is (BS) and how it's turned (degrees, piece::frame)
	void sendPose(u16 object, v3f position, v3f rotation);
	void sendPush(u16 object, v3f impulse);
	// Terra Firma: a point of an object grabbed (in its frame, BS) and pulled
	// toward `hand`, or let go (none)
	void sendGrab(u16 object, std::optional<std::pair<v3f, v3f>> grip);
	// Terra Firma: driving from a machine's seat, or getting up (none)
	void sendDrive(u16 object, std::optional<physics::Drive> drive);
	// Terra Firma: another of the last plugged in, into this socket (sprint 64)
	void sendPlugAgain(u16 object, u64 socket);
	// Terra Firma: an object's design made a shell, `thickness` sixteenths
	// rounded (sprint 75): its edges rounded to `thickness`, not hollowed
	void sendHollow(u16 object, u64 part, int thickness, bool rounded);
	// Terra Firma: machines (sprints 52–53): a joint between two parts set
	// (made at `at`, a face of a's, if there's none), two machines joined
	// face to face, a part taken off or deleted
	// joint none: the joint taken away
	// spring (sprint 75): a sprung joint's set too
	void sendLink(u16 object, u64 a, u64 b, std::optional<piece::Joint> joint,
			const std::optional<piece::Machine::Face> &at, std::optional<piece::Spring> spring = std::nullopt);
	void sendJoin(u16 a, const piece::Machine::Face &on_a, u16 b, const piece::Machine::Face &on_b, piece::Joint joint, u8 spin);
	void sendPart(u16 object, u64 part, bool remove);
	// Terra Firma: sockets (sprint 56): one made on a face (and its
	// mirror's), one taken away, a machine carried onto one
	void sendSocket(u16 object, const piece::Machine::Face &face, piece::Joint joint,
			std::optional<sculpt::Stroke::Mirror> mirror);
	void sendUnsocket(u16 object, u64 socket);
	void sendPlug(u16 a, u64 socket, u16 b, const piece::Machine::Face &on_b, u8 spin);
	// Terra Firma: the new object the server made for the player, to focus
	// when it's here; taken, it's gone
	// the object id, and how it's handed (carried: a paste, sprint 74; pulled off, 77)
	std::optional<std::pair<u16, workshop::Handed>> takeHeld() { return std::exchange(m_held, std::nullopt); }
	// Terra Firma: undo the last stroke (TOSERVER_UNDO)
	void sendUndo();
	// Terra Firma: the library (sprint 48): an object (its object id) kept
	// as a blueprint, one put down where `at` is
	void sendBlueprintSave(u16 piece, u64 part, const std::string &name);
	// Terra Firma: copy a part, paste, take one from the library or forget
	// it (TOSERVER_CLIPBOARD, sprint 74)
	void sendCopy(u16 piece, u64 part);
	void sendPaste();
	void sendPull(u16 piece, u64 part); // sprint 77: off by hand
	void sendUpright(u16 piece); // sprint 79
	void sendTake(const std::string &name);
	void sendForget(const std::string &name);
	void sendBlueprintPlace(const std::string &name, v3s16 at);
	// Terra Firma: the player's blueprints, as the server last said
	const std::vector<std::string> &getBlueprints() const { return m_blueprints; }
	// Terra Firma: what blocks weigh, as the server says (sprint 58)
	const workshop::Materials &getMaterials() const { return m_materials; }
	// Terra Firma: the player's body: this client's physics (sprint 58)
	Body &getBody();
	void sendChatMessage(const std::wstring &message);
	void clearOutChatQueue();
	void sendChangePassword(const std::string &oldpassword,
		const std::string &newpassword);
	void sendDamage(u16 damage);
	void sendRespawnLegacy();
	void sendReady();
	void sendHaveMedia(const std::vector<u32> &tokens);
	void sendUpdateClientInfo(const ClientDynamicInfo &info);

	// Terra Firma: the page's UI channel. Values the server set for this
	// player's page (key, JSON), oldest first, taken once; and events back.
	std::vector<std::pair<std::string, std::string>> takeUiValues();
	void sendUiEvent(const std::string &name, const std::string &payload_json);
	// Terra Firma: set once the server says this is a terra world (the far view's input).
	const std::optional<far::Horizon> &getHorizon() const { return m_horizon; }
	// Terra Firma: the planet the world is, if it is one
	const planet::Atlas *getAtlas() const { return m_atlas ? &*m_atlas : nullptr; }
	// Where the server says the player is over the planet, carried forward
	// to now; none while that's where they stand.

	ClientEnvironment& getEnv() { return m_env; }
	ITextureSource *tsrc() { return getTextureSource(); }
	ISoundManager *sound() { return getSoundManager(); }
	static const std::string &getBuiltinLuaPath();
	static const std::string &getClientModsLuaPath();

	const std::vector<ModSpec> &getMods() const override;
	const ModSpec* getModSpec(const std::string &modname) const override;

	// Causes urgent mesh updates (unlike Map::add/removeNodeWithEvent)
	void removeNode(v3s16 p);

	// helpers to enforce CSM restrictions
	MapNode CSMGetNode(v3s16 p, bool *is_valid_position);
	int CSMClampRadius(v3s16 pos, int radius);
	v3s16 CSMClampPos(v3s16 pos);

	void addNode(v3s16 p, MapNode n, bool remove_metadata = true);

	void setPlayerControl(PlayerControl &control);

	// Returns true if the inventory of the local player has been
	// updated from the server. If it is true, it is set to false.
	bool updateWieldedItem();

	bool consumeSkipNextWieldAnimation();

	/* InventoryManager interface */
	Inventory* getInventory(const InventoryLocation &loc) override;
	void inventoryAction(InventoryAction *a) override;

	// Send the item number 'item' as player item to the server
	void setPlayerItem(u16 item);

	const std::set<std::string> &getConnectedPlayerNames()
	{
		return m_env.getPlayerNames();
	}

	float getAnimationTime() const
	{
		return m_animation_time;
	}

	/// @return integer ∊ [0, crack_animation_length] or -1 for invalid
	int getCrackLevel();
	v3s16 getCrackPos();
	void setCrack(int level, v3s16 pos);

	u16 getHP();

	bool checkPrivilege(const std::string &priv) const
	{ return (m_privileges.count(priv) != 0); }

	const std::unordered_set<std::string> &getPrivilegeList() const
	{ return m_privileges; }

	bool getChatMessage(std::wstring &message);
	void typeChatMessage(const std::wstring& message);

	u64 getMapSeed() const { return m_map_seed; }

	void addUpdateMeshTask(v3s16 blockpos, bool ack_to_server=false, bool urgent=false);
	// Including blocks at appropriate edges
	void addUpdateMeshTaskWithEdge(v3s16 blockpos, bool ack_to_server=false, bool urgent=false);
	void addUpdateMeshTaskForNode(v3s16 nodepos, bool ack_to_server=false, bool urgent=false);
	// Terra Firma: and the meshes that read the block over an atlas's seam
	void updateMeshReaders(v3s16 blockpos, bool urgent);
	// Terra Firma: the meshes across a planet face's edge from a node on it,
	// which show it in their border (QueuedMeshUpdate::halo)

	bool hasClientEvents() const { return !m_client_event_queue.empty(); }
	// Get event from queue. If queue is empty, it triggers an assertion failure.
	ClientEvent * getClientEvent();

	bool accessDenied() const { return m_access_denied; }

	bool reconnectRequested() const { return m_access_denied_reconnect; }

	void setFatalError(const std::string &reason)
	{
		m_access_denied = true;
		m_access_denied_reason = reason;
	}
	void setFatalError(const LuaError &e);

	// Renaming accessDeniedReason to better name could be good as it's used to
	// disconnect client when CSM failed.
	const std::string &accessDeniedReason() const { return m_access_denied_reason; }

	bool itemdefReceived() const
	{ return m_itemdef_received; }
	bool nodedefReceived() const
	{ return m_nodedef_received; }
	bool mediaReceived() const
	{ return !m_media_downloader; }
	bool activeObjectsReceived() const
	{ return m_activeobjects_received; }

	u16 getProtoVersion() const
	{ return m_proto_ver; }

	// Whether the server is in "simple singleplayer mode".
	// This implies "m_internal_server = true".
	bool m_simple_singleplayer_mode;

	// Whether the server is hosted by the same Luanti instance and singletons
	// like g_settings are shared between client and server.
	//
	// This is intentionally *not* true if we're just connecting to a localhost
	// server hosted by a different Luanti instance.
	bool m_internal_server;

	bool mediaReceiveProgress(s32 &received, s32 &total, size_t &received_size) const;

	void drawLoadScreen(const std::wstring &text, float dtime, int percent);
	void afterContentReceived();
	void loadSSCSM();
	void showUpdateProgressTexture(void *args, float progress);

	float getRTT();
	float getCurRate();
	// has the server ever replied to us, used for connection retry/fallback
	bool hasServerReplied() const {
		return getProtoVersion() != 0; // (set in TOCLIENT_HELLO)
	}

	Minimap* getMinimap() { return m_minimap.get(); }
	void setCamera(Camera* camera) { m_camera = camera; }

	Camera* getCamera () { return m_camera; }
	scene::ISceneManager *getSceneManager();

	// IGameDef interface
	bool isClient() override { return true; }
	IItemDefManager* getItemDefManager() override;
	const NodeDefManager* getNodeDefManager() override;
	ICraftDefManager* getCraftDefManager() override;
	ITextureSource* getTextureSource();
	virtual IWritableShaderSource* getShaderSource();
	u16 allocateUnknownNodeId(const std::string &name) override;
	virtual ISoundManager* getSoundManager();
	MtEventManager* getEventManager();
	virtual ParticleManager* getParticleManager();

	bool checkLocalPrivilege(const std::string &priv)
	{ return checkPrivilege(priv); }

	// Gets a pointer to a named mesh
	// If you want to modify it, you may need to clone it first (-> `is_shared`)
	// (the returned pointer must be dropped)
	scene::IAnimatedMesh *getMesh(const std::string &filename, bool *is_shared = nullptr);

	ModVFS *getModVFS() { return m_mod_vfs.get(); }
	ModStorageDatabase *getModStorageDatabase() override { return m_mod_storage_database; }

	ItemVisualsManager *getItemVisualsManager() { return m_item_visuals_manager; }

	// Migrates away old files-based mod storage if necessary
	void migrateModStorage();

	// The following set of functions is used by ClientMediaDownloader
	// Insert a media file appropriately into the appropriate manager
	bool loadMedia(const std::string &data, const std::string &filename,
		bool from_media_push = false);

	// Send a request for conventional media transfer
	void requestMedia(const std::vector<std::string> &file_requests);

	LocalClientState getState() const { return m_state; }

	// Request a screenshot to be taken at the end of the frame.
	void requestScreenshot() { m_take_screenshot = true; }

	// Must be called right before endScene() to take requested screenshots.
	void takeScreenshotIfRequested();

	inline void pushToChatQueue(ChatMessage *cec)
	{
		m_chat_queue.push(cec);
	}

	ClientScripting *getScript() { return m_script; }
	bool modsLoaded() const { return m_mods_loaded; }

	void pushToEventQueue(ClientEvent *event);

	// IP and port we're connected to
	const Address getServerAddress();

	// Hostname of the connected server (but can also be a numerical IP)
	const std::string &getAddressName() const
	{
		return m_address_name;
	}

	inline u64 getCSMRestrictionFlags() const
	{
		return m_csm_restriction_flags;
	}

	inline bool checkCSMRestrictionFlag(CSMRestrictionFlags flag) const
	{
		return m_csm_restriction_flags & flag;
	}

	bool joinModChannel(const std::string &channel) override;
	bool leaveModChannel(const std::string &channel) override;
	bool sendModChannelMessage(const std::string &channel,
			const std::string &message) override;
	ModChannel *getModChannel(const std::string &channel) override;

	const std::string &getFormspecPrepend() const;

	inline MeshGrid getMeshGrid() const
	{
		return m_mesh_grid;
	}

	inline const StaticLighting &getCommittedStaticLighting() const
	{
		return m_committed_static_light;
	}

	bool inhibit_inventory_revert = false;

private:
	std::vector<std::pair<std::string, std::string>> m_ui_values;
	std::optional<far::Horizon> m_horizon;
	std::vector<std::string> m_blueprints; // Terra Firma
	std::unordered_set<v3s16> m_liquids_changed; // Terra Firma: blocks whose liquid changed (sprint 81)
	std::unordered_set<v3s16> m_liquid_ground; // Terra Firma: blocks whose nodes changed (sprint 82)
	std::vector<liquids::Stream> m_streams; // Terra Firma: liquid poured since the last frame (sprint 82)
	liquids::Rivers m_rivers; // Terra Firma: the rivers near, as the server last said (sprint 115)
	workshop::Materials m_materials; // Terra Firma
	std::unique_ptr<Body> m_body; // Terra Firma: made when it's first asked for
	// Terra Firma (sprint 84): loose ground this client's own wheels and
	// feet press, predicted here as the server runs it; its word replaces
	// it as it arrives
	std::unique_ptr<pile::Piles> m_piles;
	void pressGround(float dtime);
	// Terra Firma (sprint 85): each block's mesh detail as made, and where
	// the camera was when they were last weighed (moved a node: again)
	std::unordered_map<v3s16, u8> m_detail;
	v3s16 m_detail_camera{ S16_MIN, S16_MIN, S16_MIN };
	void meshDetail(v3s16 camera);
	std::vector<v3s16> m_detail_pending; // made again a few a frame, not all at once
	// Terra Firma (sprint 85): nodes whose ground changed this frame (piles,
	// the server's shapes): each block they reach is made again once
	std::unordered_set<v3s16> m_ground_nodes;
	std::unordered_set<v3s16> m_ground_blocks; // ... and the blocks they relit
	void flushGround();
	std::unordered_set<v3s16> m_body_ground; // ... and the physics' ground, a few times a second
	float m_body_ground_wait = 0;
	void flushBodyGround(float dtime);
	std::optional<std::pair<u16, workshop::Handed>> m_held; // Terra Firma
	std::optional<planet::Atlas> m_atlas; // Terra Firma: or an atlas planet (never both)

	struct PendingMediaDownload {
		// Tokens to ack to the server. multiple because server can send duplicate
		// requests
		std::vector<u32> tokens;
		std::string name; // Filename
		std::shared_ptr<SingleMediaDownloader> d;

		PendingMediaDownload(u32 token, const std::string &name,
				const std::shared_ptr<SingleMediaDownloader> &d) : name(name), d(d)
		{
			tokens.push_back(token);
		}
	};

	void loadMods();

	// Virtual methods from con::PeerHandler
	void peerAdded(con::IPeer *peer) override;
	void deletingPeer(con::IPeer *peer, bool timeout) override;

	void initLocalMapSaving(const Address &address, const std::string &hostname);

	void ReceiveAll();

	void deleteAuthData();
	// helper method shared with clientpackethandler
	static AuthMechanism choseAuthMech(const u32 mechs);

	void sendInit(const std::string &playerName);
	void startAuth(AuthMechanism chosen_auth_mechanism);
	void sendDeletedBlocks(std::vector<v3s16> &blocks);
	void sendGotBlocks(const std::vector<v3s16> &blocks);
	void sendRemovedSounds(const std::vector<s32> &soundList);
	void sendPlayerPos();

	bool updateStaticLighting(const StaticLighting &future);

	bool canSendChatMessage() const;

	float m_packetcounter_timer = 0.0f;
	float m_connection_reinit_timer = 0.1f;
	float m_avg_rtt_timer = 0.0f;
	float m_playerpos_send_timer = 0.0f;
	int m_playerpos_repeat_count = 0;
	IntervalLimiter m_map_timer_and_unload_interval;

	IWritableTextureSource *m_tsrc;
	IWritableShaderSource *m_shsrc;
	IWritableItemDefManager *m_itemdef;
	NodeDefManager *m_nodedef;
	ISoundManager *m_sound;
	MtEventManager *m_event;
	RenderingEngine *m_rendering_engine;
	ItemVisualsManager *m_item_visuals_manager;

	std::unique_ptr<MeshUpdateManager> m_mesh_update_manager;
	ClientEnvironment m_env;
	std::unique_ptr<ParticleManager> m_particle_manager;
	std::unique_ptr<con::IConnection> m_con;
	std::string m_address_name;
	ELoginRegister m_allow_login_or_register = ELoginRegister::Any;
	Camera *m_camera = nullptr;
	std::unique_ptr<Minimap> m_minimap;

	// Server serialization version
	u8 m_server_ser_ver;

	// Used version of the protocol with server
	// If 0, server init hasn't been received yet.
	u16 m_proto_ver = 0;

	bool m_update_wielded_item = false;
	bool m_skip_next_wield_animation = false;
	std::unique_ptr<Inventory> m_inventory_from_server;
	float m_inventory_from_server_age = 0.0f;
	s32 m_mapblock_limit_logged = 0;
	PacketCounter m_packetcounter;
	// Block mesh animation parameters
	float m_animation_time = 0.0f;
	int m_crack_level = -1;
	v3s16 m_crack_pos;
	std::queue<std::wstring> m_out_chat_queue;
	u32 m_last_chat_message_sent;
	float m_chat_message_allowance = 5.0f;
	std::queue<ChatMessage *> m_chat_queue;

	// The authentication methods we can use to enter sudo mode (=change password)
	u32 m_sudo_auth_methods;

	// The seed returned by the server in TOCLIENT_AUTH_ACCEPT is stored here
	u64 m_map_seed = 0;

	// The number of blocks the client will combine for mesh generation.
	MeshGrid m_mesh_grid;

	// The static lighting currently in use
	StaticLighting m_committed_static_light;

	// Auth data
	std::string m_playername;
	std::string m_password;
	// If set, this will be sent (and cleared) upon a TOCLIENT_ACCEPT_SUDO_MODE
	std::string m_new_password;
	// Usable by auth mechanisms.
	AuthMechanism m_chosen_auth_mech;
	void *m_auth_data = nullptr;

	bool m_access_denied = false;
	bool m_access_denied_reconnect = false;
	std::string m_access_denied_reason = "";
	std::queue<ClientEvent *> m_client_event_queue;
	bool m_itemdef_received = false;
	bool m_nodedef_received = false;
	bool m_activeobjects_received = false;
	bool m_mods_loaded = false;

	std::vector<std::string> m_remote_media_servers;
	// Media downloader, only exists during init
	std::unique_ptr<ClientMediaDownloader> m_media_downloader;
	// Pending downloads of dynamic media
	std::vector<PendingMediaDownload> m_pending_media_downloads;

	// An interval for generally sending object positions and stuff
	float m_recommended_send_interval = 0.1f;

	// Sounds
	float m_removed_sounds_check_timer = 0.0f;
	// Mapping from server sound ids to our sound ids
	std::unordered_map<s32, sound_handle_t> m_sounds_server_to_client;
	// And the other way!
	// This takes ownership for the sound handles.
	std::unordered_map<sound_handle_t, s32> m_sounds_client_to_server;
	// Relation of client id to object id
	std::unordered_map<sound_handle_t, u16> m_sounds_to_objects;

	// Privileges
	std::unordered_set<std::string> m_privileges;

	// Detached inventories
	// key = name
	std::unordered_map<std::string, Inventory*> m_detached_inventories;

	// Storage for mesh data for creating multiple instances of the same mesh
	StringMap m_mesh_data;

	// own state
	LocalClientState m_state;

	// Used for saving server map to disk client-side
	std::unique_ptr<MapDatabase> m_localdb;
	IntervalLimiter m_localdb_save_interval;
	u16 m_cache_save_interval;

	// Client modding
	ClientScripting *m_script = nullptr;
	ModStorageDatabase *m_mod_storage_database = nullptr;
	float m_mod_storage_save_timer = 10.0f;
	std::vector<ModSpec> m_mods;
	std::unique_ptr<ModVFS> m_mod_vfs;

	// SSCSM
	std::unique_ptr<SSCSMController> m_sscsm_controller;

	bool m_shutdown = false;
	bool m_take_screenshot = false;

	// CSM restrictions byteflag
	u64 m_csm_restriction_flags = CSMRestrictionFlags::CSM_RF_NONE;
	u32 m_csm_restriction_noderange = 8;

	std::unique_ptr<ModChannelMgr> m_modchannel_mgr;
};
