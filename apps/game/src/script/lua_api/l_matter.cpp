// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_matter.h"
#include "server/matter.h"
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
		const std::string node = luaL_checkstring(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);
		MatterBlock b;
		b.matter = getstringfield_default(L, 2, "matter", "");
		b.share = getfloatfield_default(L, 2, "share", 1.0f);
		const std::string state = getstringfield_default(L, 2, "state", "solid");
		if (state != "solid" && state != "liquid")
			throw LuaError("core.matter.register_block: " + node + "'s state is solid or liquid");
		b.state = state == "solid" ? matter::State::Solid : matter::State::Liquid;
		b.form = getboolfield_default(L, 2, "form", false);
		b.flowing = getboolfield_default(L, 2, "flowing", false);
		b.vessel = getboolfield_default(L, 2, "vessel", false);
		b.loose = getboolfield_default(L, 2, "loose", false);
		lua_pushinteger(L, serverMatterBlock(getServer(L), node, b));
		return 1;
	});
}

Json::Value ModApiMatter::baked(const NodeDefManager *ndef)
{
	ServerMatter *matter = serverMatterIfMade();
	if (!matter)
		throw BaseException("export: the game's matter never started");
	Json::Value out(Json::arrayValue);
	for (const WorldMatter::Spec &s : matter->plain) {
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
		const matter::Matter &t = serverMatter(getServer(L)).table;
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
			if (s.transparent) {
				lua_pushboolean(L, true);
				lua_setfield(L, -2, "transparent");
			}
			if (s.strength) {
				lua_createtable(L, 0, 3);
				setNumber(L, "tension", s.strength->tension);
				setNumber(L, "compression", s.strength->compression);
				setNumber(L, "toughness", s.strength->toughness);
				lua_setfield(L, -2, "strength");
			}
			if (s.friction)
				setNumber(L, "friction", *s.friction);
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
		serverMatterStarted(getServer(L)).world->touch(pos(L, 1), luaL_checknumber(L, 2));
		return 0;
	});
}

int ModApiMatter::l_inspect(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "inspect", [&] {
		ServerMatter &m = serverMatterStarted(getServer(L));
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
		const std::string node = luaL_checkstring(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);
		MatterFamily f;
		f.share = getfloatfield_default(L, 2, "share", 1.0f);
		const std::string state = getstringfield_default(L, 2, "state", "solid");
		if (state != "solid" && state != "liquid")
			throw LuaError("core.matter.register_family: " + node + "'s state is solid or liquid");
		f.state = state == "solid" ? matter::State::Solid : matter::State::Liquid;
		f.form = getboolfield_default(L, 2, "form", false);
		read_matter_family(L, 2, node, f);
		lua_pushinteger(L, serverMatterFamily(getServer(L), node, f));
		return 1;
	});
}

int ModApiMatter::l_heat(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "heat", [&] {
		serverMatterStarted(getServer(L)).world->heat(pos(L, 1), luaL_checknumber(L, 2));
		return 0;
	});
}

// core.matter.add(pos, substance, moles[, temperature]): matter put into a
// block (sprint 79), at the weather's temperature if none's given
int ModApiMatter::l_add(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "add", [&] {
		ServerMatter &m = serverMatterStarted(getServer(L));
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
		ServerMatter &m = serverMatterStarted(getServer(L));
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
		ServerMatter &m = serverMatter(getServer(L));
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
		const sandbox::Room room = matterRoomAt(getServer(L)->getEnv().getMap(), getServer(L)->ndef(), check_v3s16(L, 1));
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
		serverMatterStarted(getServer(L)).world->disturb(pos(L, 1));
		return 0;
	});
}

int ModApiMatter::l_blow(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "blow", [&] {
		serverMatterStarted(getServer(L)).world->blow(pos(L, 1), luaL_checknumber(L, 2));
		return 0;
	});
}

int ModApiMatter::l_step(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	return guarded(L, "step", [&] {
		serverMatterStarted(getServer(L)).world->step(luaL_checknumber(L, 1));
		return 0;
	});
}

int ModApiMatter::l_ambient(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	double k = luaL_checknumber(L, 2);
	if (k <= 0)
		throw LuaError("core.matter.ambient: a temperature above 0 K");
	serverMatter(getServer(L)).ambient[matterCube(pos(L, 1))] = k;
	return 0;
}

// core.matter.wetness(pos, w): how wet the ground is round pos (its 16-block
// cube), 0 dry .. 1 as wet as it gets (sprint 111): the pores of soil hold
// that share of the water their texture can. From the weather, as ambient.
int ModApiMatter::l_wetness(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	double w = luaL_checknumber(L, 2);
	if (!(w >= 0 && w <= 1))
		throw LuaError("core.matter.wetness: 0..1");
	serverMatter(getServer(L)).wetness[matterCube(pos(L, 1))] = w;
	return 0;
}

int ModApiMatter::l_temperature(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "temperature", [&] {
		lua_pushnumber(L, serverMatterStarted(getServer(L)).world->temperature(pos(L, 1)));
		return 1;
	});
}

int ModApiMatter::l_capacity(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "capacity", [&] {
		double c = serverMatterStarted(getServer(L)).world->capacity(pos(L, 1));
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
		ServerMatter &m = serverMatterStarted(getServer(L));
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
		ServerMatter &m = serverMatterStarted(getServer(L));
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
	lua_pushinteger(L, serverMatterIfMade() ? (lua_Integer)serverMatterIfMade()->world->awakeCount() : 0);
	return 1;
}

int ModApiMatter::l_save(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	try {
		serverMatterSave();
	} catch (const BaseException &e) {
		throw LuaError(std::string("core.matter.save: ") + e.what());
	}
	return 0;
}

int ModApiMatter::l_density(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return guarded(L, "density", [&] {
		ServerMatter &m = serverMatter(getServer(L));
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
	serverMatterShutdown();
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
		{ "wetness", l_wetness },
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
