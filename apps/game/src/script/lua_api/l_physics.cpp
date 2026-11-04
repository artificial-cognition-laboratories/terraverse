// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_physics.h"
#include "common/c_converter.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_object.h"
#include "nodedef.h"
#include "server.h"
#include "server/serveractiveobject.h"
#include "workshop/knap.h"
#include "workshop/small.h"
#include "workshop/workshop.h"
#include <stdexcept>

// The object at `index`'s id; throws on what isn't a live object.
static u16 objectId(lua_State *L, int index)
{
	ObjectRef *ref = ModApiBase::checkObject<ObjectRef>(L, index);
	ServerActiveObject *obj = ObjectRef::getobject(ref);
	if (!obj)
		throw LuaError("core.physics: that object is gone");
	return obj->getId();
}

static workshop::Motion *motion(lua_State *L)
{
	workshop::Workshop *w = ModApiBase::getServer(L)->getWorkshop();
	return w ? &w->motion() : nullptr;
}

int ModApiPhysics::l_material(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const std::string node = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_getfield(L, 2, "density");
	lua_getfield(L, 2, "friction");
	if (!lua_isnumber(L, -2) || !lua_isnumber(L, -1))
		throw LuaError("core.physics.material: " + node + " needs a density and a friction");
	workshop::Material m{ (float)lua_tonumber(L, -2), (float)lua_tonumber(L, -1) };
	lua_pop(L, 2);
	// what it does in a machine (sprint 69): `role` by name, a motor's
	// `power` (kW) and `rpm` (sprint 78)
	lua_getfield(L, 2, "role");
	if (!lua_isnil(L, -1)) {
		const std::string role = luaL_checkstring(L, -1);
		size_t r = 0;
		while (r < size_t(workshop::Role::Count) && role != workshop::ROLES[r].id)
			r++;
		if (r == size_t(workshop::Role::Count))
			throw LuaError("core.physics.material: " + node + ": no role called " + role);
		m.role = workshop::Role(r);
	}
	lua_pop(L, 1);
	lua_getfield(L, 2, "power");
	if (!lua_isnil(L, -1))
		m.power = (float)luaL_checknumber(L, -1) * 1000;
	lua_pop(L, 1);
	lua_getfield(L, 2, "rpm");
	if (!lua_isnil(L, -1))
		m.rpm = (float)luaL_checknumber(L, -1);
	lua_pop(L, 1);
	// what it takes before it fails (Pa): `tension`, and `compression`
	lua_getfield(L, 2, "tension");
	if (!lua_isnil(L, -1))
		m.tension = (float)luaL_checknumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "compression");
	if (!lua_isnil(L, -1))
		m.compression = (float)luaL_checknumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "toughness");
	if (!lua_isnil(L, -1))
		m.toughness = (float)luaL_checknumber(L, -1);
	lua_pop(L, 1);
	const content_t c = getServer(L)->ndef()->getId(node);
	if (c == CONTENT_IGNORE)
		throw LuaError("core.physics.material: no node called " + node);
	try {
		getServer(L)->getMaterials().set(c, m);
	} catch (const std::invalid_argument &e) {
		throw LuaError("core.physics.material: " + node + ": " + e.what());
	}
	return 0;
}

int ModApiPhysics::l_velocity(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	const u16 id = objectId(L, 1);
	workshop::Motion *m = motion(L);
	const std::optional<v3f> v = m ? m->velocity(id) : std::nullopt;
	if (!v)
		lua_pushnil(L);
	else
		push_v3f(L, *v / BS);
	return 1;
}

int ModApiPhysics::l_mass(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	const u16 id = objectId(L, 1);
	workshop::Motion *m = motion(L);
	const std::optional<float> kg = m ? m->mass(id) : std::nullopt;
	if (!kg)
		lua_pushnil(L);
	else
		lua_pushnumber(L, *kg);
	return 1;
}

int ModApiPhysics::l_impulse(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	const u16 id = objectId(L, 1);
	const v3f push = check_v3f(L, 2);
	workshop::Motion *m = motion(L);
	if (!m)
		throw LuaError("core.physics.impulse: there's no world yet");
	try {
		m->impulse(id, push * BS);
	} catch (const std::invalid_argument &e) {
		throw LuaError(std::string("core.physics.impulse: ") + e.what());
	}
	return 0;
}

