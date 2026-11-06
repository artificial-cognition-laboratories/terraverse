// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/motion.h"
#include "physics/pose.h"
#include <matrix4.h>
#include <functional>

/*
	Where a figure's bones go from a pose's joints (arc-movement, sprint 121). Pure,
	so what client/figure draws and what scripts/pose-sheet renders are the same answer.
	In nodes, each bone against its parent: lower holds hips and thighs; hips the chest;
	the chest the head and upper arms; then down each limb.
*/
namespace physics {

// Its proportions before it's scaled to the player's height: legs and torso three
// quarters of a node each, a head of half.
constexpr float FIGURE_SCALE = 1.77f / 2.0f;
// Bones overlap at their joints, the lower one narrower, so a bend shows no gap
// and their faces don't fight.
constexpr float BONE_OVERLAP = 0.05f, BONE_WIDE = 0.25f, BONE_NARROW = 0.22f;
constexpr float HIPS_TALL = 0.25f, CHEST_TALL = 0.5f, HEAD_SIZE = 0.5f, SHOULDER_UP = 0.375f, SHOULDER_OUT = 0.375f;
constexpr float HAND_LONG = 0.125f;

struct BonePlace
{
	v3f at;   // nodes
	v3f turn; // degrees
	core::matrix4 matrix() const;
};
struct Bones
{
	BonePlace lower, hips, chest, head;
	BonePlace thigh[2], shin[2], upper[2], fore[2], hand[2];
};

// `t` (0..1) of the way from one to the other, each bone turned the short way round.
Bones blend(const Bones &a, const Bones &b, float t);

class Skeleton
{
public:
	// `grip`: hands on something, in the figure's frame (nodes). `tilt`: leaned sideways (radians, + left). `clear`: moved this far (figure's frame) to keep its upper body out of what it's beside.
	Bones place(float dt, const Joints &j, float tilt, const Grip *grip, v3f clear = v3f(0));
	float gripping(int side) const { return m_gripping[side]; } // 0..1: how far a hand's come onto what it holds

private:
	float m_gripping[2]{};    // 0..1: how far a hand has gone from where the pose holds it to what it has hold of
	v3f m_wrist[2];           // where it was, against the chest: on its way, a hand has only so much speed
	bool m_wrist_known[2]{};
	v3f m_pivot{ 0, 1.56f, 0.3f }; // what it swings about: its hands, last it had them
	v3f m_grip_at[2], m_grip_over{ 0, 0, 1 }; // the last thing held: a hand comes away from there
};

// How a figure stands now, from how it moves: the rule client/figure draws and the wiki's scenes draw.
class Posing
{
public:
	using Ground = std::function<float(v3f)>;     // the ground under a place (figure's frame), against its feet
	using Clear = std::function<v3f(v3f, float)>; // how far a ball there has to go to be out of what's solid
	float strideShare() const { return m_strides.share(); }
	float headShake() const { return m_strides.headShake(); } // Strides::headShake
	// A punch or kick that landed this pose (Contacts::strike threw it): where its fist or foot was.
	const std::optional<Hit> &hit() const { return m_hit; }
	// Which feet are down and carrying it, as it's posed now.
	bool planted(int side) const { return m_planted[side]; }
	Bones pose(float dtime, v3f velocity, float pitch, bool airborne, Stance stance, float turned, std::optional<float> face,
			float tilt, const Contacts &contacts, const Ground &ground, const Clear &clear);

private:
	Poser m_poser;
	Skeleton m_skeleton;
	Strides m_strides; // a person's stride, recorded, for going about on its feet
	Moves m_moves;     // and a person's jump, played once
	bool m_was_airborne = false;
	bool m_let_go = false; // a move was let go of this step
	std::optional<Hit> m_hit;
	bool m_planted[2]{};
	v3f m_clear; // how far the upper body is moved out of what it's beside
	bool m_posed = false;
	Stance m_stance = Stance::STAND;
	Bones m_last, m_from; // what it was last, and as it was when the stance changed
	float m_since = 1e3f; // s since then
};

// The boxes it's made of, as client/figure builds them.
enum class BoneColour { SKIN, SHIRT, TROUSERS };
void eachBoneBox(const Bones &b, const std::function<void(const core::matrix4 &, v3f size, v3f centre, BoneColour)> &box);

} // namespace physics
