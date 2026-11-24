// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/geometry/shape/shape.h"
#include "src/shared/base/irr_v3d.h"
#include <array>
#include <functional>
#include <optional>
#include <vector>

/*
	Peeling (sprint 123: cutting): the member that comes off a log where it's
	aimed at. A log is a run of up to LONGEST blocks of one content lying
	along one grain (a trunk on its side, a post stood up); a member is a box
	of sixteenths, long along the grain, so many across, so deep in from the
	face cut. The server cuts by this, the client shows by it what will come off.
*/
namespace peel {

constexpr int LONGEST = 4; // blocks: the longest member is 4 m

// A member's box, in sixteenths: along the grain, across it, and in from the face.
struct Size
{
	s16 along, across, in;
};

// Cells of a log, inclusive both ends: along its grain 0..16·length−1, else 0..15.
struct Box
{
	v3s32 min, max;
};

// A log as it is now: its first block (lowest along the grain), its grain
// (0 x, 1 y, 2 z) and each block's shape, first to last.
struct Log
{
	v3s16 start;
	int grain;
	std::vector<shape::Shape> shapes;
};

// The grain of a block turned by facedir `param2` (a log stood up runs y).
int grain(u8 param2);

// A block as a map gives it: what it is, how it's turned, and its shape if it's cut.
struct Block
{
	u16 content;
	u8 param2;
	const shape::Shape *shape;
};
// The log the block at p is part of: the run of blocks like it (its content,
// its grain) through p, at most LONGEST, p as near its middle as can be.
// `block` reads the map (none: not loaded).
Log read(v3s16 p, const std::function<std::optional<Block>(v3s16)> &block);

// Where a sixteenth of the block at p is in the log, as a log cell.
v3s32 cell(const Log &log, v3s16 p, v3s16 cell);

// The box of `size` cut from `face` (outward, one axis, ±1) where `aim` (a log
// cell on that face) is: along and across, the one of its own grid it's in
// (centred on it where the size doesn't divide the log), as shallow as it's whole. None if there's none there, the log is too short, or the face is an end.
std::optional<Box> at(const Log &log, v3s16 face, Size size, v3s32 aim);
// Whether a box of `size` is whole anywhere off `face`.
bool any(const Log &log, v3s16 face, Size size);

// Each block's part of the box: its index in the log, and its own cells.
std::vector<std::pair<int, std::pair<v3s16, v3s16>>> split(const Log &log, const Box &box);
// Where a member goes placed against `face` (outward) of `node`, aimed at its sixteenth
// `cell`: a box `dims` big (world sixteenths along x, y, z), in world sixteenths (a node's
// ·16 plus the cell), just outside the face. Along the face it's centred on the aim, on a
// quarter of a block, or on its own width's grid where that divides a block, so members side
// by side line up; `free`, where it's aimed to the sixteenth.
Box lay(v3s16 node, v3s16 face, v3s16 cell, v3s32 dims, bool free);
// `size` turned so its along, across and in lie along the world axes `axes` gives.
v3s32 dims(Size size, const std::array<int, 3> &axes);
// The facedir of a block whose grain runs along `axis`.
u8 facedir(int axis);

// `s` with the cells min..max (inclusive) taken out.
shape::Shape without(shape::Shape s, v3s16 min, v3s16 max);

} // namespace peel
