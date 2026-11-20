// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/input.h"
#include "src/client/app/shell/host/client.h"
#include "src/client/app/shell/host/canvas.h"
#include <IEventReceiver.h>
#include <ICursorControl.h>
#include <Keycodes.h>

namespace shell {

static const int BUTTON_FLAGS[] = {
	EVENTFLAG_LEFT_MOUSE_BUTTON,   // MBT_LEFT
	EVENTFLAG_MIDDLE_MOUSE_BUTTON, // MBT_MIDDLE
	EVENTFLAG_RIGHT_MOUSE_BUTTON,  // MBT_RIGHT
};

Input::Input(gui::ICursorControl *cursor, const Canvas &canvas, const Viewport &viewport) :
	m_cursor(cursor), m_canvas(canvas), m_viewport(viewport)
{
}

bool Input::handle(const SEvent &event, CefRefPtr<CefBrowserHost> host, bool keyboard)
{
	switch (event.EventType) {
	case EET_MOUSE_INPUT_EVENT:
		return mouse(event, host);
	case EET_KEY_INPUT_EVENT:
		return keyboard && key(event, host);
	case EET_STRING_INPUT_EVENT:
		return keyboard && text(event, host);
	default:
		return false;
	}
}

bool Input::mouse(const SEvent &event, CefRefPtr<CefBrowserHost> host)
{
	// A hidden cursor means the game has the mouse for looking around.
	if (!m_cursor->isVisible() && m_held < 0)
		return false;

	const auto &in = event.MouseInput;
	CefMouseEvent out;
	out.x = (int)(in.X / m_viewport.scale);
	out.y = (int)(in.Y / m_viewport.scale);
	out.modifiers = (in.Shift ? EVENTFLAG_SHIFT_DOWN : 0) |
			(in.Control ? EVENTFLAG_CONTROL_DOWN : 0) |
			(m_held >= 0 ? BUTTON_FLAGS[m_held] : 0);

	bool over = m_canvas.alphaAt(in.X, in.Y) > 0;

	cef_mouse_button_type_t button;
	switch (in.Event) {
	case EMIE_MOUSE_MOVED:
		if (over || m_held >= 0) {
			host->SendMouseMoveEvent(out, false);
		} else if (m_over) {
			host->SendMouseMoveEvent(out, true); // left the page: clear hover
		}
		m_over = over;
		return over || m_held >= 0;

	case EMIE_MOUSE_WHEEL:
		if (!over)
			return false;
		host->SendMouseWheelEvent(out, 0, (int)(in.Wheel * 120));
		return true;

	case EMIE_LMOUSE_PRESSED_DOWN: button = MBT_LEFT; goto press;
	case EMIE_MMOUSE_PRESSED_DOWN: button = MBT_MIDDLE; goto press;
	case EMIE_RMOUSE_PRESSED_DOWN: button = MBT_RIGHT; goto press;
	press:
		if (!over || m_held >= 0)
			return over;
		m_held = button;
		out.modifiers |= BUTTON_FLAGS[button];
		host->SetFocus(true);
		host->SendMouseClickEvent(out, button, false, 1);
		return true;

	case EMIE_LMOUSE_LEFT_UP: button = MBT_LEFT; goto release;
	case EMIE_MMOUSE_LEFT_UP: button = MBT_MIDDLE; goto release;
	case EMIE_RMOUSE_LEFT_UP: button = MBT_RIGHT; goto release;
	release:
		if (m_held != (int)button)
			return false;
		m_held = -1;
		host->SendMouseClickEvent(out, button, true, 1);
		return true;

	default:
		// Double/triple clicks follow their presses; whoever took the press keeps them.
		return over;
	}
}

bool Input::key(const SEvent &event, CefRefPtr<CefBrowserHost> host)
{
	const auto &in = event.KeyInput;
	CefKeyEvent out;
	out.type = in.PressedDown ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
	out.windows_key_code = in.Key; // Irrlicht key codes are Windows virtual keys
	out.native_key_code = in.SystemKeyCode;
	out.modifiers = (in.Shift ? EVENTFLAG_SHIFT_DOWN : 0) |
			(in.Control ? EVENTFLAG_CONTROL_DOWN : 0);
	host->SendKeyEvent(out);

	// Printable text arrives as string input; Enter has no text but must still type.
	if (in.PressedDown && in.Key == KEY_RETURN) {
		out.type = KEYEVENT_CHAR;
		out.character = '\r';
		out.windows_key_code = '\r';
		host->SendKeyEvent(out);
	}
	return true;
}

bool Input::text(const SEvent &event, CefRefPtr<CefBrowserHost> host)
{
	const core::stringw &str = *event.StringInput.Str;
	for (u32 i = 0; i < str.size(); i++) {
		wchar_t c = str[i];
		CefKeyEvent out;
		out.type = KEYEVENT_CHAR;
		out.character = (char16_t)c;
		out.unmodified_character = (char16_t)c;
		out.windows_key_code = c;
		host->SendKeyEvent(out);
	}
	return true;
}

} // namespace shell
