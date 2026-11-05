// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "physics/limbs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace physics {

namespace {

constexpr float S = FIGURE_SCALE;
// A little inside what's drawn: limbs touch where they look to, and don't catch on block edges.
constexpr float SLIM = 0.85f;

enum Part { P_HIPS, P_CHEST, P_HEAD, P_THIGH, P_SHIN = P_THIGH + 2, P_UPPER = P_SHIN + 2, P_FORE = P_UPPER + 2, PARTS = P_FORE + 2 };

core::matrix4 moved(v3f by)
{
	core::matrix4 m;
	m.setTranslation(by);
	return m;
}

// A hanging bone's middle, against its joint.
constexpr v3f hanging(float length) { return v3f(0, (BONE_OVERLAP - length) / 2 * S, 0); }

// Each part's middle, against its bone. Parts are placed by their middles, so it's what they turn about.
std::array<v3f, PARTS> middles()
{
	std::array<v3f, PARTS> c;
	c[P_HIPS] = v3f(0, HIPS_TALL / 2 * S, 0);
	c[P_CHEST] = v3f(0, CHEST_TALL / 2 * S, 0);
	c[P_HEAD] = v3f(0, HEAD_SIZE / 2 * S, 0);
	for (int side : { 0, 1 }) {
		c[P_THIGH + side] = hanging(physics::THIGH);
		c[P_SHIN + side] = hanging(physics::SHIN);
		c[P_UPPER + side] = hanging(UPPER_ARM);
		c[P_FORE + side] = hanging(FOREARM);
	}
	return c;
}

// Each part in the figure's frame.
std::array<core::matrix4, PARTS> placed(const Bones &b)
{
	const std::array<v3f, PARTS> c = middles();
	const core::matrix4 lower = b.lower.matrix(), hips = lower * b.hips.matrix(), chest = hips * b.chest.matrix();
	std::array<core::matrix4, PARTS> m;
	m[P_HIPS] = hips * moved(c[P_HIPS]);
	m[P_CHEST] = chest * moved(c[P_CHEST]);
	m[P_HEAD] = chest * b.head.matrix() * moved(c[P_HEAD]);
	for (int side : { 0, 1 }) {
		const core::matrix4 thigh = lower * b.thigh[side].matrix(), upper = chest * b.upper[side].matrix();
		m[P_THIGH + side] = thigh * moved(c[P_THIGH + side]);
		m[P_SHIN + side] = thigh * b.shin[side].matrix() * moved(c[P_SHIN + side]);
		m[P_UPPER + side] = upper * moved(c[P_UPPER + side]);
		m[P_FORE + side] = upper * b.fore[side].matrix() * moved(c[P_FORE + side]);
	}
	return m;
}

core::matrix4 frame(float yaw)
{
	// +x its right, +z ahead: Actor's toWorld
	const float c = std::cos(yaw), s = std::sin(yaw);
	core::matrix4 m;
	m[0] = c; m[2] = s;
	m[8] = -s; m[10] = c;
	return m;
}

// A matrix's turn: its first three elements of each row are where x, y and z go.
Quat toQuat(const core::matrix4 &m)
{
	const float xx = m[0], xy = m[1], xz = m[2], yx = m[4], yy = m[5], yz = m[6], zx = m[8], zy = m[9], zz = m[10];
	const float trace = xx + yy + zz;
	Quat q;
	if (trace > 0) {
		const float r = 2 * std::sqrt(1 + trace);
		q = { (yz - zy) / r, (zx - xz) / r, (xy - yx) / r, r / 4 };
	} else if (xx >= yy && xx >= zz) {
		const float r = 2 * std::sqrt(1 + xx - yy - zz);
		q = { r / 4, (xy + yx) / r, (zx + xz) / r, (yz - zy) / r };
	} else if (yy >= zz) {
		const float r = 2 * std::sqrt(1 + yy - xx - zz);
		q = { (xy + yx) / r, r / 4, (yz + zy) / r, (zx - xz) / r };
	} else {
		const float r = 2 * std::sqrt(1 + zz - xx - yy);
		q = { (zx + xz) / r, (yz + zy) / r, r / 4, (xy - yx) / r };
	}
	return q;
}

core::matrix4 fromQuat(const Quat &q)
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

Quat between(const Quat &a, Quat b, float t)
{
	if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0)
		b = { -b.x, -b.y, -b.z, -b.w };
	Quat q{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t };
	const float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	return { q.x / l, q.y / l, q.z / l, q.w / l };
}

