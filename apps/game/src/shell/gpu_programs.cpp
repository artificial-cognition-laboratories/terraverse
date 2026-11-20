// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gpu_programs.h"

#ifdef __linux__

#include <mt_opengl.h>
#include <GL/gl.h>
#include <GL/glext.h>

namespace shell {

namespace {
// A quad over `rect` (clip space: x0, y0, x1, y1), `corner` 0..1 across it from its first corner.
const char *VERTEX = R"(
uniform vec4 rect;
out vec2 corner;
void main()
{
	corner = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
	gl_Position = vec4(mix(rect.xy, rect.zw, corner), 0.0, 1.0);
}
)";
const char *SHOW = R"(
uniform sampler2D page;
in vec2 corner;
out vec4 colour;
void main() { colour = texture(page, corner); }
)";
// (SAMPLER: sampler2D, or samplerExternalOES where the driver's layout needs it)
const char *COPY = R"(
uniform SAMPLER source;
uniform vec2 offset, coded;
out vec4 colour;
void main() { colour = texture(source, (offset + gl_FragCoord.xy) / coded); }
)";
// Four linear taps cover a cell of four by four: any of it solid, the cell is.
const char *MASK = R"(
uniform sampler2D page;
uniform vec2 size;
uniform float cell;
out vec4 colour;
void main()
{
	vec2 at = floor(gl_FragCoord.xy) * cell;
	float a = max(max(texture(page, (at + vec2(1.0, 1.0)) / size).a, texture(page, (at + vec2(3.0, 1.0)) / size).a),
			max(texture(page, (at + vec2(1.0, 3.0)) / size).a, texture(page, (at + vec2(3.0, 3.0)) / size).a));
	colour = vec4(a, 0.0, 0.0, 1.0);
}
)";

GLuint stage(GLenum kind, const std::string &source, std::string &log)
{
	const GLuint s = GL.CreateShader(kind);
	const char *text = source.c_str();
	GL.ShaderSource(s, 1, &text, nullptr);
	GL.CompileShader(s);
	GLint ok = 0;
	GL.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char buf[1024] = {};
		GL.GetShaderInfoLog(s, sizeof buf, nullptr, buf);
		log = buf;
		GL.DeleteShader(s);
		return 0;
	}
	return s;
}

// The program, or 0 and why in `failed`.
GLuint link(const std::string &head, const char *fragment, std::string &failed)
{
	std::string log;
	const GLuint v = stage(GL_VERTEX_SHADER, head + VERTEX, log);
	const GLuint f = v ? stage(GL_FRAGMENT_SHADER, head + fragment, log) : 0;
	GLuint p = 0;
	if (v && f) {
		p = GL.CreateProgram();
		GL.AttachShader(p, v);
		GL.AttachShader(p, f);
		GL.LinkProgram(p);
		GLint ok = 0;
		GL.GetProgramiv(p, GL_LINK_STATUS, &ok);
		if (!ok) {
			char buf[1024] = {};
			GL.GetProgramInfoLog(p, sizeof buf, nullptr, buf);
			log = buf;
			GL.DeleteProgram(p);
			p = 0;
		}
	}
	if (v)
		GL.DeleteShader(v);
	if (f)
		GL.DeleteShader(f);
	if (!p)
		failed = "a page shader didn't compile: " + log;
	return p;
}

void quad(GLuint program, float x0, float y0, float x1, float y1)
{
	GL.Uniform4f(GL.GetUniformLocation(program, "rect"), x0, y0, x1, y1);
	GL.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
} // namespace

GpuPrograms GpuPrograms::compile(bool es, bool external)
{
	const std::string version = es ? "#version 300 es\n" : "#version 150\n";
	const std::string precision = es ? "precision highp float;\n" : "";
	GpuPrograms out;
	out.show_program = link(version + precision, SHOW, out.failed);
	out.copy_2d = link(version + precision + "#define SAMPLER sampler2D\n", COPY, out.failed);
	out.mask_program = link(version + precision, MASK, out.failed);
	if (!out.ready()) {
		out.release();
		return out;
	}
	if (external)
		out.copy_external = link(version + (es ? "#extension GL_OES_EGL_image_external_essl3 : require\n" :
				"#extension GL_OES_EGL_image_external : require\n") + precision +
				"#define SAMPLER samplerExternalOES\n", COPY, out.failed);
	else
		out.failed = "GL lacks GL_OES_EGL_image_external";
	return out;
}

void GpuPrograms::release()
{
	for (unsigned *p : { &show_program, &copy_2d, &copy_external, &mask_program }) {
		if (*p)
			GL.DeleteProgram(*p);
		*p = 0;
	}
}

void GpuPrograms::copy(bool external, unsigned target, unsigned source, int x, int y, int coded_w, int coded_h) const
{
	const GLuint p = external ? copy_external : copy_2d;
	GL.UseProgram(p);
	GL.ActiveTexture(GL_TEXTURE0);
	GL.BindTexture(target, source);
	GL.Uniform1i(GL.GetUniformLocation(p, "source"), 0);
	GL.Uniform2f(GL.GetUniformLocation(p, "offset"), (float)x, (float)y);
	GL.Uniform2f(GL.GetUniformLocation(p, "coded"), (float)coded_w, (float)coded_h);
	quad(p, -1, -1, 1, 1);
}

void GpuPrograms::mask(unsigned page, u32 width, u32 height, u32 cell) const
{
	GL.UseProgram(mask_program);
	GL.ActiveTexture(GL_TEXTURE0);
	GL.BindTexture(GL_TEXTURE_2D, page);
	GL.Uniform1i(GL.GetUniformLocation(mask_program, "page"), 0);
	GL.Uniform2f(GL.GetUniformLocation(mask_program, "size"), (float)width, (float)height);
	GL.Uniform1f(GL.GetUniformLocation(mask_program, "cell"), (float)cell);
	quad(mask_program, -1, -1, 1, 1);
}

void GpuPrograms::show(unsigned page, float x0, float y0, float x1, float y1) const
{
	GL.UseProgram(show_program);
	GL.ActiveTexture(GL_TEXTURE0);
	GL.BindTexture(GL_TEXTURE_2D, page);
	GL.Uniform1i(GL.GetUniformLocation(show_program, "page"), 0);
	quad(show_program, x0, y0, x1, y1);
}

} // namespace shell

#endif
