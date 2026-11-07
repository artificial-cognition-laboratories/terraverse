// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "inventorymanager.h"
#include "irrlichttypes.h"
#include <json/json.h>
#include <string>

class Client;

namespace shell {

/*
	The page's hands in the player's inventory: move, craft and drop, turned
	into the same inventory actions the engine's own menus send. Every slot is
	checked against the local inventory here; the server checks again and has
	the final word, and the result comes back through the state stream.
*/
class Actions
{
public:
	explicit Actions(Client *client) : m_client(client) {}

	// { from: {list, index, at?}, to: {list, index?, at?}, count } — no index: first slot that
	// fits; at {x, y, z}: that node's inventory, not the player's
	void move(const Json::Value &intent) const;
	// { count, collect } — collect: move the result straight into "main"
	void craft(const Json::Value &intent) const;
	// { from: {list, index}, count }
	void drop(const Json::Value &intent) const;

private:
	struct Slot
	{
		InventoryLocation inv; // the player's, or a node's (`at`: a station's worktop)
		std::string list;
		s16 index; // -1: anywhere in the list
	};

	Slot slot(const Json::Value &value, const char *field, bool any_index) const;
	u16 count(const Json::Value &intent) const;

	Client *m_client;
};

} // namespace shell
