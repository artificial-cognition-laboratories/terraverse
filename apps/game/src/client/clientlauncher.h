// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>

class RenderingEngine;
class Settings;
class MyEventReceiver;
class InputHandler;
struct GameParams;
struct GameStartData;
struct MainMenuData;

#if USE_SHELL
#include "shell/shell.h"
#include "shell/menu.h"
#include "shell/wiki.h"
#include <memory>
#endif

class ClientLauncher
{
public:
	ClientLauncher() = default;

	~ClientLauncher();

	bool run(const GameParams &game_params, const Settings &cmd_args);

private:
	void init_args(GameStartData &start_data, const Settings &cmd_args);
#if USE_SHELL
	bool online(GameStartData &start_data, shell::Choice &choice, std::string &error_message);
#endif
	void init_engine();
	void init_input();

	static void setting_changed_callback(const std::string &name, void *data);
	void config_guienv();

	bool launch_game(GameErrorData &errordata,
		GameStartData &start_data, const Settings &cmd_args);


	bool skip_main_menu = false;
	bool random_input = false;
	RenderingEngine *m_rendering_engine = nullptr;
	InputHandler *input = nullptr;
	MyEventReceiver *receiver = nullptr;
#if USE_SHELL
	std::unique_ptr<shell::Shell> m_shell;
	std::unique_ptr<shell::Wiki> m_wiki; // after m_shell: gone before it (it unregisters its verbs)
#endif
};
