// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "physics/motion.h"
#include "physics/skeleton.h"
#include "physics/motion_cycles.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace physics {

namespace {

using namespace cycles;
constexpr float S = FIGURE_SCALE;
constexpr float LEG = (THIGH + SHIN) * S; // nodes: ours, what a recorded leg's length is made

// The run's, not the walk's, from a stroll up: a person going at the pace we call walking is jogging.
float runShare(float pace) { return std::clamp((pace - 0.8f) / 1.5f, 0.f, 1.f); }

// Where in its stride a cycle puts the right foot furthest ahead: our stride's phase 0.
int rightAhead(const Cycle &c)
{
	int best = 0;
	for (int p = 1; p < PHASES; p++)
		if (c.at[p][RIGHTANKLE][2] > c.at[best][RIGHTANKLE][2])
			best = p;
	return best;
}

v3f sample(const Cycle &c, int ahead, float phase, int joint)
{
	const float f = std::fmod(ahead + phase * PHASES, (float)PHASES);
	const int a = (int)f, b = (a + 1) % PHASES;
	const float t = f - a;
	v3f out;
	for (int k = 0; k < 3; k++)
		(&out.X)[k] = c.at[a][joint][k] + (c.at[b][joint][k] - c.at[a][joint][k]) * t;
	return out;
}

core::matrix4 axes(v3f x, v3f y, v3f z)
{
	core::matrix4 m;
	m[0] = x.X; m[1] = x.Y; m[2] = x.Z;
	m[4] = y.X; m[5] = y.Y; m[6] = y.Z;
	m[8] = z.X; m[9] = z.Y; m[10] = z.Z;
	return m;
}

// A bone that hangs along -y, pointed from its joint toward `to`, its x as near `across` as that allows.
core::matrix4 aim(v3f along, v3f across)
{
	v3f y = along * -1.f;
	y.normalize();
	v3f x = across - y * across.dotProduct(y);
	if (x.getLengthSQ() < 1e-6f)
		x = v3f(1, 0, 0) - y * y.X;
	x.normalize();
	return axes(x, y, x.crossProduct(y));
}

// The way a two-bone limb bends, as its first bone's x: square to both, toward `side`.
v3f bendAxis(v3f first, v3f second, v3f side)
{
	v3f n = first.crossProduct(second);
	if (n.getLengthSQ() < 1e-6f)
		return side;
	n.normalize();
	return n.dotProduct(side) < 0 ? n * -1.f : n;
}

core::matrix4 against(const core::matrix4 &parent, const core::matrix4 &child)
{
	core::matrix4 inv;
	parent.getInverse(inv);
	return inv * child;
}

struct Q
{
	float x, y, z, w;
};

Q toQ(const core::matrix4 &m)
{
	const float xx = m[0], xy = m[1], xz = m[2], yx = m[4], yy = m[5], yz = m[6], zx = m[8], zy = m[9], zz = m[10];
	const float trace = xx + yy + zz;
	if (trace > 0) {
		const float r = 2 * std::sqrt(1 + trace);
		return { (yz - zy) / r, (zx - xz) / r, (xy - yx) / r, r / 4 };
	} else if (xx >= yy && xx >= zz) {
		const float r = 2 * std::sqrt(1 + xx - yy - zz);
		return { r / 4, (xy + yx) / r, (zx + xz) / r, (yz - zy) / r };
	} else if (yy >= zz) {
		const float r = 2 * std::sqrt(1 + yy - xx - zz);
		return { (xy + yx) / r, r / 4, (yz + zy) / r, (zx - xz) / r };
	}
	const float r = 2 * std::sqrt(1 + zz - xx - yy);
	return { (zx + xz) / r, (yz + zy) / r, r / 4, (xy - yx) / r };
}

core::matrix4 fromQ(Q q)
{
	core::matrix4 m;
	m[0] = 1 - 2 * (q.y * q.y + q.z * q.z);
	m[1] = 2 * (q.x * q.y + q.z * q.w);
	m[2] = 2 * (q.x * q.z - q.y * q.w);
	m[4] = 2 * (q.x * q.y - q.z * q.w);
	m[5] = 1 - 2 * (q.x * q.x + q.z * q.z);
	m[6] = 2 * (q.y * q.z + q.x * q.w);
	m[8] = 2 * (q.x * q.z + q.y * q.w);
	m[9] = 2 * (q.y * q.z - q.x * q.w);
	m[10] = 1 - 2 * (q.x * q.x + q.y * q.y);
	return m;
}

BonePlace blend(const BonePlace &a, const BonePlace &b, float t)
{
	if (t <= 0)
		return a;
	if (t >= 1)
		return b;
	core::matrix4 ma, mb;
	ma.setRotationDegrees(a.turn);
	mb.setRotationDegrees(b.turn);
	Q qa = toQ(ma), qb = toQ(mb);
	if (qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w < 0)
		qb = { -qb.x, -qb.y, -qb.z, -qb.w };
	Q q{ qa.x + (qb.x - qa.x) * t, qa.y + (qb.y - qa.y) * t, qa.z + (qb.z - qa.z) * t, qa.w + (qb.w - qa.w) * t };
	const float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	q = { q.x / l, q.y / l, q.z / l, q.w / l };
	return { a.at + (b.at - a.at) * t, fromQ(q).getRotationDegrees() };
}

} // namespace

