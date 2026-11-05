// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_matter.h"
#include "common/c_content.h"
#include "common/c_converter.h"
#include "filesys.h"
#include "lua_api/l_internal.h"
#include "map.h"
#include "nodedef.h"
#include "porting.h"
#include "sandbox/sandbox.h"
#include "sandbox/world.h"
#include "planet/atlas.h"
#include "terragen/worlds.h"
#include "server.h"
#include "shape/shape.h"
#include "fill/fill.h"
#include "serverenvironment.h"
#include <fstream>
#include <sstream>
#include <unordered_map>

using namespace sandbox;

/*
	The server's world matter: made at the first registration, loaded from
	disk at its first use (once every kind is registered), and from then on
	closed to new kinds. Server Lua runs on one thread.
*/
struct ServerMatter
{
	matter::Matter table;
	std::unique_ptr<WorldMatter> world;
	Server *server; // its environment comes after mods load: looked up when the map is touched
	std::string path;
	std::vector<int32_t> kind_of;     // by content id: the (first) kind, or −1
	std::vector<int32_t> points_of;   // by content id: how many kinds its param2 picks between (1: a plain block)
	std::vector<std::pair<content_t, u8>> node_of; // by kind: the node and param2 that are it
	std::unordered_map<int64_t, double> ambient; // by 16-block cube
	bool started = false;
	std::vector<WorldMatter::Spec> plain; // the plain blocks registered, for the bake (ModApiMatter::baked)
};

static std::unique_ptr<ServerMatter> s_matter;

static int64_t cube(Pos p)
{
	auto c = [](int32_t v) { return (int64_t)((v >= 0 ? v : v - 15) / 16) & 0x1FFFFF; };
	return (c(p.x) << 42) | (c(p.y) << 21) | c(p.z);
}

// Sprint 81: a node's open space. Partly there (cut, or smoothed ground:
// Map::solidShape), its empty sixteenths; whole, all of it where it isn't
// stood on (air, a plant, the sea's water, a levelled heap), none where it
// is; unloaded, none.
struct Open
{
	std::optional<shape::Shape> solid;
	bool whole_open;
	bool at(v3s16 sixteenth) const { return solid ? !solid->has(sixteenth) : whole_open; }
};

static Open openOf(Map &map, const NodeDefManager *ndef, v3s16 p)
{
	Open o{ map.solidShape(p), false };
	if (!o.solid) {
		const MapNode n = map.getNode(p);
		const ContentFeatures &f = ndef->get(n);
		o.whole_open = n.getContent() != CONTENT_IGNORE && (!f.walkable || f.param_type_2 == CPT2_LEVELED);
	}
	return o;
}

static sandbox::Room roomAt(Map &map, const NodeDefManager *ndef, v3s16 p)
{
	const Open o = openOf(map, ndef, p);
	if (!o.solid)
		return o.whole_open ? sandbox::Room::open() : sandbox::Room::closed();
	sandbox::Room r;
	for (int y = 0; y < 16; y++) {
		uint16_t n = 0;
		for (int z = 0; z < 16; z++)
			for (int x = 0; x < 16; x++)
				n += o.solid->has(v3s16(x, y, z)) ? 0 : 1;
		r.layers[y] = n;
	}
	return r;
}

