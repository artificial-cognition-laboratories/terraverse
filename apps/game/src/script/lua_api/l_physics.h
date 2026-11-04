// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

/*
	core.physics: machines move as rigid bodies (sprint 57, src/physics on
	Jolt; the workshop compiles machines into it). Content says what its
	blocks weigh; gameplay asks how things move and pushes them. Server only.

	  material(node, { density = kg/m³, friction = 0..1+ })
	                     what a block weighs and how it grips, in a machine
	                     (as mods load; a machine of a block with none can't move)
	  velocity(object) → vector (nodes/s) | nil (not a machine physics moves)
	  mass(object) → kg | nil
	  impulse(object, vector)   a push (kg·nodes/s) at its middle
*/
class ModApiPhysics : public ModApiBase
{
private:
	static int l_material(lua_State *L);
	static int l_velocity(lua_State *L);
	static int l_mass(lua_State *L);
	static int l_impulse(lua_State *L);
	static int l_blast(lua_State *L);
	static int l_small_stone(lua_State *L);
	static int l_small_stick(lua_State *L);
	static int l_knap(lua_State *L);
	static int l_edge(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