std::vector<Pose> inWorld(const Bones &b, const v3d &foot, float yaw)
{
	const core::matrix4 turn = frame(yaw);
	std::vector<Pose> out;
	for (const core::matrix4 &m : placed(b)) {
		const core::matrix4 w = turn * m;
		out.push_back({ foot + v3d::from(w.getTranslation()), toQuat(w) });
	}
	return out;
}

std::vector<LimbSpec> specs()
{
	const auto limb = [](float wide, float length, float mass, int parent, v3f joint) {
		LimbSpec p;
		p.capsule = true;
		const float r = wide / 2 * S * SLIM;
		p.half = v3f(r, std::max(0.01f, (length + BONE_OVERLAP) / 2 * S - r), 0);
		p.mass = mass;
		p.parent = parent;
		p.joint = joint;
		return p;
	};
	std::vector<LimbSpec> parts(PARTS);
	const std::array<v3f, PARTS> c = middles();
	parts[P_HIPS].half = v3f(0.48f, HIPS_TALL, 0.23f) * (S * SLIM / 2);
	parts[P_HIPS].mass = 15;
	parts[P_CHEST].half = v3f(0.5f, CHEST_TALL, BONE_WIDE) * (S * SLIM / 2);
	parts[P_CHEST].mass = 25;
	parts[P_CHEST].parent = P_HIPS;
	parts[P_CHEST].joint = c[P_CHEST] * -1.f;
	parts[P_CHEST].cone = 0.8f;
	parts[P_HEAD].half = v3f(HEAD_SIZE) * (S * SLIM / 2);
	parts[P_HEAD].mass = 5;
	parts[P_HEAD].parent = P_CHEST;
	parts[P_HEAD].joint = c[P_HEAD] * -1.f;
	parts[P_HEAD].cone = 0.7f;
	for (int side : { 0, 1 }) {
		parts[P_THIGH + side] = limb(BONE_WIDE, physics::THIGH, 10, P_HIPS, c[P_THIGH + side] * -1.f);
		// a hip swings a long way forward, a little back; a shoulder up and forward, hardly behind
		parts[P_THIGH + side].cone = 1.4f;
		parts[P_THIGH + side].cone_axis = v3f(0, -0.7f, 0.7f);
		parts[P_THIGH + side].passes = { P_CHEST }; // (bending at the hips brings them together: that's the hips' to stop)
		parts[P_SHIN + side] = limb(BONE_NARROW, physics::SHIN, 5, P_THIGH + side, c[P_SHIN + side] * -1.f);
		parts[P_SHIN + side].bends_from = -0.1f;
		parts[P_SHIN + side].bends_to = 2.6f;
		parts[P_UPPER + side] = limb(BONE_WIDE, UPPER_ARM, 3, P_CHEST, c[P_UPPER + side] * -1.f);
		parts[P_UPPER + side].cone = 1.85f;
		parts[P_UPPER + side].cone_axis = v3f(0, -0.26f, 0.97f);
		parts[P_FORE + side] = limb(BONE_NARROW, FOREARM, 2, P_UPPER + side, c[P_FORE + side] * -1.f);
		parts[P_FORE + side].bends_from = -2.6f; // (an elbow bends one way)
		parts[P_FORE + side].bends_to = 0.05f;
	}
	return parts;
}

// The straight figure its joints are measured from, standing at the origin.
Bones standing()
{
	Bones b;
	b.hips.at = v3f(0, HIP_HEIGHT * S, 0);
	b.chest.at = v3f(0, HIPS_TALL * S, 0);
	b.head.at = v3f(0, CHEST_TALL * S, 0);
	for (int side : { 0, 1 }) {
		const float x = side == 0 ? 1.f : -1.f;
		b.thigh[side].at = v3f(HIP_WIDE * x, HIP_HEIGHT * S, 0);
		b.shin[side].at = v3f(0, -physics::THIGH * S, 0);
		b.upper[side].at = v3f(SHOULDER_OUT * x, SHOULDER_UP, 0) * S;
		b.fore[side].at = v3f(0, -UPPER_ARM * S, 0);
	}
	return b;
}

core::matrix4 fromPose(const Pose &p, const v3d &foot, const core::matrix4 &unturn)
{
	core::matrix4 m = fromQuat(p.rotation);
	m.setTranslation(v3f::from(p.position - foot));
	return unturn * m;
}

} // namespace

Limbs::Limbs(World &world, const Bones &bones, const v3d &foot, float yaw, v3f velocity) : m_world(world), m_asked(bones)
{
	m_now = m_was = inWorld(bones, foot, yaw);
	m_id = m_world.addLimbs(specs(), inWorld(standing(), v3d(0, -1000, 0), 0), m_now, velocity);
}

