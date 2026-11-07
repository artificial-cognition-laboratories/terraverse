// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "screens.h"
#include "shell.h"
#include "ui.h"
#include "exceptions.h"
#include "gui/mainmenumanager.h"
#include "client/keycode.h"
#include "gui/modalMenu.h"
#include "log.h"
#include <IGUIEnvironment.h>
#include <functional>
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
	{ "lab", false, "keymap_lab", "lab" },
	{ "wiki", false, "keymap_wiki", "wiki" }, // offered by tf_player where the world's rules allow it
	{ "making", false, nullptr, "making" }, // tf_craft: at a station, or making by hand
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
	whichever way it goes (Esc, Screens::close, or game shutdown).
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

	~Overlay() override { m_on_gone(); }

	bool OnEvent(const SEvent &event) override
	{
		if (event.EventType != EET_KEY_INPUT_EVENT || !event.KeyInput.PressedDown)
			return false;
		bool closes = event.KeyInput.Key == KEY_ESCAPE ||
				(m_kind.toggle && keySettingHasMatch(m_kind.toggle, KeyPress(event.KeyInput)));
		if (closes)
			quitMenu();
		return closes;
	}

	bool pausesGame() override { return m_kind.pauses; }
	void regenerateGui(v2u32 screensize) override {}
	void drawMenu() override {}

protected:
	std::wstring getLabelByID(s32 id) override { return L""; }
	std::string getNameByID(s32 id) override { return ""; }

private:
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

void Screens::open(const std::string &name)
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
	announce(name);
}

void Screens::close()
{
	if (m_overlay)
		m_overlay->quitMenu(); // → ~Overlay → gone()
}

void Screens::gone()
{
	m_overlay = nullptr;
	if (!m_ending)
		announce("");
}

void Screens::announce(const std::string &name) const
{
	Json::Value message;
	message["kind"] = "screen";
	message["screen"] = name.empty() ? Json::Value() : Json::Value(name);
	m_opts.shell.send(message);
}

} // namespace shell
