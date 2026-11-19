// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terragen/worlds.h"
#include "terragen/system.h"
#include "lua_api/l_planet.h"
#include "common/c_converter.h"
#include "lua_api/l_internal.h"
#include "planet/atlas.h"
#include "emerge.h"
#include "mapgen/mapgen.h"
#include "server.h"
#include "server/terra.h"
#include "lua_api/l_object.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include <cmath>

static void pushRect(lua_State *L, const planet::Atlas::Rect &r)
{
	lua_createtable(L, 0, 4);
	lua_pushinteger(L, r.x);
	lua_setfield(L, -2, "x");
	lua_pushinteger(L, r.z);
	lua_setfield(L, -2, "z");
	lua_pushinteger(L, r.width);
	lua_setfield(L, -2, "width");
	lua_pushinteger(L, r.depth);
	lua_setfield(L, -2, "depth");
}

// The region kinds, by terragen::Atlas::Region.
static const char *const REGIONS[] = { "segment", "cap", "core" };

// An atlas world's body by its id (core.planet.bodies); raises for none.
static int bodyNamed(Server *server, const std::string &id)
{
	if (!server->atlas())
		throw LuaError("core.planet: this world isn't an atlas planet");
	const std::vector<terragen::BodyWorld> &worlds = server->bodyWorlds();
	for (size_t b = 0; b < worlds.size(); b++)
		if (worlds[b].id == id)
			return (int)b;
	throw LuaError("core.planet: this world has no body '" + id + "' to stand on");
}

// core.planet.atlas() → nil, or { code, radius, core, top, bodies, tiles }:
// the home body's mapping code; its radius and the core's (blocks); one
// past its highest layer; how many bodies the atlas holds; and where each
// tile in the window is stored: tiles, each { body (its id), kind = "segment" |
// "cap" | "core", index, x, z, width, depth, y0, y1, i0, j0 } (y0..y1
// storage y, exclusive; i0, j0 the grid's cell at x, z).
int ModApiPlanet::l_atlas(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	if (!a) {
		lua_pushnil(L);
		return 1;
	}
	const terragen::Atlas &atlas = a->body(planet::Atlas::HOME);
	lua_createtable(L, 0, 7);
	lua_pushinteger(L, a->bodies());
	lua_setfield(L, -2, "bodies");
	lua_pushstring(L, atlas.code().c_str());
	lua_setfield(L, -2, "code");
	lua_pushnumber(L, atlas.params().radius);
	lua_setfield(L, -2, "radius");
	lua_pushnumber(L, atlas.params().core);
	lua_setfield(L, -2, "core");
	lua_pushinteger(L, atlas.params().height);
	lua_setfield(L, -2, "top");
	lua_createtable(L, (int)a->tiles().size(), 0);
	int n = 1;
	for (const planet::Atlas::Tile &r : a->tiles()) {
		pushRect(L, r.rect);
		lua_pushstring(L, getServer(L)->bodyWorlds().at(r.body).id.c_str());
		lua_setfield(L, -2, "body");
		lua_pushstring(L, REGIONS[(int)r.kind]);
		lua_setfield(L, -2, "kind");
		lua_pushinteger(L, r.index);
		lua_setfield(L, -2, "index");
		lua_pushinteger(L, r.k0);
		lua_setfield(L, -2, "y0");
		lua_pushinteger(L, r.k1 + 1);
		lua_setfield(L, -2, "y1");
		lua_pushinteger(L, r.i0);
		lua_setfield(L, -2, "i0");
		lua_pushinteger(L, r.j0);
		lua_setfield(L, -2, "j0");
		lua_rawseti(L, -2, n++);
	}
	lua_setfield(L, -2, "tiles");
	return 1;
}

// core.planet.atlas_place(pos) → nil (seam, the core, not an atlas), or
// latitude, longitude (radians): where a node's column is.
int ModApiPlanet::l_atlas_place(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3s16 p = check_v3s16(L, 1);
	const auto at = a ? a->surface({ p.X, p.Y, p.Z }) : std::nullopt;
	if (!at) {
		lua_pushnil(L);
		return 1;
	}
	const double x = at->x, y = at->y, z = at->z;
	lua_pushnumber(L, std::atan2(y, std::hypot(x, z)));
	lua_pushnumber(L, std::atan2(z, x));
	return 2;
}