// Which layers are open through a side (0..3: +x −x +z −z): where both
// nodes are empty somewhere along the face they share.
static uint16_t faceAt(Map &map, const NodeDefManager *ndef, v3s16 p, int side)
{
	static const v3s16 DIRS[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	const v3s16 d = DIRS[side];
	const Open a = openOf(map, ndef, p), b = openOf(map, ndef, p + d);
	if (!a.solid && !b.solid) // whole on both sides: all open or none
		return a.whole_open && b.whole_open ? 0xFFFF : 0;
	uint16_t mask = 0;
	for (int y = 0; y < 16; y++)
		for (int i = 0; i < 16 && !(mask & (1 << y)); i++) {
			// the sixteenth on p's side of the face, and the one facing it
			const v3s16 mine = d.X ? v3s16(d.X > 0 ? 15 : 0, y, i) : v3s16(i, y, d.Z > 0 ? 15 : 0);
			const v3s16 theirs = d.X ? v3s16(d.X > 0 ? 0 : 15, y, i) : v3s16(i, y, d.Z > 0 ? 0 : 15);
			if (a.at(mine) && b.at(theirs))
				mask |= 1 << y;
		}
	return mask;
}

static ServerMatter &made(lua_State *L)
{
	if (s_matter)
		return *s_matter;
	Server *server = ModApiBase::getServer(L);
	auto m = std::make_unique<ServerMatter>(ServerMatter{
		matter::Matter::load(porting::path_share + DIR_DELIM "matter"), nullptr, server,
		server->getWorldPath() + DIR_DELIM "matter.bin", {}, {}, {}, {} });
	ServerMatter *sm = m.get();
	WorldMatter::Ground ground{
		[sm](Pos p) {
			MapNode n = sm->server->getEnv().getMap().getNode(v3s16(p.x, p.y, p.z));
			content_t c = n.getContent();
			if (c >= sm->kind_of.size() || sm->kind_of[c] < 0)
				return -1; // unloaded (ignore) and unregistered: sealed
			// a family's param2 is which of its points it is
			return sm->kind_of[c] + std::min<int32_t>(n.param2, sm->points_of[c] - 1);
		},
		[sm](Pos p, int32_t kind, double level) {
			const auto &[c, param2] = sm->node_of.at(kind);
			const ContentFeatures &f = sm->server->ndef()->get(c);
			// a levelled block keeps how full it is in param2, in sixty-fourths (sprint 80)
			u8 p2 = param2;
			if (f.param_type_2 == CPT2_LEVELED)
				p2 = (u8)std::clamp<int>((int)std::lround(level * 64), 1, 64);
			sm->server->getEnv().swapNode(v3s16(p.x, p.y, p.z), MapNode(c, 0, p2));
		},
		[sm](Pos p) {
			auto it = sm->ambient.find(cube(p));
			return it != sm->ambient.end() ? it->second : 288.15; // a temperate default until the weather says
		},
		[sm](Pos p) {
			MapNode n = sm->server->getEnv().getMap().getNode(v3s16(p.x, p.y, p.z));
			const ContentFeatures &f = sm->server->ndef()->get(n);
			return f.param_type_2 == CPT2_LEVELED ? n.getLevel(sm->server->ndef()) / 64.0 : 1.0;
		},
		[sm](Pos p) { return roomAt(sm->server->getEnv().getMap(), sm->server->ndef(), v3s16(p.x, p.y, p.z)); },
		[sm](Pos p, int side) { return faceAt(sm->server->getEnv().getMap(), sm->server->ndef(), v3s16(p.x, p.y, p.z), side); },
		[sm](Pos p) {
			const fill::Fill *f = sm->server->getEnv().getMap().getFill(v3s16(p.x, p.y, p.z));
			sandbox::Liquid l;
			if (f) {
				// a fill names a substance the table may no longer have: then it's none
				const auto &all = sm->table.substances();
				const std::string &id = fill::name(f->liquid);
				for (size_t s = 0; s < all.size(); s++)
					if (all[s].id == id) {
						l.substance = (int32_t)s;
						l.share = f->amount / 255.0;
					}
			}
			return l;
		},
		[sm](Pos p, int32_t substance, double share, uint32_t colour) {
			fill::Fill f;
			if (substance >= 0) {
				f.liquid = fill::liquid(sm->table.substances().at(substance).id);
				f.amount = (u8)std::clamp<long>(std::lround(share * 255), 0, 255);
				f.r = (colour >> 16) & 0xFF;
				f.g = (colour >> 8) & 0xFF;
				f.b = colour & 0xFF;
			}
			sm->server->getEnv().getMap().setFill(v3s16(p.x, p.y, p.z), f);
		},
	};
	// each body's own air, from its card (sprint 96): a fire on the Moon starves
	ground.carve = [sm](Pos p, int cells, unsigned exposed, bool melting) {
		Map &map = sm->server->getEnv().getMap();
		map.setShape(v3s16(p.x, p.y, p.z), shape::melted(map.getShape(v3s16(p.x, p.y, p.z)), cells, exposed, melting));
	};
	std::vector<WorldMatter::Air> airs;
	const planet::Atlas *atlas = sm->server->atlas();
	if (atlas) {
		for (const terragen::BodyWorld &b : sm->server->bodyWorlds())
			airs.push_back({ b.surroundings.pressure, b.surroundings.gravity, b.surroundings.gases });
		ground.air = [atlas](Pos p) {
			const std::optional<int> body = atlas->bodyAt({ p.x, p.y, p.z });
			return (size_t)body.value_or(0); // (off the atlas: the home's, as the seams are)
		};
	} else {
		airs.push_back({ 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	}
	m->world = std::make_unique<WorldMatter>(WorldMatter::Opts{ m->table, airs, ground });
	s_matter = std::move(m);
	return *s_matter;
}

// The world matter once every kind is in: loads what was awake when the server last stopped.
static ServerMatter &started(lua_State *L)
{
	ServerMatter &m = made(L);
	if (m.started)
		return m;
	m.started = true;
	std::ifstream in(m.path, std::ios::binary);
	if (in) {
		std::stringstream bytes;
		bytes << in.rdbuf();
		m.world->load(bytes.str());
	}
	return m;
}

static Pos pos(lua_State *L, int index)
{
	v3s16 p = check_v3s16(L, index);
	return { p.X, p.Y, p.Z };
}

static void setNumber(lua_State *L, const char *key, double value)
{
	lua_pushnumber(L, value);
	lua_setfield(L, -2, key);
}

// Runs `f`, turning the world matter's errors into Lua errors.
template <typename F>
static int guarded(lua_State *L, const char *name, F f)
{
	try {
		return f();
	} catch (const SandboxError &e) {
		throw LuaError(std::string("core.matter.") + name + ": " + e.what());
	} catch (const matter::MatterError &e) {
		throw LuaError(std::string("core.matter.") + name + ": " + e.what());
	}
}

int ModApiMatter::l_register_block(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "register_block", [&] {
		ServerMatter &m = made(L);
		if (m.started)
			throw LuaError("core.matter.register_block: the world's matter has started; register blocks as mods load");
		std::string node = luaL_checkstring(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);
		std::string id = getstringfield_default(L, 2, "matter", "");
		if (id.empty())
			throw LuaError("core.matter.register_block: " + node + " needs a matter");
		double share = getfloatfield_default(L, 2, "share", 1.0f);
		std::string state = getstringfield_default(L, 2, "state", "solid");
		if (state != "solid" && state != "liquid")
			throw LuaError("core.matter.register_block: " + node + "'s state is solid or liquid");
		bool form = getboolfield_default(L, 2, "form", false);
		bool flowing = getboolfield_default(L, 2, "flowing", false);
		bool vessel = getboolfield_default(L, 2, "vessel", false); // sprint 79
		bool loose = getboolfield_default(L, 2, "loose", false);   // sprint 80
		content_t c = getServer(L)->ndef()->getId(node);
		if (c == CONTENT_IGNORE)
			throw LuaError("core.matter.register_block: no node called " + node);
		// levelled: its definition keeps how full it is in param2
		const bool leveled = getServer(L)->ndef()->get(c).param_type_2 == CPT2_LEVELED;
		const WorldMatter::Spec spec{ node, id, {}, share, state == "solid" ? matter::State::Solid : matter::State::Liquid,
				form, flowing, vessel, leveled, loose };
		int32_t kind = m.world->kind(spec);
		m.plain.push_back(spec);
		if (m.kind_of.size() <= c) {
			m.kind_of.resize(c + 1, -1);
			m.points_of.resize(c + 1, 1);
		}
		m.kind_of[c] = kind;
		m.points_of[c] = 1;
		m.node_of.push_back({ c, 0 });
		lua_pushinteger(L, kind);
		return 1;
	});
}

Json::Value ModApiMatter::baked(const NodeDefManager *ndef)
{
	if (!s_matter)
		throw BaseException("export: the game's matter never started");
	Json::Value out(Json::arrayValue);
	for (const WorldMatter::Spec &s : s_matter->plain) {
		const ContentFeatures &f = ndef->get(s.node);
		Json::Value b;
		b["node"] = s.node;
		b["matter"] = s.matter;
		b["share"] = s.share;
		b["liquid"] = s.state == matter::State::Liquid;
		b["form"] = s.form;
		b["flowing"] = s.flowing;
		b["vessel"] = s.vessel;
		b["leveled"] = s.leveled;
		b["loose"] = s.loose;
		// (top, bottom, side: as tiledef's order has them)
		for (int t : { 0, 1, 2 })
			b["tiles"].append(f.tiledef[t].name);
		if (f.node_box.type == NODEBOX_FIXED)
			for (const aabb3f &box : f.node_box.fixed) {
				Json::Value part(Json::arrayValue);
				for (float v : { box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z, box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z })
					part.append(v / BS + 0.5f); // from the block's corner
				b["boxes"].append(part);
			}
		out.append(b);
	}
	return out;
}

int ModApiMatter::l_substances(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "substances", [&] {
		const matter::Matter &t = made(L).table;
		lua_createtable(L, (int)t.substances().size(), 0);
		int n = 1;
		for (const matter::Substance &s : t.substances()) {
			lua_createtable(L, 0, 6);
			lua_pushstring(L, s.id.c_str());
			lua_setfield(L, -2, "id");
			lua_pushstring(L, s.name.c_str());
			lua_setfield(L, -2, "name");
			lua_pushstring(L, s.formula.c_str());
			lua_setfield(L, -2, "formula");
			lua_pushstring(L, matter::STATE_NAMES[(int)s.state(293.15, matter::ATMOSPHERE)]);
			lua_setfield(L, -2, "state");
			char hex[8];
			snprintf(hex, sizeof hex, "#%06x", s.colour);
			lua_pushstring(L, hex);
			lua_setfield(L, -2, "colour");
			setNumber(L, "molar_mass", s.molar_mass);
			setNumber(L, "density", s.density(s.state(293.15, matter::ATMOSPHERE), 293.15, matter::ATMOSPHERE));
			setNumber(L, "melts", s.meltingPoint(std::max(matter::ATMOSPHERE, s.triple.p)));
			if (std::optional<double> b = s.boilingPoint(matter::ATMOSPHERE))
				setNumber(L, "boils", *b);
			lua_rawseti(L, -2, n++);
		}
		return 1;
	});
}

int ModApiMatter::l_touch(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "touch", [&] {
		started(L).world->touch(pos(L, 1), luaL_checknumber(L, 2));
		return 0;
	});
}

