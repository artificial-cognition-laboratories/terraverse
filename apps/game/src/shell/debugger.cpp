// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "debugger.h"
#include "app.h"
#include "bridge.h"
#include "inbox.h"
#include "client.h"
#include "console.h"
#include "process.h"
#include "trace/marks.h"
#include "input.h"
#include "canvas.h"
#include "scheme.h"
#include <include/cef_request_context.h>
#include "exceptions.h"
#include "log.h"
#include "settings.h"
#include "filesys.h"
#include "porting.h"
#include "trace/trace.h"
#include "network/clientopcodes.h"
#include "network/serveropcodes.h"
#include <fstream>
#include <unordered_map>
#include <IEventReceiver.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace shell {

enum class Dock { Right, Bottom };

// Its sides, as the page and the settings name them.
static const struct { Dock dock; const char *name; } DOCKS[] = {
	{ Dock::Right, "right" },
	{ Dock::Bottom, "bottom" },
};

// CSS pixels: the least of the panel worth showing, and of the game beside it.
constexpr s32 MIN_PANEL = 200;
constexpr s32 MIN_GAME = 320;

static Dock dockNamed(const std::string &name)
{
	for (const auto &d : DOCKS) {
		if (name == d.name)
			return d.dock;
	}
	throw Refused("the debugger docks right or bottom, not '" + name + "'");
}

static const char *dockName(Dock dock)
{
	for (const auto &d : DOCKS) {
		if (d.dock == dock)
			return d.name;
	}
	throw BaseException("debugger: a dock without a name");
}

// The recorder counts in ns; the page in ms.
static double ms(u64 ns)
{
	return ns / 1e6;
}

// Every thread's zones in [from, to], whole, for the page to look through on
// its own: names once, and each thread's spans as one flat list, four numbers
// a span (name, depth, start ms, end ms). Threads with none are listed too, so
// a rail never comes and goes.
static Json::Value zonesOver(u64 from, u64 to)
{
	Json::Value out;
	Json::Value &names = out["names"] = Json::Value(Json::arrayValue);
	Json::Value &tracks = out["tracks"] = Json::Value(Json::arrayValue);
	std::unordered_map<u32, u32> index; // the recorder's name id to the page's
	for (const trace::Track &track : trace::between(from, to)) {
		Json::Value t;
		t["thread"] = track.thread;
		t["since"] = ms(track.since);
		Json::Value &spans = t["spans"] = Json::Value(Json::arrayValue);
		for (const trace::Span &span : track.spans) {
			auto [it, fresh] = index.try_emplace(span.name, (u32)index.size());
			if (fresh)
				names.append(trace::nameOf(span.name));
			spans.append(it->second);
			spans.append(span.depth);
			spans.append(ms(span.start));
			spans.append(ms(span.end));
		}
		tracks.append(std::move(t));
	}
	return out;
}

// The message types' names, by number, each way ("" where a number is unused).
static Json::Value messageNames()
{
	Json::Value out;
	Json::Value &in = out["in"] = Json::Value(Json::arrayValue);
	for (const ToClientCommandHandler &h : toClientCommandTable)
		in.append(h.name ? h.name : "");
	Json::Value &sent = out["out"] = Json::Value(Json::arrayValue);
	for (const ServerCommandFactory &f : serverCommandFactoryTable)
		sent.append(f.name ? f.name : "");
	return out;
}

// The client's messages in [from, to], for the page to look through: six
// numbers each (time ms, direction 0 in / 1 out, type, channel, reliable,
// bytes), and each incoming one's payload head as hex.
static Json::Value messagesOver(u64 from, u64 to)
{
	static const char HEX[] = "0123456789abcdef";
	Json::Value out;
	Json::Value &list = out["messages"] = Json::Value(Json::arrayValue);
	Json::Value &heads = out["heads"] = Json::Value(Json::arrayValue);
	for (const trace::net::Entry &e : trace::net::between(from, to)) {
		list.append(ms(e.time));
		list.append(e.dir == trace::net::Dir::In ? 0 : 1);
		list.append(e.command);
		list.append(e.channel);
		list.append(e.reliable ? 1 : 0);
		list.append(e.bytes);
		std::string hex;
		if (e.dir == trace::net::Dir::In) {
			const size_t kept = std::min<size_t>(e.bytes, trace::net::HEAD);
			for (size_t i = 0; i < kept; i++) {
				hex += HEX[e.head[i] >> 4];
				hex += HEX[e.head[i] & 15];
			}
		}
		heads.append(hex);
	}
	return out;
}

