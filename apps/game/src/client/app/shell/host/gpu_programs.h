// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <string>

namespace shell {

/*
	The GPU canvas's passes (gpu_canvas.h), compiled once on the main
	thread; GL shares programs between contexts, so Chromium's thread uses
	them too. Each draws one quad from gl_VertexID (a vertex array bound,
	nothing in it) into whatever's bound to draw into.
*/
struct GpuPrograms
{
	unsigned show_program = 0, copy_2d = 0, copy_external = 0, mask_program = 0;
	unsigned shrink_program = 0, smear_program = 0; // the frame's blur (frame_blur.h)
	std::string failed; // why the passes, or the external copy alone, didn't compile

	// `external`: the GL takes GL_OES_EGL_image_external, so try the copy that samples it
	static GpuPrograms compile(bool es, bool external);
	void release();
	bool ready() const { return show_program && copy_2d && mask_program && shrink_program && smear_program; }
	bool hasExternal() const { return copy_external != 0; }

	// Chromium's frame (its texture as imported, `target`) into the bound
	// target: its visible part, from (x, y) in its coded size.
	void copy(bool external, unsigned target, unsigned source, int x, int y, int coded_w, int coded_h) const;
	// A page's mask into the bound target: each texel how solid the page's
	// `cell`² pixels under it are at most.
	void mask(unsigned page, u32 width, u32 height, u32 cell) const;
	struct Show
	{
		unsigned page;
		unsigned frame; // the frame blurred, the viewport's size; 0: no glass
		int view[4];    // the viewport, as GL_VIEWPORT gives it
		float x0, y0, x1, y1; // the page's rectangle in clip space (top-left, bottom-right)
	};
	// The page over the frame, premultiplied, the blurred frame through its glass.
	void show(const Show &s) const;
	// `source` shrunk by four each way into the bound target (`w` by `h`).
	void shrink(unsigned source, u32 source_w, u32 source_h, u32 w, u32 h) const;
	// `source` blurred along (dx, dy) by `sigma` texels into the bound target, the same size.
	void smear(unsigned source, u32 w, u32 h, float dx, float dy, float sigma) const;
};

} // namespace shell
