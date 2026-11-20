// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/core/irrlichttypes.h"
#include <deque>

namespace shell {

/*
	How long the page takes to answer the hand: from the engine handing
	Chromium a mouse move or click, to Chromium painting a frame after it,
	to that frame reaching the screen. Reported each frame as the debugger's
	"UI:" counters, with how many frames the page painted and how much of it.
	The yardstick for the UI's rework (phases 1-3): a native menu answers
	within the frame.
*/
class Latency
{
public:
	// The engine passed Chromium input that may change what the page shows.
	void input(u64 at);
	// What Chromium painted since the last frame (Pixels::Painted).
	void painted(u64 first, u32 paints, double area);
	// The newest painted frame went to the GPU for this frame's drawing; then
	// the frame's counters are reported.
	void shown(u64 at);
	void report();

private:
	std::deque<u64> m_inputs; // inputs not yet answered by a paint, oldest first
	u64 m_answer = 0; // that input's time, once painted, till it's shown
	u32 m_paints = 0;
	double m_area = 0;
};

} // namespace shell
