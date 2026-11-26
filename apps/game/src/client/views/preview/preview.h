// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include <json/json.h>
#include <optional>

namespace shell {

// Pure terragen preview shared by the create screen and the in-game map.
// `world`: the world to preview (a body's, in game); none: the intent's code.
Json::Value previewJson(const Json::Value &intent, const std::optional<terragen::WorldCode> &world = std::nullopt);

} // namespace shell
