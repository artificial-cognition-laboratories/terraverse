// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium.h"
#include "bridge.h"
#include "shell.h"
#include "terragen/body.h"
#include "util/base64.h"
#include <IVideoDriver.h>
#include <cmath>
#include <cstdio>

namespace shell {

using planetarium::MarkerLayer;
using View = planetarium::Planetarium;

static const char *const VERBS[] = { "planetarium.show", "planetarium.hide", "planetarium.focus", "planetarium.markers",
	"planetarium.viewport", "planetarium.fly", "planetarium.frame", "planetarium.layer", "planetarium.time", "planetarium.seams", "planetarium.scene" };

// a finite number field, or none if it's absent; Refused if it's anything else
static std::optional<double> number(const Json::Value &v, const char *name)
{
	if (!v.isMember(name))
		return std::nullopt;
	const Json::Value &f = v[name];
	if (!f.isNumeric() || !std::isfinite(f.asDouble()))
		throw Refused(std::string("planetarium: '") + name + "' must be a number");
	return f.asDouble();
}

static double needed(const Json::Value &v, const char *name)
{
	const std::optional<double> n = number(v, name);
	if (!n)
		throw Refused(std::string("planetarium: needs '") + name + "'");
	return *n;
}

static std::vector<MarkerLayer::Marker> markersOf(const Json::Value &list)
{
	if (!list.isArray())
		throw Refused("planetarium: markers must be a list");
	std::vector<MarkerLayer::Marker> out;
	for (const Json::Value &m : list) {
		if (!m.isObject() || !m["colour"].isString())
			throw Refused("planetarium: a marker is { latitude, longitude, colour: \"#rrggbb\" }");
		unsigned r, g, b;
		const std::string colour = m["colour"].asString();
		if (colour.size() != 7 || std::sscanf(colour.c_str(), "#%2x%2x%2x", &r, &g, &b) != 3)
			throw Refused("planetarium: a marker's colour is \"#rrggbb\", not '" + colour + "'");
		out.push_back({ needed(m, "latitude"), needed(m, "longitude"), video::SColor(255, r, g, b),
				m.isMember("altitude") ? needed(m, "altitude") : 0.0 });
	}
	return out;
}

Planetarium::Planetarium(Opts opts) : m_opts(std::move(opts))
{
	m_opts.shell.on("planetarium.show", [this](const Json::Value &intent) { return show(intent); });
	m_opts.shell.on("planetarium.hide", [this](const Json::Value &) {
		hide();
		return Json::Value();
	});
	m_opts.shell.on("planetarium.focus", [this](const Json::Value &intent) { return focus(intent); });
	m_opts.shell.on("planetarium.markers", [this](const Json::Value &intent) { return markers(intent); });
	m_opts.shell.on("planetarium.viewport", [this](const Json::Value &intent) { return viewport(intent); });
	m_opts.shell.on("planetarium.fly", [this](const Json::Value &intent) { return fly(intent); });
	m_opts.shell.on("planetarium.frame", [this](const Json::Value &intent) { return frame(intent); });
	m_opts.shell.on("planetarium.layer", [this](const Json::Value &intent) { return layer(intent); });
	m_opts.shell.on("planetarium.time", [this](const Json::Value &intent) { return time(intent); });
	m_opts.shell.on("planetarium.seams", [this](const Json::Value &intent) { return seams(intent); });
	m_opts.shell.on("planetarium.scene", [this](const Json::Value &intent) { return scene(intent); });
}

Planetarium::~Planetarium()
{
	for (const char *verb : VERBS)
		m_opts.shell.off(verb);
}

void Planetarium::serve(Body body, std::optional<Json::Value> backdrop)
{
	if (m_making.valid())
		m_making.wait(); // its work may use what the last server owns
	m_making = {};
	m_wanted.reset();
	m_makingWanted = false;
	m_afterMade.clear();
	m_backdrop.reset();
	m_scene = Scene{}; // (whoever's served next: the game has its own view)
	hide();
	m_markers.clear();
	m_view.reset();
	m_insets = Insets{};
	m_parked.reset(); // the last window's bodies
	m_looked = false;
	m_body = std::move(body);
	m_backdrop = std::move(backdrop);
	if (m_backdrop)
		ask(*m_backdrop);
}

void Planetarium::ask(const Json::Value &body)
{
	if (!m_body)
		throw Refused("planetarium.show: no planet can be shown here now");
	if (shown() && body == m_asked)
		return; // that one's on screen (or coming): kept, not made again
	m_wanted = m_body(body);
	m_makingWanted = false; // whatever's on the worker now is out of date
	m_asked = body;
	start();
}

void Planetarium::start()
{
	if (m_making.valid() || !m_wanted)
		return;
	m_making = std::async(std::launch::async, *m_wanted);
	m_makingWanted = true;
}

void Planetarium::collect()
{
	if (!m_making.valid() || m_making.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return;
	const bool wanted = m_makingWanted;
	m_makingWanted = false;
	try {
		planetarium::Planetarium::Opts made = m_making.get();
		if (!wanted) {
			start(); // overtaken: the latest instead
			return;
		}
		m_wanted.reset();
		show(std::move(made));
	} catch (const Refused &e) {
		if (!wanted) {
			start();
			return;
		}
		// asked for from the page, which has moved on: the view stays as it is
		infostream << "shell: planetarium.show refused: " << e.what() << std::endl;
		m_wanted.reset();
		m_afterMade.clear();
		m_asked = Json::Value();
		return;
	}
	std::vector<std::function<void()>> after = std::move(m_afterMade);
	m_afterMade.clear();
	for (const auto &ask : after) {
		try {
			ask();
		} catch (const Refused &e) {
			infostream << "shell: planetarium: " << e.what() << std::endl;
		}
	}
}

void Planetarium::show(View::Opts body)
{
	if (m_parked && !shown() && m_parked->body() == body.body && m_parked->code() == body.horizon.code) {
		// the one hidden last (or prepared): back as it was, its land already built
		m_current = std::move(m_parked);
		m_current->markers(m_markers);
		m_current->layer(m_layer);
		apply(*m_current);
		return;
	}
	auto next = std::make_unique<View>(std::move(body));
	if (const View *was = m_current ? m_current.get() : m_next.get())
		next->keep(*was); // the same view of the new body
	next->markers(m_markers);
	next->layer(m_layer);
	apply(*next);
	if (m_scene.shown)
		compose(*next, 0); // (the backdrop made again, under it)
	m_next = std::move(next);
}

void Planetarium::apply(View &p) const
{
	p.seams(m_seams);
	p.orbits(!m_scene.shown);
	p.day(m_clock.date * p.system().year());
}

void Planetarium::tick(double seconds)
{
	collect();
	const double step = seconds / m_scene.seconds;
	m_scene.blend = std::clamp(m_scene.blend + (m_scene.shown ? step : -step), 0.0, 1.0);
	if (m_clock.running) {
		m_clock.date += seconds * m_clock.rate;
		m_clock.date -= std::floor(m_clock.date); // round the year again
	}
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			apply(*p);
	// the page's date follows, a few times a second while it runs
	m_clock.reported += seconds;
	if (m_clock.running && m_clock.reported > 0.25) {
		m_clock.reported = 0;
		Json::Value message;
		message["kind"] = "planetarium";
		message["event"] = "time";
		message["date"] = m_clock.date;
		message["running"] = true;
		m_opts.shell.send(message);
	}
}

Json::Value Planetarium::time(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.time takes { running?, date? } (date: 0..1 of the year)");
	if (intent.isMember("running")) {
		if (!intent["running"].isBool())
			throw Refused("planetarium.time: 'running' is true or false");
		m_clock.running = intent["running"].asBool();
	}
	if (const std::optional<double> date = number(intent, "date")) {
		if (*date < 0 || *date > 1)
			throw Refused("planetarium.time: 'date' is 0 to 1, through the year");
		m_clock.date = *date;
	}
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			apply(*p);
	return Json::Value();
}

void Planetarium::compose(planetarium::Planetarium &p, double seconds)
{
	m_scene.aspect = (float)m_screen.X / std::max(1u, m_screen.Y);
	p.compose(planetarium::HOME_SHOT, m_scene.aspect, seconds);
}

void Planetarium::opening()
{
	m_scene.shown = true;
	m_scene.blend = 1;
	m_clock.date = planetarium::HOME_SHOT.date;
	m_clock.running = false;
	for (View *p : { m_current.get(), m_next.get() })
		if (p) {
			apply(*p);
			compose(*p, 0);
		}
}

Json::Value Planetarium::scene(const Json::Value &intent)
{
	if (!intent.isObject() || !intent["shown"].isBool())
		throw Refused("planetarium.scene takes { shown, seconds }");
	const double seconds = needed(intent, "seconds");
	if (seconds <= 0 || seconds > 60)
		throw Refused("planetarium.scene: it takes up to 60 seconds");
	if (m_scene.shown && intent["shown"].asBool())
		return Json::Value(); // there already (the menu opened on it)
	m_scene.shown = intent["shown"].asBool();
	m_scene.seconds = seconds;
	if (m_scene.shown) {
		m_clock.date = planetarium::HOME_SHOT.date;
		m_clock.running = false;
	}
	for (View *p : { m_current.get(), m_next.get() })
		if (p) {
			apply(*p);
			if (m_scene.shown)
				compose(*p, seconds);
		}
	return Json::Value();
}

Json::Value Planetarium::seams(const Json::Value &intent)
{
	if (!intent.isObject() || !intent["shown"].isBool())
		throw Refused("planetarium.seams takes { shown }");
	m_seams = intent["shown"].asBool();
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->seams(m_seams);
	return Json::Value();
}

void Planetarium::dayIn(double worldDays, int32_t worldYear)
{
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->day(p->system().dayFor(worldDays, worldYear), p->system().clock(p->tiled(), worldDays, worldYear).hour(worldDays - std::floor(worldDays)));
}

void Planetarium::path(std::optional<planetarium::Planetarium::Path> path)
{
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->path(path);
}

void Planetarium::hide()
{
	if (m_backdrop) {
		// back to the universe behind the menu, over the whole window
		m_view.reset();
		m_insets = Insets{};
		m_markers.clear();
		m_layer.reset();
		for (View *p : { m_current.get(), m_next.get() })
			if (p)
				p->layer(std::nullopt);
		ask(*m_backdrop);
		return;
	}
	m_wanted.reset();
	m_makingWanted = false;
	m_afterMade.clear();
	if (std::unique_ptr<View> &shown = m_current ? m_current : m_next) {
		m_parked = std::move(shown);
		m_looked = true;
	}
	m_current.reset();
	m_next.reset();
	m_asked = Json::Value();
}

void Planetarium::prepare(View::Opts body)
{
	if (shown() || m_parked)
		return;
	m_parked = std::make_unique<View>(std::move(body));
	apply(*m_parked);
}

void Planetarium::idle(std::optional<std::pair<double, double>> player)
{
	if (!m_parked || shown())
		return;
	// moved far enough to see other tiles (a hundredth of a radian: tens of
	// blocks): over them now, not on every step
	// (once looked through, it stays where it was left)
	if (player && !m_looked && (!m_over || std::abs(player->first - m_over->first) > 0.01 ||
			std::abs(std::remainder(player->second - m_over->second, 2 * M_PI)) > 0.01)) {
		m_parked->focus(player->first, player->second, std::nullopt);
		m_over = player;
	}
	if (!m_parked->settled())
		m_parked->update(m_screen);
}

Json::Value Planetarium::show(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.show takes an object");
	if (intent.isMember("markers"))
		m_markers = markersOf(intent["markers"]);
	if (intent.isMember("viewport"))
		viewport(intent["viewport"]);
	ask(intent["body"]);
	if (intent.isMember("focus"))
		focus(intent["focus"]);
	return Json::Value();
}

Json::Value Planetarium::focus(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.focus takes { latitude, longitude, altitude? } or { body }");
	if (waiting()) {
		m_afterMade.push_back([this, intent] { focus(intent); });
		return Json::Value();
	}
	if (intent.isMember("body")) {
		// another body of the system, by its id
		if (!intent["body"].isString())
			throw Refused("planetarium.focus: 'body' is a body's id");
		for (View *p : { m_current.get(), m_next.get() })
			if (p)
				p->focusBody(bodyIndex(*p, intent["body"]));
		return Json::Value();
	}
	const double lat = needed(intent, "latitude"), lon = needed(intent, "longitude");
	const std::optional<double> altitude = number(intent, "altitude");
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->focus(lat, lon, altitude);
	return Json::Value();
}

size_t Planetarium::bodyIndex(const View &p, const Json::Value &id) const
{
	if (!id.isString())
		throw Refused("planetarium: a body is named by its id");
	if (id.asString() == "star")
		return p.star();
	const auto &bodies = p.system().spec().bodies;
	for (size_t i = 0; i < bodies.size(); i++)
		if (bodies[i].id == id.asString())
			return i;
	throw Refused("planetarium: no body '" + id.asString() + "' in this system");
}

Json::Value Planetarium::fly(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.fly takes { body?, latitude?, longitude?, altitude? or radii?, seconds }");
	if (waiting()) {
		m_afterMade.push_back([this, intent] { fly(intent); });
		return Json::Value();
	}
	const double seconds = needed(intent, "seconds");
	if (seconds < 0 || seconds > 60)
		throw Refused("planetarium.fly: a flight takes 0 to 60 seconds");
	const std::optional<double> lat = number(intent, "latitude"), lon = number(intent, "longitude"),
			altitude = number(intent, "altitude"), radii = number(intent, "radii");
	if (altitude && radii)
		throw Refused("planetarium.fly: an altitude or radii, not both");
	if (lat.has_value() != lon.has_value())
		throw Refused("planetarium.fly: a place is a latitude and a longitude");
	for (View *p : { m_current.get(), m_next.get() })
		if (p) {
			const size_t body = intent.isMember("body") ? bodyIndex(*p, intent["body"]) : p->target();
			p->fly({ body, lat, lon, altitude, radii, seconds });
		}
	return Json::Value();
}

Json::Value Planetarium::layer(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.layer takes { overlay, date?, explored? }, { width, height, rgba } (the weather: base64 RGBA, the preview's layout), { inside: true } or { none: true }");
	const bool inside = intent.isMember("inside");
	if (inside && !(intent["inside"].isBool() && intent["inside"].asBool()))
		throw Refused("planetarium.layer: 'inside' is true or absent");
	std::optional<View::Layer> layer;
	if (!inside && !intent.isMember("none") && intent.isMember("rgba")) {
		const double w = needed(intent, "width"), h = needed(intent, "height");
		if (w < 1 || h < 1 || w > 4096 || h > 4096 || w != std::floor(w) || h != std::floor(h))
			throw Refused("planetarium.layer: a size is 1 to 4096 whole pixels");
		if (!intent["rgba"].isString() || !base64_is_valid(intent["rgba"].asString()))
			throw Refused("planetarium.layer: 'rgba' is base64");
		const std::string bytes = base64_decode(intent["rgba"].asString());
		if (bytes.size() != (size_t)w * h * 4)
			throw Refused("planetarium.layer: " + std::to_string(bytes.size()) + " bytes for a " + std::to_string((int)w) + "×" +
					std::to_string((int)h) + " image");
		layer = View::Layer{ planetarium::Overlay::Biomes,
			std::make_shared<const planetarium::Painter::Opts::Image>(planetarium::Painter::Opts::Image{ (u32)w, (u32)h, std::vector<u8>(bytes.begin(), bytes.end()) }) };
	} else if (!inside && !intent.isMember("none")) {
		if (!intent["overlay"].isString())
			throw Refused("planetarium.layer: 'overlay' is an overlay's name");
		const std::optional<planetarium::Overlay> overlay = planetarium::overlayNamed(intent["overlay"].asString());
		if (!overlay)
			throw Refused("planetarium.layer: no overlay '" + intent["overlay"].asString() + "' is painted on the land");
		layer = View::Layer{ *overlay, nullptr, number(intent, "date"), std::nullopt, 0 };
		if (layer->date && (*layer->date < 0 || *layer->date >= 1))
			throw Refused("planetarium.layer: 'date' is 0 to 1, through the year");
		if (intent.isMember("explored")) {
			// survival: what the player's seen (tf_player's mask, a bit a cell)
			const Json::Value &e = intent["explored"];
			if (!e.isObject() || !e["grid"].isUInt() || e["grid"].asUInt() < 1 || e["grid"].asUInt() > 4096 ||
					!e["mask"].isString() || !base64_is_valid(e["mask"].asString()))
				throw Refused("planetarium.layer: 'explored' is { grid, mask } (base64, a bit a cell)");
			const std::string bytes = base64_decode(e["mask"].asString());
			const u32 grid = e["grid"].asUInt();
			if (bytes.size() * 8 != (size_t)grid * grid)
				throw Refused("planetarium.layer: an explored mask of " + std::to_string(bytes.size()) + " bytes isn't a " +
						std::to_string(grid) + "² grid");
			layer->explored = std::vector<u8>(bytes.begin(), bytes.end());
			layer->grid = grid;
		}
	}
	m_layer = layer;
	for (View *p : { m_current.get(), m_next.get() })
		if (p) {
			p->layer(layer);
			// cut open: the body the camera's round (or going to)
			p->cutaway(inside && p->heading() < p->star() ? std::optional<size_t>(p->heading()) : std::nullopt);
		}
	return Json::Value();
}

Json::Value Planetarium::frame(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.frame takes { left?, right?, top?, bottom? } (page pixels the page covers)");
	Insets in;
	for (auto [name, field] : { std::pair{ "left", &in.left }, { "right", &in.right }, { "top", &in.top }, { "bottom", &in.bottom } }) {
		const std::optional<double> v = number(intent, name);
		if (v && *v < 0)
			throw Refused(std::string("planetarium.frame: '") + name + "' can't be negative");
		*field = v.value_or(0); // an edge not given: the page covers none of it
	}
	m_insets = in;
	return Json::Value();
}

Json::Value Planetarium::markers(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.markers takes { markers }");
	m_markers = markersOf(intent["markers"]);
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->markers(m_markers);
	return Json::Value();
}

Json::Value Planetarium::viewport(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.viewport takes { x, y, width, height } (page pixels), or { whole: true }");
	if (intent.isMember("whole")) {
		if (!intent["whole"].isBool() || !intent["whole"].asBool())
			throw Refused("planetarium.viewport: 'whole' is true or absent");
		m_view.reset(); // the whole window
		return Json::Value();
	}
	const Rect r{ needed(intent, "x"), needed(intent, "y"), needed(intent, "width"), needed(intent, "height") };
	if (r.width <= 0 || r.height <= 0)
		throw Refused("planetarium.viewport: an empty rectangle");
	m_view = r;
	return Json::Value();
}

bool Planetarium::handle(const SEvent &event)
{
	if (m_scene.shown)
		return false; // a composed picture: not the mouse's to move
	View *p = m_current ? m_current.get() : m_next.get();
	return p && p->handle(event);
}

void Planetarium::draw(video::IVideoDriver *driver)
{
	collect();
	const core::dimension2du screen = driver->getScreenSize();
	m_screen = v2u32(screen.Width, screen.Height);
	if (m_next) {
		// the old body stays until the new one is whole, if coarsely
		if (!m_current || (m_next->update(v2u32(screen.Width, screen.Height)), m_next->ready()))
			m_current = std::move(m_next);
	}
	if (!m_current)
		return;
	core::recti view(0, 0, screen.Width, screen.Height);
	if (m_view) {
		const float k = m_opts.shell.scale();
		view = core::recti((s32)std::lround(m_view->x * k), (s32)std::lround(m_view->y * k),
				(s32)std::lround((m_view->x + m_view->width) * k), (s32)std::lround((m_view->y + m_view->height) * k));
		view.clipAgainst(core::recti(0, 0, screen.Width, screen.Height));
	}
	// the planet framed in what the page leaves free: the view's middle moved there
	const float k = m_opts.shell.scale();
	const double w = std::max(1, view.getWidth()), h = std::max(1, view.getHeight());
	const double cx = (m_insets.left - m_insets.right) * k / 2, cy = (m_insets.top - m_insets.bottom) * k / 2;
	const double eased = m_scene.blend * m_scene.blend * (3 - 2 * m_scene.blend);
	const v2f centre = planetarium::HOME_SHOT.centre;
	m_current->frame(v2f((f32)(cx / (w / 2) + centre.X * eased), (f32)(-cy / (h / 2) + centre.Y * eased)));
	m_current->fov((f32)(planetarium::OrbitCamera::FOV + (planetarium::HOME_SHOT.fov - planetarium::OrbitCamera::FOV) * eased));
	m_current->skyLevel((f32)(1 + (planetarium::HOME_SHOT.sky - 1) * eased));
	// the window changed shape: the same picture on it
	if (m_scene.shown && !m_current->flying() && m_scene.aspect != (float)screen.Width / std::max(1u, screen.Height))
		compose(*m_current, 0);
	m_current->draw(driver, view);
	tellHover(*m_current);
	if (const std::optional<std::optional<size_t>> chosen = m_current->takeChosen()) {
		Json::Value message;
		message["kind"] = "planetarium";
		message["event"] = "chosen";
		message["body"] = *chosen ? m_current->system().spec().bodies[**chosen].id : "star";
		m_opts.shell.send(message);
	}
	if (const std::optional<View::Pick> pick = m_current->takeClick()) {
		Json::Value message;
		message["kind"] = "planetarium";
		message["event"] = "picked";
		message["latitude"] = pick->latitude;
		message["longitude"] = pick->longitude;
		Json::Value cell;
		static const char *const REGIONS[] = { "segment", "cap", "core" };
		cell["region"] = REGIONS[(int)pick->cell.region];
		cell["index"] = pick->cell.index;
		cell["i"] = (Json::Int64)pick->cell.i;
		cell["j"] = (Json::Int64)pick->cell.j;
		cell["k"] = (Json::Int64)pick->cell.k;
		message["cell"] = cell;
		m_opts.shell.send(message);
	}
}

void Planetarium::tellHover(const View &p)
{
	const std::optional<View::Hover> &now = p.hovered();
	// moved under a pixel, it's the same as far as the page can tell
	const auto same = [](const View::Hover &a, const View::Hover &b) {
		return a.body == b.body && std::abs(a.at.X - b.at.X) < 1 && std::abs(a.at.Y - b.at.Y) < 1 &&
				std::abs(a.radius - b.radius) < 1;
	};
	if (now.has_value() == m_hovered.has_value() && (!now || same(*now, *m_hovered)))
		return;
	m_hovered = now;
	Json::Value message;
	message["kind"] = "planetarium";
	message["event"] = "hovered";
	message["body"] = Json::nullValue;
	if (now) {
		const float k = m_opts.shell.scale();
		Json::Value body;
		if (now->body) {
			const terragen::SystemBody &b = p.system().spec().bodies[*now->body];
			body["id"] = b.id;
			body["name"] = b.name;
			body["kind"] = terragen::KIND_NAMES[(int)p.system().cards()[*now->body].kind];
		} else {
			body["id"] = "star";
			body["name"] = "Star";
			body["kind"] = "star";
		}
		body["x"] = now->at.X / k;
		body["y"] = now->at.Y / k;
		body["radius"] = now->radius / k;
		message["body"] = body;
	}
	m_opts.shell.send(message);
}

} // namespace shell
