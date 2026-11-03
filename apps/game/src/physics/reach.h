// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include <functional>
#include <optional>

/*
	Reach (arc-movement, sprint 118): what a body can get hold of from where it
	is. A pure question of which nodes are solid. How high a ledge is above where
	the body left the ground says what to do with it: go over, climb on, or hang.
	Whole nodes for now: a cut edge counts as its node, solid if at least half's there.
*/
namespace physics {

using Solid = std::function<bool(v3s16)>;

struct Ledge
{
	v3f normal;   // out of the wall, along an axis
	v3f lip;      // on the wall's face at its top (x, z); y is the top
	bool thin;    // open just past the wall: over it, there's ground lower down
	bool room;    // a body can stand on top
	float far = 0; // thin: the ground's height just past it (the lip's, if it's further down than a vault goes)
};
enum class Climb : unsigned char { VAULT, MANTLE, HANG };

constexpr float MOST_REACH = 2.35f;      // nodes: hands above the feet
constexpr float VAULT_TO = 1.3f, MANTLE_TO = 2.3f; // nodes above where it left the ground
constexpr float HANG_DROP = 1.85f;       // the feet below the lip, hanging
constexpr float SHIMMY = 1.5f;           // nodes/s

// The best edge to hand from a foot, looking along a yaw (Luanti's: radians, 0 is +z): straight
// ahead first, then further round.
std::optional<Ledge> findLedge(const Solid &solid, v3f foot, float yaw);
// The same edge further along, if it's still there. `body` is the hanging foot.
bool ledgeGoesOn(const Solid &solid, v3f lip, v3f normal);
// What this ledge is for. `base`: where it left the ground, or lower if it's fallen since.
std::optional<Climb> climbFor(const Ledge &ledge, float base, float foot_y);

// A body going from one place to another by way of over a lip: up, then across.
struct ClimbPath
{
	v3f from, to;
	float top;       // y it clears
	float time;      // s
	bool arc = false; // across at a steady speed, over and down, not up and then across
	v3f at(float t) const; // t in s
};
// `pace`: how fast it's going in (nodes/s). A vault takes as long as that takes: it doesn't speed up.
ClimbPath climbPath(Climb kind, const Ledge &ledge, v3f foot, float radius, float pace = 4);

// A wall beside or ahead of a body, at its chest: which way is out of it, the nearest if there are several.
std::optional<v3f> findWall(const Solid &solid, v3f foot, float radius);
// Whether going this way past that wall is along it, enough to run on it.
bool runsAlong(v3f velocity, v3f normal);
// Kicking off it: most of what's along the wall kept, thrown out from it and up.
v3f wallKick(v3f velocity, v3f normal, float up);
constexpr float WALL_RUN_TIME = 1.2f;     // s at most
constexpr float WALL_RUN_LIFT = 2;        // nodes/s up, as it takes to the wall
constexpr float WALL_RUN_GRAVITY = 0.35f; // × the pull, while it's running on one

} // namespace physics
