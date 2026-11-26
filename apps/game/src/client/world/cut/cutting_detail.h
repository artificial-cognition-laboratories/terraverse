// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/game/cut/peel.h"
#include <json/json.h>
#include <optional>

class Client;
class Map;

// What cutting.cpp and placing.cpp (Cutting's two halves) share: the wire, and where things are.
namespace cut::detail {

Json::Value list(v3s16 v);
Json::Value list(v3s32 v);
void send(Client *client, const char *event, const Json::Value &payload);
// a map's block, as peel reads it
std::optional<peel::Block> blockOf(Map &map, v3s16 q);
// the block a world sixteenth is in
v3s16 blockAt(v3s32 cell);
// the world sixteenth a point (BS) is in
v3s32 cellAt(v3f at);

} // namespace cut::detail
