// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/core/irrlichttypes.h"
#include <include/cef_client.h>
#include <include/wrapper/cef_message_router.h>
#include <atomic>
#include <functional>

namespace shell {

class Canvas;
class Inbox;

// The page's size in device pixels, and how many of them make one CSS pixel.
// Set on the main thread, read on Chromium's (GetViewRect).
struct Viewport
{
	std::atomic<u32> width{ 0 };
	std::atomic<u32> height{ 0 };
	std::atomic<float> scale{ 1.0f };
};

/*
	Chromium's view of the one browser: where it paints (a Canvas), how big
	it is (Viewport), and the router plumbing that carries bridge queries.
	Every callback runs on Chromium's UI thread: `on_navigate` is posted to
	the main thread's Inbox, in order with the page's intents.
*/
class Client : public CefClient, public CefRenderHandler,
		public CefLifeSpanHandler, public CefRequestHandler,
		public CefDisplayHandler, public CefLoadHandler
{
public:
	Client(Canvas &canvas, const Viewport &viewport, Inbox &inbox,
			CefRefPtr<CefMessageRouterBrowserSide> router,
			std::function<void()> on_navigate);

	bool closed() const { return m_closed; }

	CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
	CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
	CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
	CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
	bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

	// CefRenderHandler
	void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;
	bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo &info) override;
	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
			const RectList &dirty, const void *buffer, int width, int height) override;
	void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
			const RectList &dirty, const CefAcceleratedPaintInfo &info) override;

	// CefLifeSpanHandler
	void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	// CefRequestHandler
	bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) override;
	void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status,
			int error_code, const CefString &error_string) override;

	// CefDisplayHandler
	bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
			const CefString &message, const CefString &source, int line) override;

	// CefLoadHandler
	void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			ErrorCode code, const CefString &text, const CefString &url) override;

private:
	Canvas &m_canvas;
	const Viewport &m_viewport;
	Inbox &m_inbox;
	CefRefPtr<CefMessageRouterBrowserSide> m_router;
	std::function<void()> m_on_navigate;
	std::atomic<bool> m_closed{ false };

	IMPLEMENT_REFCOUNTING(Client);
};

} // namespace shell