// Everything kept, as Chrome's trace format: opens in Perfetto and chrome://tracing.
static std::string exportTrace()
{
	const std::string dir = porting::path_user + DIR_DELIM "traces";
	if (!fs::CreateAllDirs(dir))
		throw BaseException("debugger: could not create " + dir);
	const std::string path = dir + DIR_DELIM "trace-" + std::to_string(std::time(nullptr)) + ".json";

	Json::Value events(Json::arrayValue);
	int tid = 0;
	for (const trace::Track &track : trace::between(0, trace::now())) {
		tid++;
		Json::Value meta;
		meta["ph"] = "M";
		meta["name"] = "thread_name";
		meta["pid"] = 1;
		meta["tid"] = tid;
		meta["args"]["name"] = track.thread;
		events.append(std::move(meta));
		for (const trace::Span &span : track.spans) {
			Json::Value e;
			e["ph"] = "X";
			e["name"] = trace::nameOf(span.name);
			e["pid"] = 1;
			e["tid"] = tid;
			e["ts"] = span.start / 1e3; // µs
			e["dur"] = (span.end - span.start) / 1e3;
			events.append(std::move(e));
		}
	}
	Json::Value root;
	root["traceEvents"] = std::move(events);
	std::ofstream file(path);
	if (!file)
		throw BaseException("debugger: could not write " + path);
	file << Json::FastWriter().write(root);
	return path;
}

// The game's page is index.html, or the dev server's root; the debugger's is beside it.
static std::string besides(const std::string &url)
{
	const std::string html = ".html";
	if (url.size() > html.size() && url.compare(url.size() - html.size(), html.size(), html) == 0)
		return url.substr(0, url.rfind('/') + 1) + "debugger.html";
	return url + (url.back() == '/' ? "" : "/") + "debugger.html";
}

struct Debugger::Impl
{
	Opts opts;
	Viewport viewport; // the panel's, for its page
	std::unique_ptr<Canvas> canvas; // before Input, which reads it
	Input input;
	Bridge bridge;
	Console console;
	u64 sent = 0; // the last log line the page has
	u64 frame = 0; // the last frame the page has
	u64 netEvent = 0; // the last connection event the page has
	u64 markSeq = 0; // the last startup mark the page has
	u64 server_at = 0; // when the server was last described
	Process process;
	std::function<Json::Value()> describeServer;
	u64 perf_at = 0; // when frames were last sent
	CefRefPtr<CefMessageRouterBrowserSide> router;
	CefRefPtr<Client> client;
	CefRefPtr<CefBrowser> browser;

	bool open = false;
	bool ready = false;
	bool keyboard = false; // its page has an editable element focused
	// Its edge is held: the mouse sizes it, in window pixels, until released.
	bool dragging = false;
	Dock dock;
	s32 size; // CSS pixels, as last asked for; the panel is this, kept in the window

	explicit Impl(const Opts &o) :
		opts(o),
		canvas(makeCanvas(o.driver, o.gpu, "the debugger")),
		input(o.cursor, *canvas, viewport),
		bridge(o.inbox),
		dock(dockNamed(g_settings->get("debugger_dock"))),
		size(g_settings->getS32("debugger_size"))
	{
		viewport.scale = o.scale;
	}

	s32 px(s32 css) const { return (s32)std::lround(css * opts.scale); }

	core::recti panel(core::dimension2du window) const
	{
		const s32 w = window.Width, h = window.Height;
		const s32 most = std::max(0, (dock == Dock::Right ? w : h) - px(MIN_GAME));
		const s32 across = std::clamp(px(size), std::min(px(MIN_PANEL), most), most);
		return dock == Dock::Right ? core::recti(w - across, 0, w, h) : core::recti(0, h - across, w, h);
	}

