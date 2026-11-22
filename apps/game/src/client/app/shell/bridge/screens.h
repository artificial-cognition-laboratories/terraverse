// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>
#include <json/json.h>

namespace gui { class IGUIEnvironment; }

namespace shell {

class Shell;
class Overlay;
class Ui;

/*
	Full screens the page draws over the game (pause, later inventory and
	more). While one is open, the engine holds an invisible modal menu, so
	the game behaves exactly as it does for its own menus: the cursor is
	freed, the player stops, singleplayer pauses if the screen asks, and Esc
	closes it.
*/
class Screens
{
public:
	struct Opts {
		Shell &shell;
		gui::IGUIEnvironment *guienv;
		const Ui &ui;
	};

	explicit Screens(const Opts &opts) : m_opts(opts) {}
	~Screens();

	// Throws on a screen not in the table. A screen that needs a UI value
	// (the map, the wiki) stays shut where the server hasn't offered it.
	// `with`: what it opens on, passed to the page (the palette's prefix).
	void open(const std::string &name, const Json::Value &with = Json::Value());
	void close();
	bool isOpen() const { return m_overlay != nullptr; }

private:
	void announce(const std::string &name, const Json::Value &with = Json::Value()) const;
	void gone();

	Opts m_opts;
	Overlay *m_overlay = nullptr;
	bool m_ending = false;
};

} // namespace shell
