// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

/*
	core.matter: the world's blocks as matter (src/sandbox/world.h). Content
	declares what its blocks are made of; gameplay heats, blows and asks.
	Server only. The awake blocks are saved as <world>/matter.bin.

	  register_block(node, { matter = id | "air", share = 0..1, state = "solid" | "liquid", form = bool,
	                         flowing = bool (a liquid the engine flows itself) })
	  register_family(node, { share, state, form, points = { { id = share, … }, … } })
	                               a node whose param2 picks one of its points: each a mix of
	                               substances and rocks by share of the solid part
	  substances() → { { id, name, formula, state (at 20 °C, 1 atm), melts, boils }, … }
	  heat(pos, kelvin)            bring a block to at least a temperature (a spark)
	  touch(pos, kelvin)           set a block's temperature exactly (a heat gun)
	  inspect(pos) → { temperature, substance, state, fill, activity, vapour, haze, awake } | nil
	  blow(pos, moles)             push the room's air into it (a bellows)
	  step(seconds)                run the awake blocks (the game calls it, from a globalstep)
	  ambient(pos, kelvin)         the weather's temperature for that 16-block cube
	  temperature(pos) → kelvin
	  capacity(pos) → J/K the block's matter holds (nil: no matter registered)
	  density(node [, param2]) → kg/m³ of a registered block at room temperature (nil: not registered)
	  awake(minp, maxp) → { { pos, temperature, activity, substance, state, fill, vapour, haze }, … }
	  happened() → { { pos, reaction }, … } (reactions that ran since the last call)
	  count() → blocks awake
	  save()
*/
class NodeDefManager;
namespace Json { class Value; }

class ModApiMatter : public ModApiBase
{
private:
	static int l_register_block(lua_State *L);
	static int l_register_family(lua_State *L);
	static int l_substances(lua_State *L);
	static int l_heat(lua_State *L);
	static int l_touch(lua_State *L);
	static int l_add(lua_State *L);
	static int l_pour(lua_State *L);
	static int l_disturb(lua_State *L);
	static int l_set_fill(lua_State *L);
	static int l_stream(lua_State *L);
	static int l_open(lua_State *L);
	static int l_ground_changed(lua_State *L);
	static int l_get_fill(lua_State *L);
	static int l_inspect(lua_State *L);
	static int l_blow(lua_State *L);
	static int l_step(lua_State *L);
	static int l_ambient(lua_State *L);
	static int l_wetness(lua_State *L);
	static int l_temperature(lua_State *L);
	static int l_capacity(lua_State *L);
	static int l_density(lua_State *L);
	static int l_awake(lua_State *L);
	static int l_happened(lua_State *L);
	static int l_count(lua_State *L);
	static int l_save(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
	// a process runs one server after another: the next world starts its matter afresh
	static void shutdown();
	// The plain blocks the game registered, for scenes to build from without Lua (far/export.cpp).
	static Json::Value baked(const NodeDefManager *ndef);
};
