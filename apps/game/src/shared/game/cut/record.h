// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/game/cut/peel.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

/*
	What's placed (sprint 123): each member placed into the world, remembered
	in the metadata of every block it's in (KEY), so breaking it gives it back
	whole. A box in world sixteenths (a node's ·16 plus the cell). Written by
	the server; read by it and by the client, to outline the member aimed at.
*/
namespace record {

constexpr const char *KEY = "cut:members";

struct Member
{
	std::string item;
	peel::Box box;
};

// A block's members, from its metadata's KEY ("" is none). Throws
// std::invalid_argument on text that isn't (only the server writes it).
std::vector<Member> read(const std::string &text);
std::string write(const std::vector<Member> &members);

// The one holding the world sixteenth `cell`, if any.
std::optional<Member> at(const std::vector<Member> &members, v3s32 cell);
// Whether two boxes share a sixteenth.
bool meets(const peel::Box &a, const peel::Box &b);
// The world sixteenth of a node's cell.
v3s32 world(v3s16 node, v3s16 cell);
// Whether a box rests against something: a sixteenth just outside one of its
// faces that `solid` (a world sixteenth) says is filled.
bool touches(const peel::Box &box, const std::function<bool(v3s32)> &solid);
// The blocks a box is in.
std::vector<v3s16> blocks(const peel::Box &box);

} // namespace record
