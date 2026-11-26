// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <dimension2d.h>
#include <vector2d.h>
#include <functional>
#include <json/json.h>
#include <memory>
#include <string>

struct SEvent;
namespace video { class IVideoDriver; }
namespace gui { class ICursorControl; }

namespace shell {

class Inbox;
class Pump;
class Inspectors;

/*
	The debugger: a page of its own beside the game, as a browser's devtools sit
	beside the site. While it's open the game is given a smaller screen (the
	driver's, at the window's top-left) and never learns there's more window;
	the debugger draws in the rest, once the frame is done. Its page is made the
	first time it opens, and kept, hidden, while it's closed. The engine's log
	is kept from when it's made, for its Console.
*/
class Debugger
{
public:
	struct Opts {
		video::IVideoDriver *driver;
		gui::ICursorControl *cursor;
		std::string url; // the game's page: the debugger's is beside it
		float scale;     // device pixels per CSS pixel
		// Text input on or off (the IME), for whichever page holds the keyboard.
		std::function<void(bool)> capture;
		Inbox &inbox; // the shell's: its page's intents run with the game page's
		Pump &pump;   // the shell's: Chromium's loop, run while its page closes (macOS)
		bool gpu;     // Chromium rasterises on the GPU: its frames may come shared
		std::string ui_dir; // shell://ui/, served to its page as to the game's
	};

	explicit Debugger(const Opts &opts);
	~Debugger();

	bool open() const;
	// Its page's frame asked of Chromium (Shell::beginFrame), while it's open.
	void beginFrame();
	// Its page loaded, hidden, so opening it shows it at once: once the game's page is up,
	// not to slow that. Opening loads it if it isn't yet.
	void load(core::dimension2du window);
	void toggle(core::dimension2du window);

	// The game's screen in a window this size: all of it while closed.
	core::dimension2du screen(core::dimension2du window) const;
	// Where that screen starts, from the window's bottom-left (IVideoDriver::setScreenOffset).
	core::vector2di offset(core::dimension2du window) const;
	// Once the frame is drawn, with the driver's screen the whole window.
	void draw(core::dimension2du window);
	// True when the event was the debugger's. Mouse positions are the window's.
	bool onEvent(const SEvent &event, core::dimension2du window);
	// What its Server tab shows of the server (shell/serverinfo.h), while in a world; null when not.
	void serverInfo(std::function<Json::Value()> describe);
	// What its World tab shows of the game's systems (inspectors.h)
	Inspectors &inspectors();

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace shell
