// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#ifdef __linux__

#include <mt_opengl.h>
#include <GL/gl.h>

namespace shell {

// Binds what it binds, and puts back what was bound: Irrlicht caches GL's
// state, and only what it set may be there when it next draws. Held for a
// scope on the main thread, round anything the shell draws there.
struct GlRestore
{
	GLint program, vao, active, texture, texture1, viewport[4];
	GLint src_rgb, dst_rgb, src_alpha, dst_alpha, eq_rgb, eq_alpha;
	GLboolean blend, depth, cull, scissor, stencil;
	GlRestore();
	~GlRestore();
	GlRestore(const GlRestore &) = delete;
	GlRestore &operator=(const GlRestore &) = delete;
};

} // namespace shell

#endif
