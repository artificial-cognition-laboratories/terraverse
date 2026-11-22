// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace video { class IVideoDriver; }

// What the GPU spends on a part of the frame, and how many draws the part
// issues, as debugger counters ("GPU: <name> [us]", "Draws: <name> [#]",
// "Triangles: <name> [#]"), beside the CPU's zones. A part is timed by GL
// timestamps read a few frames later, so measuring never waits on the GPU:
// the counters lag the frame they report by that much.
//
// One clock for the main GL context, as trace's zones are one record: a part
// anywhere in the render (a pipeline step, a scene node) is timed without
// being handed it. Without timestamp queries (GLES) it counts draws only.
namespace gpu {

class Clock
{
public:
	struct Opts { video::IVideoDriver &driver; };
	explicit Clock(Opts o);
	~Clock();

	// The frame's drawing starts: just after the scene's begun.
	void begin();
	// The frame's GPU work is all issued: just before the swap. Its time
	// is from begin() to here, the GPU's own work, not waiting between frames.
	void frame();

	struct Impl;

private:
	friend void install(Clock *clock);
	std::unique_ptr<Impl> m;
};

// The clock the render's parts report to while it lives: the game's, set as
// it starts drawing and cleared as it stops.
void install(Clock *clock);

// A part of the frame, from construction to destruction, on the main thread.
// No clock installed: nothing.
class Zone
{
public:
	explicit Zone(std::string_view name) noexcept;
	~Zone() noexcept;
	Zone(const Zone &) = delete;
	Zone &operator=(const Zone &) = delete;

private:
	Clock::Impl *m_clock;
	int m_slot;
};

} // namespace gpu
