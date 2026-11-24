// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/session.h"
#include "src/client/world/player/hand.h"
#include "src/server/world/run/machines/piece/machine.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/world/player/body.h"
#include "src/client/world/session/client.h"
#include "library/world/terragen/src/code/worlds.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/app/shell/host/shell.h"
#include "src/client/views/preview/preview.h"
#include "src/client/app/shell/bridge/system.h"
#include "src/client/app/menus/mainmenumanager.h"
#include "src/shared/platform/system/porting.h"

namespace shell {

// How many points the player's orbit is drawn with in the Planetarium.
static constexpr int PATH_POINTS = 2048;

static const char *const VERBS[] = {
	"screen.close", "game.leave", "game.quit",
	"inventory.move", "inventory.craft", "inventory.drop", "creative.list", "ui.send", "screen.open", "worldgen.preview",
	"system.preview", "system.world", "tools.pick", "tools.ask", "tools.brush", "hotbar.select",
};

namespace {

// The index of the row of `table` whose id is `id`; else the page is wrong.
template <class Row, class Id>
size_t lookup(const std::string &id, const Row *table, size_t count, Id idOf, const char *what)
{
	for (size_t i = 0; i < count; i++)
		if (id == idOf(table[i]))
			return i;
	throw Refused(std::string("no ") + what + " \"" + id + "\"");
}

} // namespace

Session::Session(const Opts &opts) :
	m_shell(opts.shell),
	m_page(opts.shell.pageLoads()),
	m_items({ opts.client, opts.driver, opts.shell.itemDir(), std::to_string(porting::getTimeMs()) }),
	m_state({ opts.shell, opts.client, m_items, opts.hand }),
	m_ui({ opts.shell, opts.client }),
	m_screens({ opts.shell, opts.guienv, m_ui }),
	m_chat({ .shell = opts.shell, .client = opts.client }),
	m_actions(opts.client),
	m_client(opts.client),
	m_planetarium(opts.shell.planetarium())
{
	// the page shows this world's bodies: `body { code, body? }`, the
	// home's when it names none
	m_planetarium.serve([this](const Json::Value &body) {
		// (it reads the game's own state: here. The worker only hands it back)
		auto made = std::make_shared<planetarium::Planetarium::Opts>(world(bodyNamed(body)));
		return Planetarium::Making([made] { return *made; });
	});
	m_shell.on("inventory.move", [this](const Json::Value &intent) {
		m_actions.move(intent);
		return Json::Value();
	});
	m_shell.on("inventory.craft", [this](const Json::Value &intent) {
		m_actions.craft(intent);
		return Json::Value();
	});
	m_shell.on("inventory.drop", [this](const Json::Value &intent) {
		m_actions.drop(intent);
		return Json::Value();
	});
	m_shell.on("creative.list", [this](const Json::Value &) {
		return m_items.catalog();
	});
	// a body of this world names it (its world isn't a code of its own);
	// the menu's previews are codes alone
	m_shell.onAsync("worldgen.preview", [this](const Json::Value &intent) {
		if (!intent.isMember("body"))
			return previewJson(intent);
		return previewJson(intent, bodies().at(bodyNamed(intent)).code);
	});
	// the map's bodies as the builder shows them: each one's card (stateless, as the menu's)
	m_shell.on("system.preview", [](const Json::Value &intent) {
		return systemPreview(intent);
	});
	// the world's own system, for the map's bodies
	m_shell.onAsync("system.world", [](const Json::Value &intent) {
		return worldSystem(intent);
	});
	// a tool picked on the page's rail (the focus view frees the mouse for it)
	m_shell.on("tools.pick", [&hand = opts.hand](const Json::Value &intent) {
		if (!intent["tool"].isString())
			throw Refused("tools.pick needs a string 'tool'");
		hand.pick(intent["tool"].asString());
		return Json::Value();
	});
	// an action on the page's rail: what a key does, or the library's
	m_shell.on("tools.ask", [&hand = opts.hand](const Json::Value &intent) {
		if (!intent["action"].isString() || (intent.isMember("name") && !intent["name"].isString()))
			throw Refused("tools.ask needs a string 'action', and 'name' a string if given");
		if (intent["action"].asString() == "relink") {
			// the selected part's joint to another: its id (a string: it's 64 bits) and kind
			if (!intent["other"].isString() || !intent["joint"].isString())
				throw Refused("tools.ask relink needs string 'other' and 'joint'");
			u64 other;
			try {
				other = std::stoull(intent["other"].asString());
			} catch (const std::exception &) {
				throw Refused("tools.ask relink: 'other' isn't a part's id");
			}
			// "none": the joint taken away
			const std::string joint = intent["joint"].asString();
			hand.relink(other, joint == "none" ? std::nullopt : std::optional<piece::Joint>(piece::Joint(lookup(joint,
					piece::JOINTS, size_t(piece::Joint::Count), [](const piece::JointKind &j) { return j.id; }, "joint"))));
			return Json::Value();
		}
		// a socket on the face the menu's on, of a joint; one taken away, by its id
		if (intent["action"].asString() == "socket") {
			if (!intent["joint"].isString())
				throw Refused("tools.ask socket needs a string 'joint'");
			hand.socket(piece::Joint(lookup(intent["joint"].asString(), piece::JOINTS, size_t(piece::Joint::Count),
					[](const piece::JointKind &j) { return j.id; }, "joint")));
			return Json::Value();
		}
		if (intent["action"].asString() == "unsocket") {
			if (!intent["socket"].isString())
				throw Refused("tools.ask unsocket needs a string 'socket'");
			try {
				hand.unsocket(std::stoull(intent["socket"].asString()));
			} catch (const std::logic_error &) {
				throw Refused("tools.ask unsocket: 'socket' isn't a socket's id");
			}
			return Json::Value();
		}
		hand.ask(intent["action"].asString(), intent.get("name", "").asString());
		return Json::Value();
	});
	// what the brush does with a shape: add, cut, paint, a new part
	m_shell.on("tools.brush", [&hand = opts.hand](const Json::Value &intent) {
		if (!intent["brush"].isString())
			throw Refused("tools.brush needs a string 'brush'");
		hand.pickBrush(intent["brush"].asString());
		// a new part's joint, if it's said
		if (intent.isMember("joint")) {
			if (!intent["joint"].isString())
				throw Refused("tools.brush: 'joint' must be a string");
			hand.setPartJoint(piece::Joint(lookup(intent["joint"].asString(), piece::JOINTS,
					size_t(piece::Joint::Count), [](const piece::JointKind &j) { return j.id; }, "joint")));
		}
		return Json::Value();
	});
	// a hotbar slot clicked (the mouse free: an object held, or paused)
	m_shell.on("hotbar.select", [this](const Json::Value &intent) {
		LocalPlayer *player = m_client->getEnv().getLocalPlayer();
		const u32 slots = player->getMaxHotbarItemcount();
		if (!intent["slot"].isUInt() || intent["slot"].asUInt() >= slots)
			throw Refused("hotbar.select needs a 'slot' under " + std::to_string(slots));
		m_client->setPlayerItem(u16(intent["slot"].asUInt()));
		return Json::Value();
	});
	m_shell.on("ui.send", [this](const Json::Value &intent) {
		m_ui.send(intent);
		return Json::Value();
	});
	// The page opens a screen itself where it decides when (`with`: what it opens on, as a key gives it).
	// Only screens in the table, and only where offered.
	m_shell.on("screen.open", [this](const Json::Value &intent) {
		if (!intent["name"].isString())
			throw Refused("screen.open needs a string 'name'");
		m_screens.open(intent["name"].asString(), intent["with"]);
		return Json::Value();
	});
	m_shell.on("screen.close", [this](const Json::Value &) {
		m_screens.close();
		return Json::Value();
	});
	m_shell.on("game.leave", [](const Json::Value &) {
		g_gamecallback->disconnect(); // back to the main menu
		return Json::Value();
	});
	m_shell.on("game.quit", [](const Json::Value &) {
		g_gamecallback->exitToOS();
		return Json::Value();
	});
}

bool Session::typed(const SEvent &event)
{
	// by the character typed, not the key: : @ # > are shifted, and differ by keyboard
	static const struct { wchar_t typed; const char *prefix; } LISTS[] = {
		{ L':', ":" }, { L'@', "@" }, { L'#', "#" }, { L'>', ">" },
	};
	// The engine's SDL2 gives a key's plain character, Shift or not (SDL2 can't say what Shift
	// makes of a key on the player's layout without its text input, which takes the game's
	// keys): Shift's characters for the palette's lists, on the common layouts (US, UK)
	static const struct { wchar_t plain, shifted; } SHIFTED[] = {
		{ L';', L':' }, { L'\'', L'@' }, { L'2', L'@' }, { L'3', L'#' }, { L'.', L'>' },
	};
	wchar_t typed = 0;
	if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown) {
		typed = event.KeyInput.Char;
		if (event.KeyInput.Shift) {
			wchar_t shifted = 0;
			for (const auto &key : SHIFTED)
				if (key.plain == typed)
					shifted = key.shifted;
			typed = shifted; // a shifted key not in the table is no list's
		}
	}
	else if (event.EventType == EET_STRING_INPUT_EVENT && event.StringInput.Str && event.StringInput.Str->size() == 1)
		typed = (*event.StringInput.Str)[0];
	// Ctrl+Shift+P: the actions, as an editor's command palette
	if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown && event.KeyInput.Control &&
			event.KeyInput.Shift && event.KeyInput.Key == KEY_KEY_P)
		typed = L'>';
	if (typed == 0 || isMenuActive())
		return false;
	for (const auto &list : LISTS) {
		if (typed != list.typed)
			continue;
		Json::Value with;
		with["prefix"] = list.prefix;
		m_screens.open("palette", with);
		return true;
	}
	return false;
}

