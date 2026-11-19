// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

/*
	core.planet: the planet the world is (src/planet/atlas.h), for game Lua.
	Server only. atlas() is nil on a world that isn't one; the other calls
	answer about its cells, places and nodes.
*/
class ModApiPlanet : public ModApiBase
{
private:
	static int l_atlas(lua_State *L);
	static int l_atlas_cell(lua_State *L);
	static int l_bodies(lua_State *L);
	static int l_body_at(lua_State *L);
	static int l_place(lua_State *L);
	static int l_pos(lua_State *L);
	static int l_atlas_place(lua_State *L);
	static int l_atlas_node(lua_State *L);
	static int l_spawn_level(lua_State *L);
	static int l_atlas_point(lua_State *L);
	static int l_spawn_position(lua_State *L);
	static int l_travel(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
