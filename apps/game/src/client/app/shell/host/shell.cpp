// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/render/gputime/gputime.h"
#include "src/client/app/shell/host/ui_scale.h"
#include "src/client/app/shell/host/shell.h"
#include "src/shared/platform/home/home.h"
#include "src/shared/platform/system/crash.h"
#include <IEventReceiver.h>
#include "src/client/app/shell/bridge/account.h"
#include "src/client/app/shell/bridge/profiles.h"
#include "src/client/app/shell/bridge/identity.h"
#include "src/client/app/shell/bridge/social.h"
#include "src/client/app/shell/host/app.h"
#include "src/client/app/shell/host/app_protocol.h"
#include "src/client/app/shell/host/pump.h"
#include "src/client/app/shell/host/framework.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/app/shell/host/client.h"
#include "src/client/app/shell/host/debugger.h"
#include "src/client/app/shell/bridge/entering.h"
#include "src/client/app/shell/host/ime.h"
#include "src/client/app/shell/bridge/options.h"
#include "src/client/views/scenes/kinds/planetarium.h"
#include "src/shared/platform/system/porting.h"
#include "src/client/app/shell/host/input.h"
#include "src/client/app/shell/host/scheme.h"
#include "src/client/app/shell/host/canvas.h"
#include "src/client/app/shell/host/latency.h"
#include "src/client/app/shell/host/ui_check.h"
#include "src/client/app/shell/host/inbox.h"
#include "src/client/app/shell/host/underlays.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/settings/settings.h"
#include "src/client/app/input/keycode.h"
#include "library/trace/trace.h"
#include <IVideoDriver.h>
#include <json/json.h>
#include <cctype>
#include <chrono>
#include <thread>
#include <vector>

shell::Shell *g_shell = nullptr;

namespace shell {

static const char *modeName(Mode mode)
{
	switch (mode) {
	case Mode::Menu: return "menu";
	case Mode::Entering: return "entering";
	case Mode::Game: return "game";
	case Mode::Leaving: return "leaving";
	}
	return "game";
}

struct Shell::Impl
{
	// first in, last out: on macOS no CEF call works without it
	Framework framework;
	Pump pump; // Chromium's loop, where the engine runs it (macOS)
	Underlays underlays;
	Opts opts;
	std::string item_dir;
	std::string contents_dir; // local mods' pictures, as terra drew them (bridge/local_mods.h)
	Viewport viewport;
	UiScale ui_scale;
	Inbox inbox; // before what posts to it
	// the page's frames (the GPU's if it can): before Input, which reads it;
	// let go before Chromium shuts down, after the page closes (~Shell)
	std::unique_ptr<Canvas> canvas;
	Input input;
	Latency latency;
	Ime ime;
	Options options;
	Bridge bridge;
	// Made once the page can be spoken to (they send as soon as they exist).
	// Social first: the account tells it who's signed in.
	std::unique_ptr<Social> social;
	std::unique_ptr<Account> account;
	std::unique_ptr<Profiles> profiles; // the accounts remembered here: switching, signing out
	std::unique_ptr<Identity> identity; // the account's own profile, edited
	CefRefPtr<CefMessageRouterBrowserSide> router;
	CefRefPtr<Client> client;
	CefRefPtr<CefBrowser> browser;
	std::unique_ptr<Planetarium> planetarium; // its verbs come off before the rest goes
	std::unique_ptr<Entering> entering;
	std::unique_ptr<Debugger> debugger;
	std::unique_ptr<UiCheck> check; // with --ui-check
	// The window, and the game's part of it (the driver's screen) beside the debugger.
	core::dimension2du window, screen;
	core::vector2di offset;

	// Page state, as far as the engine knows it. Reset on every navigation:
	// a reloaded page has to announce itself again before it is spoken to.
	bool ready = false;
	unsigned loads = 0; // pageLoads()
	bool keyboard = false;
	bool begun = false; // this frame's asked of Chromium (beginFrame)
	Mode mode = Mode::Menu;
	std::vector<Json::Value> pending;

	explicit Impl(const Opts &o) :
		opts(o),
		item_dir(o.cache_dir + "/items"),
		contents_dir(o.cache_dir + "/contents"),
		canvas(makeCanvas(o.driver, o.gpu, "the page")),
		input(o.cursor, *canvas, viewport),
		ui_scale({ /*density*/ o.scale, /*chosen*/ o.ui_scale }),
		ime(o.guienv),
		bridge(inbox)
	{
		auto size = o.driver->getScreenSize();
		viewport.width = size.Width;
		viewport.height = size.Height;
		viewport.scale = ui_scale.of(size.Width, size.Height);
		window = screen = size;
	}

