// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/frame_blur.h"

#ifdef __linux__

#include "src/client/app/shell/host/gpu_programs.h"
#include "src/shared/platform/system/log.h"
#include <mt_opengl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <algorithm>

namespace shell {

namespace {
// Softer than the site's `blur(8px)` (2 texels, four times shrunk): over the planet a
// lighter frost reads better, and the shrink's own box adds a little.
constexpr float SIGMA = 1.2f;
constexpr u32 SHRINK = 4;

void make(unsigned &texture, unsigned &fbo, u32 w, u32 h);
} // namespace

FrameBlur::FrameBlur(Opts o) : programs(o.programs) {}

FrameBlur::~FrameBlur()
{
	for (Target *t : { &whole, &small, &across }) {
		if (t->texture)
			GL.DeleteTextures(1, &t->texture);
		if (t->fbo)
			GL.DeleteFramebuffers(1, &t->fbo);
	}
}

unsigned FrameBlur::take(const int view[4])
{
	if (!off.empty() || view[2] <= 0 || view[3] <= 0)
		return 0;
	if (held && width > 0)
		return small.texture; // (sampled by where it is on the screen: a new size stretches it)
	GLint drawn = 0, read = 0;
	GL.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawn);
	GL.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
	const u32 w = (u32)view[2], h = (u32)view[3];
	if (w != width || h != height)
		resize(w, h);
	const u32 sw = std::max<u32>(1, w / SHRINK), sh = std::max<u32>(1, h / SHRINK);
	while (GL.GetError() != GL_NO_ERROR) {} // (only the copy's own error below)
	// the same size: a multisampled frame resolves only one to one
	GL.BindFramebuffer(GL_READ_FRAMEBUFFER, drawn);
	GL.BindFramebuffer(GL_DRAW_FRAMEBUFFER, whole.fbo);
	GL.BlitFramebuffer(view[0], view[1], view[0] + view[2], view[1] + view[3], 0, 0, w, h,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
	if (const GLenum e = GL.GetError(); e != GL_NO_ERROR) {
		off = "the frame won't copy (GL error " + std::to_string(e) + ")";
		errorstream << "shell: the page's glass won't blur: " << off << std::endl;
	} else {
		GL.Viewport(0, 0, sw, sh);
		GL.BindFramebuffer(GL_FRAMEBUFFER, small.fbo);
		programs.shrink(whole.texture, w, h, sw, sh);
		GL.BindFramebuffer(GL_FRAMEBUFFER, across.fbo);
		programs.smear(small.texture, sw, sh, 1, 0, SIGMA);
		GL.BindFramebuffer(GL_FRAMEBUFFER, small.fbo);
		programs.smear(across.texture, sw, sh, 0, 1, SIGMA);
		GL.Viewport(view[0], view[1], view[2], view[3]);
	}
	GL.BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawn);
	GL.BindFramebuffer(GL_READ_FRAMEBUFFER, read);
	return off.empty() ? small.texture : 0;
}

void FrameBlur::resize(u32 w, u32 h)
{
	make(whole.texture, whole.fbo, w, h);
	make(small.texture, small.fbo, std::max<u32>(1, w / SHRINK), std::max<u32>(1, h / SHRINK));
	make(across.texture, across.fbo, std::max<u32>(1, w / SHRINK), std::max<u32>(1, h / SHRINK));
	width = w;
	height = h;
}

namespace {
// A texture `w` by `h` and a framebuffer drawing into it, made the first time.
void make(unsigned &texture, unsigned &fbo, u32 w, u32 h)
{
	if (!texture) {
		GL.GenTextures(1, &texture);
		GL.GenFramebuffers(1, &fbo);
	}
	GL.BindTexture(GL_TEXTURE_2D, texture);
	GL.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GL.BindFramebuffer(GL_FRAMEBUFFER, fbo); // (take puts back what was bound)
	GL.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
}
} // namespace

} // namespace shell

#endif
