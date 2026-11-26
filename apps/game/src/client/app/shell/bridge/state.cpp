// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/state.h"
#include "src/server/world/run/machines/piece/machine.h"
#include "src/client/world/objects/content_cao.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/game/content/nodedef.h"
#include "src/client/world/player/hand.h"
#include "src/client/app/shell/bridge/items.h"
#include "src/client/app/shell/host/shell.h"
#include "src/client/world/session/client.h"
#include "src/client/views/game/camera.h"
#include "src/client/world/objects/content_cao.h"
#include "src/client/world/objects/localplayer.h"
#include "src/shared/base/convert_json.h"
#include "src/shared/game/objects/hud_element.h"
#include "src/shared/game/items/inventory.h"
#include "library/world/planet/atlas.h"
#include "library/trace/trace.h"
#include <cmath>

namespace shell {

void State::update(const HudView &view)
{
	TF_ZONE("UI: state"); // every section built and compared, each frame
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	// The player's object (and with it max HP/breath) arrives a moment after
	// the session starts; until then there is nothing true to report.
	if (!player || !player->getCAO())
		return;

	send("hud", hud(*player, view));
	const Hand &hand = m_opts.hand;
	Json::Value tools;
	tools["out"] = hand.out();
	if (hand.out()) {
		tools["tool"] = sculpt::KINDS[size_t(hand.tool())].id;
		tools["size"] = hand.size();
		tools["mirrored"] = hand.mirrored();
		tools["focused"] = view.focused;
		tools["cursor"] = view.cursor;
		tools["naming"] = view.naming;
		tools["brush"] = Hand::BRUSHES[size_t(hand.brush())].id; // the mode picked (Add acts as a part creating)
		tools["creating"] = hand.creating();
		tools["partJoint"] = piece::JOINTS[size_t(hand.partJoint())].id;
	}
	send("tools", tools);
	Json::Value library(Json::arrayValue);
	for (const std::string &name : m_opts.client->getBlueprints())
		library.append(name);
	send("library", library);
	send("player", this->player(*player));
	if (m_opts.client->getAtlas() && m_opts.client->getCamera()) {
		TF_ZONE("UI: state: map pose");
		if (std::optional<Json::Value> pose = mapPose(*player))
			send("mapPose", *pose);
	}
	send("hotbar", hotbar(*player));
	if ((!m_inventory || *m_inventory != player->inventory) && itemsReady(*player)) {
		TF_ZONE("UI: state: inventory");
		Json::Value inv = inventory(*player);
		sendNewItems(); // before the inventory that references them
		send("inventory", inv);
		m_inventory.emplace(player->inventory);
	}
}

void State::forget()
{
	m_sent.clear();
	m_sent_items.clear();
	m_inventory.reset();
	m_new_items = Json::objectValue;
}

Json::Value State::player(LocalPlayer &player) const
{
	const ObjectProperties &props = player.getCAO()->getProperties();
	Json::Value out;
	out["hp"] = player.hp;
	out["hpMax"] = props.hp_max;
	out["breath"] = player.getBreath();
	out["breathMax"] = props.breath_max;
	return out;
}

std::optional<Json::Value> State::mapPose(LocalPlayer &player) const
{
	const planet::Atlas &atlas = *m_opts.client->getAtlas();
	const v3f pos = player.getPosition() / BS;
	// The camera supplies the view vector directly. Its yaw convention is not
	// the same as the map's east-clockwise heading convention.
	const v3f view = m_opts.client->getCamera()->getDirection();
	const double flat_length = std::hypot(view.X, view.Z);
	const double yaw = player.getYaw() * M_PI / 180.0;
	const double dx = flat_length > 0.0001 ? view.X / flat_length : -std::sin(yaw);
	const double dz = flat_length > 0.0001 ? view.Z / flat_length : -std::cos(yaw);
	// where the player is on the planet, and a block ahead in their own grid
	// carried on (over a seam too)
	const planet::Atlas::Node node = planet::Atlas::moverNode(pos.X, pos.Y, pos.Z);
	const std::optional<terragen::Atlas::Vec> here = atlas.point(node, pos.X, pos.Y, pos.Z);
	const std::optional<terragen::Atlas::Vec> ahead = atlas.point(node, pos.X + dx, pos.Y, pos.Z + dz);
	if (!here || !ahead)
		return std::nullopt; // past the atlas's top: on no place of the planet
	const auto latitude = [](const terragen::Atlas::Vec &p) { return std::atan2(p.y, std::hypot(p.x, p.z)); };
	const auto longitude = [](const terragen::Atlas::Vec &p) { return std::atan2(p.z, p.x); };
	const double lat = latitude(*here), lon = longitude(*here);
	const double east = std::remainder(longitude(*ahead) - lon, 2 * M_PI) * std::cos(lat);
	const double north = latitude(*ahead) - lat;
	const double radius = atlas.body(*atlas.bodyAt(node)).params().radius; // it has a point: it's on a body's tile
	Json::Value out;
	out["u"] = lon / (2 * M_PI) + 0.5;
	out["v"] = 0.5 - lat / M_PI;
	out["heading"] = std::atan2(east, north) * 180.0 / M_PI;
	out["height"] = std::sqrt(here->x * here->x + here->y * here->y + here->z * here->z) - radius;
	return out;
}

Json::Value State::hud(LocalPlayer &player, const HudView &view) const
{
	// The server can hide parts of the HUD per player (hud_set_flags).
	Json::Value out;
	out["visible"] = view.visible;
	out["crosshair"] = view.crosshair;
	out["hotbar"] = (player.hud_flags & HUD_FLAG_HOTBAR_VISIBLE) != 0;
	out["health"] = (player.hud_flags & HUD_FLAG_HEALTHBAR_VISIBLE) != 0;
	out["breath"] = (player.hud_flags & HUD_FLAG_BREATHBAR_VISIBLE) != 0;
	return out;
}

Json::Value State::hotbar(LocalPlayer &player) const
{
	Json::Value out;
	out["size"] = player.getMaxHotbarItemcount();
	out["selected"] = player.getWieldIndex();
	return out;
}

// Every item the inventory holds described, a few ms of it a frame: till
// they are, it isn't sent (the page shows none rather than stacks it can't draw)
bool State::itemsReady(LocalPlayer &player)
{
	TF_ZONE("UI: state: item icons");
	std::vector<std::string> names;
	for (const InventoryList *list : player.inventory.getLists())
		for (u32 i = 0; i < list->getSize(); i++)
			if (!list->getItem(i).empty())
				names.push_back(list->getItem(i).name);
	return m_opts.items.ready(names, ICONS_MS);
}

Json::Value State::inventory(LocalPlayer &player)
{
	Json::Value lists(Json::objectValue);
	for (const InventoryList *list : player.inventory.getLists()) {
		Json::Value stacks(Json::arrayValue);
		for (u32 i = 0; i < list->getSize(); i++) {
			const ItemStack &stack = list->getItem(i);
			if (stack.empty()) {
				stacks.append(Json::nullValue);
				continue;
			}
			// The catalogue may have generated this icon before the player ever
			// held the item. Still send its metadata to the page once.
			if (m_sent_items.insert(stack.name).second)
				m_new_items[stack.name] = m_opts.items.describe(stack.name);
			Json::Value s;
			s["item"] = stack.name;
			s["count"] = stack.count;
			s["wear"] = stack.wear;
			stacks.append(s);
		}
		Json::Value l;
		l["width"] = list->getWidth();
		l["stacks"] = stacks;
		lists[list->getName()] = l;
	}
	return lists;
}

void State::sendNewItems()
{
	if (m_new_items.empty())
		return;
	Json::Value message;
	message["kind"] = "items";
	message["items"] = m_new_items;
	m_opts.shell.send(message);
	m_new_items = Json::objectValue;
}

void State::send(const char *section, const Json::Value &value)
{
	std::string encoded = fastWriteJson(value);
	std::string &last = m_sent[section];
	if (encoded == last)
		return;
	last = encoded;

	Json::Value message;
	message["kind"] = "state";
	message["section"] = section;
	message["value"] = value;
	m_opts.shell.send(message);
}

} // namespace shell