void Session::update(const HudView &view)
{
	if (m_shell.pageLoads() != m_page) { // a reloaded page: tell it everything again
		m_page = m_shell.pageLoads();
		m_state.forget();
		m_ui.replay();
	}
	HudView shown = view;
	shown.crosshair = shown.crosshair && !m_planetarium.shown(); // nothing's aimed at in the planetarium
	m_state.update(shown);
	m_ui.update();
	m_chat.update();
	{
		TF_ZONE("UI: item icons warmed");
		m_items.warm(3.f); // the creative inventory's icons, a few each frame, before it's opened
	}
	// the planet made ready as soon as the world's known, so V shows it at once
	if (!m_prepared && !m_planetarium.shown() && m_client->getAtlas() && m_client->getHorizon()) {
		TF_ZONE("Planetarium: prepared");
		m_planetarium.prepare(world(bodyHere()));
		m_prepared = true;
	}
	std::optional<std::pair<double, double>> over;
	if (!m_planetarium.shown() && m_client->getAtlas())
		if (LocalPlayer *player = m_client->getEnv().getLocalPlayer())
			if (const std::optional<Json::Value> pose = m_state.mapPose(*player))
				over = { (0.5 - (*pose)["v"].asDouble()) * M_PI, ((*pose)["u"].asDouble() - 0.5) * 2 * M_PI };
	{
		TF_ZONE("Planetarium: idle");
		m_planetarium.idle(over);
	}
	// the system as the world's date has it (its year's share of the orbit)
	if (m_planetarium.shown() && m_year > 0)
		m_planetarium.dayIn(m_client->getEnv().getDayCount() + m_client->getEnv().getTimeOfDayF(), m_year);
	// and the player's orbit round the body they're on
	if (m_planetarium.shown()) {
		const std::optional<std::vector<orbit::Vec>> path = m_client->getBody().path(PATH_POINTS);
		std::optional<planetarium::Planetarium::Path> shown;
		if (path) {
			shown = planetarium::Planetarium::Path{ bodyHere(), {} };
			for (const orbit::Vec &p : *path)
				shown->points.push_back({ p.x, p.y, p.z });
		}
		m_planetarium.path(std::move(shown));
	}
}

