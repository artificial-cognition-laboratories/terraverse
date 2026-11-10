// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Force-included first in every Windows build of our sources (CMakeLists.txt).
// <windows.h> defines words we use as names to nothing: `near` and `far` (a
// leftover from 16-bit pointers: the far view's namespace, and near and far as
// ordinary words) and `IN`, `OUT`, `OPTIONAL` (annotations). Windows' headers
// are read whole here, then the words are given back; their include guards keep
// them from coming back. `FAR` and `NEAR` spell pointer types in headers read
// later (zlib's), so they stay, as nothing.
#pragma once

#include <winsock2.h>
#include <windows.h>

#undef near
#undef far
#undef NEAR
#define NEAR
#undef FAR
#define FAR
#undef IN
#undef OUT
#undef OPTIONAL