int ModApiMatter::l_inspect(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "inspect", [&] {
		ServerMatter &m = started(L);
		Pos p = pos(L, 1);
		std::optional<WorldMatter::Awake> a = m.world->inspect(p);
		if (!a) {
			lua_pushnil(L);
			return 1;
		}
		lua_createtable(L, 0, 8);
		setNumber(L, "temperature", a->temperature);
		setNumber(L, "activity", a->activity);
		setNumber(L, "fill", a->fill);
		setNumber(L, "haze", a->haze);
		if (a->main >= 0) {
			lua_pushstring(L, m.table.substances()[a->main].id.c_str());
			lua_setfield(L, -2, "substance");
			lua_pushstring(L, matter::STATE_NAMES[(int)a->state]);
			lua_setfield(L, -2, "state");
		}
		if (a->vapour >= 0) {
			lua_pushstring(L, m.table.substances()[a->vapour].id.c_str());
			lua_setfield(L, -2, "vapour");
		}
		std::vector<WorldMatter::Awake> one = m.world->awake(p, p);
		lua_pushboolean(L, !one.empty());
		lua_setfield(L, -2, "awake");
		// what's in it, most first, and how it looks (sprint 79)
		if (std::optional<matter::Mixture> mix = m.world->mixture(p)) {
			char hex[8];
			snprintf(hex, sizeof hex, "#%06x", mix->colour());
			lua_pushstring(L, hex);
			lua_setfield(L, -2, "colour");
			std::vector<size_t> order;
			for (size_t i = 0; i < mix->amounts.size(); i++)
				if (mix->amounts[i] > 1e-9)
					order.push_back(i);
			std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return mix->amounts[a] > mix->amounts[b]; });
			constexpr size_t SHOWN = 6;
			lua_createtable(L, (int)std::min(order.size(), SHOWN), 0);
			for (size_t n = 0; n < order.size() && n < SHOWN; n++) {
				lua_createtable(L, 0, 3);
				lua_pushstring(L, m.table.substances()[order[n]].id.c_str());
				lua_setfield(L, -2, "substance");
				setNumber(L, "moles", mix->amounts[order[n]]);
				lua_pushstring(L, matter::STATE_NAMES[(int)mix->state(order[n], matter::ATMOSPHERE)]);
				lua_setfield(L, -2, "state");
				lua_rawseti(L, -2, (int)n + 1);
			}
			lua_setfield(L, -2, "contents");
		}
		return 1;
	});
}

