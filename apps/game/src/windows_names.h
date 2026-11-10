// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Force-included first in every Windows build of our sources (CMakeLists.txt).
// <minwindef.h> defines `near` and `far` as nothing, a leftover from 16-bit
// pointers, and we use both as names: the far view's namespace, and near and
// far as ordinary words. Its include guard keeps them undefined after this.
#pragma once

#include <minwindef.h>
#undef near
#undef far
