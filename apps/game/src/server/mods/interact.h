// Terra Firma: digging, placing and what falls (sprint 119: was builtin/game/item.lua,
// falling.lua and world.lua). A block dug falls where it was as items to be picked up
// (its `drop`, or itself with its colour); a block placed goes where a player puts it,
// facing the way it's put down if it rotates; what's left unsupported (sand over a hole,
// a flower whose ground is gone) falls or drops.
//
// TEMP(2026-11-19): a dug block gives itself, whatever digs it. Breaking by tool ×
// material (AGENTS.md, Making and moving) replaces this.
#pragma once

#include "src/shared/core/irrlichttypes_bloated.h"
#include "src/shared/map/mapnode.h"
#include <vector>

struct ItemStack;
class PlayerSAO;
class Server;
class ServerEnvironment;
struct PointedThing;

namespace interact {

// What digging `n` gives.
std::vector<ItemStack> drops(Server *server, MapNode n);

// Things broken at `p` falling where it was, as items lying there.
void scatter(ServerEnvironment &env, v3s16 p, const std::vector<ItemStack> &items);

// `digger` (null: the world itself) digs the block at p. False if it can't be dug.
bool dig(Server *server, PlayerSAO *digger, v3s16 p);

// `placer` places what's in `stack` against what it points at; `stack` is what's left.
// False if nothing was placed.
bool place(Server *server, PlayerSAO *placer, ItemStack &stack, const PointedThing &pointed);

// `placer` drops `stack` in front of them; it's emptied if it was.
void drop(Server *server, PlayerSAO *placer, ItemStack &stack);

// What round p (and above it) has lost its support falls, or comes off as items.
void settle(ServerEnvironment &env, v3s16 p);

// Whether the block `n` at p, given what's round it, should fall or come off (and makes
// it); false if it stays.
bool settleOne(ServerEnvironment &env, v3s16 p);

} // namespace interact
