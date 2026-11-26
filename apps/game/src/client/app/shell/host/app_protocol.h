// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace shell {

// macOS: Chromium needs the process's NSApplication to implement CefAppProtocol (whether it's
// inside sendEvent:), and SDL made that NSApplication, its own class. This adds the protocol to
// SDL's class at run time. Call after SDL's video is up, before CefInitialize. Elsewhere: nothing.
#ifdef __APPLE__
void adoptAppProtocol(); // app_protocol.mm
#else
inline void adoptAppProtocol() {}
#endif

} // namespace shell
