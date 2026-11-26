// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_aabb3d.h"
#include "src/shared/base/irrlichttypes.h"
#include <optional>
#include <string>

namespace piece {

// What a machine's text (Machine::write) says of it, without its names:
// which it is, and the boxes (BS = 1, local) of what's built (stood on)
// and of what's designed (selected; nothing where there's none), for its
// entity. An object's text (TFO1), or a piece's from before, is a machine
// of one part, its id the object's (a piece's has none yet). Throws
// SerializationError on text that's none of these.
struct Header
{
	std::optional<u64> id;
	std::optional<aabb3f> built, designed;
};
Header header(const std::string &text);
// The box of a piece's text (Piece::write), without its names; nothing if
// it's empty.
std::optional<aabb3f> boxOf(const std::string &piece);

} // namespace piece
