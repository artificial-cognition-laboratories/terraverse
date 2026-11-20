// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <string>

namespace shell {

/*
	The GPU canvas's three passes (gpu_canvas.h), compiled once on the main
	thread; GL shares programs between contexts, so Chromium's thread uses
	them too. Each draws one quad from gl_VertexID (a vertex array bound,
	nothing in it) into whatever's bound to draw into.
*/
struct GpuPrograms
{
	unsigned show_program = 0, copy_2d = 0, copy_external = 0, mask_program = 0;
	std::string failed; // why the passes, or the external copy alone, didn't compile

	// `external`: the GL takes GL_OES_EGL_image_external, so try the copy that samples it
	static GpuPrograms compile(bool es, bool external);
	void release();
	bool ready() const { return show_program && copy_2d && mask_program; }
	bool hasExternal() const { return copy_external != 0; }

	// Chromium's frame (its texture as imported, `target`) into the bound
	// target: its visible part, from (x, y) in its coded size.
	void copy(bool external, unsigned target, unsigned source, int x, int y, int coded_w, int coded_h) const;
	// A page's mask into the bound target: each texel how solid the page's
	// `cell`² pixels under it are at most.
	void mask(unsigned page, u32 width, u32 height, u32 cell) const;
	// The page over the frame, premultiplied, onto a rectangle in clip space (top-left, bottom-right).
	void show(unsigned page, float x0, float y0, float x1, float y1) const;
};

} // namespace shell