Bones blend(const Bones &a, const Bones &b, float t)
{
	Bones out;
	out.lower = blend(a.lower, b.lower, t);
	out.hips = blend(a.hips, b.hips, t);
	out.chest = blend(a.chest, b.chest, t);
	out.head = blend(a.head, b.head, t);
	for (int side : { 0, 1 }) {
		out.thigh[side] = blend(a.thigh[side], b.thigh[side], t);
		out.shin[side] = blend(a.shin[side], b.shin[side], t);
		out.upper[side] = blend(a.upper[side], b.upper[side], t);
		out.fore[side] = blend(a.fore[side], b.fore[side], t);
		out.hand[side] = blend(a.hand[side], b.hand[side], t);
	}
	return out;
}

float Strides::strideAt(float pace)
{
	const float run = runShare(pace);
	const float recorded_pace = (WALK.pace + (RUN.pace - WALK.pace) * run) * LEG;
	const float recorded_stride = (WALK.pace * WALK.period + (RUN.pace * RUN.period - WALK.pace * WALK.period) * run) * LEG;
	return recorded_stride * std::clamp(pace / std::max(recorded_pace, 0.01f), 1.f, LONGEST);
}

Bones Strides::step(float dt, const Bones &asked, v3f moving, float turned, float twist, bool on)
{
	const float pace = std::hypot(moving.X, moving.Z);
	constexpr float COMES_IN = 5; // /s: it takes over, and lets go, over a fifth of a second
	const float going = std::clamp(pace / 0.6f, 0.f, 1.f);
	m_share += ((on ? going : 0.f) - m_share) * std::min(1.f, COMES_IN * dt);
	if (m_share < 0.005f) {
		m_share = 0;
		m_planted[0] = m_planted[1] = m_locked[0] = m_locked[1] = false;
		m_held[0] = m_held[1] = 0;
		return asked;
	}
	const float run = runShare(pace);
	const float recorded_pace = (WALK.pace + (RUN.pace - WALK.pace) * run) * LEG;
	const float longer = std::clamp(pace / std::max(recorded_pace, 0.01f), 1.f, LONGEST);
	m_phase = std::fmod(m_phase + dt * pace / std::max(strideAt(pace), 0.05f), 1.f);

	static const int walk_ahead = rightAhead(WALK), run_ahead = rightAhead(RUN);
	const auto at = [&](int joint) {
		v3f p = sample(WALK, walk_ahead, m_phase, joint) * (1 - run) + sample(RUN, run_ahead, m_phase, joint) * run;
		// a longer stride: the legs reach further ahead and behind, not quicker
		if (joint == RIGHTKNEE || joint == RIGHTANKLE || joint == RIGHTTOE || joint == LEFTKNEE || joint == LEFTANKLE || joint == LEFTTOE)
			p.Z *= longer;
		return p;
	};
	const v3f hips(0, 0, 0);
	const v3f pelvis_x = at(RIGHTHIP) - at(LEFTHIP), chest_x = at(RIGHTSHOULDER) - at(LEFTSHOULDER);
	const core::matrix4 pelvis = aim(hips - at(CHEST), pelvis_x);
	const core::matrix4 chest = aim(at(CHEST) - at(NECK), chest_x);
	const core::matrix4 head = aim(at(NECK) - at(HEAD_END), chest_x);
	core::matrix4 thigh[2], shin[2], upper[2], fore[2];
	const int HIP[2] = { RIGHTHIP, LEFTHIP }, KNEE[2] = { RIGHTKNEE, LEFTKNEE }, ANKLE[2] = { RIGHTANKLE, LEFTANKLE };
	const int SHOULDER[2] = { RIGHTSHOULDER, LEFTSHOULDER }, ELBOW[2] = { RIGHTELBOW, LEFTELBOW }, WRIST[2] = { RIGHTWRIST, LEFTWRIST };
	for (int side : { 0, 1 }) {
		const v3f up = at(KNEE[side]) - at(HIP[side]), down = at(ANKLE[side]) - at(KNEE[side]);
		const v3f knee = bendAxis(up, down, pelvis_x);
		thigh[side] = aim(up, knee);
		shin[side] = aim(down, knee);
		const v3f arm = at(ELBOW[side]) - at(SHOULDER[side]), forearm = at(WRIST[side]) - at(ELBOW[side]);
		const v3f elbow = bendAxis(arm, forearm, chest_x);
		upper[side] = aim(arm, elbow);
		fore[side] = aim(forearm, elbow);
	}
	// a foot's down as long as it's within a little of as low as that foot goes
	for (int side : { 0, 1 }) {
		const Cycle &c = run > 0.5f ? RUN : WALK;
		float low = 1e3f;
		for (int p = 0; p < PHASES; p++)
			low = std::min(low, c.at[p][ANKLE[side]][1]);
		m_planted[side] = at(ANKLE[side]).Y < low + 0.04f;
	}
	// toward where it's going, as the pose turns its hips
	core::matrix4 turn;
	turn.setRotationRadians(v3f(0, twist, 0));
	const core::matrix4 lower = turn * pelvis;
	Bones b = asked;
	const float hip_y = HIP_HEIGHT * S;
	b.hips = { v3f(0, hip_y, 0), v3f(0) };
	b.lower.turn = lower.getRotationDegrees();
	b.lower.at = v3f(0) - lower.rotateAndScaleVect(v3f(0, hip_y, 0));
	b.chest.turn = against(lower, turn * chest).getRotationDegrees();
	b.head.turn = against(turn * chest, turn * head).getRotationDegrees();
	for (int side : { 0, 1 }) {
		const float x = side ? -1.f : 1.f;
		b.thigh[side] = { v3f(HIP_WIDE * x, HIP_HEIGHT, 0) * S, against(lower, turn * thigh[side]).getRotationDegrees() };
		b.shin[side].turn = against(thigh[side], shin[side]).getRotationDegrees();
		b.upper[side].turn = against(turn * chest, turn * upper[side]).getRotationDegrees();
		b.fore[side].turn = against(upper[side], fore[side]).getRotationDegrees();
		b.hand[side].turn = v3f(0);
	}
	// stood on its lower foot: whatever height that leaves the hips at
	float lowest = 1e3f;
	const core::matrix4 l = b.lower.matrix();
	for (int side : { 0, 1 }) {
		v3f foot(0, -SHIN * S, 0);
		(l * b.thigh[side].matrix() * b.shin[side].matrix()).transformVect(foot);
		lowest = std::min(lowest, foot.Y);
	}
	b.lower.at.Y -= lowest;

	// A foot that's down stays where it was put: the body's going on over it, so it goes back as fast in the
	// figure's frame, turned as the body turns. The leg's solved to it, its knee where the recording's points.
	// Coming up, it comes away from there over a moment, not all at once (sprint 105: drawn as posed, it snapped).
	constexpr float LETS_GO = 0.12f; // s
	core::matrix4 turned_by;
	turned_by.setRotationRadians(v3f(0, -turned, 0));
	const core::matrix4 l2 = b.lower.matrix();
	for (int side : { 0, 1 }) {
		v3f hip = b.thigh[side].at, foot(0, -SHIN * S, 0), knee(0, 0, 0);
		l2.transformVect(hip);
		const core::matrix4 leg = l2 * b.thigh[side].matrix();
		leg.transformVect(knee = v3f(0, -THIGH * S, 0));
		(leg * b.shin[side].matrix()).transformVect(foot);
		const bool down = m_planted[side] && m_share >= 0.5f;
		if (down && !m_locked[side] && m_held[side] <= 0) {
			m_locked[side] = true;
			m_lock[side] = v3f(foot.X, 0, foot.Z);
		} else if (m_locked[side] || m_held[side] > 0) {
			m_lock[side] = turned_by.rotateAndScaleVect(m_lock[side]) - v3f(moving.X, 0, moving.Z) * dt;
		}
		// past what the leg reaches, the foot comes up early: it's left behind, not dragged
		const v3f to = m_lock[side] - hip;
		if (!down || to.getLength() > (THIGH + SHIN) * S * 1.1f) // (a planted leg's about straight: the solve holds it so)
			m_locked[side] = false;
		m_held[side] = m_locked[side] ? 1 : std::max(0.f, m_held[side] - dt / LETS_GO);
		if (m_held[side] <= 0)
			continue;
		const v3f pole = (knee - hip) - (to / to.getLength()) * (knee - hip).dotProduct(to / to.getLength());
		const ArmTo solved = armTo(to / S, pole.getLengthSQ() > 1e-6f ? pole : v3f(0, 0, 1), THIGH, SHIN);
		// (armTo's axes are an arm's: its x is across the other way for a knee bending forward)
		const core::matrix4 thigh_now = axes(solved.x * -1.f, solved.y, solved.z * -1.f);
		b.thigh[side] = blend(b.thigh[side], { b.thigh[side].at, against(l2, thigh_now).getRotationDegrees() }, m_held[side]);
		b.shin[side] = blend(b.shin[side], { b.shin[side].at, v3f(-solved.elbow * core::RADTODEG, 0, 0) }, m_held[side]);
	}

	Bones out = asked;
	out.lower = blend(asked.lower, b.lower, m_share);
	out.hips = blend(asked.hips, b.hips, m_share);
	out.chest = blend(asked.chest, b.chest, m_share);
	out.head = blend(asked.head, b.head, m_share);
	for (int side : { 0, 1 }) {
		out.thigh[side] = blend(asked.thigh[side], b.thigh[side], m_share);
		out.shin[side] = blend(asked.shin[side], b.shin[side], m_share);
		out.upper[side] = blend(asked.upper[side], b.upper[side], m_share);
		out.fore[side] = blend(asked.fore[side], b.fore[side], m_share);
		out.hand[side] = blend(asked.hand[side], b.hand[side], m_share);
	}
	return out;
}

} // namespace physics
