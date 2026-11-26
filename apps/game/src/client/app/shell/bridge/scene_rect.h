// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <rect.h>

namespace shell {

// A native view's box as the page sends it ({ x, y, width, height, scale }: page pixels, and the
// scale it laid them out at), in the window's pixels. Refused if it isn't one.
core::recti sceneRect(const Json::Value &rect);

} // namespace shell
