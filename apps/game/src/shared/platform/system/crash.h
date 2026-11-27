// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

/*
	A crash says what it was. On a fatal signal (a segfault, an abort, Chromium's
	deliberate crash on a failed check), what's written before the process goes:
	the signal, the thread it hit (by name: "Chrome_IOThread" says whose it was),
	and a backtrace, to stderr (the launcher's session log, `bun dev`'s terminal)
	and to crash-<time>.txt in the logs, beside debug.txt. Then the signal is
	raised again as it came, so a core is still dumped.

	Watched again after Chromium has started: it installs handlers of its own.
	Windows: not wired yet (a crash there says nothing more than it did).
*/
namespace crash {

// Watch for fatal signals, writing reports into `logs`. Again to take them back.
void watch(const std::string &logs);
// Watch again, into the same logs: after something else (Chromium) installed its own.
void again();

} // namespace crash