// core.planet.atlas_point(pos) → nil, or { x, y, z }: where a storage
// position (fractional) is on the planet, blocks from its centre, in the
// grid of the node it's in.
int ModApiPlanet::l_atlas_point(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3f p = check_v3f(L, 1);
	const v3s16 n(std::lround(p.X), std::lround(p.Y), std::lround(p.Z));
	const auto at = a ? a->point({ n.X, n.Y, n.Z }, p.X, p.Y, p.Z) : std::nullopt;
	if (!at) {
		lua_pushnil(L);
		return 1;
	}
	lua_createtable(L, 0, 3);
	lua_pushnumber(L, at->x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, at->y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, at->z);
	lua_setfield(L, -2, "z");
	return 1;
}

// core.planet.atlas_node(latitude, longitude, layer[, body]) → pos, or nil
// above the atlas: the node of the cell at that place on the body (its id;
// the home's when none is named), that layer up (every grid's layer k is the
// sphere R + k, so the cell is that layer's). Its tile is put in the map's
// window if it isn't.
int ModApiPlanet::l_atlas_node(lua_State *L)
{
	MAP_LOCK_REQUIRED; // the window may change: tiles leave the map
	const planet::Atlas *a = getServer(L)->atlas();
	if (!a)
		throw LuaError("core.planet.atlas_node: this world isn't an atlas planet");
	const double lat = luaL_checknumber(L, 1), lon = luaL_checknumber(L, 2);
	const lua_Integer layer = luaL_checkinteger(L, 3);
	const int body = lua_isnoneornil(L, 4) ? planet::Atlas::HOME : bodyNamed(getServer(L), luaL_checkstring(L, 4));
	const terragen::Atlas &atlas = a->body(body);
	if (layer >= atlas.params().height) {
		lua_pushnil(L);
		return 1;
	}
	const double r = atlas.params().radius + layer + 0.5;
	if (r <= 0) {
		lua_pushnil(L); // past the centre
		return 1;
	}
	const planet::Atlas::Cell c(body, atlas.locate({ r * std::cos(lat) * std::cos(lon), r * std::sin(lat),
			r * std::cos(lat) * std::sin(lon) }));
	const planet::Atlas::Node n = getServer(L)->planetNode(c); // its tile put in the window
	push_v3s16(L, v3s16(n.x, n.y, n.z));
	return 1;
}

// core.planet.spawn_level(latitude, longitude[, body]) → layer, or nil
// where no one spawns (the sea, a lake, a river): core.get_spawn_level at a
// place on a body (its id; the home's when none is named), without a node
// of the map (so without putting its tile in the window).
int ModApiPlanet::l_spawn_level(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const int body = lua_isnoneornil(L, 3) ? planet::Atlas::HOME : bodyNamed(getServer(L), luaL_checkstring(L, 3));
	const int level = getServer(L)->getEmergeManager()->getSpawnLevelAt(body, luaL_checknumber(L, 1), luaL_checknumber(L, 2));
	if (level >= MAX_MAP_GENERATION_LIMIT)
		lua_pushnil(L);
	else
		lua_pushinteger(L, level + 1); // as core.get_spawn_level
	return 1;
}

// core.planet.atlas_cell(pos) → nil (seam, or not an atlas world), or
// { body (its id), region = "segment" | "cap" | "core", index, i, j, k }:
// the cell the node stores, as terragen::Atlas addresses it on its body.
int ModApiPlanet::l_atlas_cell(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3s16 p = check_v3s16(L, 1);
	const std::optional<planet::Atlas::Cell> c = a ? a->cell({ p.X, p.Y, p.Z }) : std::nullopt;
	if (!c) {
		lua_pushnil(L);
		return 1;
	}
	lua_createtable(L, 0, 6);
	lua_pushstring(L, getServer(L)->bodyWorlds().at(c->body).id.c_str());
	lua_setfield(L, -2, "body");
	lua_pushstring(L, REGIONS[(int)c->region]);
	lua_setfield(L, -2, "region");
	lua_pushinteger(L, c->index);
	lua_setfield(L, -2, "index");
	for (const auto &[name, v] : { std::pair{ "i", c->i }, { "j", c->j }, { "k", c->k } }) {
		lua_pushinteger(L, v);
		lua_setfield(L, -2, name);
	}
	return 1;
}