std::vector<terragen::BodyWorld> Session::bodies() const
{
	const planet::Atlas *atlas = m_client->getAtlas();
	const std::optional<far::Horizon> &horizon = m_client->getHorizon();
	if (!atlas || !horizon)
		throw Refused("this world isn't a planet: it has no bodies");
	return terragen::bodyWorlds(terragen::WorldCode::decode(horizon->code), atlas->body(planet::Atlas::HOME).params());
}

int Session::bodyNamed(const Json::Value &intent) const
{
	if (!intent.isMember("body"))
		return planet::Atlas::HOME;
	if (!intent["body"].isString())
		throw Refused("a body is named by its id (a string)");
	const std::string id = intent["body"].asString();
	const std::vector<terragen::BodyWorld> worlds = bodies();
	for (size_t b = 0; b < worlds.size(); b++)
		if (worlds[b].id == id)
			return (int)b;
	throw Refused("this world has no body '" + id + "' to show");
}

int Session::bodyHere() const
{
	const planet::Atlas *atlas = m_client->getAtlas();
	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	if (!atlas || !player)
		return planet::Atlas::HOME;
	const v3f feet = player->getPosition() / BS;
	return atlas->bodyAt(planet::Atlas::moverNode(feet.X, feet.Y, feet.Z)).value_or(planet::Atlas::HOME);
}

planetarium::Planetarium::Opts Session::world(int body) const
{
	const planet::Atlas *atlas = m_client->getAtlas();
	const std::optional<far::Horizon> &horizon = m_client->getHorizon();
	if (!atlas || !horizon)
		throw Refused("the Planetarium shows a planet, and this world isn't one");
	// made once: the body model and the palette are too slow for the main thread on every opening
	if (!m_system) {
		const terragen::WorldCode code = terragen::WorldCode::decode(horizon->code);
		m_year = code.year;
		m_system = std::make_shared<const terragen::System>(terragen::SystemSpec::forWorld(code));
		m_palette = far::Palette::live(m_client->ndef(), m_client->tsrc(), far::blocksIn(horizon->content));
	}
	return { *atlas, *horizon, *m_palette, m_system, body };
}

std::optional<std::string> Session::showWorld()
{
	try {
		m_planetarium.show(world(bodyHere()));
	} catch (const Refused &e) {
		return std::string(e.what());
	}
	return std::nullopt;
}

Session::~Session()
{
	for (const char *verb : VERBS)
		m_shell.off(verb);
	m_planetarium.serve(nullptr); // its bodies were this session's
}

} // namespace shell
