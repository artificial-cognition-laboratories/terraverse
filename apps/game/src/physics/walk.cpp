// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "walk.h"

#include <algorithm>
#include <cmath>

namespace physics {

v3f walk(const Gait &gait, const Intent &intent, const Footing &now, float dt)
{
	// which way it's asked to go, in the world: forward is the yaw's +z
	v3f ask(intent.right, 0, intent.forward);
	if (ask.getLengthSQ() > 1)
		ask.normalize();
	const float s = std::sin(intent.yaw), c = std::cos(intent.yaw);
	// Luanti's yaw turns +z toward −x
	const v3f dir(ask.X * c - ask.Z * s, 0, ask.X * s + ask.Z * c);
	const v3f target = dir * (gait.walk * gait.run);

	// across, relative to what it stands on
	const v3f ground = now.on_ground ? now.ground_velocity : v3f(0);
	v3f across(now.velocity.X - ground.X, 0, now.velocity.Z - ground.Z);
	// In the air: what it left with it keeps, unless it was barely moving.
	if (!now.on_ground && across.getLength() >= gait.air_reach)
		return v3f(across.X, now.velocity.Y, across.Z) + gait.gravity * dt;
	// Springing off: toward where it's asked, as far as a leg can throw it from the line it's
	// on, keeping nearly all its speed. So jumps can zigzag, but never send it backward.
	if (now.on_ground && intent.jump && across.getLength() > gait.walk * 0.75f && target.getLength() > 0.01f) {
		constexpr float MOST = 0.52f, COST = 0.15f; // radians; of its speed at the most
		const float pace = across.getLength();
		const v3f going = across / pace, toward = dir / dir.getLength();
		const float angle = std::acos(std::clamp(going.dotProduct(toward), -1.f, 1.f));
		const float turn = std::min(angle, MOST);
		v3f way = going;
		if (angle > 1e-4f && angle < 3.14f)
			way = (going * std::sin(angle - turn) + toward * std::sin(turn)) / std::sin(angle);
		const v3f flown = way * (pace * (1 - COST * turn / MOST));
		const float g = std::max(0.0f, -gait.gravity.Y);
		return v3f(flown.X + ground.X, ground.Y + std::sqrt(2 * g * gait.jump), flown.Z + ground.Z);
	}
	// Running, going where it's asked is a curve that costs speed: a clean line keeps it,
	// and builds. Turning back or letting go is braking, below.
	const float pace = across.getLength();
	if (now.on_ground && pace > gait.walk * 1.05f && target.getLength() > gait.walk) {
		constexpr float TURN = 4, TURN_COST = 0.35f, TURNS_BACK = 2.4f; // rad/s; speed lost per radian; radians
		const v3f going = across / pace;
		const float angle = std::acos(std::clamp(going.dotProduct(dir / dir.getLength()), -1.f, 1.f));
		const float top = gait.walk * gait.run * gait.flow;
		if (angle < TURNS_BACK && pace <= top + 1e-3f) {
			const float turn = std::min(angle, TURN * dt);
			const v3f toward = dir / dir.getLength();
			v3f way = going;
			if (angle > 1e-4f) // between the two, by slerp
				way = (going * std::sin(angle - turn) + toward * std::sin(turn)) / std::sin(angle);
			float speed = pace * std::exp(-TURN_COST * turn);
			if (angle < 0.05f) // only straight does it build
				speed = std::min(top, speed + (speed < gait.walk * gait.run ? gait.push_on : gait.flow_on) * dt);
			const v3f flown = way * speed;
			const float g = std::max(0.0f, -gait.gravity.Y);
			const float up = intent.jump ? ground.Y + std::sqrt(2 * g * gait.jump) : ground.Y;
			return v3f(flown.X + ground.X, up, flown.Z + ground.Z);
		}
	}
	const v3f gap = target - across;
	float rate;
	if (!now.on_ground) {
		rate = gait.air;
	} else if (target.dotProduct(across) < 0 || target.getLength() < across.getLength()) {
		rate = gait.brake; // slowing, or turning back
	} else {
		// getting going, and turning, is quick (feet grip); only going
		// faster the same way past a walk, to a run, is slow
		const bool onward = target.dotProduct(across) > 0.95f * target.getLength() * across.getLength();
		rate = across.getLength() >= gait.walk && onward ? gait.push_on : gait.start;
	}
	const float most = rate * dt;
	across += gap.getLength() > most ? gap * (most / gap.getLength()) : gap;

	if (now.on_ground) {
		const float g = std::max(0.0f, -gait.gravity.Y);
		const float up = intent.jump ? ground.Y + std::sqrt(2 * g * gait.jump) : ground.Y;
		return v3f(across.X + ground.X, up, across.Z + ground.Z);
	}
	// falling: the whole pull, steering only across
	return v3f(across.X, now.velocity.Y, across.Z) + gait.gravity * dt;
}

v3f slide(const Gait &gait, const Intent &intent, const Footing &now, float dt)
{
	constexpr float FRICTION = 4, STEER = 0.8f; // nodes/s²; rad/s
	const v3f ground = now.ground_velocity;
	v3f across(now.velocity.X - ground.X, 0, now.velocity.Z - ground.Z);
	// the slope pulls it along: gravity without what the ground takes
	const v3f pull = gait.gravity - now.ground_normal * gait.gravity.dotProduct(now.ground_normal);
	across += v3f(pull.X, 0, pull.Z) * dt;
	const float pace = across.getLength();
	if (pace > 1e-3f) {
		across *= std::max(0.f, pace - FRICTION * dt) / pace;
		// leaning: a little to the side it's asked
		const float turn = std::clamp(intent.right, -1.f, 1.f) * STEER * dt;
		const float c = std::cos(turn), s = std::sin(turn);
		across = v3f(across.X * c + across.Z * s, 0, -across.X * s + across.Z * c);
	}
	const float g = std::max(0.0f, -gait.gravity.Y);
	const float up = intent.jump ? ground.Y + std::sqrt(2 * g * gait.jump) : ground.Y;
	return v3f(across.X + ground.X, up, across.Z + ground.Z);
}

v3f roll(const Gait &gait, const Intent &intent, const Footing &now, float dt)
{
	const v3f ground = now.on_ground ? now.ground_velocity : v3f(0);
	v3f across(now.velocity.X - ground.X, 0, now.velocity.Z - ground.Z);
	float pace = across.getLength();
	v3f way;
	if (pace > 0.5f) {
		way = across / pace;
	} else {
		const float s = std::sin(intent.yaw), c = std::cos(intent.yaw);
		way = v3f(-s, 0, c); // the way it looks, if nothing's asked
		const float asked = std::hypot(intent.right, intent.forward);
		if (asked > 0.1f)
			way = v3f(intent.right * c - intent.forward * s, 0, intent.right * s + intent.forward * c) / asked;
	}
	pace = std::max(pace, gait.walk);
	const v3f moved = way * pace;
	return v3f(moved.X + ground.X, now.on_ground ? ground.Y : now.velocity.Y + gait.gravity.Y * dt, moved.Z + ground.Z);
}

float landingStagger(float fall, v3f velocity, v3f asked)
{
	const float hard = std::clamp((fall - 4) / 6, 0.f, 1.f) * 0.6f;
	const v3f across(velocity.X, 0, velocity.Z);
	const v3f flat(asked.X, 0, asked.Z);
	const float pace = across.getLength(), wants = flat.getLength();
	float against = 0;
	if (pace > 1 && wants > 0.1f) // (0 going the way it wants, 1 straight against it)
		against = (1 - across.dotProduct(flat) / (pace * wants)) / 2;
	return std::min(1.f, hard + against * std::min(pace / 7, 1.f) * 0.8f);
}

Fallen::Fallen(float severity, bool face_down) : m_rest(0.35f + 0.65f * std::clamp(severity, 0.f, 1.f)), m_face_down(face_down) {}

void Fallen::step(float dt, bool wants_up)
{
	constexpr float TIPS = 0.28f, LEAST_DOWN = 0.3f, GETS_UP = 1.0f, QUICKLY = 0.55f;
	m_t += dt;
	if (m_up_time == 0) {
		if (m_t >= TIPS + m_rest || (wants_up && m_t >= TIPS + LEAST_DOWN))
			m_up_time = wants_up ? QUICKLY : GETS_UP;
		return;
	}
	m_up = std::min(1.f, m_up + dt / m_up_time);
}

bool Fallen::handsDown() const
{
	return m_face_down ? (m_t > 0.08f && m_up < 0.6f) : (m_up > 0 && m_up < 0.45f);
}

float Fallen::angle() const
{
	constexpr float LYING = 1.5f, TIPS = 0.28f;
	const float over = std::min(1.f, m_t / TIPS);
	float a = LYING * over * over; // slow, then all at once
	if (m_up > 0) {
		// hips first: little at the start of the push, most of it from the knees to the feet
		const float u = m_up * m_up * (3 - 2 * m_up);
		a = LYING * (1 - u);
	}
	return m_face_down ? a : -a;
}

Stance nextStance(Stance now, Stance asked, float pace, bool on_ground, float roll_left)
{
	switch (now) {
	case Stance::ROLL:
		return roll_left > 0 ? Stance::ROLL : asked;
	case Stance::SLIDE:
		return on_ground && asked == Stance::CROUCH && pace >= SLIDE_TO ? Stance::SLIDE : asked;
	case Stance::STAND:
		if (asked == Stance::CROUCH && on_ground && pace >= SLIDE_FROM)
			return Stance::SLIDE;
		break;
	default:
		break;
	}
	// getting lower takes ground to do it on; in the air it waits till it lands
	return (int)asked > (int)now && !on_ground ? now : asked;
}

} // namespace physics