	void sendDock()
	{
		if (!ready)
			return; // it's told when it says it's ready
		Json::Value message;
		message["kind"] = "debugger.dock";
		message["side"] = dockName(dock);
		bridge.send(browser, message);
	}

	void save()
	{
		g_settings->set("debugger_dock", dockName(dock));
		g_settings->setS32("debugger_size", size);
	}

	void create()
	{
		bridge.on("ready", [this](const Json::Value &) {
			ready = true;
			sent = 0; // a fresh page has none of the log, the frames, or the events
			frame = 0;
			netEvent = 0;
			markSeq = 0;
			server_at = 0;
			sendDock();
			Json::Value names;
			names["kind"] = "net.names";
			names["names"] = messageNames();
			bridge.send(browser, names);
			return Json::Value();
		});
		// Both read the recorder under its own locks: answered off the main
		// thread, so a long range never stalls the game.
		bridge.onAsync("perf.capture", [](const Json::Value &intent) {
			if (!intent["from"].isNumeric() || !intent["to"].isNumeric())
				throw Refused("perf.capture needs numbers 'from' and 'to' (ms)");
			const double from = intent["from"].asDouble(), to = intent["to"].asDouble();
			if (from < 0 || to < from)
				throw Refused("perf.capture needs 0 <= from <= to");
			return zonesOver((u64)(from * 1e6), (u64)(to * 1e6));
		});
		bridge.onAsync("net.capture", [](const Json::Value &intent) {
			if (!intent["from"].isNumeric() || !intent["to"].isNumeric())
				throw Refused("net.capture needs numbers 'from' and 'to' (ms)");
			const double from = intent["from"].asDouble(), to = intent["to"].asDouble();
			if (from < 0 || to < from)
				throw Refused("net.capture needs 0 <= from <= to");
			return messagesOver((u64)(from * 1e6), (u64)(to * 1e6));
		});
		bridge.onAsync("perf.export", [](const Json::Value &) {
			Json::Value out;
			out["path"] = exportTrace();
			return out;
		});
		bridge.on("capture", [this](const Json::Value &intent) {
			if (!intent["keyboard"].isBool())
				throw Refused("capture needs a boolean 'keyboard'");
			capture(intent["keyboard"].asBool());
			return Json::Value();
		});
		bridge.on("debugger.dock", [this](const Json::Value &intent) {
			if (!intent["side"].isString())
				throw Refused("debugger.dock needs a string 'side'");
			dock = dockNamed(intent["side"].asString());
			save();
			sendDock();
			return Json::Value();
		});
		bridge.on("debugger.drag", [this](const Json::Value &) {
			dragging = true;
			return Json::Value();
		});
		bridge.on("debugger.close", [this](const Json::Value &) {
			hide();
			return Json::Value();
		});

		router = CefMessageRouterBrowserSide::Create(routerConfig());
		client = new Client(*canvas, viewport, opts.inbox, router, [this] {
			ready = false;
			capture(false);
		});

		CefWindowInfo window;
		window.SetAsWindowless(0);
		window.shared_texture_enabled = canvas->shared();
		window.external_begin_frame_enabled = true; // (Shell::beginFrame)
		CefBrowserSettings settings;
		settings.windowless_frame_rate = 60;
		settings.background_color = CefColorSetARGB(255, 12, 14, 16); // --stone-0: nothing of the game behind it
		const std::string url = besides(opts.url);
		onChromium([&] {
			router->AddHandler(&bridge, true);
			// A context of its own (in memory, nothing kept): Chromium then
			// can't put its page in the game page's renderer, where its charts
			// redrawing held the game page's frames up (a 50 ms gap a second)
			CefRefPtr<CefRequestContext> context = CefRequestContext::CreateContext(CefRequestContextSettings(), nullptr);
			context->RegisterSchemeHandlerFactory(SCHEME, UI_HOST, new UiScheme(opts.ui_dir));
			browser = CefBrowserHost::CreateBrowserSync(window, client, url, settings, nullptr, context);
		});
		if (!browser)
			throw BaseException("debugger: could not create its page");
		infostream << "debugger: loading " << url << std::endl;
	}

