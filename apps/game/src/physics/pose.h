// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "physics/walk.h"

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
};

struct Joints
{
	float twist = 0;    // the hips and legs against the view (radians, + right)
	float spin = 0;     // turned over whole, head over heels: a roll (radians)
	float spin_lean = 0; // and turned so it goes over one shoulder (radians)
	float back = 0;     // the whole body moved back along its legs (nodes): lying, the head stays near the feet's place
	float drop = 0;     // the hips, lowered (nodes)
	float pitch = 0;    // the hips tipped forward
	float waist = 0;    // the chest on the hips (+ forward)
	float head = 0;     // the head, against the view's level (+ down)
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
	Spring m_stride, m_air, m_ground_drop, m_low[10];
};

} // namespace physics
