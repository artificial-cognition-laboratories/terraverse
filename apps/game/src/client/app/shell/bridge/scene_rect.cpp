// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/scene_rect.h"
#include "src/client/app/shell/bridge/bridge.h"
#include <cmath>

namespace shell {

core::recti sceneRect(const Json::Value &rect)
{
	if (!rect["x"].isNumeric() || !rect["y"].isNumeric() || !rect["width"].isNumeric() || !rect["height"].isNumeric() ||
			!rect["scale"].isNumeric() || !(rect["scale"].asDouble() > 0))
		throw Refused("a scene's rect is { x, y, width, height, scale }");
	// the page's own scale, as it laid the box out: the shell's may be a frame ahead of it
	const double k = rect["scale"].asDouble();
	return core::recti((s32)std::lround(rect["x"].asDouble() * k), (s32)std::lround(rect["y"].asDouble() * k),
			(s32)std::lround((rect["x"].asDouble() + rect["width"].asDouble()) * k), (s32)std::lround((rect["y"].asDouble() + rect["height"].asDouble()) * k));
}

} // namespace shell