// core.physics.blast(pos, power): pos in nodes, power in Pa·s a node off; { node, radius } of
// the crater it dug, if it dug one
int ModApiPhysics::l_blast(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	const v3f at = check_v3f(L, 1);
	const float power = (float)luaL_checknumber(L, 2);
	workshop::Motion *m = motion(L);
	if (!m)
		throw LuaError("core.physics.blast: there's no world yet");
	const workshop::Motion::Crater crater = m->blast(at, power);
	if (crater.radius <= 0)
		return 0;
	lua_newtable(L);
	lua_pushstring(L, getServer(L)->ndef()->get(crater.material).name.c_str());
	lua_setfield(L, -2, "node");
	lua_pushnumber(L, crater.radius);
	lua_setfield(L, -2, "radius");
	return 1;
}

// A small thing's text (sprint 130), to be an entity's: what it's made of, its size, a seed.
static content_t smallMaterial(lua_State *L, const char *name)
{
	const NodeDefManager *ndef = ModApiBase::getServer(L)->ndef();
	content_t id;
	if (!ndef->getId(name, id))
		throw LuaError(std::string("core.physics: no block ") + name);
	return id;
}

int ModApiPhysics::l_small_stone(lua_State *L)
{
	const content_t material = smallMaterial(L, luaL_checkstring(L, 1));
	const v3s16 half = read_v3s16(L, 2);
	try {
		const std::string text = workshop::stone(getServer(L), material, half, (u32)luaL_checkinteger(L, 3)).write();
		lua_pushlstring(L, text.data(), text.size());
	} catch (const std::invalid_argument &e) {
		throw LuaError(std::string("core.physics.small_stone: ") + e.what());
	}
	return 1;
}

int ModApiPhysics::l_small_stick(lua_State *L)
{
	const content_t material = smallMaterial(L, luaL_checkstring(L, 1));
	try {
		const std::string text = workshop::stick(getServer(L), material, (s16)luaL_checkinteger(L, 2),
				(s16)luaL_checkinteger(L, 3), (u32)luaL_checkinteger(L, 4)).write();
		lua_pushlstring(L, text.data(), text.size());
	} catch (const std::invalid_argument &e) {
		throw LuaError(std::string("core.physics.small_stick: ") + e.what());
	}
	return 1;
}

static piece::Machine readSmall(lua_State *L, int index)
{
	size_t length;
	const char *text = luaL_checklstring(L, index, &length);
	piece::Machine m(ModApiBase::getServer(L));
	try {
		m.read(std::string(text, length));
	} catch (const SerializationError &e) {
		throw LuaError(std::string("core.physics: not a thing: ") + e.what());
	}
	return m;
}

// knap(text, cell, direction, striker_kg, clean, seed): how much came off, and
// the texts of what's left, the biggest first.
int ModApiPhysics::l_knap(lua_State *L)
{
	const piece::Machine m = readSmall(L, 1);
	const v3s16 cell = read_v3s16(L, 2);
	const v3f in = check_v3f(L, 3);
	try {
		const workshop::Struck s = workshop::strike(m, v3s32(cell.X, cell.Y, cell.Z), in, luaL_checknumber(L, 4),
				luaL_checknumber(L, 5), (u32)luaL_checkinteger(L, 6), getServer(L));
		lua_newtable(L);
		lua_pushinteger(L, s.removed);
		lua_setfield(L, -2, "removed");
		lua_newtable(L);
		for (size_t i = 0; i < s.left.size(); i++) {
			const std::string text = s.left[i].write();
			lua_pushlstring(L, text.data(), text.size());
			lua_rawseti(L, -2, i + 1);
		}
		lua_setfield(L, -2, "left");
	} catch (const std::invalid_argument &e) {
		throw LuaError(std::string("core.physics.knap: ") + e.what());
	}
	return 1;
}

// How good its edge is (0 none, 1 rough, 2 fine), and what it's made of.
int ModApiPhysics::l_edge(lua_State *L)
{
	const piece::Machine m = readSmall(L, 1);
	try {
		lua_pushinteger(L, workshop::edgeOf(m));
	} catch (const std::invalid_argument &e) {
		throw LuaError(std::string("core.physics.edge: ") + e.what());
	}
	std::string name;
	m.parts().at(0).object.built().each([&](v3s16, MapNode n, const shape::Shape *) {
		name = getServer(L)->ndef()->get(n).name;
	});
	lua_pushstring(L, name.c_str());
	return 2;
}

void ModApiPhysics::Initialize(lua_State *L, int top)
{
	lua_newtable(L);
	int t = lua_gettop(L);
	luaL_Reg functions[] = {
		{ "material", l_material },
		{ "velocity", l_velocity },
		{ "mass", l_mass },
		{ "impulse", l_impulse },
		{ "blast", l_blast },
		{ "small_stone", l_small_stone },
		{ "small_stick", l_small_stick },
		{ "knap", l_knap },
		{ "edge", l_edge },
	};
	for (const luaL_Reg &f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, t, f.name);
	}
	lua_setfield(L, top, "physics");
}
