// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/app/shell/bridge/actions.h"
#include "src/client/app/shell/bridge/chat.h"
#include "src/client/app/shell/bridge/items.h"
#include "src/client/app/shell/bridge/screens.h"
#include "src/client/app/shell/bridge/state.h"
#include "src/client/app/shell/host/ui.h"
#include "src/client/views/scenes/kinds/planetarium.h"
#include "library/world/terragen/src/code/worlds.h"
#include <optional>
#include <string>

class Client;
struct SEvent;
class Hand;
namespace video { class IVideoDriver; }
namespace gui { class IGUIEnvironment; }

namespace shell {

class Shell;

/*
	One running game, as the page sees it. Exists from the moment the client
	has its content (item and node definitions) until the game shuts down,
	and answers the in-game intents for exactly that long.
*/
class Session
{
public:
	struct Opts {
		Shell &shell;
		Client *client;
		video::IVideoDriver *driver;
		gui::IGUIEnvironment *guienv;
		Hand &hand; // Terra Firma: the tools in hand (the page picks one)
	};

	explicit Session(const Opts &opts);
	~Session();

	// Once per frame, before drawing.
	void update(const HudView &view);

	Screens &screens() { return m_screens; }
	// A key typed over the game: one of the palette's lists (: @ # >, and / for :) opens the
	// palette on it, where nothing else is open. Whether it was taken.
	bool typed(const SEvent &event);
	// The planet under the page: this world's, when the page or a key shows it.
	Planetarium &planetarium() { return m_planetarium; }
	// A body of this world (its atlas index), as the Planetarium shows it.
	// Throws Refused off a planet.
	planetarium::Planetarium::Opts world(int body) const;
	mutable std::shared_ptr<const terragen::System> m_system; // world()'s
	mutable std::optional<far::Palette> m_palette;
	// Shows the body the player is on (by a key: no page asked); why not, if it can't.
	std::optional<std::string> showWorld();

private:
	// This world's bodies (terragen/worlds.h), the home first; Refused off a planet.
	std::vector<terragen::BodyWorld> bodies() const;
	// A body the page names: `{ body: id }` (none: the home), its atlas index.
	int bodyNamed(const Json::Value &intent) const;
	// The body the player stands on now (the home before they're on a tile).
	int bodyHere() const;

	Shell &m_shell;
	unsigned m_page; // the page load this session last spoke to
	Items m_items;
	State m_state;
	Ui m_ui;
	Screens m_screens;
	Chat m_chat;
	Actions m_actions;
	Client *m_client;
	Planetarium &m_planetarium; // the window's (Shell::planetarium), showing this world
	mutable int32_t m_year = 0;
	bool m_prepared = false; // the planet's been made ready (Planetarium::prepare)  // this world's, once world() has read its code
};

} // namespace shell
