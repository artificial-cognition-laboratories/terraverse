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
constexpr float STRIDE_WALK = 1.8f, STRIDE_RUN = 2.4f;
constexpr float LIFT = 0.2f;
constexpr float MOST_FOOT_SPEED = 10; // nodes/s, against the hips: quicker than a sprint's swing
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
	// low on bent legs, back straight and tipped from the hips, arms loose outside the knees
	{ { 0.25f, 0.24f, 0.6f, 0.f, -0.15f, 0.5f, 0.36f, 0.08f, 0.f, 0.6f },
	  { 0.3f, 0.24f, 0.7f, 0.3f, -0.15f, 0.5f, 0.36f, 0.08f, 0.f, 0.6f }, 1.13f, WALK * 0.5f, 0 },
	// on the belly, arms ahead: the capsule's round the shoulders, so the legs trail behind it
	{ { 0.1f, 0.15f, 1.1f, 0.f, -2.5f, 1.4f, 0.55f, 0.68f, 0.1f, 0.4f },
	  { 0.1f, 0.15f, 1.1f, 0.35f, -2.5f, 1.4f, 0.55f, 0.68f, 0.1f, 0.4f }, 0.3f, WALK * 0.25f, 0.6f },
	// leaning back, feet ahead, no stride: it isn't walking
	{ { -0.1f, 0.45f, 0.5f, 0.f, -0.5f, -0.55f, 0.42f, -0.42f, 0.06f, 0.f },
	  { -0.1f, 0.45f, 0.5f, 0.f, -0.5f, -0.55f, 0.42f, -0.42f, 0.06f, 0.f }, 0.3f, WALK, 0 },
	// a tuck, turned over whole by Joints::spin
	{ { 0.75f, 0.2f, 1.5f, 0.f, -1.f, 0.9f, 0.3f, 0.05f, 0.22f, 0.f },
	  { 0.75f, 0.2f, 1.5f, 0.f, -1.f, 0.9f, 0.3f, 0.05f, 0.22f, 0.f }, 1.13f, WALK, 0 },
	// a hand down on it, legs swung up and across
	{ { 0.5f, 0.5f, 0.4f, 0.f, -1.f, 0.3f, 0.25f, -0.2f, 0.35f, 0.f },
	  { 0.5f, 0.5f, 0.4f, 0.f, -1.f, 0.3f, 0.25f, -0.2f, 0.35f, 0.f }, 1.13f, WALK, 0 },
	// hauled up (the hands are on the edge by Grip); let go, the arms trail down and out, knees up
	{ { 0.6f, 0.35f, 0.9f, 0.f, 0.25f, 0.3f, 0.2f, 0.1f, 0.4f, 0.f },
	  { 0.6f, 0.35f, 0.9f, 0.f, 0.25f, 0.3f, 0.2f, 0.1f, 0.4f, 0.f }, 1.13f, WALK, 0 },
	// arms overhead, legs hanging
	{ { 0.f, 0.1f, 0.15f, 0.f, -3.f, 0.f, 0.f, 0.f, 0.f, 0.f },
	  { 0.f, 0.1f, 0.15f, 0.f, -3.f, 0.f, 0.f, 0.f, 0.f, 0.f }, 3.2f, WALK, -0.12f },
	// running in the air, on the wall: it's leaned from it by Figure::pose's tilt
	{ SPRINT, SPRINT, 1.13f, WALK, 0 },
	// down: arms out to it; everything else by Joints::fallen and the feet being drawn under it
	{ { 0.f, 0.5f, 0.6f, 0.f, -1.2f, 0.f, 0.f, 0.f, 0.f, 0.f },
	  { 0.f, 0.5f, 0.6f, 0.f, -1.2f, 0.f, 0.f, 0.f, 0.f, 0.f }, 3.2f, WALK, 0 },
};
// In the air, a foot's {z, y} for the trailing leg [0] and the leading [1].
constexpr float AIR_RISE[2][2] = { { -0.5f, 0.22f }, { 0.42f, 0.42f } };
constexpr float AIR_PEAK[2][2] = { { -0.28f, 0.3f }, { 0.34f, 0.38f } };
constexpr float AIR_FALL[2][2] = { { -0.26f, 0.12f }, { 0.3f, 0.08f } };

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

