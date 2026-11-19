// Terra Firma: the checks' probes of the world model, the far view and the bodies
// (sprint 119: were Lua mods in scripts/{terragen,far,bodies}-check). See probe.h.
#include "probe.h"

#include "environment.h"
#include "filesys.h"
#include "map.h"
#include "nodedef.h"
#include "server.h"
#include "server/terra.h"
#include "serverenvironment.h"
#include "terragen/system.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

namespace probe {

namespace {

constexpr double DEG = M_PI / 180;

// how many of the weather's events are happening (a bit each)
int events(uint32_t bits)
{
	int n = 0;
	for (; bits; bits &= bits - 1)
		n++;
	return n;
}

std::string str(v3s16 p)
{
	return format("(%d,%d,%d)", p.X, p.Y, p.Z);
}

// What a block is to the far view: air, a tree, a plant, liquid or solid.
std::string kind(Context &c, const std::string &name)
{
	if (name == "air" || name == "core:seam" || name == "ignore") // (seam: above the tile's top)
		return "air";
	const ContentFeatures &f = c.def(name);
	if (itemgroup_get(f.groups, "tree") > 0 || itemgroup_get(f.groups, "leaves") > 0)
		return "tree";
	// planted after: tufts, cacti, flora's stems and flowers
	if (itemgroup_get(f.groups, "plant") > 0 || f.drawtype == NDT_PLANTLIKE || f.drawtype == NDT_NODEBOX)
		return "plant";
	// (the world's water is the matter's since sprint 80: group liquid, no Luanti liquidtype)
	if (f.liquid_type != LIQUID_NONE || itemgroup_get(f.groups, "liquid") > 0 || name == "core:ice")
		return "liquid"; // a frozen sea's top is its water
	return "solid";
}

} // namespace

// scripts/far-check.sh: the far view draws each column as terraSurface says the generator
// makes it. Areas generated for real and compared, column by column: the ground (its
// block and height) and any water over it. FAR_BODY: another body's land, not the home's.
void far(Context &c)
{
	const s16 SIZE = 96;
	const char *env = std::getenv("FAR_BODY");
	const std::optional<int> body = env && *env ? std::optional(c.body(env)) : std::nullopt;
	const int b = body.value_or(planet::Atlas::HOME);
	// by the middle of six regions, found by place: five segments' middles on the equator,
	// and the north pole; each a degree north-east of the middle, within one tile
	std::vector<v3s16> areas;
	for (double lon : { 7.5, 82.5, 157.5, 232.5, 307.5 })
		areas.push_back(*c.atlasNode(1 * DEG, (lon + 1) * DEG, 0, b));
	areas.push_back(*c.atlasNode(89.5 * DEG, 45 * DEG, 0, b));
	struct Totals { long columns = 0, ground = 0, water = 0, rooted = 0, seam = 0; int shown = 0; };
	auto totals = std::make_shared<Totals>();
	auto pending = std::make_shared<size_t>(areas.size());
	const auto miss = [&c, totals](const char *what, int x, int z, const std::string &want, const std::string &got) {
		if (totals->shown++ < 12)
			c.say(format("FAR miss %s at %d %d: far %s, world %s", what, x, z, want.c_str(), got.c_str()));
	};
	c.after(0, [&c, areas, body, totals, pending, miss, SIZE] {
		// facing its star: a new player starts where it's livable, under a sun that stays
		if (terraFacingStar(c.server(), planet::Atlas::HOME)) {
			const v3s16 at = c.spawn();
			const TerraSun sun = *terraSun(c.server(), intToFloat(at, 1));
			const TerraClimate climate = terraClimate(c.server(), intToFloat(at, 1));
			c.say(format("FAR held turns %g elevation %.1f daylight %g temperature %.1f", sun.turns, sun.elevation,
					sun.daylight, climate.temperature));
			if (!(sun.turns == 0 && sun.elevation > -30 && sun.elevation < 45))
				throw BaseException("far probe: the sun where a new player starts");
			if (!(climate.temperature > -15 && climate.temperature < 45))
				throw BaseException("far probe: the climate where a new player starts");
		}
		for (const v3s16 &a : areas) {
			// (another body's land isn't near the home's sea level: round its own ground there)
			const s16 middle = body ? terraSurface(c.server(), v3s16(a.X, 0, a.Z)).ground : 0;
			const v3s16 minp(a.X, body ? middle - 200 : -200, a.Z);
			const v3s16 maxp(a.X + SIZE - 1, body ? middle + 200 : 255, a.Z + SIZE - 1);
			c.emerge(minp, maxp, [&c, minp, maxp, totals, pending, miss] {
				// the edges' trees lean in from chunks outside the area: skip a margin
				for (s16 z = minp.Z + 12; z <= maxp.Z - 12; z++)
					for (s16 x = minp.X + 12; x <= maxp.X - 12; x++) {
						// a column that's no place on the planet (seam) has no land to compare
						if (!c.place(v3s16(x, 0, z))) {
							totals->seam++;
							continue;
						}
						const TerraSurface s = terraSurface(c.server(), v3s16(x, 0, z));
						// the real column, top down
						std::optional<std::pair<int, std::string>> ground, water;
						for (s16 y = maxp.Y; y >= minp.Y; y--) {
							const std::string name = c.node(v3s16(x, y, z));
							const std::string k = kind(c, name);
							if (k == "liquid" && !water)
								water = std::pair{ (int)y, name };
							if (k == "solid") {
								ground = std::pair{ (int)y, name };
								break;
							}
						}
						// a tree's trunk stands in its column's top block: that column can't be told
						if (kind(c, c.node(v3s16(x, s.ground, z))) == "tree") {
							totals->rooted++;
							continue;
						}
						totals->columns++;
						if (ground && ground->first == s.ground && ground->second == s.top) {
							totals->ground++;
						} else {
							std::string round;
							for (int y = s.ground + 2; y >= s.ground - 1; y--)
								round += (round.empty() ? "" : " ") + std::to_string(y) + "=" + c.node(v3s16(x, y, z));
							miss("ground", x, z, std::to_string(s.ground) + " " + s.top,
									(ground ? std::to_string(ground->first) + " " + ground->second : "none") + " [" + round + "]");
						}
						const std::string sw = s.water ? std::to_string(*s.water) + " " + s.liquid : "none";
						const std::string rw = water ? std::to_string(water->first) + " " + water->second : "none";
						// water that has flowed since it was generated is the world's doing
						const bool flowed = water && water->second.size() > 8 &&
								water->second.compare(water->second.size() - 8, 8, "_flowing") == 0;
						if (flowed || sw == rw)
							totals->water++;
						else
							miss("water", x, z, sw, rw);
					}
				if (--*pending > 0)
					return;
				c.say(format("FAR columns %ld ground %ld water %ld rooted %ld seam %ld", totals->columns, totals->ground,
						totals->water, totals->rooted, totals->seam));
				c.finish();
			});
		}
	});
}

// scripts/terragen-check.sh: the surface node at each place listed in the world's
// points.txt (a direction per line, 2^30 a unit: terragen::Place); each column's water
// and solid surface, and what the world model says there (with the clock it read). Lines
// keyed by the place's number.
void terragen(Context &c)
{
	struct Point { double x, y, z; };
	std::vector<Point> points;
	{
		std::ifstream in(c.server()->getWorldPath() + DIR_DELIM "points.txt");
		if (!in)
			throw BaseException("terragen probe: no points.txt in the world");
		for (std::string line; std::getline(in, line);) {
			std::istringstream l(line);
			Point p;
			if (l >> p.x >> p.y >> p.z)
				points.push_back(p);
		}
	}
	const s64 TOP = c.atlas().body(planet::Atlas::HOME).params().height - 1; // above it is seam
	const auto isWater = [](const std::string &n) {
		return n == "core:water_source" || n == "core:water_flowing" || n == "core:ice" ||
				n == "core:lava_source" || n == "core:lava_flowing";
	};
	// the solid surface of a column (looking past small plants, cactus and what's lying
	// about, which stand on it), its name, and its water surface (−9999: none)
	const auto column = [&c, TOP, isWater](v3s16 p) {
		int water = -9999;
		for (s64 y = TOP; y >= -120; y--) {
			const std::string name = c.node(v3s16(p.X, y, p.Z));
			if (isWater(name)) {
				if (water == -9999)
					water = (int)y;
			} else if (name != "air" && name != "ignore" && c.group(name, "plant") == 0 && name != "core:cactus" &&
					c.group(name, "lying") == 0) {
				return std::tuple{ std::optional<int>((int)y), name, water };
			}
		}
		return std::tuple{ std::optional<int>(), std::string("none"), water };
	};
	// The surface node at a place; its column's own place is near it, not on it (a node
	// is a block across): that's logged as MAP
	const auto onMap = [&c](const Point &d) {
		const double lat = std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z)), lon = std::atan2(d.z, d.x);
		const std::optional<v3s16> n = c.atlasNode(lat, lon, 0);
		if (!n)
			throw BaseException(format("terragen probe: no surface node at %.4f %.4f", lat, lon));
		return *n;
	};
	auto remaining = std::make_shared<size_t>(points.size());
	const auto probe = [&c, points, onMap, column, remaining] {
		for (size_t i = 0; i < points.size(); i++) {
			const int id = (int)i + 1;
			const v3s16 p = onMap(points[i]);
			const TerraPlace place = *terraPlace(c.server(), intToFloat(p, 1));
			c.say(format("MAP %d %lld %lld %lld", id, (long long)place.x, (long long)place.y, (long long)place.z));
			c.emerge(v3s16(p.X, -160, p.Z), v3s16(p.X, 260, p.Z), [&c, id, p, column, remaining] {
				const auto [y, name, water] = column(p);
				// and the block 20 below the surface: rock, ore or cave
				const std::string deep = y ? c.node(v3s16(p.X, *y - 20, p.Z)) : "none";
				c.say(format("PROBE %d %s %s %d %s", id, y ? std::to_string(*y).c_str() : "nil", name.c_str(), water, deep.c_str()));
				const v3f pos(p.X, 0, p.Z);
				const TerraSun sun = *terraSun(c.server(), pos);
				const TerraClimate climate = terraClimate(c.server(), pos);
				const TerraCalendar calendar = terraCalendar(c.server());
				std::string province = terraProvince(c.server(), pos);
				for (char &ch : province)
					if (ch == ' ')
						ch = '_';
				ServerEnvironment &env = c.env();
				c.say(format("TERRA %d %d %.17g %d %d %.6f %.6f %.6f %.6f %.6f %s %s %.6f %s", id, (int)env.getDayCount(),
						(double)env.getTimeOfDayF(), calendar.year, calendar.day, calendar.date, sun.latitude,
						sun.declination, sun.elevation, sun.daylight, sun.season, climate.biome.c_str(),
						climate.temperature, province.c_str()));
				// at the ground, where the climate is
				const TerraWeather w = *terraWeather(c.server(), v3f(p.X, std::max(y.value_or(0), 1), p.Z));
				c.say(format("WEATHER %d %.4f %.4f %.4f %.4f %.4f %d %d %.4f", id, w.sample.temperature - 273.15,
						climate.temperature, w.sample.precipitation, w.sample.pressure / 100, w.sample.cloud,
						events(w.sample.events), (int)env.getDayCount(), (double)env.getTimeOfDayF()));
				// and what that weather does to the top of the column (snow, ice), applied
				std::optional<v3s16> top;
				if (water != -9999)
					top = v3s16(p.X, water, p.Z);
				else if (y)
					top = v3s16(p.X, *y, p.Z);
				if (top) {
					const std::string before = c.node(*top);
					const TerraWeather here = *terraWeather(c.server(), intToFloat(*top, 1));
					std::string action = terraWeatherAct(c.server(), *top);
					if (action.empty())
						action = "none";
					const std::string now = c.node(action == "snow" ? *top + v3s16(0, 1, 0) : *top);
					c.say(format("ACT %d %s %s %s %.3f %s %.4f", id, before.c_str(), action.c_str(), now.c_str(),
							here.sample.temperature - 273.15, here.sample.snow ? "true" : "false", here.sample.precipitation));
				}
				if (--*remaining == 0)
					c.finish();
			});
		}
	};
	// once the world's weather is open (a new world's settles on a worker as it starts):
	// what's logged reads it
	auto ready = std::make_shared<std::function<void()>>();
	*ready = [&c, points, onMap, probe, ready] {
		const v3s16 first = onMap(points.at(0));
		if (terraWeather(c.server(), v3f(first.X, 1, first.Z)))
			probe();
		else
			c.after(0.5, *ready);
	};
	c.after(1, [ready] { (*ready)(); });
}