// core.planet.spawn_position([body]) → pos: where one lands on a body (its id; the
// home's when none is named), the engine's spawn on the home. Raises with no land.
int ModApiPlanet::l_spawn_position(lua_State *L)
{
	MAP_LOCK_REQUIRED; // its tile is put in the window
	Server *server = getServer(L);
	const int body = lua_isnoneornil(L, 1) ? planet::Atlas::HOME : bodyNamed(server, luaL_checkstring(L, 1));
	const std::optional<v3s16> n = terraLanding(server, body, std::nullopt);
	if (!n)
		throw LuaError("core.planet.spawn_position: no land within 60° of the equator to stand on");
	push_v3s16(L, *n);
	return 1;
}

// core.planet.travel(player, body[, latitude, longitude]) → true, or false and why
// not: to another body (its id), near a place on it (degrees), behind the page's
// travel screen while the land there is made (server/planet.cpp).
int ModApiPlanet::l_travel(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	ServerActiveObject *object = ObjectRef::getobject(checkObject<ObjectRef>(L, 1));
	if (!object || object->getType() != ACTIVEOBJECT_TYPE_PLAYER)
		throw LuaError("core.planet.travel: not a player");
	RemotePlayer *player = static_cast<PlayerSAO *>(object)->getPlayer();
	const std::string body = luaL_checkstring(L, 2);
	std::optional<std::pair<double, double>> near;
	if (!lua_isnoneornil(L, 3))
		near = std::pair{ luaL_checknumber(L, 3), luaL_checknumber(L, 4) };
	const std::string why = terraTravel(getServer(L), player, body, near);
	lua_pushboolean(L, why.empty());
	if (why.empty())
		return 1;
	lua_pushstring(L, why.c_str());
	return 2;
}

void ModApiPlanet::Initialize(lua_State *L, int top)
{
	lua_newtable(L);
	int t = lua_gettop(L);
	luaL_Reg functions[] = {
		{ "atlas", l_atlas },
		{ "atlas_cell", l_atlas_cell },
		{ "bodies", l_bodies },
		{ "body_at", l_body_at },
		{ "place", l_place },
		{ "pos", l_pos },
		{ "atlas_place", l_atlas_place },
		{ "atlas_node", l_atlas_node },
		{ "spawn_level", l_spawn_level },
		{ "atlas_point", l_atlas_point },
		{ "spawn_position", l_spawn_position },
		{ "travel", l_travel },
	};
	for (const luaL_Reg &f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, t, f.name);
	}
	lua_setfield(L, top, "planet");
}

/*
	Places (planning/sprint-40-bodies.md): where something is on a world,
	whatever slot of the map's window holds it now. A position of the map
	means nothing once its tile leaves the window, or another body's comes:
	what a mod keeps (a bed, a home, a waypoint) it keeps as a place.

	A place is { body (its id), region (its number), i, j, k (the cell),
	x, y, z (from the cell's node's centre, in its storage axes) }.
*/

// core.planet.bodies() → { { id, name, radius, size }, … }: the bodies that
// can be stood on, the home first (radius in blocks; size its world model's
// columns round); nil on a world that isn't an atlas planet.
int ModApiPlanet::l_bodies(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	Server *server = getServer(L);
	if (!server->atlas()) {
		lua_pushnil(L);
		return 1;
	}
	const std::vector<terragen::BodyWorld> &worlds = server->bodyWorlds();
	const terragen::System system(terragen::SystemSpec::forWorld(worlds.at(planet::Atlas::HOME).code));
	lua_createtable(L, (int)worlds.size(), 0);
	for (size_t b = 0; b < worlds.size(); b++) {
		lua_createtable(L, 0, 4);
		lua_pushstring(L, worlds[b].id.c_str());
		lua_setfield(L, -2, "id");
		lua_pushstring(L, system.spec().bodies.at(worlds[b].system).name.c_str());
		lua_setfield(L, -2, "name");
		lua_pushnumber(L, worlds[b].atlas.radius);
		lua_setfield(L, -2, "radius");
		lua_pushinteger(L, worlds[b].code.size);
		lua_setfield(L, -2, "size");
		lua_rawseti(L, -2, (int)b + 1);
	}
	return 1;
}

