// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include "src/shared/util/math/numeric.h"
#include <matrix4.h>

namespace piece {

// An object's grid in the world (BS): at `position`, turned by `rotation`
// (degrees, as the server sets it: yaw θ faces local +Z to (-sin θ, 0,
// cos θ)). As GenericCAO::updateNodePos draws an entity: what the tools
// aim with must be what's drawn.
inline core::matrix4 frame(v3f position, v3f rotation)
{
	core::matrix4 m;
	setPitchYawRoll(m, -rotation);
	m.setTranslation(position);
	return m;
}

// The 24 square turns (a cube's ways to sit), each as its three axes'
// images: TURNS[t][a] is where axis a goes, one of ±x, ±y, ±z. TURNS[0]
// is none. A part sits in its machine turned by one.
struct Turn
{
	v3s16 axes[3];
	// v (local) turned
	v3s32 apply(v3s32 v) const
	{
		return v3s32(axes[0].X, axes[0].Y, axes[0].Z) * v.X + v3s32(axes[1].X, axes[1].Y, axes[1].Z) * v.Y
				+ v3s32(axes[2].X, axes[2].Y, axes[2].Z) * v.Z;
	}
	v3f apply(v3f v) const
	{
		return v3f::from(axes[0]) * v.X + v3f::from(axes[1]) * v.Y + v3f::from(axes[2]) * v.Z;
	}
	core::matrix4 matrix() const
	{
		core::matrix4 m;
		for (int c = 0; c < 3; c++) {
			m[c * 4] = axes[c].X;
			m[c * 4 + 1] = axes[c].Y;
			m[c * 4 + 2] = axes[c].Z;
		}
		return m;
	}
};
constexpr int TURN_COUNT = 24;
const Turn &turn(u8 t); // throws on t >= TURN_COUNT
// Which turn this is (the one taking each axis to these); throws if it's none.
u8 turnOf(const v3s16 axes[3]);
// a then b: b's turn of what a turned
u8 compose(u8 a, u8 b);
// The turn taking `from` (a unit axis) to `to`, nearest `near` (the one
// of those most like it).
u8 turnTaking(v3s16 from, v3s16 to, u8 near);
// A quarter turn about `axis` (a unit axis), right-handed.
u8 quarterAbout(v3s16 axis);

} // namespace piece
