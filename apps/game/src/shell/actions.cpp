// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "actions.h"
#include "bridge.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "inventory.h"
#include "inventorymanager.h"

namespace shell {

static InventoryLocation player()
{
	InventoryLocation loc;
	loc.setCurrentPlayer();
	return loc;
}

// The player's own inventory, or with `at` a node's; the server checks it's in reach
static InventoryLocation where(const Json::Value &s)
{
	const Json::Value &at = s["at"];
	if (at.isNull())
		return player();
	if (!at.isObject() || !at["x"].isInt() || !at["y"].isInt() || !at["z"].isInt())
		throw Refused("'at' needs whole x, y and z");
	InventoryLocation loc;
	loc.setNodeMeta(v3s16(at["x"].asInt(), at["y"].asInt(), at["z"].asInt()));
	return loc;
}

Actions::Slot Actions::slot(const Json::Value &value, const char *field, bool any_index) const
{
	const Json::Value &s = value[field];
	if (!s.isObject() || !s["list"].isString())
		throw Refused(std::string("'") + field + "' needs a list");

	const InventoryLocation inv = where(s);
	Inventory *inventory = m_client->getInventory(inv);
	const InventoryList *list = inventory ? inventory->getList(s["list"].asString()) : nullptr;
	if (!list)
		throw Refused("no inventory list '" + s["list"].asString() + "'" + (inventory ? "" : " there"));

	if (any_index && s["index"].isNull())
		return { inv, list->getName(), -1 };
	if (!s["index"].isInt() || s["index"].asInt() < 0 || (u32)s["index"].asInt() >= list->getSize())
		throw Refused("slot " + s["index"].toStyledString() + " is outside '" + list->getName() + "'");
	return { inv, list->getName(), (s16)s["index"].asInt() };
}

u16 Actions::count(const Json::Value &intent) const
{
	const Json::Value &c = intent["count"];
	if (!c.isInt() || c.asInt() < 1 || c.asInt() > 65535)
		throw Refused("count must be a whole number from 1");
	return (u16)c.asInt();
}

void Actions::move(const Json::Value &intent) const
{
	Slot from = slot(intent, "from", false);
	Slot to = slot(intent, "to", true);

	IMoveAction *a = new IMoveAction();
	a->count = count(intent);
	a->from_inv = from.inv;
	a->from_list = from.list;
	a->from_i = from.index;
	a->to_inv = to.inv;
	a->to_list = to.list;
	a->to_i = to.index;
	a->move_somewhere = to.index < 0;
	m_client->inventoryAction(a); // takes ownership
}

void Actions::craft(const Json::Value &intent) const
{
	ICraftAction *a = new ICraftAction();
	a->count = count(intent);
	a->craft_inv = player();
	m_client->inventoryAction(a);

	if (!intent["collect"].asBool())
		return;
	// Sent in order on one connection, so the craft has landed in craftresult
	// by the time the server applies this.
	IMoveAction *m = new IMoveAction();
	m->from_inv = player();
	m->from_list = "craftresult";
	m->from_i = 0;
	m->to_inv = player();
	m->to_list = "main";
	m->move_somewhere = true;
	m_client->inventoryAction(m);
}

void Actions::drop(const Json::Value &intent) const
{
	Slot from = slot(intent, "from", false);
	IDropAction *a = new IDropAction();
	a->count = count(intent);
	a->from_inv = from.inv;
	a->from_list = from.list;
	a->from_i = from.index;
	m_client->inventoryAction(a);
}

} // namespace shell
