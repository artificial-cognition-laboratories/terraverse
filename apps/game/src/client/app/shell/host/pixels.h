// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/app/shell/host/canvas.h"
#include "src/shared/core/irrlichttypes.h"
#include <include/internal/cef_types_wrappers.h>
#include <mutex>
#include <vector>

namespace shell {

// The page's last frame on the CPU, straight (not premultiplied) BGRA.
// Chromium's thread writes it (OnPaint) and the main thread reads it, each
// holding `lock`; the main thread only ever try_locks for drawing, so a
// paint in progress costs it a frame-old page, never a wait.
struct Pixels
{
	mutable std::mutex lock;
	u32 width = 0;
	u32 height = 0;
	std::vector<u8> bgra;
	bool dirty = false;

	Canvas::Painted painted; // since the canvas last took it

	// Chromium paints premultiplied BGRA; Irrlicht's 2D drawing blends straight
	// alpha. Only the rectangles Chromium reports as changed are converted.
	void write(const void *premultiplied, u32 w, u32 h, const std::vector<CefRect> &changed); // takes the lock
	// Waits out a paint in progress (a few ms at worst) to hit-test the
	// mouse: the CPU's canvas only, where nothing faster is to hand.
	u8 alphaAt(s32 x, s32 y) const; // takes the lock
};

} // namespace shell
