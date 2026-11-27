// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/app/shell/host/shell.h"
#include <json/json.h>
#include <memory>
#include <optional>
#include <string>

namespace shell {

/*
	Agents in this world (sprint 127): the player's own, listed, brought in and sent away, by
	Axon's daemon (axond.h), which runs them; the game only asks. Made with the world and gone
	with it: going, it stops the agents it brought and lets the world go (axond stops speaking
	for it; the world itself is the game's). A world played signed out isn't open to agents:
	`world` is none, and bringing one says so.

	agents.list   the player's agents, each idle, starting, here, elsewhere or failed
	agents.embark one brought in: answered at once, it boots on a thread of its own (an agent
	              takes seconds to start), and the list shows it starting until it's here
	agents.stop   one sent away
*/
class Agents
{
public:
	struct Opts {
		Shell &shell;
		std::optional<std::string> world; // its address, terra://localhost:<port>, when it's listed (online/listed.h)
	};
	explicit Agents(Opts opts);
	~Agents();
	Agents(const Agents &) = delete;
	Agents &operator=(const Agents &) = delete;

	// What the embarking threads and the verbs share; it outlives this while one is still out.
	struct Shared;

private:
	Json::Value list();
	Json::Value embark(const Json::Value &intent);
	Json::Value stop(const Json::Value &intent);

	Shell &m_shell;
	const std::optional<std::string> m_world;
	std::shared_ptr<Shared> m_shared;
	Intents m_intents; // last: its verbs go first
};

} // namespace shell
