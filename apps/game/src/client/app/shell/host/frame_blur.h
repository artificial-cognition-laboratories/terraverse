// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <string>

namespace shell {

struct GpuPrograms;

/*
	The engine's frame, blurred, for the page's glass to show through. A
	page's `backdrop-filter` sees only the page: the frame (the planet, the
	world) is under it, out of Chromium's reach, so the blur is ours. Each
	frame the page draws over, what's been drawn is copied, shrunk by four
	and blurred both ways; GpuPrograms::show mixes it in under the page
	wherever the page is glass.

	On the main thread, in the engine's context. A GL that can't copy its
	frame (a multisampled one of another format) turns it off once, saying
	why, and the page's glass is then see-through, unblurred.
*/
class FrameBlur
{
public:
	struct Opts
	{
		const GpuPrograms &programs;
	};
	explicit FrameBlur(Opts o);
	~FrameBlur();
	FrameBlur(const FrameBlur &) = delete;
	FrameBlur &operator=(const FrameBlur &) = delete;

	// What's drawn so far in `view` (GL_VIEWPORT's four), blurred: a
	// texture the viewport's shape, or 0 where the GL can't.
	unsigned take(const int view[4]);
	// While held, take() gives the last frame it blurred again, whatever's drawn since
	void hold(bool on) { held = on; }

private:
	struct Target
	{
		unsigned texture = 0, fbo = 0;
	};
	void resize(u32 w, u32 h);

	const GpuPrograms &programs;
	Target whole, small, across; // the copy, shrunk, blurred one way (then back into small)
	u32 width = 0, height = 0;
	bool held = false;
	std::string off; // why it can't, once it's found it can't
};

} // namespace shell