ArmTo armTo(v3f to, v3f pole, float first, float second)
{
	const float most = first + second - 1e-3f, least = std::fabs(first - second) + 1e-3f;
	const float length = to.getLength();
	const float reach = std::clamp(length, least, most);
	const v3f line = length > 1e-4f ? to / length : v3f(0, -1, 0);
	// the elbow's off the line toward the pole, whichever way the arm points
	v3f off = pole - line * pole.dotProduct(line);
	if (off.getLengthSQ() < 1e-6f)
		off = v3f(0, 0, -1) - line * -line.Z;
	off.normalize();
	const float a = (first * first - second * second + reach * reach) / (2 * reach);
	const float h = std::sqrt(std::max(first * first - a * a, 0.f));
	v3f upper = line * a + off * h;
	upper.normalize();
	const v3f x = line.crossProduct(off);
	const v3f y = upper * -1.f;
	const float inside = std::acos(std::clamp((first * first + second * second - reach * reach) / (2 * first * second), -1.f, 1.f));
	return { x, y, x.crossProduct(y), -(PI - inside) };
}

void HandOverHand::place(v3f rest0, v3f rest1)
{
	m_at[0] = rest0;
	m_at[1] = rest1;
	m_rest = rest0;
	m_moving = -1;
}

void HandOverHand::step(float dt, v3f rest0, v3f rest1, const std::function<v3f(v3f)> &keep)
{
	// One, two: the leading hand reaches well out, the trailing one comes up after it. Each stays
	// till the body's gone on from it: the leading hand mustn't drift in to the head, the trailing's let go long.
	constexpr float INSIDE = 0.12f, REACH = 0.4f, LEAD_IN = 0.1f, TRAIL_OUT = 0.15f, TRAIL_TO = 0.05f, SETTLED = 0.06f, TAKES = 0.26f, LIFTS = 0.14f;
	const v3f rest[2] = { rest0, rest1 };
	v3f axis = rest0 - rest1;
	axis.normalize();
	const float going = dt > 0 ? (rest0 - m_rest).dotProduct(axis) / dt : 0;
	m_rest = rest0;
	const float way_now = going > 0.1f ? 1.f : going < -0.1f ? -1.f : 0.f;
	// turned back with a hand in the air: it comes home, not on to where it was going
	if (m_moving >= 0 && way_now != 0 && m_way != 0 && way_now != m_way) {
		m_from = m_at[m_moving];
		m_to = keep ? keep(rest[m_moving]) : rest[m_moving];
		m_t = 0;
	}
	if (way_now != 0)
		m_way = way_now;
	if (m_moving >= 0) {
		m_t = std::min(1.f, m_t + dt / TAKES);
		m_at[m_moving] = m_from + (m_to - m_from) * m_t + v3f(0, LIFTS * std::sin(m_t * PI), 0);
		if (m_t >= 1) {
			m_at[m_moving] = m_to;
			m_last = m_moving;
			m_moving = -1;
		}
		return;
	}
	const auto move = [&](int side, v3f to) {
		m_moving = side;
		m_t = 0;
		m_from = m_at[side];
		m_to = keep ? keep(to) : to;
	};
	const float off[2] = { (m_at[0] - rest0).dotProduct(axis), (m_at[1] - rest1).dotProduct(axis) };
	if (std::fabs(going) < 0.1f) { // stopped: each home, once
		const int worse = std::fabs(off[0]) >= std::fabs(off[1]) ? 0 : 1;
		if (std::fabs(off[worse]) > SETTLED)
			move(worse, rest[worse]);
		return;
	}
	const float way = going > 0 ? 1.f : -1.f;
	// to where that'll be when the hand gets there: the body goes on while it's in the air
	const v3f on = axis * (going * TAKES);
	const int lead = going > 0 ? 0 : 1, trail = 1 - lead;
	// a hand come in as far as the head goes now, whoever's turn it is
	for (const int side : { 0, 1 })
		if (off[side] * (side ? -1.f : 1.f) < -INSIDE) {
			move(side, rest[side] + on + (side == lead ? axis * (way * REACH) : v3f(0)));
			return;
		}
	if (m_last != lead && off[lead] * way < -LEAD_IN)
		move(lead, rest[lead] + on + axis * (way * REACH));
	else if (m_last != trail && off[trail] * way < -TRAIL_OUT)
		move(trail, rest[trail] + on + axis * (way * TRAIL_TO));
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
	// A roll lands first: crouched into it, arms going forward, then springs over.
	const bool absorbing = in.stance == Stance::ROLL && m_roll < ROLL_ABSORB;
	const Row &row = ROWS[(int)(absorbing ? Stance::CROUCH : in.stance)];

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
	// back within what it can twist, not at once: off a wall it may be well past that
	if (in.faces) {
		m_coming_back = true;
	} else {
		const float limit = std::clamp(m_twist, -row.twist, row.twist);
		if (m_coming_back && std::fabs(limit - m_twist) > 8 * dt)
			m_twist += std::clamp(limit - m_twist, -8 * dt, 8 * dt);
		else {
			m_twist = limit;
			m_coming_back = false;
		}
	}
	if (in.faces) { // on a wall: turning square to it (or side on) quickly, wherever the view goes
		const float gap = std::remainder(in.face - m_twist, 2 * PI);
		m_twist += gap * std::min(1.f, 16 * dt);
	}

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
	// (quick off the ground, slower back onto it: the legs take the landing, they don't snap to a stride)
	const float air = m_air.step(in.airborne ? 1.f : 0.f, in.airborne ? 14.f : 8.f, dt);

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
	out.chest_twist = in.faces ? 0 : -m_twist * 0.2f; // (held to a wall the chest stays square to it with the hips)
	m_roll = in.stance == Stance::ROLL ? m_roll + dt : 0;
	// Slow as the legs take the fall, then thrown. The body turns so the leading shoulder goes
	// down first, and comes back straight: the ground runs across the back, shoulder to hip.
	const float through = std::clamp((m_roll - ROLL_ABSORB) / (ROLL_TIME - ROLL_ABSORB), 0.f, 1.f);
	out.spin = in.stance == Stance::ROLL ? 2 * PI * through * through * (3 - 2 * through) : 0;
	out.spin_lean = in.stance == Stance::ROLL ? 0.75f * std::sin(PI * through) : 0;
	// lying, the body's over where it was, not beside it: the feet go back as far as the head came on
	const float lying = std::fabs(in.fallen) / 1.5f;
	out.fallen = in.fallen;
	out.back = m_low[9].step(row.back + 0.75f * lying * (in.fallen < 0 ? -1.f : 1.f), 14, dt);
	out.pitch = key.pitch;
	float lowest = 0;
	const float rise = m_rise.step(std::clamp(in.moving.Y / 6, -1.f, 1.f), 12, dt);
	const float up = std::max(rise, 0.f), down = std::max(-rise, 0.f);
	const float until = in.moving.Y < -0.5f ? in.above / -in.moving.Y : 1e3f; // s till the ground
	const float landing = std::clamp(1 - until / 0.3f, 0.f, 1.f);
	const float push = std::min(in.push.getLength(), 1.f);
	const v3f pushed = push > 0.01f ? intoTurned(in.push, m_twist) / in.push.getLength() : v3f(0);
	// A foot up on something is a step in its stride, late on as the body goes over it: what follows is
	// the other leg coming through, not this one thrown across again.
	if (in.steps && in.feet != 2)
		m_phase = std::fmod((in.feet == 1 || (in.feet < 0 && m_lead == 1) ? 0.5f : 0.f) + 0.4f + 1.f, 1.f);
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
		// In the air, by how it's rising: up off one leg with the other knee driving, tucked over the top, reaching down to land
		const bool lead = side == m_lead;
		const float wide = 1 + 0.3f * std::clamp(flat_pace / 6, 0.f, 1.f);
		const float at = lerp(lerp(AIR_PEAK[lead][0], AIR_RISE[lead][0], up), AIR_FALL[lead][0], down) * wide;
		const float high = lerp(lerp(AIR_PEAK[lead][1], AIR_RISE[lead][1], up), AIR_FALL[lead][1], down);
		// and about to land: the lead foot out for the place it's coming down, further the faster it's going
		const float land_at = lead ? std::min(0.12f + 0.05f * flat_pace, 0.45f) : -0.05f - 0.03f * flat_pace;
		const v3f hung = v3f(side_x, high, at) + (v3f(side_x, lead ? 0.03f : 0.14f, land_at) - v3f(side_x, high, at)) * landing;
		v3f foot_to = walked + (hung - walked) * air;
		// off a wall: the leg it kicked with is still straightening toward it, whatever the air's blend has got to
		const bool takes = in.feet == 2 || in.feet == side || (in.feet < 0 && lead);
		if (takes && push > 0)
			foot_to += (v3f(side_x, HIP_HEIGHT - 0.2f, 0) + pushed * 0.62f - foot_to) * std::sqrt(push);
		if (takes && in.steps) // (both: staggered, as feet are, not a block)
			foot_to = intoTurned(in.step_on, m_twist) + (in.feet == 2 ? v3f(side_x, lead ? 0.1f : -0.1f, 0) : v3f(0));
		if (in.stance == Stance::DOWN) {
			// drawn under it as it gets up: a knee, then a foot ahead of the other, then stood on
			const float tuck = std::sin(PI * in.rising);
			foot_to = v3f(side_x, 0.5f * tuck, lead ? 0.25f * tuck : -0.1f * tuck);
		}
		// a foot on the ground goes where it must; one in the air has only so much speed. Without
		// this a change of target (leaving the floor, landing, a new move) is a leg snapped across.
		// (a foot put up on something is going there, not on the ground yet: sprint 105, drawn as posed, it jumped)
		if (stance && air < 0.02f && !in.airborne && !(takes && in.steps)) {
			m_foot[side] = foot_to;
		} else {
			const v3f move = foot_to - m_foot[side];
			const float most = MOST_FOOT_SPEED * dt;
			m_foot[side] += move.getLength() > most ? move / move.getLength() * most : move;
		}
		out.foot[side] = m_foot[side];
		out.planted[side] = stance && air < 0.5f;
		lowest = std::min(lowest, m_ground[side]);
	}

	// The mass. What it feels is how its going changed, in the frame of the hips.
	const v3f going_now = intoTurned(flat, m_twist);
	const float take = std::min(1.f, 10 * dt);
	const v3f speeding = (going_now - m_going) * 10.f; // nodes/s², smoothed
	if (in.stance == Stance::HANG) {
		// each hand let go, it hangs from the other for a moment: the legs go that way
		constexpr float HAND_SWING = 0.9f;
		if (in.hand_moving >= 0 && in.hand_moving != m_hand_was)
			m_side_speed += in.hand_moving == 0 ? -HAND_SWING : HAND_SWING;
		m_hand_was = in.hand_moving;
		// a pendulum of its own length under its hands. Its hands stopped; it didn't.
		constexpr float RATE = 5, DAMPING = 2 * 0.5f * RATE, LENGTH = 1.3f, HELD = 0.5f; // (arms and back hold it: HELD of a push reaches the swing)
		const float h = std::min(dt, 0.05f);
		const v3f change = (going_now - m_going) * take; // (what m_going takes up this step: each bit of change counts once)
		m_side_speed -= HELD * change.X / LENGTH;
		m_back_speed += HELD * change.Z / LENGTH;
		m_side_speed -= (m_side * RATE * RATE + m_side_speed * DAMPING) * h;
		m_back_speed -= (m_back * RATE * RATE + m_back_speed * DAMPING) * h;
		m_side += m_side_speed * h;
		if (std::fabs(m_side) > 0.3f) { // as far as arms let it: it's stopped there
			m_side = std::clamp(m_side, -0.3f, 0.3f);
			m_side_speed = 0;
		}
		m_back += m_back_speed * h;
		if (m_back < 0) { // the wall's there: the feet come off it again
			m_back = 0;
			m_back_speed = std::fabs(m_back_speed) * 0.4f;
		}
		m_back = std::min(m_back, 0.3f);
		out.swing_back = m_back;
		out.swing_side = m_side;
	} else {
		m_back = m_back_speed = m_side = m_side_speed = 0;
	}
	if (in.stance == Stance::VAULT) {
		out.swing_side = in.throw_side;
		out.swing_back = in.throw_back;
	}
	// over its feet: into speeding up, back from slowing, in toward a turn; and further at a run
	const bool footed = !in.airborne && in.stance != Stance::HANG && in.stance != Stance::VAULT && in.stance != Stance::MANTLE && in.stance != Stance::ROLL;
	out.lean_ahead = m_lean_ahead.step(footed ? std::clamp(speeding.Z * 0.015f, -0.15f, 0.15f) + 0.18f * running + 0.95f * in.stagger : 0, in.stagger > 0.1f ? 14.f : 6.f, dt);
	out.lean_right = m_lean_right.step(footed ? std::clamp(speeding.X * 0.02f, -0.12f, 0.12f) : 0, 6, dt);
	m_going += (going_now - m_going) * take;
	// a landing goes into the legs and comes back up
	if (in.airborne)
		m_fell = in.moving.Y;
	else if (m_was_air)
		m_dip.speed += std::min(std::max(-m_fell, 0.f) * 0.6f, 3.5f);
	if (in.airborne && !m_was_air && in.moving.Y > 0)
		m_lead = 1 - m_lead;
	m_was_air = in.airborne;
	const float dip = std::clamp(m_dip.step(0, 9, dt), 0.f, 0.35f);

	// Hips come down for a foot on lower ground, and sit lower at a run, so legs reach.
	out.drop = m_ground_drop.step(std::clamp(-lowest, 0.f, 0.45f) * (1 - air) + 0.05f * going + 0.02f + 0.08f * running, 18, dt) + key.drop + dip + m_low_stagger.step(in.stagger * 0.28f, 12, dt);
	const float hip_y = HIP_HEIGHT - out.drop;

	for (int side : { 0, 1 }) {
		const float across = m_foot[side].X - (side ? -HIP_WIDE : HIP_WIDE);
		// at the ends of a wide stride the leg's straight and the foot's come up off the ground: on its toes, not stretched
		const float leg_long = THIGH + SHIN - 0.002f;
		const float room = leg_long * leg_long - across * across - m_foot[side].Z * m_foot[side].Z;
		const float lowest_foot = hip_y - std::sqrt(std::max(room, 0.01f));
		if (m_foot[side].Y < lowest_foot) {
			m_foot[side].Y = lowest_foot;
			out.foot[side] = m_foot[side];
			out.planted[side] = false;
		}
		const float below = hip_y - m_foot[side].Y;
		// tip the leg out to the foot, then bend it in the plane that leaves
		const float tipped = std::atan2(across, std::max(below, 0.01f));
		const Bend leg = twoBones(m_foot[side].Z, -std::hypot(across, below), THIGH, SHIN, Bends::FORWARD);
		out.hip[side] = leg.root;
		out.hip_out[side] = tipped;
		out.knee[side] = leg.joint;
	}

	// Crawling on its belly: a knee drawn up the side as the other arm reaches, then pulled along on that elbow.
	if (in.stance == Stance::PRONE) {
		const float c = std::sin(m_phase * 2 * PI);
		for (int side : { 0, 1 }) {
			const float way = side ? -1.f : 1.f;
			const float up = std::max(0.f, side ? -c : c) * m_stride.at;
			out.crawl[side] = up;
			(void)way;
		}
	}
	out.waist = key.waist + (in.stance == Stance::DOWN ? 0.9f * std::sin(PI * in.rising) : 0.f);
	out.head = std::clamp(in.pitch, -1.2f, 1.2f);
	const float swing = key.swing * std::cos(m_phase * 2 * PI);
	for (int side : { 0, 1 }) {
		// each arm opposite its leg: side 0's foot is furthest ahead at phase 0, so its arm back
		const float way = side ? -1.f : 1.f;
		// in the air: the arm against the leg going up; out for balance coming down
		const float thrown = side == m_lead ? 0.7f * up : -0.7f * up;
		out.shoulder[side] = swing * way + key.arm - key.waist + thrown * air;
		out.shoulder_out[side] = (key.arm_out + 0.5f * down * air) * way;
		out.elbow[side] = -key.elbow;
		if (in.stance == Stance::PRONE) {
			// the arm opposite the drawn knee reaches; the other's out at the elbow, pulling
			const float c = std::sin(m_phase * 2 * PI) * m_stride.at;
			const float reach = std::max(0.f, side ? c : -c), pull = 1 - reach;
			out.shoulder[side] = key.arm - key.waist + 0.35f * pull;
			out.shoulder_out[side] = -(0.35f + 0.6f * pull) * way; // (the arm's ahead, over the head: out's the other way about)
			out.elbow[side] = -(0.4f + 1.3f * pull);
		}
	}
	return out;
}

} // namespace physics
