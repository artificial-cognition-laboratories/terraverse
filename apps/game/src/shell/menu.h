// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "content/subgames.h"
#include "community.h"
#include "online/join.h"
#include "planetarium.h"
#include "planet/atlas.h"
#include "far/baked.h"
#include <map>
#include <mutex>
#include <optional>
#include <string>

class MyEventReceiver;
class RenderingEngine;

namespace shell {

class Shell;
class Worlds;
class Lab;

/*
	The main menu: the page draws it, this answers it. While a Menu exists the
	page can list, create and delete worlds, and choose one to play.
*/
// What the player chose: a world of their own (the Lab's are never shared), or a friend's.
struct Choice
{
	std::optional<WorldSpec> world;
	bool lab = false;
	std::optional<online::JoinPlan> join;
};

class Menu
{
public:
	struct Opts {
		Shell &shell;
		RenderingEngine *engine;
		MyEventReceiver &receiver; // the window's input: the Planetarium takes what the page doesn't
		const Worlds &worlds;
		const Lab &lab;
	};

	explicit Menu(const Opts &opts);
	~Menu();

	// Runs frames until the player picks a world or a friend's, or quits
	// (nullopt). `error` is why the last session ended, if it ended badly.
	std::optional<Choice> run(const std::string &error);

private:
	Shell &m_shell;
	RenderingEngine *m_engine;
	const Worlds &m_worlds;
	const Lab &m_lab;
	Community m_community;
	MyEventReceiver &m_receiver;
	// The create screen's planet (the window's: Shell::planetarium) is the
	// world being made, from a code
	Planetarium &m_planetarium;
	Planetarium::Making body(const Json::Value &body);
	std::optional<far::Baked> m_baked; // the game's palette, read once it's first needed
	std::map<std::string, planet::Atlas> m_atlases; // by the bodies' sizes: each laid out once, every body of its system
	std::optional<Choice> m_choice;
	bool m_quit = false;
	// Set on the bridge's worker once a join is ready; taken by run().
	std::mutex m_join_lock;
	std::optional<online::JoinPlan> m_join;
};

} // namespace shell
