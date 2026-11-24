// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/game/cut/cuts.h"

#include "src/server/app/server.h"
#include "src/server/game/defs.h"
#include "src/server/game/interact.h"
#include "src/server/game/registry.h"
#include "src/shared/game/cut/record.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/items/inventory.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/shared/map/map.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/util/text/string.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cut {

namespace {
const Json::Value &need(const Json::Value &v, const char *field, const std::string &where);
std::string product(const std::string &form, const std::string &size, const std::string &wood);
std::string face(const std::string &colour, int length, int thick);
std::string json(const Json::Value &v);
} // namespace

std::unique_ptr<Cuts> Cuts::boot(Server *server, const Json::Value &table)
{
	auto c = std::unique_ptr<Cuts>(new Cuts());
	IWritableItemDefManager *idef = server->getWritableItemDefManager();

	const Json::Value &tools = need(table, "tools", "cuts.json");
	for (const std::string &name : tools.getMemberNames()) {
		const Json::Value &t = tools[name];
		for (const Json::Value &g : need(t, "cuts", name))
			c->m_tools[name].push_back(g.asString());
		idef->registerItem(defs::item(name, need(t, "item", name)));
	}
	const Json::Value &woods = need(table, "woods", "cuts.json");
	for (const std::string &block : woods.getMemberNames()) {
		const Json::Value &w = woods[block];
		const Wood wood{ need(w, "id", block).asString(), need(w, "name", block).asString(),
			need(w, "colour", block).asString(), "core:timber_" + need(w, "id", block).asString() };
		c->m_woods[block] = wood;
		// placed, a member is its wood's timber: cut at sixteenths, its grain kept, cut again as a log is
		Json::Value def;
		def["description"] = wood.name + " timber";
		def["_matter"]["form"] = false;
		def["_matter"]["matter"] = "wood";
		def["_matter"]["share"] = 0.6;
		def["groups"]["choppy"] = 2;
		def["groups"]["timber"] = 1;
		def["groups"]["not_in_creative_inventory"] = 1;
		def["paramtype2"] = "facedir";
		def["tiles"] = need(w, "tiles", block);
		def["_rotates"] = true;
		addBlock(server, wood.timber, def);
	}
	// what's placed is cut as what it was cut from
	for (const auto &[block, wood] : std::map<std::string, Wood>(c->m_woods))
		c->m_woods[wood.timber] = wood;
	const Json::Value &offcut = need(table, "offcut", "cuts.json");
	c->m_offcut = need(offcut, "name", "offcut").asString();
	c->m_offcut_cells = need(offcut, "cells", "offcut").asInt();
	if (c->m_offcut_cells < 1 || c->m_offcut_cells > shape::CELLS)
		throw std::invalid_argument("offcut: cells is 1..4096");
	for (const Json::Value &f : need(table, "forms", "cuts.json")) {
		Form form{ need(f, "id", "a form").asString(), need(f, "name", "a form").asString(), {} };
		for (const Json::Value &s : need(f, "sizes", form.id)) {
			const std::string where = form.id + "'s size";
			Size size{ need(s, "id", where).asString(), need(s, "name", where).asString(),
				{ (s16)need(s, "along", where).asInt(), (s16)need(s, "across", where).asInt(), (s16)need(s, "in", where).asInt() } };
			const peel::Size &b = size.box;
			if (b.along < 1 || b.across < 1 || b.in < 1 || b.along > peel::LONGEST * shape::STEPS ||
					b.across > shape::STEPS || b.in > shape::STEPS)
				throw std::invalid_argument(where + " " + size.id + ": along 1..64 sixteenths, across and in 1..16");
			form.sizes.push_back(size);
		}
		c->m_forms.push_back(form);
	}

	// every member that can be cut is an item: its name says what, its face its wood's colour
	for (const auto &[block, wood] : c->m_woods) {
		if (block == wood.timber)
			continue;
		for (const Form &form : c->m_forms)
			for (const Size &size : form.sizes) {
				Json::Value def;
				def["description"] = wood.name + " " + lowercase(form.name) + ", " + size.name;
				def["inventory_image"] = face(wood.colour, size.box.along, size.box.in);
				def["groups"]["cut"] = 1;
				const std::string name = product(form.id, size.id, wood.id);
				idef->registerItem(defs::item(name, def));
				c->m_members[name] = { wood.timber, size.box };
			}
		Json::Value def;
		def["description"] = wood.name + " " + c->m_offcut;
		def["inventory_image"] = face(wood.colour, 4, 4);
		def["groups"]["cut"] = 1;
		idef->registerItem(defs::item("core:offcut_" + wood.id, def));
	}
	return c;
}

