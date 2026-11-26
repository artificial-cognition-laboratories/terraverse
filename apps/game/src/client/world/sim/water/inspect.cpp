// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/inspect.h"
#include "src/client/world/sim/water/base.h"
#include "src/client/world/sim/water/flat.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace liquids {

namespace {
// The views, as the debugger offers them
struct ViewInfo
{
	View view;
	const char *id, *label, *about;
};
constexpr ViewInfo VIEWS[] = {
	{ View::Water, "water", "Water", "As it is" },
	{ View::Disturbance, "disturbance", "Disturbance", "White where it's off its level: a sixteenth off is full white" },
	{ View::Kinds, "kinds", "Kinds", "Each node by whose water it is: sea navy, river cyan, lake green, a fill orange" },
	{ View::Running, "running", "Running", "Each node by what's run: red stirred, yellow easing to a new level, blue at rest" },
};
const float SPLASH = 0.5f; // nodes the debugger's splash raises its water by
// What it says of itself, as the page reads it
namespace said {
Json::Value stat(const char *label, const std::string &value);
Json::Value toggle(const char *id, const char *label, bool value, const char *about);
Json::Value button(const char *id, const char *label, const char *about);
std::string fixed(float v, int places);
} // namespace said
} // namespace

Inspect::Inspect(Opts o) : m_grid(o.grid), m_look(o.look), m_base(o.base), m_flat(o.flat) {}

Json::Value Inspect::describe()
{
	const Grid &g = m_grid;
	int known = 0, awake = 0, easing = 0;
	float most = 0;
	for (int t = 0; t < int(g.tile.size()); t++) {
		const Tile &k = g.tile[t];
		known += k.known;
		easing += k.easing;
		if (!k.awake)
			continue;
		awake++;
		const int first = g.firstOf(t);
		for (int cz = 0; cz < 16; cz++)
			for (int cx = 0; cx < 16; cx++)
				most = std::max(most, std::fabs(g.eta[first + cz * g.cols + cx]));
	}
	const s64 now = micros();
	const u64 retargeted = m_base.retargeted();
	if (m_at && now > m_at)
		m_rate = (retargeted - m_retargeted) / ((now - m_at) / 1e6f);
	m_retargeted = retargeted;
	m_at = now;

	Json::Value out;
	out["title"] = "Water";
	out["about"] = "The water near you: a base (the server's level, eased to) and a disturbance on it (wakes, splashes), "
			"run only where something's stirred it. Past the grid it's drawn flat.";
	Json::Value &stats = out["stats"] = Json::Value(Json::arrayValue);
	stats.append(said::stat("Nodes seen", std::to_string(known) + " of " + std::to_string(g.tile.size())));
	stats.append(said::stat("Stirred", std::to_string(awake) + " nodes"));
	stats.append(said::stat("Easing", std::to_string(easing) + " nodes"));
	stats.append(said::stat("Most off its level", said::fixed(most * 16, 2) + " sixteenths"));
	stats.append(said::stat("Levels found again", said::fixed(m_rate, 1) + " nodes/s"));
	stats.append(said::stat("River rise here", said::fixed(m_rise_here / 16, 2) + " blocks (the weather's, over or under the level generated)"));
	stats.append(said::stat("Flat blocks", std::to_string(m_flat.blocks())));

	Json::Value &controls = out["controls"] = Json::Value(Json::arrayValue);
	Json::Value view;
	view["id"] = "view";
	view["label"] = "Draw as";
	view["type"] = "choice";
	Json::Value &options = view["options"] = Json::Value(Json::arrayValue);
	for (const ViewInfo &v : VIEWS) {
		Json::Value o;
		o["id"] = v.id;
		o["label"] = v.label;
		o["about"] = v.about;
		options.append(o);
		if (v.view == m_look.view)
			view["value"] = v.id;
	}
	controls.append(view);
	controls.append(said::toggle("grid", "Grid water", m_look.grid, "Drawn (the sixteenths near you)"));
	controls.append(said::toggle("flat", "Flat water", m_look.flat, "Drawn (past the grid, and anything the grid doesn't own)"));
	controls.append(said::toggle("frozen", "Freeze", m_look.frozen, "Stop the disturbance and the levels where they are"));
	controls.append(said::toggle("rise", "River rise", m_look.rise, "Drawn (off: rivers at the level they were generated at)"));
	controls.append(said::button("splash", "Splash", "Drop water in front of you"));
	controls.append(said::button("calm", "Calm", "Put all of it at its level, at rest"));
	return out;
}