int ModApiMatter::l_register_family(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "register_family", [&] {
		ServerMatter &m = made(L);
		if (m.started)
			throw LuaError("core.matter.register_family: the world's matter has started; register blocks as mods load");
		std::string node = luaL_checkstring(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);
		double share = getfloatfield_default(L, 2, "share", 1.0f);
		std::string state = getstringfield_default(L, 2, "state", "solid");
		if (state != "solid" && state != "liquid")
			throw LuaError("core.matter.register_family: " + node + "'s state is solid or liquid");
		bool form = getboolfield_default(L, 2, "form", false); // its first point is the form its matter takes
		content_t c = getServer(L)->ndef()->getId(node);
		if (c == CONTENT_IGNORE)
			throw LuaError("core.matter.register_family: no node called " + node);
		lua_getfield(L, 2, "points");
		if (!lua_istable(L, -1))
			throw LuaError("core.matter.register_family: " + node + " needs points");
		int count = (int)lua_objlen(L, -1);
		if (count < 1 || count > 256)
			throw LuaError("core.matter.register_family: " + node + " has 1..256 points (its param2)");
		int32_t first = -1;
		for (int i = 1; i <= count; i++) {
			lua_rawgeti(L, -1, i);
			if (!lua_istable(L, -1))
				throw LuaError("core.matter.register_family: " + node + "'s points are { id = share, … }");
			std::vector<std::pair<std::string, double>> parts;
			lua_pushnil(L);
			while (lua_next(L, -2)) {
				parts.push_back({ luaL_checkstring(L, -2), luaL_checknumber(L, -1) });
				lua_pop(L, 1);
			}
			std::sort(parts.begin(), parts.end()); // pairs() has no order: the mix is the same either way
			lua_pop(L, 1);
			int32_t kind = m.world->kind({ node + "#" + std::to_string(i - 1), "", parts, share,
				state == "solid" ? matter::State::Solid : matter::State::Liquid, form && i == 1, false });
			if (first < 0)
				first = kind;
			m.node_of.push_back({ c, (u8)(i - 1) });
		}
		lua_pop(L, 1);
		if (m.kind_of.size() <= c) {
			m.kind_of.resize(c + 1, -1);
			m.points_of.resize(c + 1, 1);
		}
		m.kind_of[c] = first;
		m.points_of[c] = count;
		lua_pushinteger(L, first);
		return 1;
	});
}

