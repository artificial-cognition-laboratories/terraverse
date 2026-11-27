// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/util/math/half.h"
#include <cstring>

u16 toHalf(float f)
{
	u32 x;
	std::memcpy(&x, &f, 4);
	const u32 sign = (x >> 16) & 0x8000;
	const s32 exponent = (s32)((x >> 23) & 0xff) - 127 + 15;
	u32 mantissa = x & 0x7fffff;
	if (exponent <= 0) {
		if (exponent < -10)
			return (u16)sign; // too small: zero
		mantissa |= 0x800000;
		const u32 shift = (u32)(14 - exponent);
		const u32 half = mantissa >> shift;
		const u32 round = (mantissa >> (shift - 1)) & 1;
		return (u16)(sign | (half + round));
	}
	if (exponent >= 31)
		return (u16)(sign | 0x7c00);
	const u32 half = sign | ((u32)exponent << 10) | (mantissa >> 13);
	return (u16)(half + ((mantissa >> 12) & 1)); // (a carry into the exponent is the right rounding)
}
