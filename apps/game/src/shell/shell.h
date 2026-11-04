// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>

struct SEvent;
namespace video { class IVideoDriver; }
namespace gui { class IGUIEnvironment; class ICursorControl; }
namespace Json { class Value; }

namespace shell {

class Planetarium;

/*
	Chromium re-launches this executable for its renderer and GPU processes.
	Call first thing in main(): returns the exit code when this process is one
	of those helpers, or -1 when it is the game itself.
*/
int runHelper(int argc, char *argv[]);

enum class Mode { Menu, Game };

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
		float scale;           // device pixels per CSS pixel
		bool gpu;              // Chromium rasterises on the GPU
		std::string arclabs;   // the ArcLabs API accounts and friends come from
		std::string account_dir; // where sign-ins are saved: the user folder, or a profile's
	};

	explicit Shell(const Opts &opts);
	~Shell();

	// Once per frame, immediately before endScene(): pumps Chromium, draws the page.
	void frame();
	// True when the page took the event and the engine must not see it.
	bool onEvent(const SEvent &event);
	void setMode(Mode mode);
	// How many times the page has announced itself. A change means a fresh
	// page that holds nothing it was sent before (a reload).
	unsigned pageLoads() const;

	// Whoever owns a screen registers the intents it answers, and removes them
	// when it hands over: an intent nobody answers fails loudly in the page.
	void on(const std::string &kind, Verb verb);
	// Slow, thread-safe work answered off the main thread (see Bridge::onAsync).
	void onAsync(const std::string &kind, Verb verb);
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
	// Window pixels to a page (CSS) pixel: where the page puts something, ×.
	float scale() const;
	// The planet under the page (planetarium.h): the window's, as long as it.
	Planetarium &planetarium();

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace shell

// The session's shell, or null before the client starts it. The three frame
// sites (menu, game, load screen) and the event receiver reach it here.
extern shell::Shell *g_shell;
