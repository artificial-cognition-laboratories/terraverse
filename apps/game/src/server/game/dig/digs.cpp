// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/game/dig/digs.h"

#include "src/server/app/server.h"
#include "src/server/game/defs.h"
#include "src/server/game/interact.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/dig/scoop.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/shared/map/map.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/util/text/string.h"
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace dig {

namespace {
const Json::Value &need(const Json::Value &v, const char *field, const std::string &where);
// The node's sixteenths as they stand: its cut shape, else whole.
shape::Shape shapeAt(Map &map, v3s16 p);
// A form's shape, as written: "box", "round" or "level"; none if it's none of them.
std::optional<earthwork::Kind> kindOf(const std::string &v);
} // namespace

std::unique_ptr<Digs> Digs::boot(Server *server, const Json::Value &table)
{
	auto d = std::unique_ptr<Digs>(new Digs());
	IWritableItemDefManager *idef = server->getWritableItemDefManager();
	const Json::Value &tools = need(table, "tools", "digs.json");
	for (const std::string &name : tools.getMemberNames()) {
		const Json::Value &t = tools[name];
		Tool &tool = d->m_tools[name];
		for (const Json::Value &g : need(t, "moves", name))
			tool.moves.push_back(g.asString());
		tool.bite = need(t, "bite", name).asInt();
		tool.cell = need(t, "cell", name).asInt();
		tool.rate = need(t, "rate", name).asFloat();
		if (!(tool.rate > 0))
			throw std::invalid_argument(name + ": rate is cells a second it bores, more than none");
		if (tool.bite < 1 || tool.cell < 1 || tool.cell > shape::STEPS || shape::STEPS % tool.cell != 0)
			throw std::invalid_argument(name + ": bite is sixteenths a swing takes, at least 1; cell, 1, 2, 4, 8 or 16");
		idef->registerItem(defs::item(name, need(t, "item", name)));
	}
	for (const Json::Value &o : need(table, "operations", "digs.json")) {
		Operation op{ need(o, "id", "an operation").asString(), need(o, "name", "an operation").asString(), {} };
		const std::optional<earthwork::Kind> kind = kindOf(need(o, "shape", op.id).asString());
		if (!kind)
			throw std::invalid_argument(op.id + ": shape is bore, cell, box, round or level");
		op.kind = *kind;
		for (const Json::Value &z : need(o, "sizes", op.id)) {
			const std::string where = op.id + "'s size";
			Size size{ need(z, "id", where).asString(), need(z, "name", where).asString(),
				{ need(z, "along", where).asInt(), need(z, "across", where).asInt(), need(z, "deep", where).asInt() } };
			if (size.size.along < 1 || size.size.along > MOST || size.size.across < 1 || size.size.across > MOST ||
					size.size.deep < 1 || size.size.deep > MOST)
				throw std::invalid_argument(where + " " + size.id + ": along, across and deep are 1..128 sixteenths");
			op.sizes.push_back(size);
		}
		if (op.sizes.empty())
			throw std::invalid_argument(op.id + " has no sizes");
		d->m_operations.push_back(op);
	}
	if (d->m_operations.empty())
		throw std::invalid_argument("digs.json has no operations");
	const Json::Value &scoop = need(table, "scoop", "digs.json");
	d->m_scoop = need(scoop, "name", "scoop").asString();
	idef->registerItem(defs::item(d->m_scoop, need(scoop, "item", "scoop")));
	return d;
}

void Digs::tell(Server *server, RemotePlayer *player) const
{
	Json::Value v;
	for (const auto &[name, tool] : m_tools) {
		for (const std::string &g : tool.moves)
			v["tools"][name]["moves"].append(g);
		v["tools"][name]["cell"] = tool.cell;
		v["tools"][name]["rate"] = tool.rate;
	}
	v["operations"] = Json::Value(Json::arrayValue);
	for (const Operation &o : m_operations) {
		Json::Value op;
		op["id"] = o.id;
		op["name"] = o.name;
		op["shape"] = o.kind == earthwork::Kind::Bore ? "bore" : o.kind == earthwork::Kind::Cell ? "cell" : o.kind == earthwork::Kind::Box ? "box" : o.kind == earthwork::Kind::Round ? "round" : "level";
		for (const Size &z : o.sizes) {
			Json::Value size;
			size["id"] = z.id;
			size["name"] = z.name;
			size["along"] = z.size.along;
			size["across"] = z.size.across;
			size["deep"] = z.size.deep;
			op["sizes"].append(size);
		}
		v["operations"].append(op);
	}
	const auto c = m_chosen.find(player->getName());
	if (c == m_chosen.end()) {
		v["chosen"] = Json::Value();
	} else {
		v["chosen"]["operation"] = m_operations[c->second.first].id;
		v["chosen"]["size"] = m_operations[c->second.first].sizes[c->second.second].id;
	}
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	server->setUi(player, "dig", Json::writeString(writer, v));
}

void Digs::choose(Server *server, RemotePlayer *player, const Json::Value &payload)
{
	const std::string who = player->getName();
	if (payload.isNull()) {
		m_chosen.erase(who);
		return tell(server, player);
	}
	if (!payload.isObject() || !payload["operation"].isString() || !payload["size"].isString()) {
		warningstream << "dig: " << who << " chose an operation that isn't { operation, size }" << std::endl;
		return;
	}
	for (size_t o = 0; o < m_operations.size(); o++)
		for (size_t z = 0; o < m_operations.size() && z < m_operations[o].sizes.size(); z++)
			if (m_operations[o].id == payload["operation"].asString() && m_operations[o].sizes[z].id == payload["size"].asString()) {
				m_chosen[who] = { o, z };
				return tell(server, player);
			}
	warningstream << "dig: " << who << " chose " << payload["operation"].asString() << " " << payload["size"].asString()
			<< ", which isn't an operation" << std::endl;
}

void Digs::take(Server *server, PlayerSAO *who, const Json::Value &payload)
{
	const std::string name = who->getPlayer()->getName();
	const auto [op, size] = chosen(name);
	if (op.kind == earthwork::Kind::Bore)
		return bore(server, who, payload["cell"], size.size);
	const auto vec = [](const Json::Value &v) -> std::optional<v3s32> {
		if (!v.isArray() || v.size() != 3 || !v[0].isInt() || !v[1].isInt() || !v[2].isInt())
			return std::nullopt;
		return v3s32(v[0].asInt(), v[1].asInt(), v[2].asInt());
	};
	const std::optional<v3s32> lo = vec(payload["min"]), hi = vec(payload["max"]);
	if (!lo || !hi) {
		warningstream << "dig: " << name << " dug something that isn't { min, max }" << std::endl;
		return;
	}
	const earthwork::Target target{ *lo, *hi, op.kind };
	if (!earthwork::fits(target, size.size)) {
		warningstream << "dig: " << name << " dug a box that isn't their " << op.id << " " << size.id << std::endl;
		return;
	}
	const v3f middle = v3f(lo->X + hi->X + 1, lo->Y + hi->Y + 1, lo->Z + hi->Z + 1) / (2.0f * shape::STEPS) * BS;
	if (who->getEyePosition().getDistanceFrom(middle) > REACH * BS) {
		warningstream << "dig: " << name << " dug out of reach, at " << *lo << std::endl;
		return;
	}
	ItemStack selected, hand;
	const auto tool = m_tools.find(who->getWieldedItem(&selected, &hand).name);
	if (tool == m_tools.end())
		return refuse(server, who, "Hold a shovel to dig.");
	// a swing's worth of it, the highest first
	Shapes shapes;
	excavate(server, who, earthwork::bite(target, tool->second.bite, [&](s32 x, s32 z) {
		return surface(server, shapes, x, z, target.max.Y);
	}));
}

std::pair<const Digs::Operation &, const Digs::Size &> Digs::chosen(const std::string &player) const
{
	const auto c = m_chosen.find(player);
	const auto [o, z] = c == m_chosen.end() ? std::pair<size_t, size_t>(0, 0) : c->second;
	return { m_operations[o], m_operations[o].sizes[z] };
}

void Digs::excavate(Server *server, PlayerSAO *who, const std::vector<earthwork::Column> &columns)
{
	Map &map = server->getEnv().getMap();
	const auto blockOf = [](s32 c) { return (s16)(c >> 4); }; // sixteenths to their block (floor)
	// each block it reaches, smoothed ground made cut as it looks first, so it's dug as it's seen
	std::map<v3s16, shape::Shape> cut;
	for (const earthwork::Column &c : columns)
		for (s32 y = c.bed; y < c.top; y += shape::STEPS - ((y & 15)))
			cut.emplace(v3s16(blockOf(c.x), blockOf(y), blockOf(c.z)), shape::Shape());
	for (auto it = cut.begin(); it != cut.end();) {
		if (!moves(server, it->first)) {
			it = cut.erase(it);
			continue;
		}
		map.settleGround(it->first);
		++it;
	}
	for (auto &[p, s] : cut)
		s = shapeAt(map, p);
	// what's settled into the block over the top of what was looked at
	for (const earthwork::Column &c : columns)
		for (s32 y = c.bed; y < c.top; y++) {
			const v3s16 p(blockOf(c.x), blockOf(y), blockOf(c.z));
			if (!cut.count(p) && moves(server, p))
				cut[p] = shapeAt(map, p);
			const auto in = cut.find(p);
			if (in != cut.end())
				in->second.set(v3s16(c.x & 15, y & 15, c.z & 15), false);
		}
	// what came off, soil by soil, counted to the sixteenth: a scoop each whole one
	const std::string name = who->getPlayer()->getName();
	std::map<std::string, std::pair<ItemStack, int>> got;
	for (const auto &[p, s] : cut) {
		const shape::Shape before = shapeAt(map, p);
		const int cells = (int)(before.count() - s.count());
		if (cells == 0)
			continue;
		ItemStack soil = scoopOf(server, p);
		std::ostringstream key;
		soil.metadata.serialize(key);
		got[key.str()].first = soil;
		got[key.str()].second += cells;
		const MapNode was = map.getNode(p);
		if (s.empty())
			map.removeNodeWithEvent(p);
		else
			map.keepShape(p, s);
		server->groundChanged(p, was); // what's round it holds its shape, and settles as it can
	}
	InventoryList *main = who->getInventory()->getList("main");
	for (auto &[key, soil] : got) {
		int &dug = m_dug[name][key];
		dug += soil.second;
		ItemStack scoops = soil.first;
		scoops.count = (u16)(dug / scoop::CELLS);
		dug %= scoop::CELLS;
		if (scoops.count == 0)
			continue;
		const ItemStack over = main ? main->addItem(scoops) : scoops;
		if (!over.empty())
			interact::scatter(server->getEnv(), floatToInt(who->getBasePosition(), BS), { over });
	}
	server->SendInventory(who->getPlayer(), false);
}

void Digs::bore(Server *server, PlayerSAO *who, const Json::Value &at, const earthwork::Size &size)
{
	const std::string name = who->getPlayer()->getName();
	ItemStack selected, hand;
	const auto tool = m_tools.find(who->getWieldedItem(&selected, &hand).name);
	if (tool == m_tools.end())
		return refuse(server, who, "Hold a shovel to dig.");
	const s32 cell = tool->second.cell;
	if (!at.isArray() || at.size() != 3 || !at[0].isInt() || !at[1].isInt() || !at[2].isInt() ||
			at[0].asInt() % cell || at[1].asInt() % cell || at[2].asInt() % cell) {
		warningstream << "dig: " << name << " bored something that isn't { cell }, on the tool's grid" << std::endl;
		return;
	}
	const v3s32 c(at[0].asInt(), at[1].asInt(), at[2].asInt());
	// within the bore's reach of their eyes, a cell's slack
	const v3f eye = who->getEyePosition() / BS * shape::STEPS + v3f(8, 8, 8);
	if ((v3f(c.X, c.Y, c.Z) + v3f(cell, cell, cell) / 2.0f - eye).getLength() > size.along + size.across / 2.0f + cell) {
		warningstream << "dig: " << name << " bored out of reach, at " << c << std::endl;
		return;
	}
	// no faster than the tool: a little to spare for the network's bunching
	const auto now = std::chrono::steady_clock::now();
	Pace &pace = m_pace[name];
	pace.owed = std::min<f32>(pace.owed + std::chrono::duration<f32>(now - pace.at).count() * tool->second.rate, BURST);
	pace.at = now;
	if (pace.owed < 1)
		return;
	pace.owed -= 1;
	const v3s16 p(c.X >> 4, c.Y >> 4, c.Z >> 4);
	if (!moves(server, p))
		return;
	std::vector<earthwork::Column> columns;
	for (s32 z = c.Z; z < c.Z + cell; z++)
		for (s32 x = c.X; x < c.X + cell; x++)
			columns.push_back({ x, z, c.Y, c.Y + cell });
	excavate(server, who, columns);
}

s32 Digs::surface(Server *server, Shapes &shapes, s32 x, s32 z, s32 near)
{
	return earthwork::surface(x, z, near, [&](v3s32 c) { return solid(server, shapes, c); }).value_or(near);
}

bool Digs::solid(Server *server, Shapes &shapes, v3s32 c)
{
	Map &map = server->getEnv().getMap();
	const v3s16 p(c.X >> 4, c.Y >> 4, c.Z >> 4);
	auto known = shapes.find(p);
	if (known == shapes.end()) {
		bool valid;
		const MapNode n = map.getNode(p, &valid);
		known = shapes.emplace(p, !valid || server->ndef()->get(n).buildable_to ? std::nullopt :
				std::optional<std::optional<shape::Shape>>(map.solidShape(p))).first;
	}
	if (!known->second)
		return false;
	const std::optional<shape::Shape> &s = *known->second;
	return !s || s->has(v3s16(c.X & 15, c.Y & 15, c.Z & 15));
}

bool Digs::moves(Server *server, v3s16 p) const
{
	const ContentFeatures &f = server->ndef()->get(server->getEnv().getMap().getNode(p));
	for (const auto &[name, tool] : m_tools)
		for (const std::string &g : tool.moves)
			if (f.groups.count(g))
				return true;
	return false;
}

ItemStack Digs::scoopOf(Server *server, v3s16 p) const
{
	const MapNode n = server->getEnv().getMap().getNode(p);
	const ContentFeatures &f = server->ndef()->get(n);
	ItemStack s;
	s.name = m_scoop;
	s.count = 1;
	s.metadata.setString("soil", f.name);
	s.metadata.setString("param2", std::to_string(n.param2));
	const std::string &called = server->idef()->get(f.name).description;
	s.metadata.setString("description", "Scoop of " + lowercase(called.empty() ? f.name : called));
	return s;
}

void Digs::refuse(Server *server, PlayerSAO *who, const std::string &why)
{
	server->notifyPlayer(who->getPlayer()->getName().c_str(), utf8_to_wide(why));
}

namespace {
const Json::Value &need(const Json::Value &v, const char *field, const std::string &where)
{
	if (!v.isObject() || !v.isMember(field))
		throw std::invalid_argument(where + " has no " + field);
	return v[field];
}

shape::Shape shapeAt(Map &map, v3s16 p)
{
	const shape::Shape *s = map.getShape(p);
	return s ? *s : shape::Shape::whole();
}

std::optional<earthwork::Kind> kindOf(const std::string &v)
{
	if (v == "bore")
		return earthwork::Kind::Bore;
	if (v == "cell")
		return earthwork::Kind::Cell;
	if (v == "box")
		return earthwork::Kind::Box;
	if (v == "round")
		return earthwork::Kind::Round;
	if (v == "level")
		return earthwork::Kind::Level;
	return std::nullopt;
}
} // namespace

} // namespace dig
