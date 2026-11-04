// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "wiki.h"
#include "bridge.h"
#include "shell.h"
#include "filesys.h"
#include "porting.h"
#include "settings.h"
#include "exceptions.h"
#include <cmath>
#include <fstream>

namespace shell {

static const char *const VERBS[] = {
	"matter.list", "matter.state", "matter.react", "sandbox.create", "sandbox.step", "sandbox.drop",
	"orbit.coast", "moves.list", "moves.show", "moves.set", "moves.restart", "moves.state", "relief.show", "relief.set", "relief.restart", "relief.state", "scene.show", "scene.place", "scene.hide",
};

static const size_t MOST_SANDBOXES = 8; // a page shows one or two; more is a leak

static double number(const Json::Value &v, const char *field)
{
	if (!v[field].isNumeric())
		throw Refused(std::string("missing number '") + field + "'");
	return v[field].asDouble();
}

Wiki::Wiki(Shell &shell) : m_shell(shell)
{
	m_shell.on("matter.list", [this](const Json::Value &) { return m_matter.list(); });
	m_shell.on("matter.state", [this](const Json::Value &intent) { return m_matter.state(intent); });
	m_shell.on("matter.react", [this](const Json::Value &intent) { return m_matter.react(intent); });
	// sandboxes are touched from the main thread and, stepping, from a worker
	// (a busy demo at speed takes tens of ms a step: never on the frame)
	m_shell.on("sandbox.create", [this](const Json::Value &intent) {
		std::lock_guard<std::mutex> lock(m_lock);
		return create(intent);
	});
	m_shell.onAsync("sandbox.step", [this](const Json::Value &intent) {
		std::lock_guard<std::mutex> lock(m_lock);
		return step(intent);
	});
	m_shell.on("scene.show", [this](const Json::Value &intent) {
		std::vector<video::SColor> substances;
		std::vector<bool> water;
		for (const matter::Substance &sub : m_matter.table().substances()) {
			substances.emplace_back(255, (sub.colour >> 16) & 255, (sub.colour >> 8) & 255, sub.colour & 255);
			water.push_back(sub.id == "water");
		}
		{
			std::lock_guard<std::mutex> lock(m_lock);
			m_shown = intent["id"].asInt();
			publish(m_shown, find(intent));
		}
		m_scene.looks(&blocks(), std::move(substances), std::move(water));
		m_scene.heat(intent["heat"].asBool());
		m_scene.place(rectOf(intent["rect"]), (float)intent["right"].asDouble() * m_shell.scale());
		m_scene.show();
		return Json::Value();
	});
	m_shell.on("scene.place", [this](const Json::Value &intent) {
		m_scene.place(rectOf(intent["rect"]), (float)intent["right"].asDouble() * m_shell.scale());
		return Json::Value();
	});
	m_shell.on("scene.hide", [this](const Json::Value &) {
		m_shown = 0;
		m_course.reset();
		m_relief.reset();
		m_scene.hide();
		return Json::Value();
	});
	m_shell.on("moves.list", [this](const Json::Value &intent) { return moves(intent["scenes"].asString()); });
	m_shell.on("relief.show", [this](const Json::Value &intent) {
		blocks(); // (and the river baked beside them)
		m_relief = std::make_unique<Relief>(m_river);
		m_course.reset();
		m_shown = 0;
		m_latest.reset();
		m_scene.looks(&blocks(), {}, {});
		m_scene.heat(false);
		m_scene.frame(Scene::Frame{ 4, 1, 4, std::vector<sandbox::Stage::Cell>(16) });
		m_scene.distance(7);
		m_scene.place(rectOf(intent["rect"]), (float)intent["right"].asDouble() * m_shell.scale());
		m_scene.show();
		m_course_at = porting::getTimeMs();
		return Json::Value();
	});
	m_shell.on("relief.set", [this](const Json::Value &intent) {
		if (!m_relief)
			throw Refused("no time-lapse is playing");
		m_relief->lens = intent["lens"].asString();
		m_relief->speed = number(intent, "speed");
		m_relief->running = intent["running"].asBool();
		m_relief->touch();
		return Json::Value();
	});
	m_shell.on("relief.restart", [this](const Json::Value &) {
		if (!m_relief)
			throw Refused("no time-lapse is playing");
		m_relief->at = 0;
		m_relief->touch();
		return Json::Value();
	});
	m_shell.on("relief.state", [this](const Json::Value &) {
		if (!m_relief)
			throw Refused("no time-lapse is playing");
		return m_relief->state();
	});
	m_shell.on("moves.show", [this](const Json::Value &intent) {
		const std::string id = intent["id"].asString();
		for (const Json::Value &move : moves(intent["scenes"].asString())) {
			if (move["id"].asString() != id)
				continue;
			blocks(); // (and the machines baked beside them)
			m_relief.reset();
			m_course = std::make_unique<Course>(move, blocks(), m_machines);
			m_shown = 0;
			m_latest.reset();
			m_scene.looks(&blocks(), {}, {});
			m_scene.heat(false);
			m_scene.frame(m_course->frame());
			m_scene.distance(move.get("distance", 9).asFloat());
			m_scene.figure(std::nullopt);
			m_scene.machine(m_course->looks());
			m_scene.place(rectOf(intent["rect"]), (float)intent["right"].asDouble() * m_shell.scale());
			m_scene.show();
			m_course_at = porting::getTimeMs();
			return Json::Value();
		}
		throw Refused("no such move: " + id);
	});
	m_shell.on("moves.set", [this](const Json::Value &intent) {
		if (!m_course)
			throw Refused("no move is playing");
		m_course->speed = number(intent, "speed");
		m_course->running = intent["running"].asBool();
		return Json::Value();
	});
	m_shell.on("moves.restart", [this](const Json::Value &) {
		if (!m_course)
			throw Refused("no move is playing");
		m_course->restart();
		return Json::Value();
	});
	m_shell.on("moves.state", [this](const Json::Value &) {
		if (!m_course)
			throw Refused("no move is playing");
		return m_course->state();
	});
	m_shell.underlay([this](video::IVideoDriver *driver) {
		if (m_relief) {
			const u64 now = porting::getTimeMs();
			m_relief->update(std::min(0.1f, (now - m_course_at) / 1000.f));
			m_course_at = now;
			m_scene.ground(m_relief->ground());
			m_scene.follow(m_relief->focus());
		}
		if (m_course) {
			const u64 now = porting::getTimeMs();
			m_course->update(std::min(0.1f, (now - m_course_at) / 1000.f));
			m_course_at = now;
			m_scene.figure(m_course->figure());
			m_scene.machine(m_course->machine());
			m_scene.follow(m_course->centre());
			m_scene.ground(m_course->ground());
		}
		{
			std::lock_guard<std::mutex> lock(m_published);
			if (m_latest && m_latestId == m_shown)
				m_scene.frame(std::move(*m_latest));
			m_latest.reset();
		}
		m_scene.draw(driver);
		if (m_scene.shown())
			if (const std::optional<int> hover = m_scene.takeHover()) {
				Json::Value message;
				message["kind"] = "scene";
				message["event"] = "hover";
				message["index"] = *hover;
				m_shell.send(message);
			}
	}, [this](const SEvent &event) { return m_scene.handle(event); });
	m_shell.on("orbit.coast", [this](const Json::Value &intent) { return coast(intent); });
	m_shell.on("sandbox.drop", [this](const Json::Value &intent) {
		std::lock_guard<std::mutex> lock(m_lock);
		m_stages.erase(intent["id"].asInt());
		return Json::Value();
	});
}

Wiki::~Wiki()
{
	m_shell.underlay(nullptr, nullptr);
	for (const char *verb : VERBS)
		m_shell.off(verb);
}

static sandbox::Pos at(const Json::Value &v)
{
	if (!v.isArray() || v.size() != 3 || !v[0].isInt() || !v[1].isInt() || !v[2].isInt())
		throw Refused("a block's position is [x, y, z]");
	return { v[0].asInt(), v[1].asInt(), v[2].asInt() };
}

const Json::Value &Wiki::moves(const std::string &kind)
{
	static const std::map<std::string, std::string> FILES = { { "movement", "moves.json" }, { "vehicles", "vehicles.json" }, { "ground", "ground.json" } };
	const auto file = FILES.find(kind);
	if (file == FILES.end())
		throw Refused("no scenes of kind " + kind);
	Json::Value &kept = m_moves[kind];
	if (kept.isNull())
		kept = Course::read(porting::path_share + DIR_DELIM "games" DIR_DELIM + g_settings->get("game_id") + DIR_DELIM + file->second);
	return kept;
}

const std::vector<sandbox::StageBlock> &Wiki::blocks()
{
	if (!m_blocks.empty())
		return m_blocks;
	const std::string path = porting::path_share + DIR_DELIM "games" DIR_DELIM + g_settings->get("game_id") + DIR_DELIM "palette.json";
	std::ifstream in(path);
	Json::Value json;
	std::string errors;
	if (!in || !Json::parseFromStream(Json::CharReaderBuilder(), in, &json, &errors) || !json["scene"].isArray())
		throw BaseException("wiki: no scene blocks in " + path + " (bun palette, bun stage) " + errors);
	m_machines = json["machines"];
	m_river = json["river"];
	m_blocks = sandbox::Stage::read(json["scene"]);
	return m_blocks;
}

Json::Value Wiki::create(const Json::Value &intent)
{
	if (m_stages.size() >= MOST_SANDBOXES)
		throw Refused("too many scenes open: drop one first");
	const Json::Value &size = intent["size"];
	if (!size.isArray() || size.size() != 3)
		throw Refused("sandbox.create needs size: [x, y, z]");
	try {
		std::vector<std::pair<std::string, double>> air;
		for (const std::string &id : intent["air"].getMemberNames())
			air.push_back({ id, intent["air"][id].asDouble() });
		auto s = std::make_unique<sandbox::Stage>(sandbox::Stage::Opts{ m_matter.table(), blocks(), size[0].asInt(), size[1].asInt(),
				size[2].asInt(), number(intent, "ambient"), number(intent, "pressure"), 9.81, std::move(air) });
		s->setUp(intent, number(intent, "ambient"));
		int id = m_next++;
		m_stages[id] = std::move(s);
		Json::Value out;
		out["id"] = id;
		return out;
	} catch (const std::runtime_error &e) { // SandboxError, MatterError
		throw Refused(e.what());
	}
}

sandbox::Stage &Wiki::find(const Json::Value &intent)
{
	auto it = m_stages.find(intent["id"].asInt());
	if (it == m_stages.end())
		throw Refused("no such sandbox (dropped, or never made)");
	return *it->second;
}

Json::Value Wiki::step(const Json::Value &intent)
{
	sandbox::Stage &s = find(intent);
	double seconds = number(intent, "seconds");
	if (seconds <= 0 || seconds > 3600)
		throw Refused("a sandbox steps 0..3600 s at a time");
	try {
		s.step(seconds);
	} catch (const std::runtime_error &e) {
		throw Refused(e.what());
	}
	publish(intent["id"].asInt(), s);
	return view(s);
}

void Wiki::publish(int id, const sandbox::Stage &s)
{
	Scene::Frame frame{ s.sizeX(), s.sizeY(), s.sizeZ(), s.view(), s.time(), s.pouring() };
	std::lock_guard<std::mutex> lock(m_published);
	m_latest = std::move(frame);
	m_latestId = id;
}

core::recti Wiki::rectOf(const Json::Value &rect) const
{
	if (!rect["x"].isNumeric() || !rect["y"].isNumeric() || !rect["width"].isNumeric() || !rect["height"].isNumeric())
		throw Refused("a scene's rect is { x, y, width, height }");
	const float k = m_shell.scale();
	return core::recti((s32)std::lround(rect["x"].asDouble() * k), (s32)std::lround(rect["y"].asDouble() * k),
			(s32)std::lround((rect["x"].asDouble() + rect["width"].asDouble()) * k), (s32)std::lround((rect["y"].asDouble() + rect["height"].asDouble()) * k));
}

/*
	{ size, time, main[], state[], fill[], temperature[], activity[], vapour[],
	haze[] }: per block, x fastest, then z, then y from the bottom. main and
	vapour index matter.list's substances (−1: none); state as STATE_NAMES.
*/
Json::Value Wiki::view(const sandbox::Stage &s) const
{
	Json::Value out, size(Json::arrayValue);
	size.append(s.sizeX());
	size.append(s.sizeY());
	size.append(s.sizeZ());
	out["size"] = size;
	out["time"] = s.time();
	Json::Value main(Json::arrayValue), state(Json::arrayValue), fill(Json::arrayValue), t(Json::arrayValue),
			activity(Json::arrayValue), vapour(Json::arrayValue), haze(Json::arrayValue);
	for (const sandbox::Stage::Cell &c : s.view()) {
		// what's lying in it counts as what fills it
		const bool lying = c.fill.substance >= 0 && c.fill.share > 0.005;
		main.append(lying ? c.fill.substance : c.main);
		state.append(matter::STATE_NAMES[lying ? (int)matter::State::Liquid : (int)c.state]);
		fill.append(lying ? c.fill.share : c.filled);
		t.append(c.temperature);
		activity.append(c.activity);
		vapour.append(c.vapour);
		haze.append(c.haze);
	}
	out["main"] = main;
	out["state"] = state;
	out["fill"] = fill;
	out["temperature"] = t;
	out["activity"] = activity;
	out["vapour"] = vapour;
	out["haze"] = haze;
	return out;
}

// Newton's cannonball: fired from the equator, east or west, at a speed and
// an angle above the horizon, then left to gravity and the air (orbit::Flight,
// the same flight the game's craft use). The path comes back in the plane it
// flies in, against the stars, in planet radii (x along the launch's east, y up).
Json::Value Wiki::coast(const Json::Value &intent)
{
	double altitude = number(intent, "altitude"), speed = number(intent, "speed"), angle = number(intent, "angle");
	double mass = number(intent, "mass"), drag = number(intent, "drag_area"), seconds = number(intent, "seconds");
	if (altitude < 0 || speed < 0 || mass <= 0 || drag < 0 || seconds <= 0 || seconds > 86400)
		throw Refused("orbit.coast: altitude, speed, drag ≥ 0; mass > 0; 0 < seconds ≤ a day");
	const orbit::Flight &f = m_flight;
	double a = angle * M_PI / 180;
	orbit::State s = f.at({ 0, 0, altitude, { speed * std::cos(a), speed * std::sin(a), 0 } });
	orbit::Arc arc = f.coast(s, { mass, drag }, seconds, std::max(1.0, seconds / 2000));
	const terragen::Planet &p = f.planet();
	Json::Value out;
	out["radius"] = p.radius;
	out["scale_height"] = p.scale_height;
	out["seconds"] = arc.seconds;
	out["end"] = arc.end == orbit::Arc::End::Ground ? "ground" : arc.end == orbit::Arc::End::Around ? "around" : "time";
	orbit::Elements e = f.elements(s);
	out["bound"] = e.bound;
	out["periapsis"] = e.periapsis;
	if (e.bound)
		out["apoapsis"] = e.apoapsis;
	out["orbital_speed"] = e.orbital_speed;
	Json::Value points(Json::arrayValue);
	for (const orbit::Vec &v : arc.points) {
		Json::Value pt(Json::arrayValue);
		pt.append(v.x / p.radius);
		pt.append(v.z / p.radius);
		points.append(pt);
	}
	out["points"] = points;
	return out;
}

} // namespace shell
