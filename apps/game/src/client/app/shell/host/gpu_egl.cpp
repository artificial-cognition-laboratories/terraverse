// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/gpu_egl.h"

#ifdef __linux__

#include "src/client/app/shell/host/inbox.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <mt_opengl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <map>
#include <stdexcept>
#include <vector>

namespace shell {

// Chromium's colour types as DRM's fourccs ('AR24', 'AB24': ARGB8888 and
// ABGR8888 are B,G,R,A and R,G,B,A bytes in memory, little-endian)
static constexpr u32 FOURCC_ARGB8888 = 0x34325241, FOURCC_ABGR8888 = 0x34324241;
// GL_OES_EGL_image_external's target (GLES's headers have it, desktop GL's don't)
static constexpr GLenum TEXTURE_EXTERNAL_OES = 0x8D65;
typedef void (*ImageTargetTexture)(GLenum target, void *image);

struct GpuEgl::State
{
	// the main thread's context, which ours shares
	EGLDisplay display = EGL_NO_DISPLAY;
	EGLConfig config = nullptr;
	EGLContext main = EGL_NO_CONTEXT;
	EGLenum api = EGL_OPENGL_API;
	std::vector<EGLint> attribs; // ours made as it was
	PFNEGLCREATEIMAGEKHRPROC createImage = nullptr;
	PFNEGLDESTROYIMAGEKHRPROC destroyImage = nullptr;
	ImageTargetTexture imageTexture = nullptr;
	// A fourcc and modifier: whether it's sampled only as external. (NVIDIA's
	// linear layout is, and its desktop GL can't sample those; Chromium paints
	// its tiled ones, which aren't.)
	std::map<std::pair<u32, u64>, bool> external;
	EGLContext own = EGL_NO_CONTEXT; // Chromium's thread's
};

namespace {
bool has(const char *list, const char *ext)
{
	return list && (std::string(" ") + list + " ").find(std::string(" ") + ext + " ") != std::string::npos;
}
} // namespace

GpuEgl::Boot GpuEgl::boot()
{
	auto state = std::make_unique<State>();
	State &s = *state;
	s.display = eglGetCurrentDisplay();
	s.main = eglGetCurrentContext();
	if (s.display == EGL_NO_DISPLAY || s.main == EGL_NO_CONTEXT)
		return { nullptr, "the game's GL context isn't EGL's" };
	const char *egl = eglQueryString(s.display, EGL_EXTENSIONS);
	for (const char *ext : { "EGL_EXT_image_dma_buf_import", "EGL_EXT_image_dma_buf_import_modifiers",
				"EGL_KHR_surfaceless_context", "EGL_KHR_image_base" })
		if (!has(egl, ext))
			return { nullptr, std::string("EGL lacks ") + ext };
	if (!GL.IsExtensionPresent("GL_OES_EGL_image"))
		return { nullptr, "GL lacks GL_OES_EGL_image" };

	EGLint config_id = 0, client = 0;
	eglQueryContext(s.display, s.main, EGL_CONFIG_ID, &config_id);
	eglQueryContext(s.display, s.main, EGL_CONTEXT_CLIENT_TYPE, &client);
	const EGLint wanted[] = { EGL_CONFIG_ID, config_id, EGL_NONE };
	EGLint found = 0;
	if (!eglChooseConfig(s.display, wanted, &s.config, 1, &found) || found != 1)
		return { nullptr, "the game context's EGL config can't be found again" };
	s.api = (EGLenum)client;
	const bool es = s.api == EGL_OPENGL_ES_API;
	GLint major = 0, minor = 0;
	GL.GetIntegerv(GL_MAJOR_VERSION, &major);
	GL.GetIntegerv(GL_MINOR_VERSION, &minor);
	if (es ? major < 3 : (major < 3 || (major == 3 && minor < 2)))
		return { nullptr, "needs OpenGL 3.2 or ES 3.0 (the game's is " + std::to_string(major) + "." +
				std::to_string(minor) + (es ? " ES)" : ")") };
	s.attribs = { EGL_CONTEXT_MAJOR_VERSION, major, EGL_CONTEXT_MINOR_VERSION, minor };
	if (!es) {
		GLint profile = 0;
		GL.GetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
		s.attribs.insert(s.attribs.end(), { EGL_CONTEXT_OPENGL_PROFILE_MASK,
				(profile & GL_CONTEXT_CORE_PROFILE_BIT) ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT :
						EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT });
	}
	s.attribs.push_back(EGL_NONE);

	s.createImage = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
	s.destroyImage = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
	s.imageTexture = (ImageTargetTexture)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	auto queryModifiers = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
	if (!s.createImage || !s.destroyImage || !s.imageTexture || !queryModifiers)
		return { nullptr, "EGL's image functions aren't there" };
	for (u32 fourcc : { FOURCC_ARGB8888, FOURCC_ABGR8888 }) {
		EGLint n = 0;
		queryModifiers(s.display, (EGLint)fourcc, 0, nullptr, nullptr, &n);
		std::vector<EGLuint64KHR> modifiers(n);
		std::vector<EGLBoolean> only(n);
		queryModifiers(s.display, (EGLint)fourcc, n, modifiers.data(), only.data(), &n);
		for (EGLint i = 0; i < n; i++)
			s.external[{ fourcc, modifiers[i] }] = only[i];
	}
	return { std::unique_ptr<GpuEgl>(new GpuEgl(std::move(state))), "" };
}

GpuEgl::GpuEgl(std::unique_ptr<State> state) : s(std::move(state)) {}

GpuEgl::~GpuEgl()
{
	if (s->own == EGL_NO_CONTEXT)
		return;
	onChromium([this] { // (it's current there)
		eglMakeCurrent(s->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(s->display, s->own);
	});
}

bool GpuEgl::es() const
{
	return s->api == EGL_OPENGL_ES_API;
}

void GpuEgl::enter()
{
	if (s->own != EGL_NO_CONTEXT)
		return;
	if (!eglBindAPI(s->api))
		throw std::runtime_error("shell: EGL won't bind the game's GL API on Chromium's thread");
	s->own = eglCreateContext(s->display, s->config, s->main, s->attribs.data());
	if (s->own == EGL_NO_CONTEXT)
		throw std::runtime_error("shell: no GL context shared with the game's (EGL error " +
				std::to_string(eglGetError()) + ")");
	if (!eglMakeCurrent(s->display, EGL_NO_SURFACE, EGL_NO_SURFACE, s->own))
		throw std::runtime_error("shell: Chromium's GL context won't be made current");
}

GpuEgl::Frame GpuEgl::take(const CefAcceleratedPaintInfo &info)
{
	if (info.plane_count != 1)
		throw std::runtime_error("shell: a shared texture of " + std::to_string(info.plane_count) + " planes");
	const u32 fourcc = info.format == CEF_COLOR_TYPE_RGBA_8888 ? FOURCC_ABGR8888 : FOURCC_ARGB8888;
	const EGLint attribs[] = {
		EGL_WIDTH, info.extra.coded_size.width, EGL_HEIGHT, info.extra.coded_size.height,
		EGL_LINUX_DRM_FOURCC_EXT, (EGLint)fourcc,
		EGL_DMA_BUF_PLANE0_FD_EXT, info.planes[0].fd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, (EGLint)info.planes[0].offset,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)info.planes[0].stride,
		EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)(info.modifier & 0xffffffff),
		EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)(info.modifier >> 32),
		EGL_NONE };
	Frame out;
	out.image = s->createImage(s->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
	if (out.image == EGL_NO_IMAGE_KHR)
		throw std::runtime_error("shell: Chromium's frame won't import (EGL error " + std::to_string(eglGetError()) + ")");
	const auto found = s->external.find({ fourcc, info.modifier });
	out.external = found != s->external.end() && found->second;
	out.target = out.external ? TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
	GL.GenTextures(1, &out.texture);
	GL.BindTexture(out.target, out.texture);
	GL.TexParameteri(out.target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GL.TexParameteri(out.target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	s->imageTexture(out.target, out.image);
	return out;
}

void GpuEgl::letGo(const Frame &frame)
{
	GL.DeleteTextures(1, &frame.texture);
	s->destroyImage(s->display, frame.image);
}

} // namespace shell

#endif