void Cuts::tell(Server *server, RemotePlayer *player) const
{
	Json::Value v;
	for (const auto &[tool, groups] : m_tools)
		for (const std::string &g : groups)
			v["tools"][tool].append(g);
	v["forms"] = Json::Value(Json::arrayValue);
	for (const Form &f : m_forms) {
		Json::Value form;
		form["id"] = f.id;
		form["name"] = f.name;
		form["sizes"] = Json::Value(Json::arrayValue);
		for (const Size &s : f.sizes) {
			Json::Value size;
			size["id"] = s.id;
			size["name"] = s.name;
			size["along"] = s.box.along;
			size["across"] = s.box.across;
			size["in"] = s.box.in;
			form["sizes"].append(size);
		}
		v["forms"].append(form);
	}
	// what's placed: each member's box, so the client shows it before it's put
	for (const auto &[item, member] : m_members) {
		Json::Value &m = v["members"][item];
		m["along"] = member.box.along;
		m["across"] = member.box.across;
		m["in"] = member.box.in;
		m["timber"] = member.timber;
	}
	auto chosen = m_chosen.find(player->getName());
	if (chosen == m_chosen.end()) {
		v["chosen"] = Json::Value();
	} else {
		const Form &f = m_forms[chosen->second.form];
		v["chosen"]["form"] = f.id;
		v["chosen"]["size"] = f.sizes[chosen->second.size].id;
	}
	server->setUi(player, "cut", json(v));
}

void Cuts::choose(Server *server, RemotePlayer *player, const Json::Value &payload)
{
	const std::string who = player->getName();
	if (payload.isNull()) {
		m_chosen.erase(who);
		tell(server, player);
		return;
	}
	if (!payload.isObject() || !payload["form"].isString() || !payload["size"].isString()) {
		warningstream << "cut: " << who << " chose a cut that isn't { form, size }" << std::endl;
		return;
	}
	for (size_t f = 0; f < m_forms.size(); f++) {
		if (m_forms[f].id != payload["form"].asString())
			continue;
		for (size_t s = 0; s < m_forms[f].sizes.size(); s++) {
			if (m_forms[f].sizes[s].id == payload["size"].asString()) {
				m_chosen[who] = { f, s };
				tell(server, player);
				return;
			}
		}
	}
	warningstream << "cut: " << who << " chose " << payload["form"].asString() << " "
			<< payload["size"].asString() << ", which isn't a cut" << std::endl;
}

