// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include <functional>
#include <optional>
#include <vector>

/*
	Earthworks (sprint 124: the shovel): digging is placing run backwards.
	A form (a hole, a trench, a pit, a flattened pad) is a Target: a box in
	world sixteenths (a node's ·16 plus the cell), snapped to the GRID as a
	member is, its top the ground where it's aimed (or, levelled, a level
	and what's over it). The whole of it is outlined before it's dug; held,
	the shovel takes it away a bite at a time, the highest of it first, till
	it's gone. The server digs by this; the client outlines it.
*/
namespace earthwork {

constexpr s32 GRID = 8;    // sixteenths: what a target's corner snaps to, half a block
constexpr s32 ABOVE = 48;  // sixteenths: how far over and under a column's expected ground it's looked for

// What a form takes: a bore (the tool's cells along where they look, `along` across, nearest
// first: bore()); the tool's cell of the world aimed into (its grid, along every axis: the
// size's along, the same each way), a box, a round pit in the box, or all above a level in it.
enum class Kind { Bore, Cell, Box, Round, Level };
// Where it goes: snapped to the grid (a level at their feet), freely (Ctrl), or snapped,
// a level at the ground aimed at (Shift).
enum class Mode { Snap, Free, Aimed };

// A form's size, sixteenths: `along` the way they face, `across` it, `deep` down (a level: how
// high over it is cleared).
struct Size
{
	s32 along, across, deep;
};

// What's to be dug: the box (inclusive corners) and what of it.
struct Target
{
	v3s32 min, max;
	Kind kind;
};

// A column (x, z) cut from `bed` up to `top` (exclusive).
struct Column
{
	s32 x, z, bed, top;
};

// The target aimed at `aim` (the ground's top there is `ground`, their feet `feet`), facing
// `yaw` (radians; along x or z, whichever's nearer).
Target place(v3s32 aim, s32 ground, s32 feet, f32 yaw, Size size, Kind kind, Mode mode);
// A bore's cells (their lowest corners, on the tool's grid of `cell`): every one whose middle is
// within `radius` of the line from `eye` along `dir` (unit), ahead of it and out to `reach`,
// the nearest along it first. World sixteenths. A swing digs the first few of them still there,
// so what's between them and the face goes first, then the face chip by chip: a round hole.
std::vector<v3s32> bore(v3f eye, v3f dir, f32 radius, f32 reach, s32 cell);
// Whether the column (x, z) is the target's.
bool holds(const Target &t, s32 x, s32 z);
// Whether a target is the size's, either way round.
bool fits(const Target &t, Size size);
// What a bite of `budget` sixteenths takes of the target, whose columns' ground is `ground(x, z)`:
// whole layers from the highest down, at least one.
std::vector<Column> bite(const Target &t, s32 budget, const std::function<s32(s32, s32)> &ground);
// The ground's top over the column (x, z) near `near`: the sixteenth over the highest one
// `solid` says is filled, looking ABOVE up and down from it. None if there's none.
std::optional<s32> surface(s32 x, s32 z, s32 near, const std::function<bool(v3s32)> &solid);

} // namespace earthwork
