// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include <include/internal/cef_types_wrappers.h>
#include <position2d.h>
#include <memory>
#include <vector>

namespace video { class IVideoDriver; }

namespace shell {

/*
	Where a page's frames go between Chromium painting them and the engine
	drawing them over its frame. Two real kinds: the GPU's (gpu_canvas.h:
	Chromium's own textures, copied GPU to GPU) and the CPU's (cpu_canvas.h:
	its pixels, uploaded each change), for software rendering and wherever
	the GPU's can't start. Chromium paints on its thread; everything else is
	the main thread's.
*/
class Canvas
{
public:
	// What's been painted since it was last taken: how often, the first
	// one's time (trace::now) and how much of the page all of them changed
	// (1: all of it, once). For the UI's latency counters (latency.h).
	struct Painted
	{
		u32 paints = 0;
		u64 first = 0;
		double area = 0;
	};

	virtual ~Canvas() = default;

	// Whether Chromium hands it shared textures (CefWindowInfo::shared_texture_enabled)
	// rather than pixels: which of the paints below it takes.
	virtual bool shared() const = 0;
	// Chromium's thread: a frame, as pixels (premultiplied BGRA) or a shared texture.
	virtual void paint(const void *premultiplied, u32 width, u32 height, const std::vector<CefRect> &dirty) = 0;
	virtual void paint(const CefAcceleratedPaintInfo &info, const std::vector<CefRect> &dirty) = 0;

	// Whether the last frame painted is taken in, so Chromium may paint the
	// next (Shell::beginFrame asks it only then). Never waits.
	virtual bool settled() = 0;
	// The newest frame painted, its top-left corner here on the screen.
	// Whether it's one not drawn before. Never waits on Chromium.
	virtual bool draw(core::position2di at) = 0;
	// How solid the page is at a point (0: the game shows through), for
	// whether the mouse is the page's.
	virtual u8 alphaAt(s32 x, s32 y) const = 0;
	virtual Painted taken() = 0;
};

// The GPU's canvas if `gpu` and it starts here, else the CPU's, saying why.
// On the main thread, with the driver's GL context current.
std::unique_ptr<Canvas> makeCanvas(video::IVideoDriver *driver, bool gpu, const char *page);

} // namespace shell
