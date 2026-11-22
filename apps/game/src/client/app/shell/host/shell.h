// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SEvent;
namespace video { class IVideoDriver; }
namespace gui { class IGUIEnvironment; class ICursorControl; }
namespace Json { class Value; }
namespace home { class Home; }

namespace shell {

class Planetarium;
class Entering;

/*
	Chromium re-launches this executable for its renderer and GPU processes.
	Call first thing in main(): returns the exit code when this process is one
	of those helpers, or -1 when it is the game itself.
*/
int runHelper(int argc, char *argv[]);

// Entering: a world chosen, on its way (entering.h); the menu's page stays up, locked
enum class Mode { Menu, Entering, Game, Leaving };

// Answers one kind of intent from the page (see bridge.h: Refused).
using Verb = std::function<Json::Value(const Json::Value &intent)>;

/*
	The web UI, drawn over every frame. One Chromium page lives for the whole
	session; the engine tells it what state it is in, the page sends intents back.
*/
class Shell
{
public:
	struct Opts {
		video::IVideoDriver *driver;
		gui::IGUIEnvironment *guienv;
		gui::ICursorControl *cursor;
		std::string url;       // page to load: terra://ui/index.html, or a dev server
		std::string ui_dir;    // served as terra://ui/
		std::string cache_dir; // Chromium profile and log
		float scale;           // the display's density: device pixels per CSS pixel, before the window's fit (ui_scale.h)
		bool gpu;              // Chromium rasterises on the GPU
		std::string arclabs;   // the ArcLabs API accounts and friends come from
		home::Home &home;      // whose sign-in: the active profile keeps it, and signing in picks the profile
		bool ui_check = false;   // held to its budgets, then quit (--ui-check: ui_check.h)
		float ui_scale = 1.0f;   // the player's multiplier on the page's size (the `ui_scale` setting)
	};

	explicit Shell(const Opts &opts);
	~Shell();

	// Once per frame, once the window's events are in (RenderingEngine::run):
	// asks Chromium for the page's frame. It draws while the world does, so
	// what this frame's input changed is ready to draw at its end. Later
	// calls in the frame do nothing.
	void beginFrame();
	// Once per frame, immediately before endScene(): the page's intents run, the page drawn.
	void frame();
	// True when the page took the event and the engine must not see it.
	bool onEvent(const SEvent &event);
	// Whether the debugger's open: the window keeps its full frame rate unfocused
	// then (it shares the game's loop, and a capped game measures as sleep).
	bool debuggerOpen() const;
	void setMode(Mode mode);
	// How many times the page has announced itself. A change means a fresh
	// page that holds nothing it was sent before (a reload).
	unsigned pageLoads() const;

	// Whoever owns a screen registers the intents it answers, and removes them
	// when it hands over: an intent nobody answers fails loudly in the page.
	void on(const std::string &kind, Verb verb);
	// Slow, thread-safe work answered off the main thread (see Bridge::onAsync).
	void onAsync(const std::string &kind, Verb verb);
	// What draws between the planet and the page, and takes the mouse the page leaves (a wiki scene, the figure).
	class Underlays &underlays();
	void drain(); // until the worker has finished what it was given
	void off(const std::string &kind);
	// A one-off message to the page; held until the page is ready.
	void send(const Json::Value &message);

	// Friends and presence; the menu says which world is being entered.
	class Social &social();
	// The ArcLabs account (who's signed in).
	class Account &account();

	// Where item icons are written; served to the page as terra://item/.
	const std::string &itemDir() const;
	// Where a local mod's pictures are written for its Contents: served as terra://contents/.
	const std::string &contentsDir() const;
	// The player's multiplier on the page's size: it's drawn at it from the next frame.
	void chooseUiScale(float chosen);
	// Window pixels to a page (CSS) pixel: where the page puts something, ×.
	float scale() const;
	// The planet under the page (planetarium.h): the window's, as long as it.
	Planetarium &planetarium();
	// A world being entered, Play to its first frame (entering.h).
	Entering &entering();
	// What the debugger's Server tab shows of the server, while in a world (shell/serverinfo.h); null when leaving it.
	void serverInfo(std::function<Json::Value()> describe);

private:
	void frameUi();

	struct Impl;
	std::unique_ptr<Impl> m;
};

/*
	The intents a screen answers, registered through this and removed together
	(clear(), or when it goes, a screen that failed partway through being made
	too): a list kept by hand beside them missed one, and the next screen to
	register it threw.
*/
class Intents
{
public:
	explicit Intents(Shell &shell) : m_shell(shell) {}
	~Intents() { clear(); }
	Intents(const Intents &) = delete;
	Intents &operator=(const Intents &) = delete;

	void on(const std::string &kind, Verb verb)
	{
		m_shell.on(kind, std::move(verb));
		m_kinds.push_back(kind);
	}
	void onAsync(const std::string &kind, Verb verb)
	{
		m_shell.onAsync(kind, std::move(verb));
		m_kinds.push_back(kind);
	}
	void clear()
	{
		for (const std::string &kind : m_kinds)
			m_shell.off(kind);
		m_kinds.clear();
	}

private:
	Shell &m_shell;
	std::vector<std::string> m_kinds;
};

} // namespace shell

// The session's shell, or null before the client starts it. The three frame
// sites (menu, game, load screen) and the event receiver reach it here.
extern shell::Shell *g_shell;
