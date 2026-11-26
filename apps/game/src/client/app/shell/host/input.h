// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <include/cef_browser.h>

struct SEvent;
namespace gui { class ICursorControl; }

namespace shell {

class Canvas;
struct Viewport;

/*
	Decides which events belong to the page and translates them for Chromium.
	The mouse goes to the page where it has painted something opaque (or while
	a press that started there is held); the keyboard only while the page has
	asked for it. Everything else falls through to the engine.
*/
class Input
{
public:
	Input(gui::ICursorControl *cursor, const Canvas &canvas, const Viewport &viewport);

	bool handle(const SEvent &event, CefRefPtr<CefBrowserHost> host, bool keyboard);

private:
	bool mouse(const SEvent &event, CefRefPtr<CefBrowserHost> host);
	bool key(const SEvent &event, CefRefPtr<CefBrowserHost> host);
	bool text(const SEvent &event, CefRefPtr<CefBrowserHost> host);

	gui::ICursorControl *m_cursor;
	const Canvas &m_canvas;
	const Viewport &m_viewport;
	// The button held since a press landed on the page; the page owns the
	// mouse until it is released, so drags can leave its opaque area.
	int m_held = -1;
	bool m_over = false;
};

} // namespace shell
