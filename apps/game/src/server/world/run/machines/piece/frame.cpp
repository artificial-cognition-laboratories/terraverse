// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/machines/piece/frame.h"

#include "src/shared/core/exceptions.h"
#include <array>

namespace piece {

namespace {

v3s16 cross(v3s16 a, v3s16 b)
{
	return v3s16(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
}

std::array<Turn, TURN_COUNT> make()
{
	// every signed axis for x, every one at right angles for y, z across
	// them (so each is a turn, not a mirror); none first
	const v3s16 units[6] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { -1, 0, 0 }, { 0, -1, 0 }, { 0, 0, -1 } };
	std::array<Turn, TURN_COUNT> out;
	size_t n = 0;
	for (const v3s16 &x : units)
		for (const v3s16 &y : units) {
			if (x.X * y.X + x.Y * y.Y + x.Z * y.Z != 0)
				continue;
			out[n++] = Turn{ { x, y, cross(x, y) } };
		}
	if (n != TURN_COUNT)
		throw BaseException("piece::turns: not 24");
	return out;
}

const std::array<Turn, TURN_COUNT> &all()
{
	static const std::array<Turn, TURN_COUNT> turns = make();
	return turns;
}

} // namespace

const Turn &turn(u8 t)
{
	if (t >= TURN_COUNT)
		throw SerializationError("piece: no turn " + std::to_string(t));
	return all()[t];
}

u8 turnOf(const v3s16 axes[3])
{
	for (u8 t = 0; t < TURN_COUNT; t++) {
		const Turn &q = all()[t];
		if (q.axes[0] == axes[0] && q.axes[1] == axes[1] && q.axes[2] == axes[2])
			return t;
	}
	throw BaseException("piece::turnOf: not a square turn");
}

u8 compose(u8 a, u8 b)
{
	const Turn &ta = turn(a), &tb = turn(b);
	v3s16 axes[3];
	for (int i = 0; i < 3; i++) {
		const v3s32 v = tb.apply(v3s32(ta.axes[i].X, ta.axes[i].Y, ta.axes[i].Z));
		axes[i] = v3s16(v.X, v.Y, v.Z);
	}
	return turnOf(axes);
}

u8 turnTaking(v3s16 from, v3s16 to, u8 near)
{
	const Turn &n = turn(near);
	int best = -1, best_fit = 0;
	for (u8 t = 0; t < TURN_COUNT; t++) {
		const Turn &q = all()[t];
		const v3s32 v = q.apply(v3s32(from.X, from.Y, from.Z));
		if (v != v3s32(to.X, to.Y, to.Z))
			continue;
		int fit = 0;
		for (int i = 0; i < 3; i++)
			fit += q.axes[i].X * n.axes[i].X + q.axes[i].Y * n.axes[i].Y + q.axes[i].Z * n.axes[i].Z;
		if (best < 0 || fit > best_fit) {
			best = t;
			best_fit = fit;
		}
	}
	if (best < 0)
		throw BaseException("piece::turnTaking: not unit axes");
	return u8(best);
}

u8 quarterAbout(v3s16 n)
{
	// each axis e to n (n · e) + n × e
	const v3s16 units[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	v3s16 axes[3];
	for (int i = 0; i < 3; i++) {
		const v3s16 e = units[i];
		const s16 along = n.X * e.X + n.Y * e.Y + n.Z * e.Z;
		axes[i] = v3s16(n.X * along, n.Y * along, n.Z * along) + cross(n, e);
	}
	return turnOf(axes);
}

} // namespace piece
