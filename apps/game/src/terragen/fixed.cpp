// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "fixed.h"
#include <stdexcept>

namespace terragen {

// atan(2^-i) in Q16, and the CORDIC gain 1/K = ∏ 1/sqrt(1 + 2^-2i).
static const fix ATAN[16] = {
	51472, 30386, 16055, 8150, 4091, 2047, 1024, 512,
	256, 128, 64, 32, 16, 8, 4, 2,
};
static const fix CORDIC_GAIN = 39797; // 0.60725

uint64_t isqrt(uint64_t n)
{
	uint64_t r = 0, bit = (uint64_t)1 << 62;
	while (bit > n)
		bit >>= 2;
	while (bit) {
		if (n >= r + bit) {
			n -= r + bit;
			r = (r >> 1) + bit;
		} else {
			r >>= 1;
		}
		bit >>= 2;
	}
	return r;
}

fix sqrtWide64(int64_t v)
{
	if (v <= 0)
		return 0;
	// √(v·2^16) in 64 bits: the result is back in Q16
	return (fix)isqrt((uint64_t)v << FRAC);
}

fix sqrt(fix v)
{
	if (v <= 0)
		return 0;
	return (fix)isqrt((uint64_t)v << FRAC);
}

void sincos(fix angle, fix &s, fix &c)
{
	// reduce to (−π, π], then to [−π/2, π/2] remembering a cosine flip
	angle %= TWO_PI;
	if (angle > PI)
		angle -= TWO_PI;
	if (angle <= -PI)
		angle += TWO_PI;
	bool flip = false;
	if (angle > HALF_PI) {
		angle = PI - angle;
		flip = true;
	} else if (angle < -HALF_PI) {
		angle = -PI - angle;
		flip = true;
	}

	int64_t x = CORDIC_GAIN, y = 0, z = angle;
	for (int i = 0; i < 16; i++) {
		int64_t dx = x >> i, dy = y >> i;
		if (z >= 0) {
			x -= dy;
			y += dx;
			z -= ATAN[i];
		} else {
			x += dy;
			y -= dx;
			z += ATAN[i];
		}
	}
	s = (fix)y;
	c = flip ? (fix)-x : (fix)x;
}

fix sin(fix angle)
{
	fix s, c;
	sincos(angle, s, c);
	return s;
}

fix cos(fix angle)
{
	fix s, c;
	sincos(angle, s, c);
	return c;
}

fix atan2(fix y, fix x)
{
	if (y == 0)
		return x >= 0 ? 0 : PI; // exact on the axis, where CORDIC error could tip π to −π
	// rotate into the right half-plane first
	fix base = 0;
	int64_t vx = x, vy = y;
	if (vx < 0) {
		base = vy >= 0 ? PI : -PI;
		vx = -vx;
		vy = -vy;
	}
	int64_t z = 0;
	for (int i = 0; i < 16; i++) {
		int64_t dx = vx >> i, dy = vy >> i;
		if (vy > 0) {
			vx += dy;
			vy -= dx;
			z += ATAN[i];
		} else {
			vx -= dy;
			vy += dx;
			z -= ATAN[i];
		}
	}
	fix angle = base + (fix)z;
	return angle > PI ? angle - TWO_PI : angle;
}

fix acos(fix v)
{
	v = clamp(v, -ONE, ONE);
	return atan2(sqrt(ONE - mul(v, v)), v);
}

// log₂ by normalising to [1, 2) and squaring out one bit at a time: exact
// integer steps, the same everywhere.
fix log2(fix v)
{
	if (v <= 0)
		throw std::domain_error("terragen::log2 of a value that isn't positive");
	int32_t whole = 0;
	uint64_t m = (uint32_t)v; // Q16
	while (m >= (uint64_t)2 << FRAC) {
		m >>= 1;
		whole++;
	}
	while (m < (uint64_t)1 << FRAC) {
		m <<= 1;
		whole--;
	}
	// m in [1, 2) as Q30 for the squaring
	uint64_t x = m << 14;
	fix frac = 0;
	for (int bit = FRAC - 1; bit >= 0; bit--) {
		x = (x * x) >> 30;
		if (x >= (uint64_t)2 << 30) {
			x >>= 1;
			frac |= (fix)1 << bit;
		}
	}
	return toFix(whole) + frac;
}

// 2^(2^-i) in Q30, i = 1..16: a fraction's bits multiply these together.
static const uint64_t ROOTS[16] = {
	1518500250, 1276901417, 1170923762, 1121280436, 1097253708, 1085434106,
	1079572136, 1076653033, 1075196443, 1074468888, 1074105294, 1073923544,
	1073832680, 1073787251, 1073764537, 1073753181,
};

fix exp2(fix v)
{
	int32_t whole = floorInt(v);
	fix frac = fraction(v);
	if (whole >= 15)
		return INT32_MAX;
	if (whole < -FRAC - 1)
		return 0;
	uint64_t x = (uint64_t)1 << 30;
	for (int i = 0; i < 16; i++) {
		if (frac & ((fix)1 << (FRAC - 1 - i)))
			x = (x * ROOTS[i]) >> 30;
	}
	// x is 2^frac in Q30; shift to Q16 and by the whole part
	int shift = 30 - FRAC - whole;
	return (fix)(shift >= 0 ? x >> shift : x << -shift);
}

fix pow(fix base, fix exponent)
{
	return exp2(mul(exponent, log2(base)));
}

fix exp(fix v)
{
	return exp2(mul(v, 94548)); // log₂ e
}

int64_t mulShift(int64_t a, int64_t b, int shift)
{
	// |a|·|b| as 128 bits in two halves, from 32-bit limbs
	const bool negative = (a < 0) != (b < 0);
	const uint64_t ua = a < 0 ? 0 - (uint64_t)a : (uint64_t)a, ub = b < 0 ? 0 - (uint64_t)b : (uint64_t)b;
	const uint64_t a0 = ua & 0xffffffffu, a1 = ua >> 32, b0 = ub & 0xffffffffu, b1 = ub >> 32;
	const uint64_t low = a0 * b0, mid1 = a1 * b0, mid2 = a0 * b1, high = a1 * b1;
	const uint64_t mid = (low >> 32) + (mid1 & 0xffffffffu) + (mid2 & 0xffffffffu);
	const uint64_t lo = (low & 0xffffffffu) | (mid << 32);
	const uint64_t hi = high + (mid1 >> 32) + (mid2 >> 32) + (mid >> 32);
	uint64_t q = (lo >> shift) | (hi << (64 - shift));
	if (negative) {
		// floor of a negative: one further down if anything was shifted out
		const bool rest = (lo & ((1ull << shift) - 1)) != 0;
		return -(int64_t)q - (rest ? 1 : 0);
	}
	return (int64_t)q;
}

} // namespace terragen
