// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include "src/shared/game/dig/earthwork.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/game/items/inventory.h"
#include <chrono>
#include <json/json.h>
#include <map>
#include <optional>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

class PlayerSAO;
class RemotePlayer;
class Server;

/*
	Digging by hand (sprint 124, .agents/planning/sprint-124-shovel.md): digging
	is placing run backwards. F with a shovel held offers its forms (a hole, a
	trench, a pit, a flattened pad, each in sizes); the client outlines the
	chosen one whole where they aim (shared/game/dig/earthwork.h) and, the
	button held, sends that box each swing. A swing takes the tool's bite of
	it, the highest first, into the inventory as the soils' scoops, counted to
	the sixteenth (scoop.h). It never adds ground; what's left settles as the
	ground does (piles). Tools, forms and the scoop are assets/world/digs.json;
	each player's chosen form is kept here and told as the UI value "dig".
*/
namespace dig {

class Digs
{
public:
	// The table (digs.json, read): its tools and the scoop registered as items.
	// Throws std::invalid_argument naming what's wrong.
	static std::unique_ptr<Digs> boot(Server *server, const Json::Value &table);

	// The operations and their chosen one, as the UI value "dig".
	void tell(Server *server, RemotePlayer *player) const;
	// An operation chosen on the page ({ operation, size }, or null: the first). Checked here:
	// what isn't one is warned of and ignored.
	void choose(Server *server, RemotePlayer *player, const Json::Value &payload);
	// Sent by their client each swing in place of a dig, holding a digging tool ({ min, max }: the
	// chosen form's box, world sixteenths, as outlined): a swing's worth of it dug, the highest first.
	void take(Server *server, PlayerSAO *who, const Json::Value &payload);

private:
	Digs() = default;
	struct Tool
	{
		std::vector<std::string> moves; // the groups of blocks it moves
		s32 bite;                       // sixteenths a swing takes of a form
		s32 cell;                       // sixteenths: the side of the cells it bores
		f32 rate;                       // cells a second it bores
	};
	struct Size
	{
		std::string id, name;
		earthwork::Size size;
	};
	struct Operation
	{
		std::string id, name;
		earthwork::Kind kind;
		std::vector<Size> sizes;
	};
	static constexpr s32 MOST = 128; // sixteenths: the most a form's side is
	// Their chosen operation and its size (the first's first, if none's chosen).
	std::pair<const Operation &, const Size &> chosen(const std::string &player) const;
	// The columns cut, what's loose of them, into their inventory as scoops.
	void excavate(Server *server, PlayerSAO *who, const std::vector<earthwork::Column> &columns);
	// Blocks' solid shapes read for one dig (none: whole; an absent entry: open or not loaded):
	// a footprint's columns share a few blocks, and reading smoothed ground is dear.
	using Shapes = std::unordered_map<v3s16, std::optional<std::optional<shape::Shape>>>;
	// The ground's top over a column near `near` (world sixteenths), or `near` if there's none.
	static s32 surface(Server *server, Shapes &shapes, s32 x, s32 z, s32 near);
	// Whether the world sixteenth c is filled.
	static bool solid(Server *server, Shapes &shapes, v3s32 c);
	// A bore's cell (`cell`, as sent: the one their client lit next, on the tool's grid), dug if
	// it's in reach and they're not boring faster than the tool does.
	void bore(Server *server, PlayerSAO *who, const Json::Value &cell, const earthwork::Size &size);
	struct Pace
	{
		std::chrono::steady_clock::time_point at; // when they last bored
		f32 owed = 0;                             // cells they may bore now
	};
	static constexpr f32 BURST = 4; // cells: the most a pause saves up
	// Whether the block at p is one a tool moves.
	bool moves(Server *server, v3s16 p) const;
	// The scoop of the soil at p.
	ItemStack scoopOf(Server *server, v3s16 p) const;
	static void refuse(Server *server, PlayerSAO *who, const std::string &why);
	static constexpr float REACH = 8; // nodes: how far from their eyes they dig

	std::map<std::string, Pace> m_pace;                       // player → how fast they're boring
	std::map<std::string, Tool> m_tools;                      // a digging tool's item → it
	std::vector<Operation> m_operations;
	std::string m_scoop;                                      // the scoop's item
	std::map<std::string, std::pair<size_t, size_t>> m_chosen; // player → their operation, its size
	// player → soil (a scoop's metadata) → sixteenths dug short of a whole scoop
	std::map<std::string, std::map<std::string, int>> m_dug;
};

} // namespace dig
