// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "physics/pose.h"

#include <algorithm>
#include <cmath>

namespace physics {

namespace {

constexpr float PI = 3.14159265f;
constexpr float WALK = 4, RUN = 6.4f; // nodes/s
// A stride: both feet's steps, this far along the ground at a run or a walk.
constexpr float STRIDE_WALK = 1.6f, STRIDE_RUN = 2.f;
constexpr float LIFT = 0.2f;
constexpr float STEP_RATE = 24; // a foot's height follows the ground

// The body's key poses. A new stance is a new row; every field eases on its spring.
struct Key
{
	float waist, arm_out, elbow, swing;
	float arm;          // the arms' rest against the chest (+ back)
	float pitch, drop;  // the hips tipped, and lowered
	float back, raise;  // where feet rest against the hips and the ground
	float stride;       // × a standing stride (and a foot's lift)
};
constexpr int KEY_FIELDS = 10;
constexpr Key STAND{ 0.f, 0.06f, 0.15f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f };
constexpr Key STROLL{ 0.03f, 0.05f, 0.4f, 0.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f };
constexpr Key SPRINT{ 0.25f, 0.05f, 1.45f, 0.85f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f };
constexpr Key FALL{ 0.06f, 0.5f, 0.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f };
// Rows by stance: still, and going at its pace. Standing also has SPRINT.
struct Row
{
	Key still, going;
	float twist; // how far the hips turn from the view
	float pace;  // nodes/s it counts as going at
	float back;  // the body moved back along the legs
};
const Row ROWS[] = {
	{ STAND, STROLL, 1.13f, WALK, 0 },
	{ { 0.5f, 0.1f, 0.8f, 0.f, -0.35f, 0.25f, 0.33f, 0.02f, 0.f, 0.6f },
	  { 0.55f, 0.1f, 0.9f, 0.3f, -0.35f, 0.25f, 0.33f, 0.02f, 0.f, 0.6f }, 1.13f, WALK * 0.5f, 0 },
	// on the belly, arms ahead: the capsule's round the shoulders, so the legs trail behind it
	{ { 0.1f, 0.15f, 1.1f, 0.f, -2.5f, 1.4f, 0.55f, 0.68f, 0.1f, 0.4f },
	  { 0.1f, 0.15f, 1.1f, 0.35f, -2.5f, 1.4f, 0.55f, 0.68f, 0.1f, 0.4f }, 0.3f, WALK * 0.25f, 0.6f },
	// leaning back, feet ahead, no stride: it isn't walking
	{ { -0.1f, 0.45f, 0.5f, 0.f, -0.5f, -0.55f, 0.42f, -0.42f, 0.06f, 0.f },
	  { -0.1f, 0.45f, 0.5f, 0.f, -0.5f, -0.55f, 0.42f, -0.42f, 0.06f, 0.f }, 0.3f, WALK, 0 },
	// a tuck, turned over whole by Joints::spin
	{ { 0.8f, 0.2f, 1.5f, 0.f, -1.f, 0.5f, 0.3f, 0.05f, 0.22f, 0.f },
	  { 0.8f, 0.2f, 1.5f, 0.f, -1.f, 0.5f, 0.3f, 0.05f, 0.22f, 0.f }, 1.13f, WALK, 0 },
	// a hand down on it, legs swung up and across
	{ { 0.5f, 0.5f, 0.4f, 0.f, -1.f, 0.3f, 0.25f, -0.2f, 0.35f, 0.f },
	  { 0.5f, 0.5f, 0.4f, 0.f, -1.f, 0.3f, 0.25f, -0.2f, 0.35f, 0.f }, 1.13f, WALK, 0 },
	// pressing up on both hands, knees up
	{ { 0.6f, 0.2f, 1.3f, 0.f, -1.2f, 0.3f, 0.2f, 0.1f, 0.4f, 0.f },
	  { 0.6f, 0.2f, 1.3f, 0.f, -1.2f, 0.3f, 0.2f, 0.1f, 0.4f, 0.f }, 1.13f, WALK, 0 },
	// arms overhead, legs hanging
	{ { 0.f, 0.1f, 0.15f, 0.f, -3.f, 0.f, 0.f, 0.f, 0.f, 0.f },
	  { 0.f, 0.1f, 0.15f, 0.f, -3.f, 0.f, 0.f, 0.f, 0.f, 0.f }, 3.2f, WALK, 0 },
	// running in the air, on the wall: it's leaned from it by Figure::pose's tilt
	{ SPRINT, SPRINT, 1.13f, WALK, 0 },
};
// In the air the feet hang here, for side 0 and 1: a stride frozen, knees up.
constexpr float AIR_Z[2] = { 0.26f, -0.2f }, AIR_Y[2] = { 0.26f, 0.14f };

float lerp(float a, float b, float t) { return a + (b - a) * t; }

Key blend(const Key &a, const Key &b, float t)
{
	return { lerp(a.waist, b.waist, t), lerp(a.arm_out, b.arm_out, t), lerp(a.elbow, b.elbow, t), lerp(a.swing, b.swing, t),
		lerp(a.arm, b.arm, t), lerp(a.pitch, b.pitch, t), lerp(a.drop, b.drop, t), lerp(a.back, b.back, t),
		lerp(a.raise, b.raise, t), lerp(a.stride, b.stride, t) };
}

// A vector in the view's frame as the frame turned `by` right sees it, and back.
v3f intoTurned(v3f v, float by)
{
	const float c = std::cos(by), s = std::sin(by);
	return v3f(v.X * c - v.Z * s, v.Y, v.X * s + v.Z * c);
}
v3f outOfTurned(v3f v, float by)
{
	const float c = std::cos(by), s = std::sin(by);
	return v3f(v.X * c + v.Z * s, v.Y, -v.X * s + v.Z * c);
}

} // namespace

Bend twoBones(float z, float y, float first, float second, Bends way)
{
	const float most = first + second - 1e-3f, least = std::fabs(first - second) + 1e-3f;
	const float reach = std::clamp(std::hypot(z, y), least, most);
	const float at = std::atan2(-z, -y);
	const float open = std::acos(std::clamp((first * first + reach * reach - second * second) / (2 * first * reach), -1.f, 1.f));
	const float inside = std::acos(std::clamp((first * first + second * second - reach * reach) / (2 * first * second), -1.f, 1.f));
	const float sign = way == Bends::FORWARD ? 1.f : -1.f;
	return { at - sign * open, sign * (PI - inside) };
}

void endOf(const Bend &bend, float first, float second, float &z, float &y)
{
	const float below = bend.root + bend.joint;
	z = -first * std::sin(bend.root) - second * std::sin(below);
	y = -first * std::cos(bend.root) - second * std::cos(below);
}

// Exactly, so a long frame can't blow it up: critically damped toward `to`.
float Poser::Spring::step(float to, float rate, float dt)
{
	const float gap = at - to;
	const float carry = (speed + rate * gap) * dt;
	const float fade = std::exp(-rate * dt);
	at = to + (gap + carry) * fade;
	speed = (speed - rate * carry) * fade;
	return at;
}

v3f Poser::footAt(int side) const
{
	return outOfTurned(m_foot[side] - v3f(0, 0, m_low[9].at), m_twist);
}

Joints Poser::step(float dt, const PoseIn &in)
{
	const Row &row = ROWS[(int)in.stance];

	// The feet hold their ground as the view turns, till the hips can't twist further.
	// Going, the hips face the way it goes (or the way it's backing from): the head
	// keeps looking where it looks, and a stride stays along the legs.
	m_twist -= in.turned;
	const v3f flat(in.moving.X, 0, in.moving.Z);
	const float flat_pace = flat.getLength();
	if (!in.airborne && flat_pace > 0.5f) {
		const bool backing = in.moving.Z < -0.1f * flat_pace;
		const float want = backing ? std::atan2(-in.moving.X, -in.moving.Z) : std::atan2(in.moving.X, in.moving.Z);
		const float most = 6 * dt;
		m_twist += std::clamp(want - m_twist, -most, most);
	}
	m_twist = std::clamp(m_twist, -row.twist, row.twist);
	if (in.faces) // on a wall: square to it, wherever the view goes
		m_twist = in.face;

	const v3f along = intoTurned(flat, m_twist);
	const float pace = in.airborne ? 0 : along.getLength();
	if (pace > 0.2f) {
		// feet turn toward going backward by running their stride in reverse, not by spinning round
		m_way = along / pace;
		if (m_way.Z < -0.1f)
			m_way = m_way * -1.f;
	}
	const float backward = along.Z < -0.1f * along.getLength() ? -1.f : 1.f;

	const float running = in.stance == Stance::STAND || in.stance == Stance::WALL_RUN ? std::clamp((pace - WALK) / (RUN - WALK), 0.f, 1.f) : 0.f;
	const float going = std::clamp(pace / row.pace, 0.f, 1.f);
	const float air = m_air.step(in.airborne ? 1.f : 0.f, 14, dt);

	const Key on_ground = blend(blend(row.still, row.going, going), SPRINT, running);
	const Key target = blend(on_ground, FALL, air);
	Key key;
	key.waist = m_low[0].step(target.waist, 7, dt);
	key.arm_out = m_low[1].step(target.arm_out, 12, dt);
	key.elbow = m_low[2].step(target.elbow, 12, dt);
	key.swing = m_low[3].step(target.swing, 12, dt);
	key.arm = m_low[4].step(target.arm, 9, dt);
	key.pitch = m_low[5].step(target.pitch, 9, dt);
	key.drop = m_low[6].step(target.drop, 9, dt);
	key.back = m_low[7].step(target.back, 9, dt);
	key.raise = m_low[8].step(target.raise, 9, dt);
	key.stride = target.stride; // follows its share below; a jump in it would skate a foot
	static_assert(KEY_FIELDS == 10);

	const float stride = lerp(STRIDE_WALK, STRIDE_RUN, running) * key.stride;
	// How much of a stride: short steps when slow, so the cadence stays a person's.
	const float share = m_stride.step(std::sqrt(going), 16, dt);
	if (share > 0.01f && stride > 0.01f)
		m_phase = std::fmod(m_phase + backward * pace * dt / (stride * std::max(share, 0.3f)) + 1.f, 1.f);

	Joints out;
	out.twist = m_twist;
	m_roll = in.stance == Stance::ROLL ? m_roll + dt : 0;
	// Slow as the legs take the fall, then thrown. The body turns so the leading shoulder goes
	// down first, and comes back straight: the ground runs across the back, shoulder to hip.
	const float through = std::clamp(m_roll / ROLL_TIME, 0.f, 1.f);
	out.spin = in.stance == Stance::ROLL ? 2 * PI * through * through * (3 - 2 * through) : 0;
	out.spin_lean = in.stance == Stance::ROLL ? 0.75f * std::sin(PI * through) : 0;
	out.back = m_low[9].step(row.back, 9, dt);
	out.pitch = key.pitch;
	float lowest = 0;
	for (int side : { 0, 1 }) {
		// half a stride apart. On the ground for half of one, moving back exactly as
		// fast as the body goes on: that's what stops it skating.
		const float t = std::fmod(m_phase + (side ? 0.5f : 0.f), 1.f);
		const bool stance = t < 0.5f;
		const float reach = stride * share / 4;
		const float offset = stance ? reach * (1 - 4 * t) : reach * (4 * t - 3);
		const float lift = stance ? 0 : LIFT * key.stride * share * std::sin((t - 0.5f) * 2 * PI);

		// a step taken is a foot lifted to it: follow the ground fast, but not at once
		m_ground[side] += (in.ground[side] - m_ground[side]) * std::min(1.f, STEP_RATE * dt);
		const float side_x = side ? -HIP_WIDE : HIP_WIDE;
		const v3f walked(side_x + m_way.X * offset, m_ground[side] + lift + key.raise, m_way.Z * offset - key.back);
		const v3f hung(side_x, AIR_Y[side], AIR_Z[side]);
		m_foot[side] = walked + (hung - walked) * air;
		out.foot[side] = m_foot[side];
		out.planted[side] = stance && air < 0.5f;
		lowest = std::min(lowest, m_ground[side]);
	}

	// Hips come down for a foot on lower ground, and sit lower at a run, so legs reach.
	out.drop = m_ground_drop.step(std::clamp(-lowest, 0.f, 0.45f) * (1 - air) + 0.02f + 0.05f * running, 18, dt) + key.drop;
	const float hip_y = HIP_HEIGHT - out.drop;

	for (int side : { 0, 1 }) {
		const float across = m_foot[side].X - (side ? -HIP_WIDE : HIP_WIDE);
		const float below = hip_y - m_foot[side].Y;
		// tip the leg out to the foot, then bend it in the plane that leaves
		const float tipped = std::atan2(across, std::max(below, 0.01f));
		const Bend leg = twoBones(m_foot[side].Z, -std::hypot(across, below), THIGH, SHIN, Bends::FORWARD);
		out.hip[side] = leg.root;
		out.hip_out[side] = tipped;
		out.knee[side] = leg.joint;
	}

	out.waist = key.waist;
	out.head = std::clamp(in.pitch, -1.2f, 1.2f);
	const float swing = key.swing * std::cos(m_phase * 2 * PI);
	for (int side : { 0, 1 }) {
		// each arm opposite its leg: side 0's foot is furthest ahead at phase 0, so its arm back
		const float way = side ? -1.f : 1.f;
		out.shoulder[side] = swing * way + key.arm - key.waist;
		out.shoulder_out[side] = key.arm_out * way;
		out.elbow[side] = -key.elbow;
	}
	return out;
}

} // namespace physics