// scripts/bodies-check.sh (sprint 40): the home and its moon in one window; a mark on each,
// kept as a place; every tile of both pushed out of the map (past the pager's hold), and
// each mark found again by its place.
void bodies(Context &c)
{
	const std::string MARK = "core:seam"; // a node the ground never has
	struct State { bool ok = true; };
	auto st = std::make_shared<State>();
	const auto say = [&c](const std::string &s) { c.say("BODIES " + s); };
	const auto check = [st, say](bool cond, const std::string &what) {
		if (!cond) {
			st->ok = false;
			say("FAILED: " + what);
		}
		return cond;
	};
	const auto finish = [&c, st, say] {
		say(st->ok ? "pass" : "fail");
		c.finish();
	};
	const std::vector<terragen::BodyWorld> &worlds = c.server()->bodyWorlds();
	std::string names;
	bool hasMoon = false;
	for (const terragen::BodyWorld &b : worlds) {
		names += (names.empty() ? "" : ", ") + format("%s %.0f", b.id.c_str(), b.atlas.radius);
		hasMoon = hasMoon || b.id == "moon";
	}
	say("bodies: " + names);
	check(hasMoon, "the moon among the bodies");
	if (!hasMoon)
		return finish();
	const int home = planet::Atlas::HOME, moon = c.body("moon");
	const auto bodyAt = [&c](v3s16 p) { return c.atlas().bodyAt({ p.X, p.Y, p.Z }); };
	// a place (sprint 40): where something is on its body (its cell), whatever slot holds it now
	const auto placeOf = [&c](v3s16 p) -> std::optional<planet::Atlas::Cell> { return c.cell(p); };
	const auto posOf = [&c](const planet::Atlas::Cell &cell) {
		const planet::Atlas::Node n = c.server()->planetNode(cell);
		return v3s16(n.x, n.y, n.z);
	};
	struct Mark { planet::Atlas::Cell place; v3s16 pos; };
	auto marks = std::make_shared<std::map<int, Mark>>();

	// a column of a body made, layers −40 to 120
	const auto emergeColumn = [&c](double lat, double lon, int body, std::function<void()> then) {
		const v3s16 low = *c.atlasNode(lat, lon, -40, body), high = *c.atlasNode(lat, lon, 120, body);
		const v3s16 a(std::min(low.X, high.X), low.Y, std::min(low.Z, high.Z));
		const v3s16 b(std::max(low.X, high.X), high.Y, std::max(low.Z, high.Z));
		c.emerge(a - v3s16(16, 16, 16), b + v3s16(16, 16, 16), then);
	};
	// the first air over walkable ground in a made column; none over the sea
	const auto groundAt = [&c](double lat, double lon, int body) -> std::optional<v3s16> {
		for (int layer = -40; layer <= 119; layer++) {
			const v3s16 here = *c.atlasNode(lat, lon, layer, body), above = *c.atlasNode(lat, lon, layer + 1, body);
			if (c.node(above) == "air" && c.def(c.node(here)).walkable)
				return above;
		}
		return std::nullopt;
	};
	// both tiles pushed out, past the hold, and each mark found by its place
	const auto comeBack = [&c, marks, check, say, finish, bodyAt, posOf, MARK, worlds] {
		auto left = std::make_shared<size_t>(marks->size());
		for (const auto &[body, m] : *marks) {
			const v3s16 pos = posOf(m.place);
			check(bodyAt(pos) == body, worlds[body].id + "'s place is back on " + worlds[body].id);
			c.emerge(pos - v3s16(16, 16, 16), pos + v3s16(16, 16, 16), [&c, body, pos, left, check, say, finish, MARK, worlds] {
				const std::string name = c.node(pos);
				check(name == MARK, worlds[body].id + "'s mark kept (found " + name + ")");
				say(format("%s's mark at %s now: %s", worlds[body].id.c_str(), str(pos).c_str(), name == MARK ? "kept" : "LOST"));
				if (--*left == 0)
					finish();
			});
		}
	};
	const auto pushOut = [&c, marks, check, say, comeBack, placeOf, worlds] {
		for (int n = 0; n <= 400; n++) {
			const double la = (-60 + (n % 23) * 5) * DEG, lo = (180 + (n / 23) * 10) * DEG;
			try {
				c.atlasNode(la, lo, 0);
			} catch (const std::exception &) {
				// (the window full: as it should be)
			}
		}
		for (const auto &[body, m] : *marks) {
			const auto now = placeOf(m.pos);
			check(!now || now->body != body || now->i != m.place.i || now->j != m.place.j,
					worlds[body].id + "'s mark's tile left the map");
		}
		int moons = 0;
		for (const planet::Atlas::Tile &t : c.atlas().tiles())
			moons += worlds[t.body].id == "moon";
		say(format("after filling: %d tiles, %d of the moon", (int)c.atlas().tiles().size(), moons));
		c.after(31, comeBack);
	};
	const auto mark = [&c, marks, check, placeOf, posOf, MARK, worlds](int body, v3s16 pos) {
		c.set(pos, MARK);
		const auto place = placeOf(pos);
		check(place && place->body == body, worlds[body].id + "'s mark has a place on it");
		// while its tile is still in, a place is its position
		check(place && posOf(*place) == pos, worlds[body].id + "'s place is its position");
		if (place)
			marks->insert({ body, Mark{ *place, pos } });
	};
	const auto onTheMoon = [&c, check, say, mark, pushOut, bodyAt, home, moon](v3s16 on, v3s16 spawn) {
		say(format("the moon's ground at %s: %s", str(on).c_str(), c.node(on - v3s16(0, 1, 0)).c_str()));
		// its sky, its land and its air, its own
		const std::optional<TerraSun> sun = terraSun(c.server(), intToFloat(on, 1));
		const TerraSurface surface = terraSurface(c.server(), on);
		const std::optional<TerraWeather> w = terraWeather(c.server(), intToFloat(on, 1));
		check(sun.has_value(), "the moon's sun");
		check(w.has_value() && !w->moving, "the moon's air isn't simulated");
		if (sun && w)
			say(format("the moon's sun %.1f° up, its ground %d, %.1f °C", sun->elevation, surface.ground,
					w->sample.temperature - 273.15));
		// where /travel lands on the moon: on its land, by its own spawn levels
		const v3s16 landing = c.spawn(moon);
		check(bodyAt(landing) == moon, "the moon's landing is on the moon");
		say("the moon's landing: " + str(landing));
		mark(home, spawn + v3s16(0, 3, 0));
		mark(moon, on + v3s16(0, 2, 0));
		say(format("tiles in the window: %d", (int)c.atlas().tiles().size()));
		c.after(32, pushOut);
	};
	// land on the moon: the first of a few places that isn't sea
	static const double SPOTS[][2] = { { 0.3, 1.0 }, { -0.4, 2.0 }, { 0.6, -1.5 }, { -0.2, -2.5 }, { 0.1, 0.2 }, { 0.8, 3.0 } };
	auto findLand = std::make_shared<std::function<void(size_t, v3s16)>>();
	*findLand = [&c, check, say, finish, emergeColumn, groundAt, onTheMoon, bodyAt, moon, findLand](size_t n, v3s16 spawn) {
		if (n >= std::size(SPOTS)) {
			check(false, "land on the moon");
			return finish();
		}
		const double lat = SPOTS[n][0], lon = SPOTS[n][1];
		check(bodyAt(*c.atlasNode(lat, lon, 0, moon)) == moon, "a moon node is the moon's");
		emergeColumn(lat, lon, moon, [&c, say, groundAt, onTheMoon, findLand, n, lat, lon, spawn, moon] {
			const std::optional<v3s16> on = groundAt(lat, lon, moon);
			if (!on) {
				say(format("sea at %.1f, %.1f on the moon", lat, lon));
				return (*findLand)(n + 1, spawn);
			}
			onTheMoon(*on, spawn);
		});
	};
	c.after(0.5, [&c, check, bodyAt, home, findLand] {
		const v3s16 spawn = c.spawn();
		check(bodyAt(spawn) == home, "the spawn on the home");
		c.emerge(spawn - v3s16(16, 16, 16), spawn + v3s16(16, 16, 16), [findLand, spawn] { (*findLand)(0, spawn); });
	});
}

} // namespace probe
