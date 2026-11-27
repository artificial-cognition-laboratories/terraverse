// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"

// A float as an IEEE 754 half (a vertex's packed values: the GPU reads them
// back as floats): rounded to nearest, out of range to infinity.
u16 toHalf(float f);
