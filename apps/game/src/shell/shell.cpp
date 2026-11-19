// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "shell.h"
#include <IEventReceiver.h>
#include "account.h"
#include "social.h"
#include "app.h"
#include "bridge.h"
#include "client.h"
#include "debugger.h"
#include "entering.h"
#include "ime.h"
#include "options.h"
#include "planetarium.h"
#include "porting.h"
#include "input.h"
#include "pixels.h"
#include "scheme.h"
#include "surface.h"
#include "exceptions.h"
#include "filesys.h"
#include "log.h"
#include "settings.h"
#include "client/keycode.h"
#include "trace/trace.h"
#include <IVideoDriver.h>
#include <json/json.h>
#include <cctype>
#include <vector>

shell::Shell *g_shell = nullptr;

namespace shell {

// One saved sign-in per backend: a key from the local stack means nothing to
// production, so the two never overwrite each other.
static std::string accountPath(const std::string &dir, const std::string &api)
{
	if (!fs::CreateAllDirs(dir))
		throw BaseException("shell: could not create " + dir);
	if (api == Settings::getLayer(SL_DEFAULTS)->get("arclabs_url"))
		return dir + DIR_DELIM "account.json";
	std::string host = api.substr(api.find("://") == std::string::npos ? 0 : api.find("://") + 3);
	for (char &c : host) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-')
			c = '_';
	}
	return dir + DIR_DELIM "account-" + host + ".json";
}

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
	std::function<void(video::IVideoDriver *)> underlayDraw;
	std::function<bool(const SEvent &)> underlayMouse;
	Opts opts;
	std::string item_dir;
	Viewport viewport;
	Pixels pixels;
	Surface surface;
	Input input;
	Ime ime;
	Options options;
	Bridge bridge;
	// Made once the page can be spoken to (they send as soon as they exist).
	// Social first: the account tells it who's signed in.
	std::unique_ptr<Social> social;
	std::unique_ptr<Account> account;
	CefRefPtr<CefMessageRouterBrowserSide> router;
	CefRefPtr<Client> client;
	CefRefPtr<CefBrowser> browser;
	std::unique_ptr<Planetarium> planetarium; // its verbs come off before the rest goes
	std::unique_ptr<Entering> entering;
	std::unique_ptr<Debugger> debugger;
	// The window, and the game's part of it (the driver's screen) beside the debugger.
	core::dimension2du window, screen;
	core::vector2di offset;

	// Page state, as far as the engine knows it. Reset on every navigation:
	// a reloaded page has to announce itself again before it is spoken to.
	bool ready = false;
	unsigned loads = 0; // pageLoads()
	bool keyboard = false;
	Mode mode = Mode::Menu;
	std::vector<Json::Value> pending;

	explicit Impl(const Opts &o) :
		opts(o),
		item_dir(o.cache_dir + "/items"),
		surface(o.driver),
		input(o.cursor, pixels, viewport),
		ime(o.guienv)
	{
		auto size = o.driver->getScreenSize();
		viewport = { size.Width, size.Height, o.scale };
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

// On macOS, CEF's functions exist only once its framework is loaded from the app bundle
// (CefScopedLibraryLoader), and the bundle isn't built yet: any CEF call would crash, so
// the shell refuses before making one. The engine runs without it (tests, --server).
static const Shell::Opts &wired(const Shell::Opts &opts)
{
#ifdef __APPLE__
	throw BaseException("shell: not wired on macOS yet (it needs the app bundle and CEF's loader)");
#endif
	return opts;
}

Shell::Shell(const Opts &opts) : m(std::make_unique<Impl>(wired(opts)))
{
	CefSettings settings;
	settings.windowless_rendering_enabled = true;
	settings.no_sandbox = true; // the suid chrome-sandbox helper is not installed
	settings.multi_threaded_message_loop = false; // pumped from frame()
	settings.log_severity = LOGSEVERITY_WARNING;
	CefString(&settings.root_cache_path) = opts.cache_dir;
	CefString(&settings.log_file) = opts.cache_dir + "/chromium.log";

	CefRefPtr<App> app = new App(opts.gpu);
	if (!CefInitialize(mainArgs(), settings, app, nullptr))
		throw BaseException("shell: Chromium failed to initialize (see " +
				opts.cache_dir + "/chromium.log)");

	CefRegisterSchemeHandlerFactory(SCHEME, UI_HOST, new UiScheme(opts.ui_dir));
	CefRegisterSchemeHandlerFactory(SCHEME, ITEM_HOST, new UiScheme(m->item_dir));

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
	m->account = std::make_unique<Account>(opts.arclabs, accountPath(opts.account_dir, opts.arclabs),
			[this](const Json::Value &message) {
				m->send(message);
				bool in = message["account"]["status"].asString() == "signedIn";
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
	m->bridge.on("account.signOut", [this](const Json::Value &) {
		m->account->signOut();
		return Json::Value();
	});
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
	m->planetarium = std::make_unique<Planetarium>(Planetarium::Opts{ *this });
	m->entering = std::make_unique<Entering>(Entering::Opts{ *this, opts.driver, opts.cursor });
	m->debugger = std::make_unique<Debugger>(Debugger::Opts{ opts.driver, opts.cursor, opts.url, opts.scale,
			[this](bool on) { m->ime.capture(on); } });
	m->router = CefMessageRouterBrowserSide::Create(routerConfig());
	m->router->AddHandler(&m->bridge, true);
	m->client = new Client(m->pixels, m->viewport, m->router, [this] { m->onNavigate(); });

	CefWindowInfo window;
	window.SetAsWindowless(0);
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;
	browser_settings.background_color = 0; // transparent: the game shows through

	m->browser = CefBrowserHost::CreateBrowserSync(window, m->client, opts.url,
			browser_settings, nullptr, nullptr);
	if (!m->browser)
		throw BaseException("shell: could not create the page browser");

	infostream << "shell: loading " << opts.url << std::endl;
	g_shell = this;
}

Shell::~Shell()
{
	g_shell = nullptr;
	m->planetarium.reset();
	m->debugger.reset(); // its page closes while Chromium is still up

	m->browser->GetHost()->CloseBrowser(true);
	while (!m->client->closed())
		CefDoMessageLoopWork();
	m->browser = nullptr;

	m->router->RemoveHandler(&m->bridge);
	m->router = nullptr;
	m->bridge.stop(); // no answers posted into a Chromium that's shutting down
	m->client = nullptr;

	CefShutdown();
}

bool Shell::debuggerOpen() const
{
	return m->debugger && m->debugger->open();
}

void Shell::frame()
{
	{
		TF_ZONE("UI");
		frameUi();
	}
	// every screen's frame ends here: menu, loading and game alike
	trace::frame();
}

void Shell::frameUi()
{
	{
		TF_ZONE("Chromium");
		CefDoMessageLoopWork();
	}
	m->account->step();
	m->social->step();

	// The device tells the driver when the window changes; any other size it
	// holds is the screen set here, the game's part of the window.
	video::IVideoDriver *driver = m->opts.driver;
	if (driver->getScreenSize() != m->screen)
		m->window = driver->getScreenSize();
	const core::dimension2du screen = m->debugger->screen(m->window);
	const core::vector2di offset = m->debugger->offset(m->window);
	if (screen != driver->getScreenSize())
		driver->OnResize(screen);
	if (offset != m->offset)
		driver->setScreenOffset(offset);
	m->screen = screen;
	m->offset = offset;

	auto size = driver->getScreenSize();
	if (size.Width != m->viewport.width || size.Height != m->viewport.height) {
		m->viewport.width = size.Width;
		m->viewport.height = size.Height;
		m->browser->GetHost()->WasResized();
	}

	if (m->underlayDraw)
		m->underlayDraw(m->opts.driver);
	m->surface.draw(m->pixels);

	if (m->debugger->open()) {
		TF_ZONE("Debugger");
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
			m->debugger->toggle();
		return true;
	}
	if (m->debugger->onEvent(event, m->window))
		return true;

	const bool took = m->input.handle(event, m->browser->GetHost(), m->keyboard);
	if (!m->underlayMouse || event.EventType != EET_MOUSE_INPUT_EVENT)
		return took;
	// (a drag carries on over the page's panels)
	const EMOUSE_INPUT_EVENT what = event.MouseInput.Event;
	if (took && what != EMIE_MOUSE_MOVED && what != EMIE_LMOUSE_LEFT_UP)
		return true;
	return m->underlayMouse(event) || took;
}

void Shell::setMode(Mode mode)
{
	m->mode = mode;
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

void Shell::underlay(std::function<void(video::IVideoDriver *)> draw, std::function<bool(const SEvent &)> mouse)
{
	m->underlayDraw = std::move(draw);
	m->underlayMouse = std::move(mouse);
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

float Shell::scale() const
{
	return m->viewport.scale;
}

const std::string &Shell::itemDir() const
{
	return m->item_dir;
}

} // namespace shell