Limbs::~Limbs() { m_world.removeLimbs(m_id); }

void Limbs::drive(const Bones &bones, const v3d &foot, float yaw, float stiff, float tone)
{
	m_asked = bones;
	m_world.driveLimbs(m_id, inWorld(bones, foot, yaw), stiff, tone);
}

void Limbs::read()
{
	m_was = std::move(m_now);
	m_now = m_world.limbs(m_id);
}

Felt Limbs::felt() const
{
	const std::array<v3f, PARTS> c = middles();
	const auto end = [&](int part, float length) {
		const v3f off = fromQuat(m_now[part].rotation).rotateAndScaleVect(v3f(0, -length * S, 0) - c[part]);
		return v3f::from(m_now[part].position) + off;
	};
	Felt f;
	for (int side : { 0, 1 }) {
		f.wrist[side] = end(P_FORE + side, FOREARM);
		f.foot[side] = end(P_SHIN + side, physics::SHIN);
	}
	f.shoulders = v3f::from(m_now[P_CHEST].position) + fromQuat(m_now[P_CHEST].rotation).rotateAndScaleVect(v3f(0, CHEST_TALL / 2 * S, 0));
	return f;
}

std::vector<v3f> Limbs::parts() const
{
	std::vector<v3f> out;
	for (const Pose &p : m_now)
		out.push_back(v3f::from(p.position));
	return out;
}

v3d Limbs::hips() const { return m_now[P_HIPS].position; }

float Limbs::inItself() const
{
	const std::vector<LimbSpec> parts = specs();
	float deepest = 0;
	const v3d at = m_now[P_HIPS].position;
	const auto box = [&](int part, int limb) {
		core::matrix4 place = fromQuat(m_now[part].rotation), un;
		place.setTranslation(v3f::from(m_now[part].position - at));
		place.getInverse(un);
		core::matrix4 turn = fromQuat(m_now[limb].rotation);
		turn.setTranslation(v3f::from(m_now[limb].position - at));
		for (const float along : { -1.f, 0.f, 1.f }) {
			v3f p = turn.transformVect(v3f(0, parts[limb].half.Y * along, 0));
			un.transformVect(p);
			const v3f h = parts[part].half;
			deepest = std::max(deepest, std::min({ h.X - std::fabs(p.X), h.Y - std::fabs(p.Y), h.Z - std::fabs(p.Z) }));
		}
	};
	for (int side : { 0, 1 }) {
		for (const int part : { P_CHEST, P_HIPS, P_HEAD }) {
			if (part != P_CHEST)
				box(part, P_UPPER + side);
			box(part, P_FORE + side);
			if (part != P_HIPS && part != P_CHEST)
				box(part, P_THIGH + side);
			if (part != P_HIPS)
				box(part, P_SHIN + side);
		}
	}
	return deepest;
}

Bones Limbs::drawn(const v3d &foot, float yaw, float t) const
{
	core::matrix4 turn = frame(yaw), unturn;
	turn.getInverse(unturn);
	std::array<core::matrix4, PARTS> m;
	for (size_t i = 0; i < PARTS; i++) {
		const Pose &a = m_was[i], &b = m_now[i];
		m[i] = fromPose({ a.position + (b.position - a.position) * t, between(a.rotation, b.rotation, t) }, foot, unturn);
	}
	const std::array<v3f, PARTS> c = middles();
	// From each part's middle back to its bone, against its parent as the figure has them.
	const auto bone = [&](int part) { return m[part] * moved(c[part] * -1.f); };
	const auto against = [](const core::matrix4 &parent, const core::matrix4 &child) {
		core::matrix4 inv;
		parent.getInverse(inv);
		const core::matrix4 r = inv * child;
		return BonePlace{ r.getTranslation(), r.getRotationDegrees() };
	};
	Bones out = m_asked;
	const core::matrix4 hips = bone(P_HIPS), chest = bone(P_CHEST);
	out.lower = { hips.getTranslation(), hips.getRotationDegrees() };
	out.hips = {};
	out.chest = against(hips, chest);
	out.head = against(chest, bone(P_HEAD));
	for (int side : { 0, 1 }) {
		out.thigh[side] = against(hips, bone(P_THIGH + side));
		out.shin[side] = against(bone(P_THIGH + side), bone(P_SHIN + side));
		out.upper[side] = against(chest, bone(P_UPPER + side));
		out.fore[side] = against(bone(P_UPPER + side), bone(P_FORE + side));
	}
	return out;
}

} // namespace physics
