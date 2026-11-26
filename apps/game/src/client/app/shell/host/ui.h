// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>
#include <unordered_map>

class Client;

namespace shell {

class Shell;

/*
	The UI channel's client end. Values game Lua sets for this player's page
	(player:set_ui) go to the page as they arrive; the page's events go back
	to the server (core.register_on_ui_event). Remembers which keys are set,
	so a screen that needs one (the map, the wiki) opens only where it's offered.
*/
class Ui
{
public:
	struct Opts {
		Shell &shell;
		Client *client;
	};

	explicit Ui(const Opts &opts) : m_opts(opts) {}

	// Once per frame: forwards what arrived since the last frame.
	void update();
	// The page has reloaded: every value set, again.
	void replay();
	// The page's `ui.send` intent: { name, payload }. Throws Refused.
	void send(const Json::Value &intent);
	bool has(const std::string &key) const { return m_values.count(key) != 0; }
	// The latest value for a key, or null when unset.
	const Json::Value &get(const std::string &key) const;

private:
	void post(const std::string &key, const Json::Value &value);

	Opts m_opts;
	std::unordered_map<std::string, Json::Value> m_values;
};

} // namespace shell