void Cuts::take(Server *server, PlayerSAO *who, const Json::Value &payload)
{
	RemotePlayer *player = who->getPlayer();
	const std::string name = player->getName();
	const std::optional<v3s16> node = vec(payload["node"]), side = vec(payload["face"]), aimed = vec(payload["aim"]);
	if (!node || !side || !aimed || std::abs(side->X) + std::abs(side->Y) + std::abs(side->Z) != 1 ||
			!shape::Shape::inside(*aimed)) {
		warningstream << "cut: " << name << " took a cut that isn't { node, face, aim }" << std::endl;
		return;
	}
	if (who->getEyePosition().getDistanceFrom(intToFloat(*node, BS)) > REACH * BS) {
		warningstream << "cut: " << name << " took a cut out of reach, at " << *node << std::endl;
		return;
	}
	auto chosen = m_chosen.find(name);
	ItemStack selected, hand;
	auto tool = m_tools.find(who->getWieldedItem(&selected, &hand).name);
	Map &map = server->getEnv().getMap();
	const ContentFeatures &f = server->ndef()->get(map.getNode(*node));
	auto wood = m_woods.find(f.name);
	if (chosen == m_chosen.end() || tool == m_tools.end() || wood == m_woods.end() || !cuts(tool->first, f.groups))
		return refuse(server, who, "Hold a cutting tool, choose a cut (F), and aim at what it cuts.");

	const Form &form = m_forms[chosen->second.form];
	const Size &size = form.sizes[chosen->second.size];
	const peel::Log log = peel::read(*node, [&](v3s16 q) -> std::optional<peel::Block> {
		bool valid;
		const MapNode n = map.getNode(q, &valid);
		if (!valid)
			return std::nullopt;
		return peel::Block{ n.getContent(), n.getParam2(), map.getShape(q) };
	});
	const std::optional<peel::Box> box = peel::at(log, *side, size.box, peel::cell(log, *node, *aimed));
	ItemStack got;
	if (box) {
		for (const auto &[i, cells] : peel::split(log, *box)) {
			const v3s16 p = log.start + v3s16(log.grain == 0, log.grain == 1, log.grain == 2) * i;
			const shape::Shape left = peel::without(log.shapes[i], cells.first, cells.second);
			if (left.empty())
				map.removeNodeWithEvent(p);
			else
				map.setShape(p, left);
		}
		// a placed member it went through isn't one any more
		forget(server, peel::Box{ record::world(log.start, v3s16(0, 0, 0)) + box->min, record::world(log.start, v3s16(0, 0, 0)) + box->max });
		got.name = product(form.id, size.id, wood->second.id);
		got.count = 1;
	} else if (!peel::any(log, *side, size.box)) {
		// none that size left anywhere off this side: what's left of the block aimed at comes off as offcuts
		const shape::Shape *was = map.getShape(*node);
		const size_t cells = was ? was->count() : shape::CELLS;
		map.removeNodeWithEvent(*node);
		got.name = "core:offcut_" + wood->second.id;
		got.count = std::max<u16>(1, (u16)std::lround(double(cells) / m_offcut_cells));
	} else {
		return refuse(server, who, "No whole " + lowercase(form.name) + " of " + size.name + " there: aim along the grain, from a side.");
	}

	give(server, who, *node, got);
	actionstream << "cut: " << name << " cut " << got.count << " " << got.name << " off " << f.name << " at " << *node << std::endl;
}

bool Cuts::cuts(const std::string &tool, const ItemGroupList &groups) const
{
	auto t = m_tools.find(tool);
	return t != m_tools.end() &&
			std::any_of(t->second.begin(), t->second.end(), [&](const std::string &g) { return groups.count(g) > 0; });
}

void Cuts::refuse(Server *server, PlayerSAO *who, const std::string &why)
{
	server->notifyPlayer(who->getPlayer()->getName().c_str(), utf8_to_wide(why));
}

void Cuts::give(Server *server, PlayerSAO *who, v3s16 at, const ItemStack &stack)
{
	InventoryList *main = who->getInventory()->getList("main");
	const ItemStack over = main ? main->addItem(stack) : stack;
	if (!over.empty())
		interact::scatter(server->getEnv(), at, { over });
	server->SendInventory(who->getPlayer(), false);
}

namespace {
const Json::Value &need(const Json::Value &v, const char *field, const std::string &where)
{
	if (!v.isObject() || !v.isMember(field))
		throw std::invalid_argument(where + " has no " + field);
	return v[field];
}

std::string product(const std::string &form, const std::string &size, const std::string &wood)
{
	return "core:" + form + "_" + size + "_" + wood;
}

// TEMP(2026-11-23): a member drawn as a bar of its wood's colour, longer for a longer one,
// thicker for a thicker, until the asset tools draw members
std::string face(const std::string &colour, int length, int thick)
{
	const int w = std::clamp(4 + (int)std::lround(4 * std::log2(std::max(length, 1) / 8.0)), 4, 16);
	const int h = std::clamp(thick + 2, 2, 8);
	const int x = (16 - w) / 2, y = (16 - h) / 2;
	return "[fill:16x16:0,0:#00000000^[fill:" + std::to_string(w) + "x" + std::to_string(h) + ":" +
			std::to_string(x) + "," + std::to_string(y) + ":" + colour + "^[fill:" + std::to_string(w) + "x1:" +
			std::to_string(x) + "," + std::to_string(y + h - 1) + ":#00000050";
}

std::string json(const Json::Value &v)
{
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	return Json::writeString(writer, v);
}

} // namespace

std::optional<v3s16> Cuts::vec(const Json::Value &v)
{
	if (!v.isArray() || v.size() != 3 || !v[0].isInt() || !v[1].isInt() || !v[2].isInt())
		return std::nullopt;
	return v3s16(v[0].asInt(), v[1].asInt(), v[2].asInt());
}

} // namespace cut
