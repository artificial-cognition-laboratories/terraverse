// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include "geology.h"
#include "sphere.h"

// What geology.cpp and history.cpp share about plates: hashes for scattering
// them, vectors on the sphere, and how a plate moves.
namespace terragen::plates {

// Noise streams for the plates (offsets from the world's seed hash; geology.cpp's others).
enum : uint32_t { PLATE = 101, PLATE_WARP = 103 };

inline uint32_t mix(uint32_t a, uint32_t b, uint32_t seed)
{
	uint32_t h = seed ^ (a * 0x27d4eb2du) ^ (b * 0x165667b1u);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

// 0..1 from a hash
inline fix unit(uint32_t h)
{
	return (fix)(h & (ONE - 1));
}

inline fix sq(fix v)
{
	return mul(v, v);
}

// 1 − d/reach, squared, for d < reach: how strongly a boundary is felt
inline fix falloff(int32_t d, int32_t reach)
{
	if (d >= reach)
		return 0;
	return sq(ONE - ratio(d, reach));
}

// A direction from three hashes: a point in a cube, made unit (not quite
// even over the sphere; enough for scattering a few centres).
inline Dir scattered(uint32_t a, uint32_t b, uint32_t c)
{
	return normalise((int64_t)(a & 0xFFFF) - 0x8000, (int64_t)(b & 0xFFFF) - 0x8000, (int64_t)(c & 0xFFFF) - 0x7FFF);
}

inline int64_t dot(const Dir &a, const Dir &b)
{
	return (int64_t)a.x * b.x + (int64_t)a.y * b.y + (int64_t)a.z * b.z;
}

// a × b, fixed
inline Dir cross(const Dir &a, const Dir &b)
{
	return { mul(a.y, b.z) - mul(a.z, b.y), mul(a.z, b.x) - mul(a.x, b.z), mul(a.x, b.y) - mul(a.y, b.x) };
}

// How a plate moves at a place: its spin about its pole, ω × p.
inline Dir velocity(const Plate &p, const Dir &at)
{
	Dir v = cross(p.pole, at);
	return { mul(v.x, p.spin), mul(v.y, p.spin), mul(v.z, p.spin) };
}

} // namespace terragen::plates
