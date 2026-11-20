// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Force-included first in every Windows build of our sources (CMakeLists.txt).
// <windows.h> defines words we use as names: `near` and `far` (as nothing, a
// leftover from 16-bit pointers), and `NEAR` and `FAR` after them; we have the
// far view's namespace and constants called NEAR and FAR; and `small`, as
// char (rpcndr.h). Every Windows header
// the engine includes is read here, then the words are given back, and their
// include guards keep them from coming back. A new Windows header in the engine
// goes on this list. zlib defines FAR itself when it's missing. GDI, which
// nothing of ours draws with, is left out: its `Arc` is a word of ours too.
#pragma once

#ifndef NOGDI
#define NOGDI
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <dbghelp.h>

#undef near
#undef far
#undef NEAR
#undef FAR
#undef small
