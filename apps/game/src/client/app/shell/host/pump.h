// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <atomic>
#include <cstdint>

namespace shell {

/*
	Chromium's message loop, where the engine must run it. On Windows and Linux Chromium has a
	thread of its own (multi_threaded_message_loop) and this does nothing. macOS has no such
	mode: Chromium's UI thread is the main thread, and its work is done from the engine's frames
	(external_message_pump) when Chromium asks for it (App's OnScheduleMessagePumpWork).
*/
class Pump
{
public:
	// Whether this platform's loop is pumped from here.
#ifdef __APPLE__
	static constexpr bool EXTERNAL = true;
#else
	static constexpr bool EXTERNAL = false;
#endif

	Pump();

	// From any thread: Chromium wants its work done in `delay_ms` (0: now).
	void schedule(int64_t delay_ms);
	// On the main thread: Chromium's work, if it's due. A frame calls it once; a wait on
	// Chromium (a browser closing) calls it in its loop.
	void work();

private:
	// When the work is due, in steady-clock milliseconds; INT64_MAX: not asked for.
	std::atomic<int64_t> m_due;
	bool m_working = false; // CefDoMessageLoopWork isn't reentrant
};

} // namespace shell
