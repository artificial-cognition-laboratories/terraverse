// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "release/release.h"
#include "online/http.h"
#include "settings.h"
#include "version.h"
#include "menu.h"
#include "terragen/worlds.h"
#include "lab.h"
#include "system.h"
#include "terragen/system.h"
#include "far/baked.h"
#include "client/inputhandler.h"
#include "light.h"
#include "mapgen/mapgen.h"
#include "terragen/code.h"
#include "terragen/progress.h"
#include "bridge.h"
#include "shell.h"
#include "worlds.h"
#include "preview.h"
#include "weather/weather.h"
#include "filesys.h"
#include "util/base64.h"
#include <optional>
#include <exception>
#include <string>
#include "exceptions.h"
#include "client/renderingengine.h"
#include "porting.h"
#include "settings.h"
#include "social.h"
#include <ICursorControl.h>
#include <IVideoDriver.h>
#include <IrrlichtDevice.h>
#include <json/json.h>

namespace shell {

// The world the menu's universe is when none has been visited yet.

static Json::Value worldJson(const Worlds &worlds, const WorldSpec &world)
{
	Json::Value out;
	out["name"] = world.name;
	out["path"] = world.path;
	Settings settings;
	out["code"] = Json::Value();
	out["mode"] = Json::Value();
	out["damage"] = Json::Value();
	out["pinned"] = false;
	out["lastOpened"] = Json::Value();
	if (const std::optional<int64_t> t = worlds.lastOpened(world))
		out["lastOpened"] = Json::Int64(*t);
	out["rules"] = Json::Value(Json::objectValue);
	if (settings.readConfigFile((world.path + DIR_DELIM "world.mt").c_str())) {
		if (settings.exists("creative_mode"))
			out["mode"] = settings.getBool("creative_mode") ? "creative" : "survival";
		if (settings.exists("enable_damage"))
			out["damage"] = settings.getBool("enable_damage");
		if (settings.exists("tf_pinned"))
			out["pinned"] = settings.getBool("tf_pinned");

		for (const char *key : { "tf_difficulty", "tf_hunger", "tf_weather", "tf_seasons",
				"tf_temperature", "tf_keep_inventory" }) {
			if (settings.exists(key))
				out["rules"][key] = settings.get(key);
		}
	}
	Settings meta("[end_of_params]");
	if (meta.readConfigFile((world.path + DIR_DELIM "map_meta.txt").c_str()) &&
			meta.exists("mgterra_code"))
		out["code"] = meta.get("mgterra_code");
	return out;
}

static std::string bytes(const void *data, size_t size)
{
	return base64_encode(std::string_view(static_cast<const char *>(data), size));
}

/*
	A settled year of the world's weather for the create screen: `snapshots`
	maps at evenly spaced dates (from the northern spring equinox), on the
	weather's own grid. Each field holds every snapshot in turn, as base64:
	rain uint8 (√(mm/h) × 100), cloud uint8 (× 255), temperature int8 (°C),
	wind east and north int8 (m/s), pressure int8 (hPa from its latitude's
	mean: the storms and the land's highs and lows, not the circulation's bands).
*/
static Json::Value weatherJson(const Json::Value &intent)
{
	if (!intent["code"].isString())
		throw Refused("weather.preview needs a world code");
	int snapshots = intent["snapshots"].isInt() ? intent["snapshots"].asInt() : 0;
	if (snapshots < 1 || snapshots > 64)
		throw Refused("weather.preview: snapshots must be 1..64");
	static const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	terragen::World world(readWorldCode(intent["code"].asString()));
	std::vector<fluid::Layer::Fields> year = weather::Weather::year(world, table, snapshots);

	std::string rain, cloud, temperature, east, north, pressure;
	auto byte = [](double v, double lo, double hi) { return (char)(int8_t)std::lround(std::clamp(v, lo, hi)); };
	int32_t width = 64;
	for (const fluid::Layer::Fields &f : year) {
		std::vector<double> zonal(f.t.size() / width, 0);
		for (size_t i = 0; i < f.t.size(); i++)
			zonal[i / width] += f.pressure[i] / width;
		for (size_t i = 0; i < f.t.size(); i++) {
			rain += (char)(uint8_t)std::lround(std::min(255.0, std::sqrt(f.rain[i]) * 100));
			cloud += (char)(uint8_t)std::lround(f.cloud[i] * 255);
			temperature += byte(f.t[i] - 273.15, -128, 127);
			east += byte(f.u[i], -128, 127);
			north += byte(f.v[i], -128, 127);
			pressure += byte((f.pressure[i] - zonal[i / width]) / 100, -128, 127);
		}
	}
	Json::Value out;
	out["resolution"] = 64;
	out["snapshots"] = snapshots;
	out["rain"] = bytes(rain.data(), rain.size());
	out["cloud"] = bytes(cloud.data(), cloud.size());
	out["temperature"] = bytes(temperature.data(), temperature.size());
	out["east"] = bytes(east.data(), east.size());
	out["north"] = bytes(north.data(), north.size());
	out["pressure"] = bytes(pressure.data(), pressure.size());
	return out;
}

static std::string stringField(const Json::Value &intent, const char *field)
{
	if (!intent[field].isString())
		throw Refused(std::string("missing string field '") + field + "'");
	return intent[field].asString();
}

Menu::Menu(const Opts &opts) :
	m_shell(opts.shell), m_intents(opts.shell), m_engine(opts.engine), m_worlds(opts.worlds), m_lab(opts.lab),
	m_community(opts.worlds, g_settings->get("game_id")),
	m_receiver(opts.receiver),
	m_planetarium(opts.shell.planetarium())
{
	// the universe behind every page: always the one world, so its picture
	// can be composed (Home.vue: planetarium.scene)
	Json::Value universe;
	universe["code"] = planetarium::HOME_SHOT.code;
	m_planetarium.release(); // a world that was being entered, and wasn't
	m_planetarium.serve([this](const Json::Value &b) { return body(b); }, universe);
	m_planetarium.opening();
	m_intents.onAsync("community.list", [this](const Json::Value &) {
		return m_community.list();
	});
	m_intents.on("community.mod.set", [this](const Json::Value &intent) {
		if (!intent["enabled"].isBool())
			throw Refused("community.mod.set needs enabled");
		m_community.setMod(stringField(intent, "id"), stringField(intent, "world"), intent["enabled"].asBool());
		return Json::Value();
	});
	m_intents.on("community.texture.set", [this](const Json::Value &intent) {
		m_community.setTexture(stringField(intent, "id"));
		return Json::Value();
	});
	// (they read every world's files: on the worker, or opening the page freezes it)
	// Asks the backend, on the worker. Failing is quiet for the player, who can
	// still play: the menu shows the running version and no notice.
	m_intents.onAsync("update.check", [](const Json::Value &) {
		Json::Value out;
		out["current"] = g_version_hash;
		out["latest"] = Json::Value();
		out["available"] = false;
		out["launcher"] = release::launched();
		const auto ours = release::parse(g_version_hash);
		if (!ours)
			return out; // a development build has nothing to update to
		HTTPFetchResult res = online::fetch(online::request(
				{ g_settings->get("arclabs_url"), "" }, HTTP_GET, "/api/terrafirma/latest"));
		if (!res.succeeded || res.response_code != 200) {
			warningstream << "update check: the backend answered " << res.response_code << std::endl;
			return out;
		}
		const Json::Value answer = online::parse(res.data);
		const auto latest = release::latest(answer["manifest"].asString(),
				answer["signature"].asString(), release::UPDATE_KEY);
		if (!latest) {
			errorstream << "update check: the latest release's signature is wrong; ignoring it" << std::endl;
			return out;
		}
		out["latest"] = *latest;
		out["available"] = *release::parse(*latest) > *ours;
		return out;
	});
	m_intents.onAsync("worlds.list", [this](const Json::Value &) {
		Json::Value out(Json::arrayValue);
		for (const auto &world : m_worlds.list())
			out.append(worldJson(m_worlds, world));
		return out;
	});
	m_intents.on("worlds.create", [this](const Json::Value &intent) {
		return worldJson(m_worlds, m_worlds.create(stringField(intent, "name"), stringField(intent, "code"),
				intent["settings"]));
	});
	m_intents.on("worlds.delete", [this](const Json::Value &intent) {
		m_worlds.remove(stringField(intent, "path"));
		return Json::Value();
	});
	m_intents.on("worlds.pin", [this](const Json::Value &intent) {
		if (!intent["pinned"].isBool())
			throw Refused("worlds.pin needs pinned");
		m_worlds.pin(stringField(intent, "path"), intent["pinned"].asBool());
		return Json::Value();
	});
	m_intents.on("worlds.rename", [this](const Json::Value &intent) {
		m_worlds.rename(stringField(intent, "path"), stringField(intent, "name"));
		return Json::Value();
	});
	m_intents.on("servers.list", [](const Json::Value &) {
		Json::Value out(Json::objectValue);
		out["status"] = "unavailable";
		out["servers"] = Json::Value(Json::arrayValue);
		return out;
	});
	m_intents.on("servers.join", [](const Json::Value &) -> Json::Value {
		throw Refused("Public server discovery is not connected yet.");
	});
	m_intents.on("servers.favorite.set", [](const Json::Value &) -> Json::Value {
		throw Refused("Public server discovery is not connected yet.");
	});
	m_intents.on("play", [this](const Json::Value &intent) {
		WorldSpec world = m_worlds.find(stringField(intent, "path"));
		m_worlds.select(world);
		m_shell.social().enter(world.name, true);
		m_choice = Choice{ world, false, std::nullopt };
		return Json::Value();
	});
	// ~150 ms when the land changes (the water simulation): off the main
	// thread, so the menu keeps drawing while a slider is dragged.
	m_intents.onAsync("worldgen.preview", [](const Json::Value &intent) {
		return previewJson(intent);
	});
	// about a second: a year of weather to settle, then a year to sample
	m_intents.onAsync("weather.preview", [](const Json::Value &intent) {
		try {
			return weatherJson(intent);
		} catch (const fluid::FluidError &e) {
			throw Refused(e.what());
		}
	});
	// microseconds: a few hundred fixed-point steps per body
	m_intents.on("system.preview", [](const Json::Value &intent) {
		return systemPreview(intent);
	});
	// the system a new world's draft starts from
	m_intents.on("system.world", [](const Json::Value &intent) {
		return worldSystem(intent);
	});
	m_intents.on("system.home", [](const Json::Value &) {
		return systemJson(terragen::SystemSpec::home());
	});
	m_intents.on("quit", [this](const Json::Value &) {
		m_quit = true;
		return Json::Value();
	});
	// A few seconds of STUN and ArcLabs: off the main thread. The menu
	// leaves for the friend's world on its next frame.
	m_intents.onAsync("social.join", [this](const Json::Value &intent) {
		online::JoinPlan plan = m_shell.social().join(stringField(intent, "id"), stringField(intent, "name"));
		std::lock_guard<std::mutex> lock(m_join_lock);
		m_join = std::move(plan);
		return Json::Value();
	});

}

Menu::~Menu()
{
	m_intents.clear();
	m_shell.drain(); // a verb on the worker may be using this menu
	if (m_choice)
		m_planetarium.hold(); // the world chosen stays on screen while it's entered (entering.h)
	else
		m_planetarium.serve(nullptr); // its bodies were this menu's
}

std::optional<Choice> Menu::run(const std::string &error)
{
	if (!error.empty()) {
		Json::Value message;
		message["kind"] = "error";
		message["message"] = error;
		m_shell.send(message);
	}

	auto *device = m_engine->get_raw_device();
	auto *cursor = device->getCursorControl();
	cursor->setVisible(true);
	cursor->setRelativeMode(false);

	video::IVideoDriver *driver = m_engine->get_video_driver();
	// the create screen's planet is lit as a game's is (its curve: set_light_curve)
	set_light_curve(g_settings->getFloat("display_gamma"));
	m_receiver.capture = [this](const SEvent &event) {
		return m_planetarium.shown() && event.EventType == EET_MOUSE_INPUT_EVENT && m_planetarium.handle(event);
	};
	u64 last = porting::getTimeMs();
	volatile auto *kill = porting::signal_handler_killstatus();
	const u64 frame_ms = 1000 / 60;

	while (m_engine->run() && !*kill && !m_choice && !m_quit) {
		{
			std::lock_guard<std::mutex> lock(m_join_lock);
			if (m_join) {
				m_shell.social().enter(m_join->world, false); // in a friend's world
				m_choice = Choice{ std::nullopt, false, std::move(m_join) };
				m_join.reset();
				break;
			}
		}
		u64 start = porting::getTimeMs();
		driver->beginScene(true, true, video::SColor(255, 12, 14, 12));
		m_planetarium.pointer(cursor->getPosition());
		m_planetarium.tick((start - last) / 1000.0); // the universe's clock (the page may stop it, or set it)
		last = start;
		m_planetarium.draw(driver); // always shown: the backdrop when no page asks for another
		m_shell.frame();
		driver->endScene();
		u64 spent = porting::getTimeMs() - start;
		if (spent < frame_ms)
			sleep_ms(frame_ms - spent);
	}
	m_receiver.capture = nullptr;
	return m_choice;
}

// The world a code makes, as a new world of it is laid out (worlds.cpp: as
// many blocks round as it is), its land from the game's baked palette.
Planetarium::Making Menu::body(const Json::Value &body)
{
	if (!body.isObject() || !body["code"].isString())
		throw Refused("planetarium.show in the menu needs body { code }");
	const std::string code = body["code"].asString();
	terragen::WorldCode world;
	try {
		world = terragen::WorldCode::decode(code);
	} catch (const std::exception &e) {
		throw Refused(std::string("that world code doesn't decode: ") + e.what());
	}
	// on the worker, one at a time: m_baked and m_atlases are only touched here
	return [this, code, world]() -> planetarium::Planetarium::Opts {
	terragen::Step step(code, "system laid out"); // (shell/loading.h)
	if (!m_baked)
		m_baked = far::Baked::load(g_settings->get("game_id"));
	// every body of its system, as the server lays them out (server.cpp), so
	// the others show their land too; laid out once a shape (40 ms a body): most edits move bodies, not resize them
	std::vector<terragen::Atlas::Params> bodies;
	std::string shape;
	try {
		for (const terragen::BodyWorld &b : terragen::bodyWorlds(world, terragen::Atlas::standard(world.size / (2 * M_PI)))) {
			bodies.push_back(b.atlas);
			shape += terragen::Atlas(b.atlas).code() + ";";
		}
	} catch (const std::exception &e) {
		throw Refused(std::string("that system can't exist: ") + e.what());
	}
	auto laid = m_atlases.find(shape);
	if (laid == m_atlases.end()) {
		if (m_atlases.size() >= 8)
			m_atlases.clear(); // a setting dragged through many: only the latest few kept
		laid = m_atlases.emplace(shape, planet::Atlas(bodies)).first;
	}
	const planet::Atlas &atlas = laid->second;
	return { atlas, far::Horizon{ code, MapgenParams().water_level, m_baked->content }, m_baked->palette,
		std::make_shared<const terragen::System>(terragen::SystemSpec::forWorld(world)) };
	};
}

} // namespace shell
