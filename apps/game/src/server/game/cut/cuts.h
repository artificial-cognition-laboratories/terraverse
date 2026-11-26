// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/game/cut/peel.h"
#include "src/shared/game/cut/record.h"
#include "src/shared/game/items/itemgroup.h"
#include <json/json.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class PlayerSAO;
class RemotePlayer;
class Server;
struct ItemStack;

/*
	Cutting (sprint 123, .agents/planning/sprint-123-cutting.md): a tool held
	peels standard members off the logs it cuts, where it's aimed, one a dig;
	a member placed is its wood's timber written into the world, at sixteenths,
	to be built with and cut again.
	What's cut, in which sizes, by which tool, is assets/world/cuts.json; each
	player's chosen cut is kept here and told to their page and client as the
	UI value "cut", which also lists the menu.
*/
namespace cut {

class Cuts
{
public:
	// The table (cuts.json, read), its tools and every form × size × wood and
	// offcut registered as items. Throws std::invalid_argument naming what's wrong.
	static std::unique_ptr<Cuts> boot(Server *server, const Json::Value &table);

	// The menu and their chosen cut, as the UI value "cut".
	void tell(Server *server, RemotePlayer *player) const;
	// A cut chosen on the page ({ form, size }, or null: none, the tool digs
	// whole blocks). Checked here: what isn't one is warned of and ignored.
	void choose(Server *server, RemotePlayer *player, const Json::Value &payload);
	// A cut taken where they aim ({ node, face, aim: the sixteenth }), sent by
	// their client in place of a dig: the member comes off into their
	// inventory, or, where none fits any more, what's left as offcuts.
	void take(Server *server, PlayerSAO *who, const Json::Value &payload);
	// The member held placed where they aim ({ node, face, aim, axis: its grain, run: lay }),
	// sent by their client in place of placing: written into the world as its
	// wood's timber, cut at sixteenths. Refused where it would overlap anything.
	void place(Server *server, PlayerSAO *who, const Json::Value &payload);
	// A placed member broken where they aim ({ node, aim }), sent by their
	// client in place of a dig of timber: it comes back whole into their
	// inventory; timber that isn't one (cut, or never placed) comes as offcuts.
	void lift(Server *server, PlayerSAO *who, const Json::Value &payload);
	// A dig of the block at p: true if it was timber, given back as offcuts
	// (a member's cut off it with a cutting tool instead: take).
	bool dig(Server *server, PlayerSAO *who, v3s16 p);

private:
	struct Size
	{
		std::string id, name;
		peel::Size box;
	};
	struct Form
	{
		std::string id, name;
		std::vector<Size> sizes;
	};
	struct Wood
	{
		std::string id, name, colour;
		std::string timber; // the block it's placed as
	};
	struct Choice
	{
		size_t form, size;
	};
	struct Member
	{
		std::string timber; // what a placed one is made of
		peel::Size box;
	};

	Cuts() = default;
	// Tells them why not, in their chat.
	static void refuse(Server *server, PlayerSAO *who, const std::string &why);
	// Into their inventory, or at their feet if it's full.
	static void give(Server *server, PlayerSAO *who, v3s16 at, const ItemStack &stack);
	// A position sent as [x, y, z]; none if it isn't one.
	static std::optional<v3s16> vec(const Json::Value &v);
	static constexpr float REACH = 8; // nodes: how far from their eyes they cut and place
	// What's placed (record.h), kept in the metadata of the blocks it's in:
	// a member remembered; every member meeting `box` (world sixteenths) forgotten.
	static void remember(Server *server, const record::Member &member);
	static void forget(Server *server, const peel::Box &box);
	static std::vector<record::Member> members(Server *server, v3s16 p);
	static void keep(Server *server, v3s16 p, const std::vector<record::Member> &members);
	// The timber at p given as offcuts, by how much of it there is.
	void offcuts(Server *server, PlayerSAO *who, v3s16 p, const std::string &wood);

	std::map<std::string, std::vector<std::string>> m_tools; // tool → the groups of blocks it cuts
	// whether `tool` cuts a block of these groups
	bool cuts(const std::string &tool, const ItemGroupList &groups) const;
	std::map<std::string, Wood> m_woods;        // block (a log, a timber) → the wood it is
	std::vector<Form> m_forms;
	std::string m_offcut;                       // what offcuts are called
	int m_offcut_cells = 256;                   // sixteenths to an offcut
	std::map<std::string, Member> m_members;    // a member's item → what it is
	std::map<std::string, Choice> m_chosen;     // player → their cut
};

} // namespace cut