int ModApiMatter::l_heat(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "heat", [&] {
		started(L).world->heat(pos(L, 1), luaL_checknumber(L, 2));
		return 0;
	});
}

// core.matter.add(pos, substance, moles[, temperature]): matter put into a
// block (sprint 79), at the weather's temperature if none's given
int ModApiMatter::l_add(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "add", [&] {
		ServerMatter &m = started(L);
		const Pos p = pos(L, 1);
		const size_t substance = m.table.index(luaL_checkstring(L, 2));
		const double temperature = lua_isnoneornil(L, 4) ? m.world->temperature(p) : luaL_checknumber(L, 4);
		m.world->add(p, substance, luaL_checknumber(L, 3), temperature);
		return 0;
	});
}

// core.matter.pour(pos, id, moles[, T]): as add, but it heaps and runs (sprint 80)
int ModApiMatter::l_pour(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "pour", [&] {
		ServerMatter &m = started(L);
		const Pos p = pos(L, 1);
		const size_t substance = m.table.index(luaL_checkstring(L, 2));
		const double temperature = lua_isnoneornil(L, 4) ? m.world->temperature(p) : luaL_checknumber(L, 4);
		m.world->pour(p, substance, luaL_checknumber(L, 3), temperature);
		return 0;
	});
}

// core.matter.set_fill(pos, id, share) (sprint 81): liquid lying in a node's
// open space as the world keeps it asleep (generation filling the shore; nil
// or 0 empties it). Its colour is the substance's own.
int ModApiMatter::l_set_fill(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "set_fill", [&] {
		ServerMatter &m = made(L);
		const v3s16 p = check_v3s16(L, 1);
		fill::Fill f;
		if (!lua_isnoneornil(L, 2)) {
			const std::string id = luaL_checkstring(L, 2);
			const matter::Substance &sub = m.table.substances()[m.table.index(id)]; // throws on an unknown id
			const double share = luaL_checknumber(L, 3);
			if (share < 0 || share > 1)
				throw LuaError("core.matter.set_fill: a share of its open space, 0..1");
			f.liquid = fill::liquid(sub.id);
			f.amount = (u8)std::lround(share * 255);
			f.r = (sub.colour >> 16) & 0xFF;
			f.g = (sub.colour >> 8) & 0xFF;
			f.b = sub.colour & 0xFF;
		}
		getServer(L)->getEnv().getMap().setFill(p, f);
		return 0;
	});
}

