// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>

/*
	Fixed-point arithmetic for terragen. Everything that decides a block uses
	these, never floating point or libm, so a world code builds the same world
	on every machine and compiler. Q16.16 in a 32-bit int: 65536 is 1.0.
	(Right shifts of negative values are arithmetic on every compiler we
	target; C++20 guarantees it.)
*/
namespace terragen {

using fix = int32_t;

constexpr int FRAC = 16;
constexpr fix ONE = 1 << FRAC;
constexpr fix HALF = ONE / 2;
constexpr fix PI = 205887;      // π
constexpr fix HALF_PI = 102944; // π/2
constexpr fix TWO_PI = 411775;  // 2π

constexpr fix toFix(int32_t n) { return n * ONE; }

// a / b as a fixed value, rounded toward zero. Exact for the ratios we use.
constexpr fix ratio(int64_t a, int64_t b) { return (fix)((a * ONE) / b); }

inline fix mul(fix a, fix b) { return (fix)(((int64_t)a * b) >> FRAC); }
inline fix div(fix a, fix b) { return (fix)(((int64_t)a << FRAC) / b); }
inline fix lerp(fix a, fix b, fix t) { return a + mul(b - a, t); }
inline fix clamp(fix v, fix lo, fix hi) { return v < lo ? lo : v > hi ? hi : v; }
inline fix fabsx(fix v) { return v < 0 ? -v : v; }
inline fix maxx(fix a, fix b) { return a > b ? a : b; }

// 3t² − 2t³ on [0, 1]: smooth interpolation between lattice points.
inline fix smooth(fix t) { return mul(mul(t, t), toFix(3) - 2 * t); }

// The integer part (floor) and the fraction of a fixed value.
inline int32_t floorInt(fix v) { return v >> FRAC; }
inline fix fraction(fix v) { return v & (ONE - 1); }

// Rounded to the nearest integer, halves up (as JavaScript's Math.round).
inline int32_t roundInt(fix v) { return (v + HALF) >> FRAC; }

// ⌊a·b / 2^shift⌋, exactly, for products past 64 bits (0 < shift < 64; the
// result must fit 64 bits). Inline, in 128 bits: the land reads it tens of
// millions of times a second, and called it was a twelfth of a far column's
// time (sprint 114)
inline int64_t mulShift(int64_t a, int64_t b, int shift)
{
	return (int64_t)(((__int128)a * b) >> shift);
}

fix sqrt(fix v);                     // v ≥ 0
uint64_t isqrt(uint64_t n);          // ⌊√n⌋ of a plain integer
fix sqrtWide64(int64_t v);           // √v for a fixed value held in 64 bits, v ≥ 0
void sincos(fix angle, fix &s, fix &c); // any angle, radians
fix sin(fix angle);
fix cos(fix angle);
fix atan2(fix y, fix x);             // radians in (−π, π]
fix acos(fix v);                     // v in [−1, 1], radians in [0, π]
fix log2(fix v);                     // v > 0
fix exp2(fix v);                     // saturates at the largest fix; 0 far below
fix pow(fix base, fix exponent);     // base > 0
fix exp(fix v);                      // e^v

} // namespace terragen