// core.planet.body_at(pos) → the id of the body whose tile pos is in, or nil
// (seam between tiles, or not an atlas world).
int ModApiPlanet::l_body_at(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3f p = check_v3f(L, 1);
	const std::optional<int> b = a ? a->bodyAt(planet::Atlas::moverNode(p.X, p.Y, p.Z)) : std::nullopt;
	if (!b)
		lua_pushnil(L);
	else
		lua_pushstring(L, getServer(L)->bodyWorlds().at(*b).id.c_str());
	return 1;
}

// core.planet.place(pos) → the place a position of the map is, or nil
// (seam: no place; not an atlas world).
int ModApiPlanet::l_place(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3f p = check_v3f(L, 1);
	const planet::Atlas::Node n = planet::Atlas::moverNode(p.X, p.Y, p.Z);
	const std::optional<planet::Atlas::Cell> c = a ? a->cell(n) : std::nullopt;
	if (!c) {
		lua_pushnil(L);
		return 1;
	}
	lua_createtable(L, 0, 8);
	lua_pushstring(L, getServer(L)->bodyWorlds().at(c->body).id.c_str());
	lua_setfield(L, -2, "body");
	lua_pushinteger(L, a->regionNumber(c->body, c->region, c->index));
	lua_setfield(L, -2, "region");
	for (const auto &[name, v] : { std::pair{ "i", c->i }, { "j", c->j }, { "k", c->k } }) {
		lua_pushinteger(L, v);
		lua_setfield(L, -2, name);
	}
	for (const auto &[name, v] : { std::pair{ "x", p.X - n.x }, { "y", p.Y - n.y }, { "z", p.Z - n.z } }) {
		lua_pushnumber(L, v);
		lua_setfield(L, -2, name);
	}
	return 1;
}

// core.planet.pos(place) → the position of the map a place is at now, its
// tiles put in the window if they aren't. Raises for a table that isn't a
// place of this world.
int ModApiPlanet::l_pos(lua_State *L)
{
	MAP_LOCK_REQUIRED; // the window may change: tiles leave the map
	Server *server = getServer(L);
	const planet::Atlas *a = server->atlas();
	if (!a)
		throw LuaError("core.planet.pos: this world isn't an atlas planet");
	luaL_checktype(L, 1, LUA_TTABLE);
	const auto field = [&](const char *name) {
		lua_getfield(L, 1, name);
		if (!lua_isnumber(L, -1))
			throw LuaError(std::string("core.planet.pos: a place's ") + name + " is a number");
		const lua_Number v = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return v;
	};
	lua_getfield(L, 1, "body");
	if (!lua_isstring(L, -1))
		throw LuaError("core.planet.pos: a place names its body");
	const int body = bodyNamed(server, lua_tostring(L, -1));
	lua_pop(L, 1);
	std::pair<terragen::Atlas::Region, int> region;
	try {
		region = a->regionOfNumber(body, (int)field("region"));
	} catch (const std::invalid_argument &e) {
		throw LuaError(std::string("core.planet.pos: ") + e.what());
	}
	const planet::Atlas::Cell cell(body, { region.first, region.second, (int64_t)field("i"), (int64_t)field("j"),
			(int64_t)field("k") });
	planet::Atlas::Node n;
	try {
		n = server->planetNode(cell);
	} catch (const std::invalid_argument &e) {
		throw LuaError(std::string("core.planet.pos: that place isn't on its body: ") + e.what());
	}
	push_v3f(L, v3f(n.x + (float)field("x"), n.y + (float)field("y"), n.z + (float)field("z")));
	return 1;
}
