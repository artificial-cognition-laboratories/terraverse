// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/screens.h"
#include "src/client/app/shell/host/shell.h"
#include "src/client/app/shell/host/ui.h"
#include "src/shared/base/exceptions.h"
#include "src/client/app/menus/mainmenumanager.h"
#include "src/client/app/input/keycode.h"
#include "src/client/app/menus/modalMenu.h"
#include "src/shared/platform/system/log.h"
#include <IGUIEnvironment.h>
#include <functional>
#include <utility>
#include <json/json.h>

namespace shell {

// Each screen the page can show in-game: whether it pauses singleplayer, the
// key setting that also closes it (besides Esc), if any, and the UI value
// the server must have set for it to open, if any.
struct ScreenKind
{
	const char *name;
	bool pauses;
	const char *toggle;
	const char *needs;
};

static const ScreenKind SCREENS[] = {
	{ "pause", true, nullptr, nullptr },
	{ "inventory", false, "keymap_inventory", nullptr },
	{ "map", true, "keymap_minimap", "map" },
	{ "wiki", false, "keymap_wiki", "wiki" }, // offered where the world's rules allow it (server/player.cpp)
	{ "library", false, "keymap_library", nullptr },
	// chat and commands (the page's palette): Esc is the page's, its input has the keyboard
	{ "palette", false, nullptr, nullptr },
};

static const ScreenKind &find(const std::string &name)
{
	for (const ScreenKind &kind : SCREENS) {
		if (name == kind.name)
			return kind;
	}
	throw BaseException("shell: no screen called '" + name + "'");
}

/*
	The engine's stand-in for a page screen: a modal menu that draws nothing.
	Owned by the GUI environment like any menu; tells Screens when it goes,
	whichever way it goes (Esc, Screens::close, or game shutdown). It tells
	when it quits, not when it's destroyed: the GUI environment holds the
	element under the mouse, so the full-screen overlay outlives its quitting
	till the mouse moves, and meanwhile the game plays under a page still
	showing the screen (and an old overlay's late goodbye would close the next).
*/
class Overlay : public GUIModalMenu
{
public:
	Overlay(gui::IGUIEnvironment *env, const ScreenKind &kind, std::function<void()> on_gone) :
		GUIModalMenu(env, env->getRootGUIElement(), -1, &g_menumgr),
		m_kind(kind), m_on_gone(std::move(on_gone))
	{
		Environment->setFocus(this);
	}

	~Overlay() override { tell(); }

	void quit()
	{
		tell();
		quitMenu();
	}

	// Before focus, not on it: the menu manager offers every event to the front menu, so Esc
	// closes this however the GUI's focus has wandered (a click on the page moves it off us).
	// Keys the page holds (it has an input focused) never get here: the shell takes them first.
	bool preprocessEvent(const SEvent &event) override
	{
		if (event.EventType != EET_KEY_INPUT_EVENT || !event.KeyInput.PressedDown)
			return GUIModalMenu::preprocessEvent(event);
		bool closes = event.KeyInput.Key == KEY_ESCAPE ||
				(m_kind.toggle && keySettingHasMatch(m_kind.toggle, KeyPress(event.KeyInput)));
		if (closes)
			quit();
		return closes;
	}

	bool pausesGame() override { return m_kind.pauses; }
	void regenerateGui(v2u32 screensize) override {}
	void drawMenu() override {}

protected:
	std::wstring getLabelByID(s32 id) override { return L""; }
	std::string getNameByID(s32 id) override { return ""; }

private:
	void tell()
	{
		if (m_on_gone)
			std::exchange(m_on_gone, nullptr)();
	}

	const ScreenKind &m_kind;
	std::function<void()> m_on_gone;
};

Screens::~Screens()
{
	// The session is ending: the page drops its screen with the mode change,
	// and nothing in the session answers it any more.
	m_ending = true;
	close();
}

void Screens::open(const std::string &name, const Json::Value &with)
{
	const ScreenKind &kind = find(name);
	if (kind.needs && !m_opts.ui.has(kind.needs)) {
		infostream << "shell: screen " << name << " isn't offered here" << std::endl;
		return;
	}
	close();
	m_overlay = new Overlay(m_opts.guienv, kind, [this] { gone(); });
	m_overlay->drop(); // the GUI environment owns it now, as it does every menu
	infostream << "shell: screen " << name << " opened" << std::endl;
	announce(name, with);
}

void Screens::close()
{
	if (m_overlay)
		m_overlay->quit(); // → gone()
}

void Screens::gone()
{
	m_overlay = nullptr;
	if (!m_ending)
		announce("");
}

void Screens::announce(const std::string &name, const Json::Value &with) const
{
	Json::Value message;
	message["kind"] = "screen";
	message["screen"] = name.empty() ? Json::Value() : Json::Value(name);
	if (!with.isNull())
		message["with"] = with;
	m_opts.shell.send(message);
}

} // namespace shell