// core.matter.stream(from, to, colour, cups) (sprint 82): liquid poured
// from a spout to where it lands, for the clients near to draw falling (each
// cup a sixteenth-cube drop). Only a picture: what lands is core.matter.pour's.
int ModApiMatter::l_stream(lua_State *L)
{
	return guarded(L, "stream", [&] {
		const v3f from = check_v3f(L, 1), to = check_v3f(L, 2);
		video::SColor colour;
		if (!read_color(L, 3, &colour))
			throw LuaError("core.matter.stream: a colour");
		const lua_Number cups = luaL_checknumber(L, 4);
		if (cups < 0)
			throw LuaError("core.matter.stream: cups, 0 or more");
		getServer(L)->sendStream(from, to, colour, (u16)std::min<lua_Number>(cups, 65535));
		return 0;
	});
}

// core.matter.open(pos) (sprint 82): the share of a node's space that's
// open (cut away, or smoothed ground's air): where liquid can lie in it
int ModApiMatter::l_open(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "open", [&] {
		const sandbox::Room room = roomAt(getServer(L)->getEnv().getMap(), getServer(L)->ndef(), check_v3s16(L, 1));
		lua_pushnumber(L, room.total() / 4096.0);
		return 1;
	});
}

// core.matter.ground_changed(pos, oldnode) (sprint 85): a node was dug or
// built there; the loose ground round it keeps its shape and slides as it can
int ModApiMatter::l_ground_changed(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "ground_changed", [&] {
		const v3s16 p = check_v3s16(L, 1);
		const MapNode was = readnode(L, 2);
		getServer(L)->groundChanged(p, was);
		return 0;
	});
}

// core.matter.get_fill(pos): the liquid lying in a node's open space,
// { id, share } (of its open space), or nil (sprint 81)
int ModApiMatter::l_get_fill(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	const fill::Fill *f = getServer(L)->getEnv().getMap().getFill(check_v3s16(L, 1));
	if (!f)
		return 0;
	lua_newtable(L);
	lua_pushstring(L, fill::name(f->liquid).c_str());
	lua_setfield(L, -2, "id");
	setNumber(L, "share", f->amount / 255.0);
	return 1;
}

// core.matter.disturb(pos): something changed there; what can run or slide wakes
int ModApiMatter::l_disturb(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "disturb", [&] {
		started(L).world->disturb(pos(L, 1));
		return 0;
	});
}

int ModApiMatter::l_blow(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "blow", [&] {
		started(L).world->blow(pos(L, 1), luaL_checknumber(L, 2));
		return 0;
	});
}

int ModApiMatter::l_step(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "step", [&] {
		started(L).world->step(luaL_checknumber(L, 1));
		return 0;
	});
}

int ModApiMatter::l_ambient(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	double k = luaL_checknumber(L, 2);
	if (k <= 0)
		throw LuaError("core.matter.ambient: a temperature above 0 K");
	made(L).ambient[cube(pos(L, 1))] = k;
	return 0;
}

int ModApiMatter::l_temperature(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "temperature", [&] {
		lua_pushnumber(L, started(L).world->temperature(pos(L, 1)));
		return 1;
	});
}

int ModApiMatter::l_capacity(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "capacity", [&] {
		double c = started(L).world->capacity(pos(L, 1));
		if (c < 0)
			lua_pushnil(L);
		else
			lua_pushnumber(L, c);
		return 1;
	});
}

