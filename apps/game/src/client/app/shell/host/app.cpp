// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/app.h"
#include "src/client/app/shell/host/pump.h"
#include "src/client/app/shell/host/scheme.h"
#include "src/client/app/shell/host/shell.h"

namespace shell {

static int s_argc = 0;
static char **s_argv = nullptr;

int runHelper(int argc, char *argv[])
{
	s_argc = argc;
	s_argv = argv;
	CefRefPtr<App> app = new App();
	return CefExecuteProcess(mainArgs(), app, nullptr);
}

CefMainArgs mainArgs()
{
#ifdef _WIN32
	// Windows' helpers read their switches from the process's own command line
	return CefMainArgs(GetModuleHandle(nullptr));
#else
	return CefMainArgs(s_argc, s_argv);
#endif
}

CefMessageRouterConfig routerConfig()
{
	CefMessageRouterConfig config;
	config.js_query_function = "terraQuery";
	config.js_cancel_function = "terraQueryCancel";
	return config;
}

void App::OnBeforeCommandLineProcessing(const CefString &process_type,
		CefRefPtr<CefCommandLine> command_line)
{
	if (!process_type.empty())
		return;
	// With the GPU, Chromium rasterises in its own GPU process and reads the
	// frame back for us; without it, blur and large animated layers are slow.
	if (!m_gpu) {
		command_line->AppendSwitch("disable-gpu");
		command_line->AppendSwitch("disable-gpu-compositing");
	}
	// Forked helpers return through our main(), which is built with stack
	// protection; a canary rotated after fork aborts them on exit.
	command_line->AppendSwitchWithValue("change-stack-guard-on-fork", "disable");
	// The page is the game's own UI, not a website: its menu music starts on
	// boot, not after the first click Chromium's autoplay policy waits for.
	command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
}

void App::OnScheduleMessagePumpWork(int64_t delay_ms)
{
	if (m_pump)
		m_pump->schedule(delay_ms);
}

void App::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
{
	registrar->AddCustomScheme(SCHEME, CEF_SCHEME_OPTION_STANDARD |
			CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_CORS_ENABLED |
			CEF_SCHEME_OPTION_FETCH_ENABLED);
}

void App::OnWebKitInitialized()
{
	m_router = CefMessageRouterRendererSide::Create(routerConfig());
}

void App::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context)
{
	m_router->OnContextCreated(browser, frame, context);
}

void App::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context)
{
	m_router->OnContextReleased(browser, frame, context);
}

bool App::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
	return m_router->OnProcessMessageReceived(browser, frame, source_process, message);
}

} // namespace shell