std::optional<std::string> Inspect::act(const std::string &control, const Json::Value &value)
{
	const auto flag = [&](bool &f) -> std::optional<std::string> {
		if (!value.isBool())
			return "water: '" + control + "' is on or off";
		f = value.asBool();
		return std::nullopt;
	};
	if (control == "view") {
		const ViewInfo *v = std::find_if(std::begin(VIEWS), std::end(VIEWS),
				[&](const ViewInfo &i) { return value.isString() && value.asString() == i.id; });
		if (v == std::end(VIEWS))
			return "water: no view '" + value.toStyledString() + "'";
		m_look.view = v->view;
		for (Tile &t : m_grid.tile)
			t.drawn = false; // drawn again as it's now shown
		return std::nullopt;
	}
	if (control == "grid")
		return flag(m_look.grid);
	if (control == "flat")
		return flag(m_look.flat);
	if (control == "frozen")
		return flag(m_look.frozen);
	if (control == "rise") {
		const std::optional<std::string> refused = flag(m_look.rise);
		for (Tile &t : m_grid.tile)
			t.scanned = false; // looked at again with it, or without (the flat water redraws by itself)
		return refused;
	}
	if (control == "splash")
		return splash();
	if (control == "calm") {
		calm();
		return std::nullopt;
	}
	return "water: no control '" + control + "'";
}

// A bump on the wet column nearest the grid's middle (where the camera is)
std::optional<std::string> Inspect::splash()
{
	Grid &g = m_grid;
	const int middle = g.cols / 2;
	int best = -1, nearest = INT32_MAX;
	for (int z = 0; z < g.cols; z++)
		for (int x = 0; x < g.cols; x++) {
			const int c = z * g.cols + x, away = (x - middle) * (x - middle) + (z - middle) * (z - middle);
			if (away < nearest && g.wet(c) && g.tile[g.tileOf(c)].known)
				best = c, nearest = away;
		}
	if (best < 0)
		return "water: no water in reach to splash";
	g.eta[best] += SPLASH;
	g.wake(g.tileOf(best));
	return std::nullopt;
}

void Inspect::calm()
{
	Grid &g = m_grid;
	std::fill(g.eta.begin(), g.eta.end(), 0.0f);
	std::fill(g.out.begin(), g.out.end(), std::array<float, 4>{});
	for (Tile &t : g.tile) {
		t.awake = false;
		t.drawn = false;
	}
}

namespace {
namespace said {

Json::Value stat(const char *label, const std::string &value)
{
	Json::Value s;
	s["label"] = label;
	s["value"] = value;
	return s;
}

Json::Value toggle(const char *id, const char *label, bool value, const char *about)
{
	Json::Value c;
	c["id"] = id;
	c["label"] = label;
	c["type"] = "toggle";
	c["value"] = value;
	c["about"] = about;
	return c;
}

Json::Value button(const char *id, const char *label, const char *about)
{
	Json::Value c;
	c["id"] = id;
	c["label"] = label;
	c["type"] = "button";
	c["about"] = about;
	return c;
}

std::string fixed(float v, int places)
{
	char text[32];
	std::snprintf(text, sizeof text, "%.*f", places, v);
	return text;
}

} // namespace said
} // namespace

} // namespace liquids