	void capture(bool on)
	{
		if (on == keyboard)
			return;
		keyboard = on;
		opts.capture(on);
	}

	// What's been logged since the page last heard, in one message.
	void sendLog()
	{
		if (!ready)
			return;
		u64 last;
		Json::Value lines = console.since(sent, last);
		if (lines.empty())
			return;
		sent = last;
		Json::Value message;
		message["kind"] = "debugger.log";
		message["lines"] = std::move(lines);
		bridge.send(browser, message);
	}

	// The frames ended since the page last heard, about 27 times a second:
	// often enough that the Live charts move smoothly, each message a frame or two.
	void sendPerf()
	{
		const u64 at = trace::now();
		if (!ready || at - perf_at < 37'000'000)
			return;
		perf_at = at;
		const std::vector<trace::Frame> frames = trace::framesSince(frame);
		if (frames.empty())
			return;
		Json::Value list(Json::arrayValue);
		Json::Value units(Json::objectValue);
		for (const trace::Frame &f : frames) {
			Json::Value out;
			out["id"] = (Json::UInt64)f.id;
			out["start"] = ms(f.start);
			out["end"] = ms(f.end);
			Json::Value &threads = out["threads"] = Json::Value(Json::objectValue);
			for (const trace::Frame::Thread &thread : f.threads) {
				Json::Value &parts = threads[thread.thread] = Json::Value(Json::objectValue);
				for (const auto &[name, took] : thread.parts)
					parts[trace::nameOf(name)] = ms(took);
			}
			// what the client sent and received: five numbers a type (type, 0 in / 1 out, count, bytes, largest)
			Json::Value &net = out["net"] = Json::Value(Json::arrayValue);
			for (const trace::net::Total &t : f.net) {
				net.append(t.command);
				net.append(t.dir == trace::net::Dir::In ? 0 : 1);
				net.append(t.count);
				net.append((Json::UInt64)t.bytes);
				net.append(t.largest);
			}
			Json::Value &counters = out["counters"] = Json::Value(Json::objectValue);
			for (const auto &[id, value] : f.counters) {
				const std::string &name = trace::nameOf(id);
				counters[name] = value;
				if (!units.isMember(name))
					units[name] = trace::unitOf(id);
			}
			list.append(std::move(out));
		}
		frame = frames.back().id;
		Json::Value message;
		message["kind"] = "perf.frames";
		message["frames"] = std::move(list);
		message["units"] = std::move(units);
		bridge.send(browser, message);

		const std::vector<trace::net::Event> events = trace::net::eventsSince(netEvent);
		if (!events.empty()) {
			Json::Value said;
			said["kind"] = "net.events";
			Json::Value &list = said["events"] = Json::Value(Json::arrayValue);
			for (const trace::net::Event &e : events) {
				Json::Value one;
				one["time"] = ms(e.time);
				one["what"] = e.what;
				one["command"] = e.command;
				list.append(std::move(one));
			}
			netEvent = events.back().seq;
			bridge.send(browser, said);
		}
	}

