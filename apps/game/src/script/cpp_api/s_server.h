// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include <array>
#include <set>

class ScriptApiServer
		: virtual public ScriptApiBase
{
public:
	// Calls when mods are loaded
	void on_mods_loaded();

	// Terra Firma: a TypeScript mod's block, registered through core.register_mod_block
	// (builtin/game/sidecar.lua) so Lua's systems see it as one of their own (sprint 119)
	void register_mod_block(const std::string &name, const std::string &description,
			const std::array<std::string, 6> &tiles, const std::string &matter, double share);

	// Calls on_shutdown handlers
	void on_shutdown();

	/* dynamic media handling */
	static u32 allocateDynamicMediaCallback(lua_State *L, int f_idx);
	void freeDynamicMediaCallback(u32 token);
	void on_dynamic_media_added(u32 token, const std::string &playername);
};
