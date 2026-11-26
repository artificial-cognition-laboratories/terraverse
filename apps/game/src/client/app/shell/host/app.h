// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <include/cef_app.h>
#include <include/wrapper/cef_message_router.h>

namespace shell { class Pump; }

namespace shell {

// The page's JS entry points: window.terraQuery / window.terraQueryCancel.
CefMessageRouterConfig routerConfig();

/*
	Process-level Chromium hooks, shared by the game process and the helpers
	it spawns: command-line switches, the shell:// scheme, and the renderer
	half of the message router.
*/
class App : public CefApp, public CefBrowserProcessHandler, public CefRenderProcessHandler
{
public:
	// gpu: let Chromium rasterise on the GPU (the shell_gpu setting). Helper
	// processes take their switches from the browser process, so only the
	// game process's App decides.
	// pump: where Chromium asks for its loop's work on macOS (pump.h); null in helpers.
	explicit App(bool gpu = true, Pump *pump = nullptr) : m_gpu(gpu), m_pump(pump) {}

	void OnBeforeCommandLineProcessing(const CefString &process_type,
			CefRefPtr<CefCommandLine> command_line) override;
	void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
	CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
	CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

	void OnScheduleMessagePumpWork(int64_t delay_ms) override;

	void OnWebKitInitialized() override;
	void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefRefPtr<CefV8Context> context) override;
	void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefRefPtr<CefV8Context> context) override;
	bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

private:
	bool m_gpu;
	Pump *m_pump;
	CefRefPtr<CefMessageRouterRendererSide> m_router;

	IMPLEMENT_REFCOUNTING(App);
};

// argc/argv as main() received them; CefInitialize needs them again later.
CefMainArgs mainArgs();

} // namespace shell
