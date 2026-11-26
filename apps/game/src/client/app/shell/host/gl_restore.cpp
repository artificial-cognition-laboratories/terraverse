// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/gl_restore.h"

#ifdef __linux__

#include <GL/gl.h>
#include <GL/glext.h>

namespace shell {

GlRestore::GlRestore()
{
	GL.GetIntegerv(GL_CURRENT_PROGRAM, &program);
	GL.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
	GL.GetIntegerv(GL_ACTIVE_TEXTURE, &active);
	GL.ActiveTexture(GL_TEXTURE1); // (show reads the blurred frame there)
	GL.GetIntegerv(GL_TEXTURE_BINDING_2D, &texture1);
	GL.ActiveTexture(GL_TEXTURE0);
	GL.GetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	GL.GetIntegerv(GL_VIEWPORT, viewport);
	GL.GetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
	GL.GetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
	GL.GetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
	GL.GetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
	GL.GetIntegerv(GL_BLEND_EQUATION_RGB, &eq_rgb);
	GL.GetIntegerv(GL_BLEND_EQUATION_ALPHA, &eq_alpha);
	blend = GL.IsEnabled(GL_BLEND);
	depth = GL.IsEnabled(GL_DEPTH_TEST);
	cull = GL.IsEnabled(GL_CULL_FACE);
	scissor = GL.IsEnabled(GL_SCISSOR_TEST);
	stencil = GL.IsEnabled(GL_STENCIL_TEST);
}

GlRestore::~GlRestore()
{
	const auto set = [](GLenum cap, GLboolean on) { on ? GL.Enable(cap) : GL.Disable(cap); };
	set(GL_BLEND, blend);
	set(GL_DEPTH_TEST, depth);
	set(GL_CULL_FACE, cull);
	set(GL_SCISSOR_TEST, scissor);
	set(GL_STENCIL_TEST, stencil);
	GL.BlendEquationSeparate(eq_rgb, eq_alpha);
	GL.BlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
	GL.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	GL.ActiveTexture(GL_TEXTURE1);
	GL.BindTexture(GL_TEXTURE_2D, texture1);
	GL.ActiveTexture(GL_TEXTURE0);
	GL.BindTexture(GL_TEXTURE_2D, texture);
	GL.ActiveTexture(active);
	GL.BindVertexArray(vao);
	GL.UseProgram(program);
}

} // namespace shell

#endif
