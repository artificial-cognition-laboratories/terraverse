// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_server.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "util/numeric.h" // myrand

void ScriptApiServer::on_mods_loaded()
{
	SCRIPTAPI_PRECHECKHEADER

	// Get registered shutdown hooks
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_mods_loaded");
	// Call callbacks
	runCallbacks(0, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiServer::register_mod_block(const std::string &name, const std::string &description,
		const std::array<std::string, 6> &tiles, const std::string &matter, double share)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "register_mod_block");
	lua_remove(L, -2); // core
	if (lua_type(L, -1) != LUA_TFUNCTION)
		throw LuaError("builtin is missing core.register_mod_block");
	lua_createtable(L, 0, 5);
	lua_pushstring(L, name.c_str());
	lua_setfield(L, -2, "name");
	lua_pushstring(L, description.c_str());
	lua_setfield(L, -2, "description");
	lua_createtable(L, 6, 0);
	for (int i = 0; i < 6; i++) {
		lua_pushstring(L, tiles[i].c_str());
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "tiles");
	lua_pushstring(L, matter.c_str());
	lua_setfield(L, -2, "matter");
	lua_pushnumber(L, share);
	lua_setfield(L, -2, "share");
	PCALL_RES(lua_pcall(L, 1, 0, error_handler));
	lua_pop(L, 1); // error handler
}

void ScriptApiServer::on_shutdown()
{
	SCRIPTAPI_PRECHECKHEADER

	// Get registered shutdown hooks
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_shutdown");
	// Call callbacks
	runCallbacks(0, RUN_CALLBACKS_MODE_FIRST);
}

u32 ScriptApiServer::allocateDynamicMediaCallback(lua_State *L, int f_idx)
{
	if (f_idx < 0)
		f_idx = lua_gettop(L) + f_idx + 1;

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "dynamic_media_callbacks");
	luaL_checktype(L, -1, LUA_TTABLE);

	// Find a randomly generated token that doesn't exist yet
	int tries = 100;
	u32 token;
	while (1) {
		token = myrand();
		lua_rawgeti(L, -2, token);
		bool is_free = lua_isnil(L, -1);
		lua_pop(L, 1);
		if (is_free)
			break;
		if (--tries < 0)
			FATAL_ERROR("Ran out of callbacks IDs?!");
	}

	// core.dynamic_media_callbacks[token] = callback_func
	lua_pushvalue(L, f_idx);
	lua_rawseti(L, -2, token);

	lua_pop(L, 2);

	verbosestream << "allocateDynamicMediaCallback() = " << token << std::endl;
	return token;
}

void ScriptApiServer::freeDynamicMediaCallback(u32 token)
{
	SCRIPTAPI_PRECHECKHEADER

	verbosestream << "freeDynamicMediaCallback(" << token << ")" << std::endl;

	// core.dynamic_media_callbacks[token] = nil
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "dynamic_media_callbacks");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushnil(L);
	lua_rawseti(L, -2, token);
	lua_pop(L, 2);
}

void ScriptApiServer::on_dynamic_media_added(u32 token, const std::string &playername)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "dynamic_media_callbacks");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_rawgeti(L, -1, token);
	luaL_checktype(L, -1, LUA_TFUNCTION);

	lua_pushstring(L, playername.c_str());
	PCALL_RES(lua_pcall(L, 1, 0, error_handler));
}