int ModApiMatter::l_awake(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "awake", [&] {
		ServerMatter &m = started(L);
		std::vector<WorldMatter::Awake> all = m.world->awake(pos(L, 1), pos(L, 2));
		lua_createtable(L, (int)all.size(), 0);
		int n = 1;
		for (const WorldMatter::Awake &a : all) {
			lua_createtable(L, 0, 8);
			push_v3s16(L, v3s16(a.pos.x, a.pos.y, a.pos.z));
			lua_setfield(L, -2, "pos");
			setNumber(L, "temperature", a.temperature);
			setNumber(L, "activity", a.activity);
			if (a.main >= 0) {
				lua_pushstring(L, m.table.substances()[a.main].id.c_str());
				lua_setfield(L, -2, "substance");
				lua_pushstring(L, matter::STATE_NAMES[(int)a.state]);
				lua_setfield(L, -2, "state");
			}
			setNumber(L, "fill", a.fill);
			if (a.vapour >= 0) {
				lua_pushstring(L, m.table.substances()[a.vapour].id.c_str());
				lua_setfield(L, -2, "vapour");
			}
			setNumber(L, "haze", a.haze);
			lua_rawseti(L, -2, n++);
		}
		return 1;
	});
}

int ModApiMatter::l_happened(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "happened", [&] {
		ServerMatter &m = started(L);
		std::vector<std::pair<Pos, size_t>> all = m.world->happened();
		lua_createtable(L, (int)all.size(), 0);
		int n = 1;
		for (const auto &[p, j] : all) {
			lua_createtable(L, 0, 2);
			push_v3s16(L, v3s16(p.x, p.y, p.z));
			lua_setfield(L, -2, "pos");
			lua_pushstring(L, m.table.reactions()[j].id.c_str());
			lua_setfield(L, -2, "reaction");
			lua_rawseti(L, -2, n++);
		}
		return 1;
	});
}

int ModApiMatter::l_count(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushinteger(L, s_matter ? (lua_Integer)s_matter->world->awakeCount() : 0);
	return 1;
}

int ModApiMatter::l_save(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	if (!s_matter || !s_matter->started)
		return 0; // nothing has run: nothing to save
	std::string tmp = s_matter->path + ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		std::string bytes = s_matter->world->save();
		out.write(bytes.data(), bytes.size());
		if (!out)
			throw LuaError("core.matter.save: couldn't write " + tmp);
	}
	if (std::rename(tmp.c_str(), s_matter->path.c_str()) != 0)
		throw LuaError("core.matter.save: couldn't replace " + s_matter->path);
	return 0;
}

int ModApiMatter::l_density(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "density", [&] {
		ServerMatter &m = made(L);
		const std::string node = luaL_checkstring(L, 1);
		const int param2 = (int)luaL_optinteger(L, 2, 0);
		const content_t c = getServer(L)->ndef()->getId(node);
		if (c == CONTENT_IGNORE)
			throw LuaError("core.matter.density: no node called " + node);
		if (c >= m.kind_of.size() || m.kind_of[c] < 0) {
			lua_pushnil(L);
			return 1;
		}
		lua_pushnumber(L, m.world->density(m.kind_of[c] + std::clamp(param2, 0, m.points_of[c] - 1)));
		return 1;
	});
}

void ModApiMatter::shutdown()
{
	s_matter.reset();
}

void ModApiMatter::Initialize(lua_State *L, int top)
{
	lua_newtable(L);
	int t = lua_gettop(L);
	luaL_Reg functions[] = {
		{ "register_block", l_register_block },
		{ "register_family", l_register_family },
		{ "substances", l_substances },
		{ "heat", l_heat },
		{ "touch", l_touch },
		{ "inspect", l_inspect },
		{ "blow", l_blow },
		{ "add", l_add },
		{ "pour", l_pour },
		{ "disturb", l_disturb },
		{ "set_fill", l_set_fill },
		{ "stream", l_stream },
		{ "open", l_open },
		{ "ground_changed", l_ground_changed },
		{ "get_fill", l_get_fill },
		{ "step", l_step },
		{ "ambient", l_ambient },
		{ "temperature", l_temperature },
		{ "capacity", l_capacity },
		{ "density", l_density },
		{ "awake", l_awake },
		{ "happened", l_happened },
		{ "count", l_count },
		{ "save", l_save },
	};
	for (const luaL_Reg &f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, t, f.name);
	}
	lua_setfield(L, top, "matter");
}
