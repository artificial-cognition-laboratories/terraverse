// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/player/hold.h"

#include "src/shared/base/constants.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/util/math/numeric.h"
#include <algorithm>
#include <cmath>

namespace {

core::matrix4 about(v3f axis, f32 radians)
{
	core::matrix4 m;
	m.setRotationAxisRadians(radians, axis);
	return m;
}

} // namespace

Hold::Hold(u16 object, v3f middle, f32 radius, const core::matrix4 &turned, v3f anchor) :
	m_object(object), m_middle(middle), m_radius(radius), m_turned(turned), m_anchor(anchor),
	m_eye(anchor - v3f(0, 0, 3)), m_look(0, 0, 1)
{
	m_turned.setTranslation(v3f(0));
}

void Hold::watch(v3f eye, v3f look)
{
	m_eye = eye;
	v3f level(look.X, 0, look.Z);
	if (level.getLengthSQ() > 1e-6f) // looking straight up or down: as it was
		m_look = level.normalize();
}

v3f Hold::axis(Spin spin) const
{
	switch (spin) {
	case Spin::Yaw: return v3f(0, 1, 0);
	case Spin::Pitch: return right();
	case Spin::Roll: return (m_eye - held()).normalize();
	}
	throw BaseException("Hold::axis: no such spin");
}

void Hold::begin(Spin spin)
{
	m_from = From{ spin, m_turned };
}

void Hold::spin(f32 angle, f32 step)
{
	if (!m_from)
		throw BaseException("Hold::spin: no turn begun");
	const f32 by = step > 0 ? std::round(angle / step) * step : angle;
	m_turned = about(axis(m_from->spin), by) * m_from->turned;
}

void Hold::turn(Spin spin, f32 angle)
{
	m_turned = about(axis(spin), angle) * m_turned;
}

Hold::Ring Hold::disc() const
{
	return { held() - v3f(0, m_radius + 0.5f, 0), v3f(0, 1, 0), m_radius + 0.5f };
}

Hold::Ring Hold::ring() const
{
	return { held(), axis(Spin::Roll), m_radius + 0.5f };
}

std::optional<f32> Hold::angle(Spin spin, const core::line3d<f32> &ray) const
{
	const Ring r = spin == Spin::Roll ? ring() : disc();
	const v3f s = ray.start / BS, d = (ray.end - ray.start) / BS;
	const f32 along = d.dotProduct(r.axis);
	if (std::abs(along) < 1e-5f)
		return std::nullopt;
	const f32 t = (r.centre - s).dotProduct(r.axis) / along;
	if (t < 0)
		return std::nullopt;
	const v3f p = s + d * t - r.centre;
	// the ring's own two ways across, then the angle between them
	const v3f u = std::abs(r.axis.Y) > 0.9f ? v3f(1, 0, 0) : v3f(0, 1, 0).crossProduct(r.axis).normalize();
	const v3f w = r.axis.crossProduct(u);
	return std::atan2(p.dotProduct(w), p.dotProduct(u));
}

std::optional<Hold::Grip> Hold::grip(const core::line3d<f32> &ray) const
{
	// the ring's rim first (it's in front), then anywhere on the disc
	constexpr f32 RIM = 0.35f; // nodes either side of the ring
	for (const Spin spin : { Spin::Roll, Spin::Yaw }) {
		const Ring r = spin == Spin::Roll ? ring() : disc();
		const v3f s = ray.start / BS, d = (ray.end - ray.start) / BS;
		const f32 along = d.dotProduct(r.axis);
		if (std::abs(along) < 1e-5f)
			continue;
		const f32 t = (r.centre - s).dotProduct(r.axis) / along;
		if (t < 0 || t > 1)
			continue;
		const f32 from = (s + d * t - r.centre).getLength();
		const bool on = spin == Spin::Roll ? std::abs(from - r.radius) < RIM : from < r.radius + RIM;
		if (on)
			if (const std::optional<f32> a = angle(spin, ray))
				return Grip{ spin, *a };
	}
	return std::nullopt;
}

void Hold::reshape(v3f middle, f32 radius)
{
	m_middle = middle;
	m_radius = radius;
}

void Hold::level()
{
	// the axis of it most up kept up; of the others, the more level keeps
	// its heading, flattened
	const v3f world[3] = { v3f(m_turned[0], m_turned[1], m_turned[2]), v3f(m_turned[4], m_turned[5], m_turned[6]),
		v3f(m_turned[8], m_turned[9], m_turned[10]) };
	int up = 0;
	for (int i = 1; i < 3; i++)
		if (std::abs(world[i].Y) > std::abs(world[up].Y))
			up = i;
	const int a = (up + 1) % 3, b = (up + 2) % 3;
	const int keep = std::abs(world[a].Y) <= std::abs(world[b].Y) ? a : b;
	v3f h(world[keep].X, 0, world[keep].Z);
	if (h.getLengthSQ() < 1e-6f)
		h = m_look;
	h.normalize();
	const v3f u(0, world[up].Y > 0 ? 1.f : -1.f, 0);
	// the third across them both, its sign so it's a turn, not a mirror
	const int third = 3 - up - keep;
	v3f cols[3];
	cols[up] = u;
	cols[keep] = h;
	cols[third] = u.crossProduct(h);
	const f32 det = cols[0].dotProduct(cols[1].crossProduct(cols[2]));
	if (det < 0)
		cols[third] = -cols[third];
	for (int c = 0; c < 3; c++) {
		m_turned[c * 4] = cols[c].X;
		m_turned[c * 4 + 1] = cols[c].Y;
		m_turned[c * 4 + 2] = cols[c].Z;
	}
}

void Hold::reset()
{
	// square to the player: its +z away from them, its +y up
	const v3f r = right();
	m_turned.makeIdentity();
	m_turned[0] = r.X; m_turned[1] = r.Y; m_turned[2] = r.Z;
	m_turned[8] = m_look.X; m_turned[9] = m_look.Y; m_turned[10] = m_look.Z;
}


Hold::Pose Hold::pose() const
{
	// the entity's origin: where its middle, turned, comes to `held`
	v3f offset = m_middle;
	m_turned.transformVect(offset); // no translation: a rotation
	return { (held() - offset) * BS, -getPitchYawRoll(m_turned) };
}

v3f Hold::right() const
{
	// left-handed (Irrlicht's): up × look points right
	return v3f(0, 1, 0).crossProduct(m_look).normalize();
}




int Hold::across() const
{
	// the screen's right, in its grid: the inverse of a rotation is its transpose
	core::matrix4 back;
	m_turned.getTransposed(back);
	v3f r = right();
	back.transformVect(r);
	const v3f a(std::abs(r.X), std::abs(r.Y), std::abs(r.Z));
	return a.X >= a.Y && a.X >= a.Z ? 0 : a.Y >= a.Z ? 1 : 2;
}
