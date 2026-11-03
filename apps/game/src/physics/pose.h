// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "physics/walk.h"
#include <functional>
#include <optional>

/*
	How a body stands (arc-movement, sprint 115): from how it moves and the
	ground under its feet, the angle of every joint. A pure rule, with no
	drawing in it; client/figure shows the answer, and the server will be able
	to ask it where a fist is.

	It decides nothing about movement: the physics moves the body, and this
	only shows it. No clips. Legs go by where the feet must be (inverse
	kinematics, so a foot lands on a step and doesn't skate); the rest eases
	between a few key poses on springs.

	Lengths are nodes before scaling, in the figure's frame: +z ahead, +x right,
	feet at y = 0. A rotation about x of +a swings a hanging limb back by a.
*/
namespace physics {

constexpr float HIP_HEIGHT = 0.75f, THIGH = 0.375f, SHIN = 0.375f, HIP_WIDE = 0.125f;
constexpr float UPPER_ARM = 0.3f, FOREARM = 0.25f;

// A limb of two bones reaching for a point in its swing plane, from its root.
struct Bend
{
	float root;  // the first bone's angle (+ back)
	float joint; // the second's against the first
};
enum class Bends { FORWARD, BACK }; // which way the middle joint points: a knee forward, an elbow back
// Out of reach, it reaches as far as it can that way.
Bend twoBones(float z, float y, float first, float second, Bends way);
// Where that leaves the end: {z, y}.
void endOf(const Bend &bend, float first, float second, float &z, float &y);

// An arm reaching for a point (the wrist, from the shoulder, in the chest's frame), its elbow
// toward `pole`. The upper arm's axes in that frame (it hangs along -y and bends about x), and the
// forearm's turn about that x. Out of reach, it goes straight that way.
struct ArmTo
{
	v3f x, y, z;
	float elbow;
};
ArmTo armTo(v3f to, v3f pole, float first, float second);

// Hands on something, in the frame asked in: where each wrist is, and which way its fingers go.
struct Grip
{
	bool held[2]{};
	v3f wrist[2];
	v3f over{ 0, 0, 1 };
	int moving = -1; // a hand in the air, going to its next hold
};

// What a body has hold of or is pushing against, in the figure's frame (nodes): what shows contact in a pose.
struct Contacts
{
	const Grip *grip = nullptr;
	v3f push;
	std::optional<v3f> step_on;
	int feet = -1;
	float fallen = 0, rising = 0;
	float stagger = 0;
	float throw_side = 0, throw_back = 0;
};

// Two hands along an edge, moved one at a time: each stays where it's put till the body's
// left it behind, so a shimmy is hand over hand and no hand skates.
class HandOverHand
{
public:
	void place(v3f rest0, v3f rest1);
	// `keep`: where a hand can be put instead of there (on what's there to hold).
	void step(float dt, v3f rest0, v3f rest1, const std::function<v3f(v3f)> &keep = nullptr);
	v3f at(int side) const { return m_at[side]; }
	int moving() const { return m_moving; } // which hand's in the air, or -1
private:
	v3f m_at[2], m_from, m_to, m_rest;
	int m_moving = -1, m_last = -1;
	float m_way = 0;
	float m_t = 0;
};

struct PoseIn
{
	v3f moving;            // nodes/s, in the figure's frame
	float pitch = 0;       // radians, + down
	bool airborne = false;
	Stance stance = Stance::STAND;
	float turned = 0;      // radians the view went right since the last step
	bool faces = false;    // held to a wall, its hips turned `face` from the view
	float face = 0;
	float ground[2]{};     // under each foot, against where the body stands (nodes)
	float above = 1e3f;    // nodes over the ground under it
	bool steps = false;    // a leg going up on something, here (figure's frame)
	v3f step_on;
	int feet = -1;         // which take it (and a push's follow-through): 0, 1, 2 for both, -1 the leg leading
	float fallen = 0;      // down: over from upright (radians, + face down)
	float rising = 0;      // getting up, 0..1
	float stagger = 0;     // off balance, 0..1: pitched over and low till the feet catch up
	float throw_side = 0, throw_back = 0; // a vault: thrown about its hands (as swing_side, swing_back)
	int hand_moving = -1;  // hung: a hand off, the weight on the other
	v3f push;              // just kicked off a wall: toward it, fading from 1 (the leg still driving)
};

struct Joints
{
	float twist = 0;    // the hips and legs against the view (radians, + right)
	float chest_twist = 0; // the chest against the hips
	float spin = 0;     // turned over whole, head over heels: a roll (radians)
	float spin_lean = 0; // and turned so it goes over one shoulder (radians)
	float back = 0;     // the whole body moved back along its legs (nodes): lying, the head stays near the feet's place
	float drop = 0;     // the hips, lowered (nodes)
	float pitch = 0;    // the hips tipped forward
	float waist = 0;    // the chest on the hips (+ forward)
	float fallen = 0;   // over about the feet (radians, + face down)
	float head = 0;     // the head, against the view's level (+ down)
	float swing_back = 0, swing_side = 0; // hanging: swung about its hands (radians: feet back from what it faces, feet to its right)
	float lean_ahead = 0, lean_right = 0; // the upper body into how it's speeding up or turning, over the feet (radians)
	float hip[2]{}, hip_out[2]{}, knee[2]{};
	float shoulder[2]{}, shoulder_out[2]{}, elbow[2]{};
	v3f foot[2];        // where each foot was asked to be
	bool planted[2]{};  // it's on the ground, carrying the body
};

// Side 0 is the right (+x), side 1 the left.
class Poser
{
public:
	// Where to look for the ground under each foot next: figure's frame, y unused.
	v3f footAt(int side) const;
	Joints step(float dt, const PoseIn &in);

private:
	struct Spring
	{
		float at = 0, speed = 0;
		float step(float to, float rate, float dt);
	};
	float m_phase = 0;     // 0..1 through a stride
	v3f m_way{ 0, 0, 1 };  // along the ground, the way it last moved
	v3f m_foot[2]{ { HIP_WIDE, 0, 0 }, { -HIP_WIDE, 0, 0 } };
	float m_ground[2]{};
	float m_twist = 0;     // the hips against the view
	float m_roll = 0;      // s into a roll
	// The mass (sprint 122): a body is held by what it touches. Hung, it's a pendulum under its hands;
	// on its feet it's balanced over them, leaning into what it's doing.
	float m_back = 0, m_back_speed = 0, m_side = 0, m_side_speed = 0;
	v3f m_going;           // how it's been going, smoothed: what changes in it is what the mass feels
	Spring m_lean_ahead, m_lean_right, m_dip, m_rise, m_low_stagger;
	bool m_was_air = false;
	float m_fell = 0;      // nodes/s, last in the air
	int m_hand_was = -1;
	bool m_coming_back = false; // let go of a wall: the hips ease back to what they can twist
	int m_lead = 0;        // the leg this jump goes up on: they take turns
	Spring m_stride, m_air, m_ground_drop, m_low[10];
};

} // namespace physics