	void sendMode()
	{
		Json::Value message;
		message["kind"] = "mode";
		message["mode"] = modeName(mode);
		bridge.send(browser, message);
	}

	void send(const Json::Value &message)
	{
		if (ready)
			bridge.send(browser, message);
		else
			pending.push_back(message);
	}

	void onReady()
	{
		ready = true;
		loads++;
		// The page's scale is its zoom (client.h: Viewport), the engine's alone: Chromium keeps one
		// per site (Ctrl+wheel set it, before the page took that), so it's set again on every load.
		browser->GetHost()->SetZoomLevel(UiScale::zoomLevel(viewport.scale));
		sendMode();
		for (const auto &message : pending)
			bridge.send(browser, message);
		pending.clear();
	}

	void onCapture(bool on)
	{
		keyboard = on;
		ime.capture(on);
	}

	void onNavigate()
	{
		ready = false;
		onCapture(false);
	}
};

Shell::Shell(const Opts &opts) : m(std::make_unique<Impl>(opts))
{
	CefSettings settings;
	settings.windowless_rendering_enabled = true;
	settings.no_sandbox = true; // the suid chrome-sandbox helper is not installed
	// Chromium on a thread of its own: however long its work, a frame never
	// waits on it (what reaches the engine comes through the Inbox)
	// (macOS has no such mode: its loop is pumped from the frames, pump.h)
	settings.multi_threaded_message_loop = !Pump::EXTERNAL;
	settings.external_message_pump = Pump::EXTERNAL;
	settings.log_severity = LOGSEVERITY_WARNING;
	CefString(&settings.root_cache_path) = opts.cache_dir;
	CefString(&settings.log_file) = opts.cache_dir + "/chromium.log";
	m->framework.place(settings);
	adoptAppProtocol();

	CefRefPtr<App> app = new App(opts.gpu, &m->pump);
	if (!CefInitialize(mainArgs(), settings, app, nullptr))
		throw BaseException("shell: Chromium failed to initialize (see " +
				opts.cache_dir + "/chromium.log)");
	// Chromium's browser takes SIGINT and SIGTERM for its own shutdown (its
	// CrShutdownDetector thread), so the engine's kill flag never saw a quit.
	// Set back once its UI thread is running: by then it's done installing.
	onChromium([] {
		porting::signal_handler_init();
		crash::again(); // its crash handlers too: ours report first
	});

	CefRegisterSchemeHandlerFactory(SCHEME, UI_HOST, new UiScheme(opts.ui_dir));
	CefRegisterSchemeHandlerFactory(SCHEME, ITEM_HOST, new UiScheme(m->item_dir));
	CefRegisterSchemeHandlerFactory(SCHEME, CONTENTS_HOST, new UiScheme(m->contents_dir));

	m->bridge.on("ready", [this](const Json::Value &) {
		m->onReady();
		return Json::Value();
	});
	m->bridge.on("capture", [this](const Json::Value &intent) {
		if (!intent["keyboard"].isBool())
			throw Refused("capture needs a boolean 'keyboard'");
		m->onCapture(intent["keyboard"].asBool());
		return Json::Value();
	});
	// The page's own cost (ui/src/bridge/pagePerf.ts), once a second: counters for the debugger.
	m->bridge.on("page.perf", [](const Json::Value &intent) {
		if (!intent["longestTask"].isNumeric() || !intent["busy"].isNumeric() || !intent["longestGap"].isNumeric())
			throw Refused("page.perf needs longestTask, busy and longestGap (ms)");
		trace::count("UI: page longest task", intent["longestTask"].asDouble() * 1000, trace::Fold::Max);
		trace::count("UI: page script", intent["busy"].asDouble() * 1000, trace::Fold::Sum);
		trace::count("UI: page frame gap", intent["longestGap"].asDouble() * 1000, trace::Fold::Max);
		return Json::Value();
	});
	// Available on every screen, menu or game.
	m->bridge.on("settings.get", [this](const Json::Value &) {
		return m->options.get();
	});
	m->bridge.on("settings.set", [this](const Json::Value &intent) {
		if (!intent["key"].isString())
			throw Refused("settings.set needs a string 'key'");
		m->options.set(intent["key"].asString(), intent["value"]);
		return Json::Value();
	});
	m->bridge.on("settings.apply", [this](const Json::Value &intent) {
		m->options.apply(intent["values"]);
		return Json::Value();
	});
	m->bridge.on("settings.bindings.get", [this](const Json::Value &) {
		return m->options.bindings();
	});
	m->bridge.on("settings.bindings.set", [this](const Json::Value &intent) {
		if (!intent["key"].isString() || !intent["scancode"].isInt() || !intent["replace"].isBool())
			throw Refused("settings.bindings.set needs key, scancode, and replace");
		m->options.bind(intent["key"].asString(), intent["scancode"].asInt(), intent["replace"].asBool());
		return Json::Value();
	});
	m->bridge.on("settings.bindings.reset", [this](const Json::Value &) {
		m->options.resetBindings();
		return Json::Value();
	});
	m->bridge.on("open", [](const Json::Value &intent) {
		std::string url = intent["url"].isString() ? intent["url"].asString() : "";
		if (url.rfind("https://", 0) != 0)
			throw Refused("only https links can be opened");
		if (!porting::open_url(url))
			throw BaseException("shell: the system could not open " + url);
		return Json::Value();
	});
	m->social = std::make_unique<Social>(opts.arclabs, [this](const Json::Value &message) { m->send(message); });
	m->account = std::make_unique<Account>(opts.arclabs, Profiles::keyPath(opts.home.profileDir(), opts.arclabs),
			[this, &home = opts.home](const Json::Value &message) {
				bool in = message["account"]["status"].asString() == "signedIn";
				// signed in: the account's own profile is the one running (home.md, "Profiles")
				if (in)
					m->account->keyAt(home.signedIn(m->account->username(), m->account->id(), m->account->keyPath()));
				m->send(message);
				m->social->signedIn(in ? m->account->key() : "");
			});
	m->bridge.on("account.get", [this](const Json::Value &) {
		return m->account->get();
	});
	m->bridge.on("account.signIn", [this](const Json::Value &) {
		m->account->signIn();
		return Json::Value();
	});
	m->bridge.on("account.cancel", [this](const Json::Value &) {
		m->account->cancel();
		return Json::Value();
	});
	m->profiles = std::make_unique<Profiles>(Profiles::Opts{ .home = opts.home, .account = *m->account, .api = opts.arclabs, .page = opts.url });
	m->bridge.on("account.signOut", [this](const Json::Value &) {
		m->profiles->signOut();
		return Json::Value();
	});
	m->bridge.on("account.leave", [this](const Json::Value &) {
		m->profiles->leave();
		return Json::Value();
	});
	m->bridge.on("profiles.list", [this](const Json::Value &) {
		return m->profiles->list();
	});
	m->bridge.on("profiles.use", [this](const Json::Value &intent) {
		m->profiles->use(intent["name"].isString() ? intent["name"].asString() : "");
		return Json::Value();
	});
	m->bridge.on("profiles.playAs", [this](const Json::Value &intent) {
		m->profiles->playAs(intent["name"].isString() ? intent["name"].asString() : "");
		return Json::Value();
	});
	m->bridge.on("profiles.forget", [this](const Json::Value &intent) {
		m->profiles->forget(intent["name"].isString() ? intent["name"].asString() : "");
		return Json::Value();
	});
	m->identity = std::make_unique<Identity>(Identity::Opts{ .bridge = m->bridge, .social = *m->social });
	m->bridge.on("account.retry", [this](const Json::Value &) {
		m->account->retry();
		return Json::Value();
	});
	m->bridge.on("social.get", [this](const Json::Value &) {
		return m->social->get();
	});
	m->bridge.on("social.open", [this](const Json::Value &intent) {
		if (!intent["open"].isBool())
			throw Refused("social.open needs a boolean 'open'");
		m->social->open(intent["open"].asBool());
		return Json::Value();
	});
	// The player's own actions wait on the backend: off the main thread.
	auto idOf = [](const Json::Value &intent) {
		if (!intent["id"].isString())
			throw Refused("needs a string 'id'");
		return intent["id"].asString();
	};
	m->bridge.onAsync("social.request", [this](const Json::Value &intent) {
		if (!intent["username"].isString())
			throw Refused("social.request needs a string 'username'");
		m->social->request(intent["username"].asString());
		return Json::Value();
	});
	m->bridge.onAsync("social.accept", [this, idOf](const Json::Value &intent) {
		m->social->accept(idOf(intent));
		return Json::Value();
	});
	m->bridge.onAsync("social.remove", [this, idOf](const Json::Value &intent) {
		m->social->remove(idOf(intent));
		return Json::Value();
	});
	m->bridge.onAsync("social.invite", [this, idOf](const Json::Value &intent) {
		m->social->invite(idOf(intent));
		return Json::Value();
	});
	m->bridge.onAsync("social.dismiss", [this, idOf](const Json::Value &intent) {
		m->social->dismiss(idOf(intent));
		return Json::Value();
	});
	m->planetarium = std::make_unique<Planetarium>(Planetarium::Opts{ *this, opts.driver });
	m->entering = std::make_unique<Entering>(Entering::Opts{ *this, opts.driver, opts.cursor });
	m->debugger = std::make_unique<Debugger>(Debugger::Opts{ opts.driver, opts.cursor, opts.url, opts.scale,
			[this](bool on) { m->ime.capture(on); }, m->inbox, m->pump, opts.gpu, opts.ui_dir });
	m->router = CefMessageRouterBrowserSide::Create(routerConfig());
	m->client = new Client(*m->canvas, m->viewport, m->inbox, m->router, [this] { m->onNavigate(); });

	CefWindowInfo window;
	window.SetAsWindowless(0);
	window.shared_texture_enabled = m->canvas->shared();
	// its frames when we ask (beginFrame), in step with ours, not on a timer of its own
	window.external_begin_frame_enabled = true;
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;
	browser_settings.background_color = 0; // transparent: the game shows through

	onChromium([&] {
		m->router->AddHandler(&m->bridge, true);
		m->browser = CefBrowserHost::CreateBrowserSync(window, m->client, opts.url,
				browser_settings, nullptr, nullptr);
	});
	if (!m->browser)
		throw BaseException("shell: could not create the page browser");

	if (opts.ui_check)
		m->check = std::make_unique<UiCheck>(UiCheck::Opts{
			[this](const SEvent &event) { return onEvent(event); },
			[this] { return m->ready; },
		});
	infostream << "shell: loading " << opts.url << std::endl;
	g_shell = this;
}

Shell::~Shell()
{
	g_shell = nullptr;
	m->planetarium.reset();
	m->debugger.reset(); // its page closes while Chromium is still up

	m->browser->GetHost()->CloseBrowser(true);
	while (!m->client->closed()) {
		m->pump.work();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	m->browser = nullptr;

	onChromium([&] { m->router->RemoveHandler(&m->bridge); });
	m->router = nullptr;
	m->bridge.stop(); // no answers posted into a Chromium that's shutting down
	m->client = nullptr;
	m->canvas.reset(); // (its GL context on Chromium's thread goes with it)

	CefShutdown();
}

bool Shell::debuggerOpen() const
{
	return m->debugger && m->debugger->open();
}

void Shell::beginFrame()
{
	if (m->begun)
		return;
	m->begun = true;
	// (its last frame still on the GPU: not yet, or it would paint over it)
	if (m->canvas->settled())
		m->browser->GetHost()->SendExternalBeginFrame();
	else // (its last paint still on the GPU: what the page's frame gaps are, if it isn't its script)
		trace::count("UI: page frames held [#]", 1, trace::Fold::Sum);
	m->debugger->beginFrame();
}

void Shell::frame()
{
	{
		TF_ZONE("UI: begin frame");
		beginFrame(); // (a loop that drew without pumping the window)
	}
	m->begun = false;
	{
		TF_ZONE("UI");
		frameUi();
	}
	// every screen's frame ends here: menu, loading and game alike
	{
		TF_ZONE("Trace: frame");
		trace::frame();
	}
}

void Shell::frameUi()
{
	{
		TF_ZONE("UI: Chromium"); // its own work, where it runs on this thread (macOS)
		m->pump.work();
	}
	{
		TF_ZONE("UI: inbox"); // the page's intents, run by the engine
		m->inbox.run();
	}
	m->account->step();
	m->social->step();

	// The device tells the driver when the window changes; any other size it
	// holds is the screen set here, the game's part of the window.
	video::IVideoDriver *driver = m->opts.driver;
	if (driver->getScreenSize() != m->screen)
		m->window = driver->getScreenSize();
	if (m->ready)
		m->debugger->load(m->window);
	const core::dimension2du screen = m->debugger->screen(m->window);
	const core::vector2di offset = m->debugger->offset(m->window);
	if (screen != driver->getScreenSize())
		driver->OnResize(screen);
	if (offset != m->offset)
		driver->setScreenOffset(offset);
	m->screen = screen;
	m->offset = offset;

	auto size = driver->getScreenSize();
	const float scale = m->ui_scale.of(size.Width, size.Height);
	if (size.Width != m->viewport.width || size.Height != m->viewport.height || scale != m->viewport.scale) {
		m->viewport.width = size.Width;
		m->viewport.height = size.Height;
		const bool zoomed = scale != m->viewport.scale;
		m->viewport.scale = scale;
		m->browser->GetHost()->WasResized();
		if (zoomed)
			m->browser->GetHost()->SetZoomLevel(UiScale::zoomLevel(scale));
	}

	m->underlays.draw(m->opts.driver);
	const Canvas::Painted painted = m->canvas->taken();
	m->latency.painted(painted.first, painted.paints, painted.area);
	{
		gpu::Zone gpu("UI page");
		if (m->canvas->draw({ 0, 0 }))
			m->latency.shown(trace::now());
	}
	m->latency.report();
	if (m->check)
		m->check->step(driver->getScreenSize());

	if (m->debugger->open()) {
		TF_ZONE("Debugger");
		gpu::Zone gpu("Debugger");
		// the whole window for it, then the game's part back for the next frame
		driver->setScreenOffset({ 0, 0 });
		driver->OnResize(m->window);
		m->debugger->draw(m->window);
		driver->OnResize(screen);
		driver->setScreenOffset(offset);
	}
}

bool Shell::onEvent(const SEvent &event)
{
	// The debugger's key works on every screen, typing or not, as a browser's devtools do.
	if (event.EventType == EET_KEY_INPUT_EVENT &&
			keySettingHasMatch("keymap_toggle_debug", KeyPress(event.KeyInput))) {
		if (event.KeyInput.PressedDown)
			m->debugger->toggle(m->window);
		return true;
	}
	if (m->debugger->onEvent(event, m->window))
		return true;

	// In the menu Esc is always the page's: there's no game to hand it to, and its screens close on it
	const bool escape = event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.Key == KEY_ESCAPE;
	const bool keys = m->keyboard || (m->mode == Mode::Menu && escape);
	const bool took = m->input.handle(event, m->browser->GetHost(), keys);
	if (took)
		m->latency.input(trace::now());
	if (m->underlays.empty() || event.EventType != EET_MOUSE_INPUT_EVENT)
		return took;
	// (a drag carries on over the page's panels)
	const EMOUSE_INPUT_EVENT what = event.MouseInput.Event;
	if (took && what != EMIE_MOUSE_MOVED && what != EMIE_LMOUSE_LEFT_UP)
		return true;
	return m->underlays.mouse(event) || took;
}

void Shell::setMode(Mode mode)
{
	m->mode = mode;
	// the world's scene goes as it's saved: its last frame stays behind the page's glass
	m->canvas->holdFrame(mode == Mode::Leaving);
	if (mode == Mode::Menu)
		m->social->leave();
	if (m->ready)
		m->sendMode();
}

unsigned Shell::pageLoads() const
{
	return m->loads;
}

void Shell::on(const std::string &kind, Verb verb)
{
	m->bridge.on(kind, std::move(verb));
}

Underlays &Shell::underlays()
{
	return m->underlays;
}

void Shell::drain()
{
	m->bridge.drain();
}

void Shell::onAsync(const std::string &kind, Verb verb)
{
	m->bridge.onAsync(kind, std::move(verb));
}

void Shell::off(const std::string &kind)
{
	m->bridge.off(kind);
}

void Shell::send(const Json::Value &message)
{
	m->send(message);
}

Social &Shell::social()
{
	return *m->social;
}

Account &Shell::account()
{
	return *m->account;
}

Planetarium &Shell::planetarium()
{
	return *m->planetarium;
}

Entering &Shell::entering()
{
	return *m->entering;
}

void Shell::serverInfo(std::function<Json::Value()> describe)
{
	m->debugger->serverInfo(std::move(describe));
}

Inspectors &Shell::inspectors()
{
	return m->debugger->inspectors();
}

void Shell::chooseUiScale(float chosen)
{
	m->ui_scale.choose(chosen);
}

float Shell::scale() const
{
	return m->viewport.scale;
}

const std::string &Shell::itemDir() const
{
	return m->item_dir;
}

const std::string &Shell::contentsDir() const
{
	return m->contents_dir;
}

} // namespace shell
