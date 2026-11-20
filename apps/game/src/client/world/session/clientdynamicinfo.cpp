// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022-3 rubenwardy <rw@rubenwardy.com>

#include "src/client/world/session/clientdynamicinfo.h"

#if CHECK_CLIENT_BUILD()

#include "src/shared/platform/settings.h"
#include "src/client/render/core/renderingengine.h"

ClientDynamicInfo ClientDynamicInfo::getCurrent()
{
	v2u32 screen_size = RenderingEngine::getWindowSize();
	f32 density = RenderingEngine::getDisplayDensity();
	f32 gui_scaling = g_settings->getFloat("gui_scaling", 0.5f, 20.0f);
	f32 hud_scaling = g_settings->getFloat("hud_scaling", 0.5f, 20.0f);
	f32 real_gui_scaling = gui_scaling * density;
	f32 real_hud_scaling = hud_scaling * density;
	bool touch_controls = false; // (no touch controls: desktop only)

	return {
			screen_size, real_gui_scaling, real_hud_scaling,
			ClientDynamicInfo::calculateMaxFSSize(screen_size, density, gui_scaling),
			touch_controls
	};
}

v2f32 ClientDynamicInfo::calculateMaxFSSize(v2u32 render_target_size, f32 density, f32 gui_scaling)
{
	// The protocol still carries the largest formspec that fits (Luanti's formula,
	// from the formspec menu that's gone): 0.53 inch a cell, or a fifteenth of the
	// screen's short side, whichever is bigger.
	const double screen_dpi = density * 96;
	const double fixed_imgsize = 0.5555 * screen_dpi * gui_scaling;
	const s32 min_screen_dim = std::min(render_target_size.X, render_target_size.Y);
	const double prefer_imgsize = std::max(min_screen_dim / 15 * (double)gui_scaling, fixed_imgsize);
	return v2f32(render_target_size.X / prefer_imgsize,
			render_target_size.Y / prefer_imgsize);
}

#endif
