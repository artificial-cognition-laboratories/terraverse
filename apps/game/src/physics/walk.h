// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"

/*
	Walking (sprint 58): how a body moves under its own power. A pure rule:
	from what the player asks (which way, jump) and how the body is (its
	velocity, standing or not, what it stands on), the velocity it moves at
	next. physics::World's characters move at it.

	It has inertia. On the ground it gets up to walking speed in about a
	quarter of a second, and past it, to a run, more slowly; it brakes
	harder than it starts; changing direction at speed is a curve, not a
	snap. In the air it barely steers. It moves relative to what it stands
	on, so a cart carries it. A jump is a height, not a speed: the speed
	that height takes under this gravity.

	Gravity is the field's where it is (sprint 59): in a grid's axes it's
	all but straight down, so "up" for walking stays the grid's +y, and in
	the air the whole pull acts.
*/
namespace physics {

// How a body holds itself (arc-movement, sprint 116). Each is a row of STANCES.
enum class Stance : unsigned char { STAND, CROUCH, PRONE, SLIDE, ROLL, VAULT, MANTLE, HANG, WALL_RUN };
struct StanceInfo
{
	float height;      // nodes: its capsule (0: as tall as the body is)
	float speed;       // × walking
	bool jumps;
	float eye_drop;    // nodes: the view below standing
	float eye_forward; // nodes: and ahead of the feet
};
constexpr StanceInfo STANCES[] = {
	{ 0, 1, true, 0, 0 },
	{ 1.2f, 0.5f, true, 0.6f, 0.08f },
	{ 0.7f, 0.25f, false, 1.2f, 0.28f },
	{ 0.9f, 1, true, 0.75f, 0.1f },
	{ 0.9f, 1, false, 0.8f, 0.1f },
	{ 0.9f, 1, false, 0.3f, 0 },
	{ 0.9f, 1, false, 0.3f, 0 },
	{ 1.2f, 1, false, 0, 0 },
	{ 1.2f, 1, false, 0, 0 },
};
inline const StanceInfo &stanceInfo(Stance s) { return STANCES[(int)s]; }

struct Gait
{
	float walk = 4;          // nodes/s
	float run = 1;           // × walk at most when running (sprint: the caller's)
	float start = 18;        // nodes/s² up to walking speed
	float push_on = 5;       // nodes/s² from walking up to running
	float brake = 28;        // nodes/s² slowing (letting go, or turning back)
	float air = 3;           // nodes/s² steering in the air
	float air_reach = 1.5f;  // nodes/s: only slower than this can it steer at all. A jump's a throw:
	                         // what it left the ground with it keeps; a standing hop can still reach a ledge
	float flow = 1;          // × a run at most, reached slowly by holding a clean line
	float flow_on = 0.5f;    // nodes/s² from a run up to that
	float jump = 1.25f;      // nodes high
	v3f gravity{ 0, -9.81f, 0 }; // nodes/s²
};

struct Intent
{
	float forward = 0, right = 0; // −1..1 each (a stick's: it's capped at 1 together)
	float yaw = 0;                // radians: which way "forward" is (Luanti's: 0 faces +z)
	bool jump = false;
};

struct Footing
{
	v3f velocity;
	bool on_ground = false;
	v3f ground_velocity{ 0 };
	v3f ground_normal{ 0, 1, 0 };
};

// Its velocity after `dt` more of walking.
v3f walk(const Gait &gait, const Intent &intent, const Footing &now, float dt);
// Sliding: friction and the slope act on what it came in with, it hardly steers.
v3f slide(const Gait &gait, const Intent &intent, const Footing &now, float dt);
// Rolling: it keeps going, at least at a walk, the way it was going (or asked, from a standstill).
v3f roll(const Gait &gait, const Intent &intent, const Footing &now, float dt);

constexpr float SLIDE_FROM = 4.4f, SLIDE_TO = 2.5f; // nodes/s across: fast enough to start one, and where it ends
constexpr float SLIDE_KICK = 6;                     // nodes/s it drops into one at, at least
constexpr float ROLL_LATE = 0.2f;                   // s after a hard landing a crouch still makes it a roll
constexpr float ROLL_TIME = 0.55f;               // s
constexpr float HARD_LANDING = 7;                // nodes/s down: a roll takes it, or it costs speed and hurts
constexpr float STUMBLE = 0.4f;                  // what's kept across, landing that hard with no roll

// What a body's holding moves to. `asked` is what the player wants, only ever
// STAND, CROUCH or PRONE; slides and rolls come of how it's moving as they ask.
Stance nextStance(Stance now, Stance asked, float pace, bool on_ground, float roll_left);
// Landing at this speed down, asking this: whether it rolls out of it.
inline bool rollsOut(float fall, Stance asked) { return fall >= HARD_LANDING && asked != Stance::STAND; }

} // namespace physics
