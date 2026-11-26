// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/cut/cutting.h"
#include "src/client/world/cut/cutting_detail.h"

#include "src/client/app/shell/host/ui.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/world/session/client.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/content/nodemetadata.h"
#include "src/shared/game/cut/record.h"
#include "src/shared/game/cut/stack.h"
#include "src/shared/game/pointing/pointedthing.h"
#include "src/shared/map/map.h"
#include <algorithm>
#include <cmath>

namespace cut {

using namespace detail;

namespace {
// the chosen cut's box, as the UI value "cut" lists it
std::optional<peel::Size> chosenSize(const Json::Value &cut);
} // namespace

bool Cutting::menu(const PointedThing &pointed) const
{
	return cuts(pointed);
}

std::optional<aabb3f> Cutting::preview(const PointedThing &pointed) const
{
	std::optional<peel::Box> world;
	const std::optional<Aim> a = aim(pointed);
	const std::optional<peel::Size> size = chosenSize(m_opts.ui.get("cut"));
	if (a && size) {
		Map &map = m_opts.client->getEnv().getMap();
		const peel::Log log = peel::read(a->node, [&](v3s16 q) { return blockOf(map, q); });
		if (const std::optional<peel::Box> box = peel::at(log, a->face, *size, peel::cell(log, a->node, a->cell))) {
			const v3s32 start = record::world(log.start, v3s16(0, 0, 0));
			world = peel::Box{ start + box->min, start + box->max };
		}
	} else {
		world = placed(pointed);
	}
	if (!world)
		return std::nullopt;
	// world sixteenths to BS about the pointed node's centre
	const v3s32 from = record::world(pointed.node_undersurface, v3s16(0, 0, 0));
	const auto edge = [&](v3s32 c) { return (v3f(c.X - from.X, c.Y - from.Y, c.Z - from.Z) / shape::STEPS - v3f(0.5f)) * BS; };
	return aabb3f(edge(world->min), edge(world->max + v3s32(1, 1, 1)));
}

bool Cutting::take(const PointedThing &pointed) const
{
	std::optional<Aim> a = aim(pointed);
	const char *event = "cut.take";
	if (!a) {
		// timber, no cut: whatever was placed there taken back (the server gives offcuts if it's not one)
		a = aimed(pointed);
		if (!a || !m_opts.client->ndef()->get(m_opts.client->getEnv().getMap().getNode(a->node)).groups.count("timber"))
			return false;
		event = "cut.lift";
	}
	Json::Value payload;
	payload["node"] = list(a->node);
	payload["face"] = list(a->face);
	payload["aim"] = list(a->cell);
	send(m_opts.client, event, payload);
	return true;
}

std::optional<Cutting::Aim> Cutting::aim(const PointedThing &pointed) const
{
	if (!m_opts.ui.get("cut")["chosen"].isObject() || !cuts(pointed))
		return std::nullopt;
	return aimed(pointed);
}

std::optional<peel::Box> Cutting::placed(const PointedThing &pointed) const
{
	const std::optional<Aim> a = aimed(pointed);
	if (!a)
		return std::nullopt;
	Map &map = m_opts.client->getEnv().getMap();
	if (!m_opts.client->ndef()->get(map.getNode(a->node)).groups.count("timber"))
		return std::nullopt;
	const NodeMetadata *meta = map.getNodeMetadata(a->node);
	if (!meta)
		return std::nullopt;
	const std::optional<record::Member> m = record::at(record::read(meta->getString(record::KEY)), record::world(a->node, a->cell));
	if (!m)
		return std::nullopt;
	// lying flat, it comes off the top of what of its kind is stacked on it
	peel::Box box = m->box;
	if (box.max.Y - box.min.Y + 1 != m_opts.ui.get("cut")["members"][m->item]["in"].asInt())
		return box;
	for (;;) {
		std::vector<peel::Box> same;
		for (const record::Member &n : near(peel::Box{ box.min, box.max + v3s32(0, 1, 0) }))
			if (n.item == m->item)
				same.push_back(n.box);
		const std::optional<peel::Box> up = stack::above(box, same);
		if (!up)
			return box;
		box = *up;
	}
}

std::optional<Cutting::Aim> Cutting::aimed(const PointedThing &pointed)
{
	if (pointed.type != POINTEDTHING_NODE)
		return std::nullopt;
	const v3s16 node = pointed.node_undersurface, face = pointed.node_abovesurface - node;
	if (std::abs(face.X) + std::abs(face.Y) + std::abs(face.Z) != 1)
		return std::nullopt;
	// the sixteenth under the crosshair, kept in the node pointed at
	v3s16 cell = shape::hit(node, pointed.intersection_point, face).cell;
	cell = v3s16(std::clamp<s16>(cell.X, 0, shape::STEPS - 1), std::clamp<s16>(cell.Y, 0, shape::STEPS - 1),
			std::clamp<s16>(cell.Z, 0, shape::STEPS - 1));
	return Aim{ node, face, cell };
}

bool Cutting::cuts(const PointedThing &pointed) const
{
	if (pointed.type != POINTEDTHING_NODE)
		return false;
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	if (!player)
		return false;
	ItemStack selected, hand;
	const std::string &held = player->getWieldedItem(&selected, &hand).name;
	const MapNode n = m_opts.client->getEnv().getMap().getNode(pointed.node_undersurface);
	const ItemGroupList &groups = m_opts.client->ndef()->get(n).groups;
	for (const Json::Value &g : m_opts.ui.get("cut")["tools"][held])
		if (g.isString() && groups.count(g.asString()))
			return true;
	return false;
}

const Json::Value &Cutting::held() const
{
	static const Json::Value none;
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	if (!player)
		return none;
	ItemStack selected, hand;
	return m_opts.ui.get("cut")["members"][player->getWieldedItem(&selected, &hand).name];
}

namespace {
std::optional<peel::Size> chosenSize(const Json::Value &cut)
{
	const Json::Value &chosen = cut["chosen"];
	for (const Json::Value &f : cut["forms"])
		if (f["id"] == chosen["form"])
			for (const Json::Value &s : f["sizes"])
				if (s["id"] == chosen["size"])
					return peel::Size{ (s16)s["along"].asInt(), (s16)s["across"].asInt(), (s16)s["in"].asInt() };
	return std::nullopt;
}
} // namespace

namespace detail {
Json::Value list(v3s32 v)
{
	Json::Value out(Json::arrayValue);
	out.append(v.X);
	out.append(v.Y);
	out.append(v.Z);
	return out;
}

Json::Value list(v3s16 v)
{
	Json::Value out(Json::arrayValue);
	out.append(v.X);
	out.append(v.Y);
	out.append(v.Z);
	return out;
}

void send(Client *client, const char *event, const Json::Value &payload)
{
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	client->sendUiEvent(event, Json::writeString(writer, payload));
}

std::optional<peel::Block> blockOf(Map &map, v3s16 q)
{
	bool valid;
	const MapNode n = map.getNode(q, &valid);
	if (!valid)
		return std::nullopt;
	return peel::Block{ n.getContent(), n.getParam2(), map.getShape(q) };
}

v3s16 blockAt(v3s32 cell)
{
	const auto down = [](s32 v) { return (s16)(v >= 0 ? v / shape::STEPS : -((-v + shape::STEPS - 1) / shape::STEPS)); };
	return v3s16(down(cell.X), down(cell.Y), down(cell.Z));
}

v3s32 cellAt(v3f at)
{
	const v3f c = at / BS * shape::STEPS + v3f(shape::STEPS / 2.0f);
	return v3s32(std::floor(c.X), std::floor(c.Y), std::floor(c.Z));
}
} // namespace detail

} // namespace cut
