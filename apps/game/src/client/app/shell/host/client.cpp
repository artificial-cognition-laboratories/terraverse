// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/client.h"
#include "src/client/app/shell/host/canvas.h"
#include "src/client/app/shell/host/inbox.h"
#include "src/shared/platform/system/log.h"
#include "library/trace/trace.h"
#include <cmath>

namespace shell {

Client::Client(Canvas &canvas, const Viewport &viewport, Inbox &inbox,
		CefRefPtr<CefMessageRouterBrowserSide> router,
		std::function<void()> on_navigate) :
	m_canvas(canvas), m_viewport(viewport), m_inbox(inbox), m_router(router),
	m_on_navigate(std::move(on_navigate))
{
}

bool Client::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
	return m_router->OnProcessMessageReceived(browser, frame, source_process, message);
}

void Client::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
{
	// Device pixels: the page's scale is its zoom (Viewport), not a device scale factor
	rect = CefRect(0, 0, std::max(1, (int)m_viewport.width.load()), std::max(1, (int)m_viewport.height.load()));
}

bool Client::GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo &info)
{
	CefRect rect;
	GetViewRect(browser, rect);
	info.device_scale_factor = 1;
	info.rect = rect;
	info.available_rect = rect;
	return true;
}

void Client::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
		const RectList &dirty, const void *buffer, int width, int height)
{
	if (type != PET_VIEW)
		return; // <select> popups are not composited yet
	m_canvas.paint(buffer, width, height, dirty);
}

void Client::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
		const RectList &dirty, const CefAcceleratedPaintInfo &info)
{
	if (type != PET_VIEW)
		return; // <select> popups are not composited yet
	m_canvas.paint(info, dirty);
}

void Client::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	m_router->OnBeforeClose(browser);
	m_closed = true;
}

bool Client::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect)
{
	m_router->OnBeforeBrowse(browser, frame);
	if (frame->IsMain())
		m_inbox.post(m_on_navigate);
	return false;
}

void Client::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status,
		int error_code, const CefString &error_string)
{
	m_router->OnRenderProcessTerminated(browser);
	errorstream << "shell: page renderer died (" << error_code << ": "
			<< error_string.ToString() << "), reloading" << std::endl;
	m_inbox.post(m_on_navigate);
	browser->Reload();
}

bool Client::OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
		const CefString &message, const CefString &source, int line)
{
	std::ostream &out = level >= LOGSEVERITY_ERROR ? errorstream :
			level == LOGSEVERITY_WARNING ? warningstream : infostream;
	out << "shell: [page] " << message.ToString() << " (" << source.ToString()
			<< ":" << line << ")" << std::endl;
	return true;
}

void Client::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		ErrorCode code, const CefString &text, const CefString &url)
{
	if (code == ERR_ABORTED)
		return; // superseded by another navigation, not a failure
	errorstream << "shell: failed to load " << url.ToString() << ": "
			<< text.ToString() << " (" << code << ")" << std::endl;
}

} // namespace shell
