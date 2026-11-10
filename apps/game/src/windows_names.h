// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Force-included first in every Windows build of our sources (CMakeLists.txt).
// <windows.h> defines words we use as names: `near` and `far` (as nothing, a
// leftover from 16-bit pointers), and `NEAR` and `FAR` after them; we have the
// far view's namespace and constants called NEAR and FAR. Windows' headers are
// read whole here, then the words are given back, and their include guards keep
// them from coming back. zlib defines FAR itself when it's missing. GDI, which
// nothing of ours draws with, is left out: its `Arc` is a word of ours too.
#pragma once

#ifndef NOGDI
#define NOGDI
#endif
#include <winsock2.h>
#include <windows.h>

#undef near
#undef far
#undef NEAR
#undef FAR
