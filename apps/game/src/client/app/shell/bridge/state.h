// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include "src/shared/base/irrlichttypes.h"
#include "src/shared/base/irr_v2d.h"
#include "src/shared/game/items/inventory.h"
#include <optional>
#include <utility>
#include <string>
#include <unordered_set>
#include <unordered_map>

class Client;
class LocalPlayer;
class Hand;

namespace shell {

class Shell;
class Items;

// What the engine decided about the HUD this frame (F1, camera mode, touch).
struct HudView
{
	bool visible;
	bool crosshair;
	bool focused; // Terraverse: an object's on the workbench
	bool cursor;  // Terraverse: the cursor's free (the page is clicked)
	bool naming;  // Terraverse: a part's to be saved, the page asks its name (sprint 74)
};

/*
	The game as the HUD sees it, read from the local player once per frame.
	Each section (player, hotbar, inventory) is sent when its content differs
	from what the page last received — compared by value, not dirty flags,
	which the engine resets for its own purposes.
*/
class State
{
public:
	struct Opts {
		Shell &shell;
		Client *client;
		Items &items;
		const Hand &hand; // Terraverse: the tools in hand
	};

	explicit State(const Opts &opts) : m_opts(opts) {}

	void update(const HudView &view);
	// The page has reloaded: send everything again.
	void forget();
	// Where the player is on the planet's map (u, v, heading, height), and
	// which way they face; none past the atlas's top
	std::optional<Json::Value> mapPose(LocalPlayer &player) const;

private:
	Json::Value player(LocalPlayer &player) const;
	Json::Value hud(LocalPlayer &player, const HudView &view) const;
	Json::Value hotbar(LocalPlayer &player) const;
	Json::Value inventory(LocalPlayer &player);
	bool itemsReady(LocalPlayer &player);
	static constexpr float ICONS_MS = 3; // a frame's drawing of the inventory's new icons
	void sendNewItems();
	void send(const char *section, const Json::Value &value);

	Opts m_opts;
	std::unordered_map<std::string, std::string> m_sent;
	std::unordered_set<std::string> m_sent_items;
	// The inventory as last sent: compared as stacks each frame, made into
	// JSON only when it differs (every slot as JSON, each frame, cost ms)
	std::optional<Inventory> m_inventory;
	Json::Value m_new_items = Json::objectValue;
};

} // namespace shell