	// Once a second: the server (local, or what's known of a remote one), the
	// process, and any startup marks the page hasn't had.
	void sendServer()
	{
		const u64 at = trace::now();
		if (!ready || (server_at && at - server_at < 1'000'000'000))
			return;
		server_at = at;
		Json::Value message;
		message["kind"] = "server.summary";
		message["server"] = describeServer ? describeServer() : Json::Value();
		message["process"] = process.sample();
		bridge.send(browser, message);

		const std::vector<trace::Mark> marks = trace::marksSince(markSeq);
		if (marks.empty())
			return;
		Json::Value said;
		said["kind"] = "server.marks";
		Json::Value &list = said["marks"] = Json::Value(Json::arrayValue);
		for (const trace::Mark &m : marks) {
			Json::Value one;
			one["lane"] = m.lane;
			one["what"] = m.what;
			one["start"] = ms(m.start);
			one["end"] = ms(m.end);
			list.append(std::move(one));
		}
		markSeq = marks.back().seq;
		bridge.send(browser, said);
	}

	void hide()
	{
		open = false;
		dragging = false;
		capture(false); // the game has its keys back
		browser->GetHost()->WasHidden(true);
	}
};

Debugger::Debugger(const Opts &opts) : m(std::make_unique<Impl>(opts))
{
}

Debugger::~Debugger()
{
	if (!m->browser)
		return;
	m->browser->GetHost()->CloseBrowser(true);
	while (!m->client->closed())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	m->browser = nullptr;
	onChromium([&] { m->router->RemoveHandler(&m->bridge); });
	m->router = nullptr;
	m->bridge.stop();
	m->client = nullptr;
	m->canvas.reset(); // while Chromium's up: its GL context goes on Chromium's thread
}

void Debugger::serverInfo(std::function<Json::Value()> describe)
{
	m->describeServer = std::move(describe);
}

bool Debugger::open() const
{
	return m->open;
}

void Debugger::beginFrame()
{
	if (m->open && m->browser && m->canvas->settled())
		m->browser->GetHost()->SendExternalBeginFrame();
}

void Debugger::toggle()
{
	if (m->open) {
		m->hide();
		return;
	}
	m->open = true;
	if (!m->browser)
		m->create();
	else
		m->browser->GetHost()->WasHidden(false);
}

core::dimension2du Debugger::screen(core::dimension2du window) const
{
	if (!m->open)
		return window;
	const core::recti panel = m->panel(window);
	return m->dock == Dock::Right ?
			core::dimension2du(panel.UpperLeftCorner.X, window.Height) :
			core::dimension2du(window.Width, panel.UpperLeftCorner.Y);
}

core::vector2di Debugger::offset(core::dimension2du window) const
{
	// GL counts from the bottom: the game sits above a bottom panel, beside a right one
	if (!m->open || m->dock == Dock::Right)
		return { 0, 0 };
	return { 0, m->panel(window).getHeight() };
}

void Debugger::draw(core::dimension2du window)
{
	const core::recti panel = m->panel(window);
	if ((s32)m->viewport.width != panel.getWidth() || (s32)m->viewport.height != panel.getHeight()) {
		m->viewport.width = panel.getWidth();
		m->viewport.height = panel.getHeight();
		m->browser->GetHost()->WasResized();
	}
	m->sendLog();
	m->sendPerf();
	m->sendServer();
	// what its page hasn't painted yet (opening, or growing) is its own ground
	m->opts.driver->draw2DRectangle(video::SColor(255, 12, 14, 16), panel);
	m->canvas->draw(panel.UpperLeftCorner);
}

bool Debugger::onEvent(const SEvent &event, core::dimension2du window)
{
	if (!m->open)
		return false;
	if (event.EventType != EET_MOUSE_INPUT_EVENT)
		return m->input.handle(event, m->browser->GetHost(), m->keyboard);

	const auto &in = event.MouseInput;
	if (m->dragging && in.Event == EMIE_MOUSE_MOVED) {
		const s32 across = m->dock == Dock::Right ? (s32)window.Width - in.X : (s32)window.Height - in.Y;
		m->size = (s32)std::lround(across / m->opts.scale);
		return true;
	}
	if (m->dragging && in.Event == EMIE_LMOUSE_LEFT_UP) {
		m->dragging = false;
		// kept as the window allows: a later, larger window gives it back what was asked
		m->size = std::max(MIN_PANEL, m->size);
		m->save();
	}

	// the page counts from the panel's corner
	const core::recti panel = m->panel(window);
	SEvent local = event;
	local.MouseInput.X -= panel.UpperLeftCorner.X;
	local.MouseInput.Y -= panel.UpperLeftCorner.Y;
	return m->input.handle(local, m->browser->GetHost(), m->keyboard);
}

} // namespace shell
