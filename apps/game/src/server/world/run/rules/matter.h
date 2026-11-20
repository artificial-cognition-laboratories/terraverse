// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/core/irr_v3d.h"
#include "library/matter/matter/block.h"
#include "library/matter/matter/matter.h"
#include "src/shared/map/mapnode.h"
#include "library/matter/sandbox/sandbox.h"
#include "library/matter/sandbox/world.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Map;
class NodeDefManager;
class Server;

/*
	The server's world matter (sprint 119: out of script/lua_api/l_matter.cpp):
	made at the first registration, loaded from disk at its first use (once
	every kind is registered), and from then on closed to new kinds. One per
	process: the server thread's.
*/
struct ServerMatter
{
	matter::Matter table;
	std::unique_ptr<sandbox::WorldMatter> world;
	Server *server; // its environment comes after mods load: looked up when the map is touched
	std::string path;
	std::vector<int32_t> kind_of;     // by content id: the (first) kind, or −1
	std::vector<int32_t> points_of;   // by content id: how many kinds its param2 picks between (1: a plain block)
	std::vector<std::pair<content_t, u8>> node_of; // by kind: the node and param2 that are it
	std::unordered_map<int64_t, double> ambient; // by 16-block cube
	std::unordered_map<int64_t, double> wetness; // by 16-block cube (sprint 111): the ground's, 0..1
	bool started = false;
	// its clock (s since each was last done) and where a flame node stands (server/matter.cpp)
	struct Clock
	{
		float step = 0, show = 0, weather = 5, save = 0; // the weather's told at once
	};
	Clock clock;
	std::unordered_set<v3s16> lit;
	std::vector<sandbox::WorldMatter::Spec> plain; // the plain blocks registered, for the bake (serverMatterBaked)
};

ServerMatter &serverMatter(Server *server);
// Once every kind is in: loads what was awake when the server last stopped.
ServerMatter &serverMatterStarted(Server *server);
// None until something made it.
ServerMatter *serverMatterIfMade();
void serverMatterShutdown();
// Throws matter::MatterError (or SandboxError) for what can't be.
int32_t serverMatterBlock(Server *server, const std::string &node, const MatterBlock &block);
int32_t serverMatterFamily(Server *server, const std::string &node, const MatterFamily &family);
// Saves what's awake beside the world (matter.bin); nothing if it never ran.
void serverMatterSave();

int64_t matterCube(sandbox::Pos p);
sandbox::Room matterRoomAt(Map &map, const NodeDefManager *ndef, v3s16 p);

// Every block that says what it's made of, registered with the world's matter, and every
// solid one with machines (workshop::Materials): once the mods have declared their blocks
// (sprint 119: was tf_matter's, in Lua).
void terraRegisterMatter(Server *server);

// Running it, each server step (sprint 119: was tf_matter's globalstep).
void terraStepMatter(Server *server, float dtime);
// A block dug or built: liquids and heaps beside it run.
void terraMatterDisturb(Server *server, v3s16 p);
// The reactions they've seen happen, for the wiki.
class PlayerSAO;
void terraMatterJoin(Server *server, PlayerSAO *sao);
namespace Json { class Value; }
// Every plain block's matter and look, for the wiki's scenes (bun palette: far/export).
Json::Value serverMatterBaked(const NodeDefManager *ndef);
// A chunk's wet shore (mapgenShore) filled with water where its smoothed ground is open.
void terraFillShore(Server *server, const std::vector<v3s16> &shore);
