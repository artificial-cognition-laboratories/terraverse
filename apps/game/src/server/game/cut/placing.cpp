// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Cuts' placing (cuts.h): members put into the world as timber, remembered
// there (record.h), and given back whole when broken. Cutting itself is cuts.cpp.

#include "src/server/game/cut/cuts.h"

#include "src/server/app/server.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/cut/stack.h"
#include "src/shared/game/content/nodemetadata.h"
#include "src/shared/game/items/inventory.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/shared/map/map.h"
#include "src/shared/platform/system/log.h"
#include <algorithm>
#include <cmath>

namespace cut {

void Cuts::place(Server *server, PlayerSAO *who, const Json::Value &payload)
{
	const std::string name = who->getPlayer()->getName();
	// where its lowest corner goes (world sixteenths), as the client laid and lined it up;
	// and its along, across and in: which world axis each lies on, each once
	const Json::Value &at = payload["at"], &a = payload["axes"];
	const auto ints = [](const Json::Value &v) {
		return v.isArray() && v.size() == 3 && v[0].isInt() && v[1].isInt() && v[2].isInt();
	};
	if (!ints(at) || !ints(a) || a[0].asInt() + a[1].asInt() + a[2].asInt() != 3 || a[0].asInt() * a[1].asInt() * a[2].asInt() != 0 ||
			std::max({ a[0].asInt(), a[1].asInt(), a[2].asInt() }) != 2 || std::min({ a[0].asInt(), a[1].asInt(), a[2].asInt() }) != 0) {
		warningstream << "cut: " << name << " placed a member that isn't { at, axes }" << std::endl;
		return;
	}
	const v3s32 corner(at[0].asInt(), at[1].asInt(), at[2].asInt());
	const v3f where = v3f(corner.X, corner.Y, corner.Z) / shape::STEPS * BS;
	if (who->getEyePosition().getDistanceFrom(where) > REACH * BS) {
		warningstream << "cut: " << name << " placed a member out of reach, at " << corner << std::endl;
		return;
	}
	ItemStack selected, hand;
	ItemStack held = who->getWieldedItem(&selected, &hand);
	auto member = m_members.find(held.name);
	if (member == m_members.end())
		return refuse(server, who, "Hold a member to place it.");

	const int axis = a[0].asInt(); // its grain
	const v3s32 dims = peel::dims(member->second.box, { a[0].asInt(), a[1].asInt(), a[2].asInt() });
	const peel::Box box{ corner, corner + dims - v3s32(1, 1, 1) };
	Map &map = server->getEnv().getMap();
	const content_t timber = server->ndef()->getId(member->second.timber);
	const u8 param2 = peel::facedir(axis);
	// each block it goes into: what's there now and what it'll be, all checked before any is written
	std::vector<std::pair<v3s16, shape::Shape>> writes;
	for (const v3s16 &p : record::blocks(box)) {
		bool valid;
		const MapNode n = map.getNode(p, &valid);
		if (!valid)
			return refuse(server, who, "That's not loaded yet.");
		// timber already there (a post the beam meets) is shared: it keeps its own wood and way
		const bool open = server->ndef()->get(n).buildable_to;
		if (!open && !server->ndef()->get(n).groups.count("timber"))
			return refuse(server, who, "Something's in the way there.");
		const shape::Shape *was = map.getShape(p);
		shape::Shape s = open ? shape::Shape() : was ? *was : shape::Shape::whole();
		const v3s32 base = record::world(p, v3s16(0, 0, 0));
		for (s32 cy = std::max(box.min.Y, base.Y); cy <= std::min(box.max.Y, base.Y + shape::STEPS - 1); cy++)
			for (s32 cz = std::max(box.min.Z, base.Z); cz <= std::min(box.max.Z, base.Z + shape::STEPS - 1); cz++)
				for (s32 cx = std::max(box.min.X, base.X); cx <= std::min(box.max.X, base.X + shape::STEPS - 1); cx++) {
					const v3s16 cell(cx - base.X, cy - base.Y, cz - base.Z);
					if (s.has(cell))
						return refuse(server, who, "Something's in the way there.");
					s.set(cell, true);
				}
		writes.push_back({ p, s });
	}
	// it must rest against something: the ground, a block, another member
	const auto solid = [&](v3s32 c) {
		const v3s16 p(c.X >> 4, c.Y >> 4, c.Z >> 4); // sixteenths to their block (floor)
		bool valid;
		const MapNode n = map.getNode(p, &valid);
		if (!valid || server->ndef()->get(n).buildable_to)
			return false;
		const shape::Shape *s = map.getShape(p);
		return !s || s->has(v3s16(c.X & 15, c.Y & 15, c.Z & 15));
	};
	if (!record::touches(box, solid))
		return refuse(server, who, "It needs something to rest against.");
	for (const auto &[p, s] : writes) {
		if (server->ndef()->get(map.getNode(p)).buildable_to)
			map.addNodeWithEvent(p, MapNode(timber, 0, param2));
		map.setShape(p, s);
	}
	remember(server, { member->first, box });
	held.remove(1);
	if (who->setWieldedItem(held))
		server->SendInventory(who->getPlayer(), true);
	actionstream << "cut: " << name << " placed " << member->first << " at " << corner << std::endl;
}

void Cuts::lift(Server *server, PlayerSAO *who, const Json::Value &payload)
{
	const std::string name = who->getPlayer()->getName();
	const std::optional<v3s16> node = vec(payload["node"]), aimed = vec(payload["aim"]);
	if (!node || !aimed || !shape::Shape::inside(*aimed)) {
		warningstream << "cut: " << name << " broke a member that isn't { node, aim }" << std::endl;
		return;
	}
	if (who->getEyePosition().getDistanceFrom(intToFloat(*node, BS)) > REACH * BS) {
		warningstream << "cut: " << name << " broke a member out of reach, at " << *node << std::endl;
		return;
	}
	Map &map = server->getEnv().getMap();
	const ContentFeatures &f = server->ndef()->get(map.getNode(*node));
	auto wood = m_woods.find(f.name);
	if (wood == m_woods.end() || f.name != wood->second.timber)
		return;
	std::optional<record::Member> m = record::at(members(server, *node), record::world(*node, *aimed));
	// lying flat, it comes off the top of what of its kind is stacked on it
	const auto member = m ? m_members.find(m->item) : m_members.end();
	if (member != m_members.end() && m->box.max.Y - m->box.min.Y + 1 == member->second.box.in)
		for (bool up = true; up;) {
			std::vector<peel::Box> same;
			for (const v3s16 &p : record::blocks(peel::Box{ m->box.min, m->box.max + v3s32(0, 1, 0) }))
				for (const record::Member &n : members(server, p))
					if (n.item == m->item)
						same.push_back(n.box);
			const std::optional<peel::Box> above = stack::above(m->box, same);
			if ((up = above.has_value()))
				m->box = *above;
		}
	// whole, every sixteenth of it still there; else it's been cut, and what's here is timber
	bool whole = m.has_value();
	for (const v3s16 &p : m ? record::blocks(m->box) : std::vector<v3s16>()) {
		const shape::Shape *s = map.getShape(p);
		const v3s32 base = record::world(p, v3s16(0, 0, 0));
		for (s32 y = std::max(m->box.min.Y, base.Y); whole && y <= std::min(m->box.max.Y, base.Y + 15); y++)
			for (s32 z = std::max(m->box.min.Z, base.Z); whole && z <= std::min(m->box.max.Z, base.Z + 15); z++)
				for (s32 x = std::max(m->box.min.X, base.X); whole && x <= std::min(m->box.max.X, base.X + 15); x++)
					whole = server->ndef()->get(map.getNode(p)).groups.count("timber") &&
							(!s || s->has(v3s16(x - base.X, y - base.Y, z - base.Z)));
	}
	if (!whole) {
		if (m)
			forget(server, m->box);
		return offcuts(server, who, *node, wood->second.id);
	}
	forget(server, m->box);
	for (const v3s16 &p : record::blocks(m->box)) {
		const shape::Shape *s = map.getShape(p);
		const v3s32 base = record::world(p, v3s16(0, 0, 0));
		const v3s32 lo(std::max(m->box.min.X, base.X), std::max(m->box.min.Y, base.Y), std::max(m->box.min.Z, base.Z));
		const v3s32 hi(std::min(m->box.max.X, base.X + 15), std::min(m->box.max.Y, base.Y + 15), std::min(m->box.max.Z, base.Z + 15));
		const shape::Shape left = peel::without(s ? *s : shape::Shape::whole(),
				v3s16(lo.X - base.X, lo.Y - base.Y, lo.Z - base.Z), v3s16(hi.X - base.X, hi.Y - base.Y, hi.Z - base.Z));
		if (left.empty())
			map.removeNodeWithEvent(p);
		else
			map.setShape(p, left);
	}
	ItemStack got;
	got.name = m->item;
	got.count = 1;
	give(server, who, *node, got);
	actionstream << "cut: " << name << " took back " << m->item << " at " << *node << std::endl;
}

bool Cuts::dig(Server *server, PlayerSAO *who, v3s16 p)
{
	Map &map = server->getEnv().getMap();
	const ContentFeatures &f = server->ndef()->get(map.getNode(p));
	auto wood = m_woods.find(f.name);
	if (wood == m_woods.end() || f.name != wood->second.timber)
		return false;
	offcuts(server, who, p, wood->second.id);
	return true;
}

void Cuts::offcuts(Server *server, PlayerSAO *who, v3s16 p, const std::string &wood)
{
	Map &map = server->getEnv().getMap();
	const shape::Shape *was = map.getShape(p);
	const size_t cells = was ? was->count() : shape::CELLS;
	// what was placed partly in it isn't whole any more
	forget(server, peel::Box{ record::world(p, v3s16(0, 0, 0)), record::world(p, v3s16(15, 15, 15)) });
	map.removeNodeWithEvent(p);
	ItemStack got;
	got.name = "core:offcut_" + wood;
	got.count = std::max<u16>(1, (u16)std::lround(double(cells) / m_offcut_cells));
	give(server, who, p, got);
}

void Cuts::remember(Server *server, const record::Member &member)
{
	for (const v3s16 &p : record::blocks(member.box)) {
		std::vector<record::Member> here = members(server, p);
		here.push_back(member);
		keep(server, p, here);
	}
}

void Cuts::forget(Server *server, const peel::Box &box)
{
	// every member meeting it, found in the blocks it's in, forgotten in every block they're in
	std::vector<record::Member> gone;
	for (const v3s16 &p : record::blocks(box))
		for (const record::Member &m : members(server, p))
			if (record::meets(m.box, box))
				gone.push_back(m);
	for (const record::Member &g : gone)
		for (const v3s16 &p : record::blocks(g.box)) {
			std::vector<record::Member> here = members(server, p);
			here.erase(std::remove_if(here.begin(), here.end(), [&](const record::Member &m) {
				return m.item == g.item && m.box.min == g.box.min && m.box.max == g.box.max;
			}), here.end());
			keep(server, p, here);
		}
}

std::vector<record::Member> Cuts::members(Server *server, v3s16 p)
{
	const NodeMetadata *meta = server->getEnv().getMap().getNodeMetadata(p);
	return meta ? record::read(meta->getString(record::KEY)) : std::vector<record::Member>();
}

void Cuts::keep(Server *server, v3s16 p, const std::vector<record::Member> &members)
{
	Map &map = server->getEnv().getMap();
	NodeMetadata *meta = map.getNodeMetadata(p);
	if (!meta) {
		if (members.empty())
			return;
		meta = new NodeMetadata(server->idef());
		map.setNodeMetadata(p, meta);
	}
	meta->setString(record::KEY, record::write(members));
	// so it's saved, and sent
	MapEditEvent event;
	event.type = MEET_BLOCK_NODE_METADATA_CHANGED;
	event.setPositionModified(p);
	map.dispatchEvent(event);
}

} // namespace cut
