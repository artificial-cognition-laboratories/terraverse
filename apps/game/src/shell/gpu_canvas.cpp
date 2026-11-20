// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gpu_canvas.h"
#include <stdexcept>

#ifndef __linux__

namespace shell {

struct GpuCanvas::Impl {};

GpuCanvas::Boot GpuCanvas::boot()
{
	// TEMP(2026-11-20): Windows hands D3D11 shared handles (into GL by
	// WGL_NV_DX_interop2), macOS IOSurfaces; neither is wired yet, so their
	// frames go through the CPU.
	return { nullptr, "shared textures are wired on Linux only so far" };
}

GpuCanvas::GpuCanvas(std::unique_ptr<Impl> impl) : m(std::move(impl)) {}
GpuCanvas::~GpuCanvas() = default;
static void unreachable() { throw std::logic_error("shell: a GpuCanvas where none can boot"); }
void GpuCanvas::paint(const void *, u32, u32, const std::vector<CefRect> &) { unreachable(); }
void GpuCanvas::paint(const CefAcceleratedPaintInfo &, const std::vector<CefRect> &) { unreachable(); }
bool GpuCanvas::settled() { unreachable(); return false; }
bool GpuCanvas::draw(core::position2di) { unreachable(); return false; }
u8 GpuCanvas::alphaAt(s32, s32) const { unreachable(); return 0; }
Canvas::Painted GpuCanvas::taken() { unreachable(); return {}; }

} // namespace shell

#else

#include "gpu_egl.h"
#include "gpu_programs.h"
#include "trace/trace.h"
#include <mt_opengl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <array>
#include <mutex>

namespace shell {

// The page's mask is this many times smaller each way than the page: a
// cell is solid where any of it is (gpu_programs.cpp), so the mouse is the
// page's up to 3 device pixels past its edges.
static constexpr u32 MASK_CELL = 4;

struct GpuCanvas::Impl
{
	std::unique_ptr<GpuEgl> egl;
	GpuPrograms programs; // shared by both contexts

	// Chromium's thread's
	GLuint fbo = 0, vao = 0, mask_texture = 0;
	GLuint pack = 0; // the mask on its way back (a buffer: shared, so the main thread maps it)
	size_t pack_size = 0;
	u32 mask_width = 0, mask_height = 0;

	// the main thread's
	GLuint main_vao = 0;

	// The frames: Chromium writes any slot but the newest and the one being
	// drawn, the main thread draws the newest. Each is fenced after it's drawn
	// so it isn't written under a frame still on the GPU.
	struct Slot
	{
		GLuint texture = 0;
		u32 width = 0, height = 0;
		GLsync read = nullptr;
	};
	std::array<Slot, 3> slots;
	mutable std::mutex lock; // what's below, and slots' `read`
	int latest = -1, shown = -1;
	bool fresh = false; // latest is newer than shown
	// A paint on the GPU still: its slot and mask become the newest once `done` has passed.
	struct Pending
	{
		GLsync done = nullptr;
		int slot = -1;
		u32 mask_width = 0, mask_height = 0;
	};
	Pending pending;
	std::vector<u8> mask; // MASK_CELL² cells' alpha, row by row
	u32 mask_stride = 0, mask_rows = 0;
	Painted painted;

	void start();
	int chooseSlot();
	// Holding the lock, `pending` done: its mask read out and its slot the newest.
	void takeIn();
};

namespace {
// Binds what it binds, and puts back what was bound: Irrlicht caches GL's
// state, and only what it set may be there when it next draws.
struct Restore
{
	GLint program, vao, active, texture, viewport[4];
	GLint src_rgb, dst_rgb, src_alpha, dst_alpha, eq_rgb, eq_alpha;
	GLboolean blend, depth, cull, scissor, stencil;
	Restore();
	~Restore();
};
} // namespace

GpuCanvas::Boot GpuCanvas::boot()
{
	GpuEgl::Boot egl = GpuEgl::boot();
	if (!egl.egl)
		return { nullptr, egl.why };
	auto impl = std::make_unique<Impl>();
	Impl &m = *impl;
	m.egl = std::move(egl.egl);
	m.programs = GpuPrograms::compile(m.egl->es(), GL.IsExtensionPresent("GL_OES_EGL_image_external"));
	if (!m.programs.ready())
		return { nullptr, m.programs.failed };
	Restore keep;
	for (Impl::Slot &slot : m.slots) {
		GL.GenTextures(1, &slot.texture);
		GL.BindTexture(GL_TEXTURE_2D, slot.texture);
		GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	GL.GenVertexArrays(1, &m.main_vao);
	return { std::unique_ptr<GpuCanvas>(new GpuCanvas(std::move(impl))), "" };
}

GpuCanvas::GpuCanvas(std::unique_ptr<Impl> impl) : m(std::move(impl)) {}

GpuCanvas::~GpuCanvas()
{
	m->egl.reset(); // Chromium's context first: what's below is shared, and goes here
	for (Impl::Slot &slot : m->slots) {
		if (slot.read)
			GL.DeleteSync(slot.read);
		GL.DeleteTextures(1, &slot.texture);
	}
	if (m->mask_texture) // (made on Chromium's thread, but textures and buffers are shared: they outlive its context)
		GL.DeleteTextures(1, &m->mask_texture);
	if (m->pack)
		GL.DeleteBuffers(1, &m->pack);
	if (m->pending.done)
		GL.DeleteSync(m->pending.done);
	GL.DeleteVertexArrays(1, &m->main_vao);
	m->programs.release();
}

// Chromium's thread, its first paint: what it draws with in its own context.
void GpuCanvas::Impl::start()
{
	egl->enter();
	GL.GenFramebuffers(1, &fbo);
	GL.GenVertexArrays(1, &vao);
	GL.GenTextures(1, &mask_texture);
	GL.BindTexture(GL_TEXTURE_2D, mask_texture);
	GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GL.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL.GenBuffers(1, &pack);
}

void GpuCanvas::Impl::takeIn()
{
	GLint bound = 0;
	GL.GetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &bound);
	GL.BindBuffer(GL_PIXEL_PACK_BUFFER, pack);
	const size_t size = (size_t)pending.mask_width * pending.mask_height * 4;
	const void *read = GL.MapBufferRange(GL_PIXEL_PACK_BUFFER, 0, size, GL_MAP_READ_BIT);
	if (!read)
		throw std::runtime_error("shell: the page's mask won't map back");
	mask.assign((const u8 *)read, (const u8 *)read + size);
	GL.UnmapBuffer(GL_PIXEL_PACK_BUFFER);
	GL.BindBuffer(GL_PIXEL_PACK_BUFFER, bound);
	mask_stride = pending.mask_width;
	mask_rows = pending.mask_height;
	GL.DeleteSync(pending.done);
	latest = pending.slot;
	fresh = true;
	pending = {};
}

int GpuCanvas::Impl::chooseSlot()
{
	for (int i = 0; i < (int)slots.size(); i++)
		if (i != latest && i != shown && i != pending.slot)
			return i;
	throw std::logic_error("shell: no slot free of the newest and the drawn");
}

void GpuCanvas::paint(const void *, u32, u32, const std::vector<CefRect> &)
{
	throw std::logic_error("shell: pixels painted to the GPU's canvas (Chromium was asked for textures)");
}

void GpuCanvas::paint(const CefAcceleratedPaintInfo &info, const std::vector<CefRect> &dirty)
{
	TF_ZONE("Page paint");
	const u64 at = trace::now();
	Impl &m = *this->m;
	if (!m.vao)
		m.start();
	const CefRect visible = info.extra.visible_rect;
	const u32 w = (u32)visible.width, h = (u32)visible.height;
	if (w == 0 || h == 0)
		return;

	const GpuEgl::Frame frame = m.egl->take(info);
	if (frame.external && !m.programs.hasExternal()) {
		m.egl->letGo(frame);
		throw std::runtime_error("shell: Chromium painted a layout (modifier " + std::to_string(info.modifier) +
				") only external samplers take, and this GL has none: " + m.programs.failed);
	}

	int i;
	GLsync read;
	{
		std::lock_guard<std::mutex> guard(m.lock);
		// (painted unasked, a resize: the last paint's taken in here first)
		if (m.pending.done) {
			GL.ClientWaitSync(m.pending.done, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
			m.takeIn();
		}
		i = m.chooseSlot();
		read = m.slots[i].read;
		m.slots[i].read = nullptr;
	}
	Impl::Slot &slot = m.slots[i];
	if (read) { // (the main thread's frame that drew it, done on the GPU first: the GPU waits, not us)
		GL.WaitSync(read, 0, GL_TIMEOUT_IGNORED);
		GL.DeleteSync(read);
	}
	if (slot.width != w || slot.height != h) {
		GL.BindTexture(GL_TEXTURE_2D, slot.texture);
		GL.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		slot.width = w;
		slot.height = h;
	}
	GL.BindVertexArray(m.vao);
	GL.BindFramebuffer(GL_FRAMEBUFFER, m.fbo);
	GL.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, slot.texture, 0);
	GL.Viewport(0, 0, w, h);
	m.programs.copy(frame.external, frame.target, frame.texture, visible.x, visible.y,
			info.extra.coded_size.width, info.extra.coded_size.height);

	// where it's solid, on its way back to memory while the GPU gets to it
	const u32 mw = (w + MASK_CELL - 1) / MASK_CELL, mh = (h + MASK_CELL - 1) / MASK_CELL;
	if (m.mask_width != mw || m.mask_height != mh) {
		GL.BindTexture(GL_TEXTURE_2D, m.mask_texture);
		GL.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m.mask_width = mw;
		m.mask_height = mh;
	}
	GL.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m.mask_texture, 0);
	GL.Viewport(0, 0, mw, mh);
	m.programs.mask(slot.texture, w, h, MASK_CELL);
	GL.BindBuffer(GL_PIXEL_PACK_BUFFER, m.pack);
	if (m.pack_size != (size_t)mw * mh * 4) {
		m.pack_size = (size_t)mw * mh * 4;
		GL.BufferData(GL_PIXEL_PACK_BUFFER, m.pack_size, nullptr, GL_STREAM_READ);
	}
	GL.PixelStorei(GL_PACK_ALIGNMENT, 1);
	GL.ReadPixels(0, 0, mw, mh, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	GL.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	GLsync done = GL.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	GL.Flush(); // (the main thread polls it)
	m.egl->letGo(frame); // (GL keeps them till the GPU's done with them)

	double area = 0;
	for (const CefRect &r : dirty)
		area += (double)r.width * r.height / ((double)w * h);
	std::lock_guard<std::mutex> guard(m.lock);
	m.pending = { done, i, mw, mh };
	if (m.painted.paints++ == 0)
		m.painted.first = at;
	m.painted.area += area;
}

bool GpuCanvas::settled()
{
	std::lock_guard<std::mutex> guard(m->lock);
	if (!m->pending.done)
		return true;
	if (GL.ClientWaitSync(m->pending.done, 0, 0) == GL_TIMEOUT_EXPIRED)
		return false;
	m->takeIn();
	return true;
}

bool GpuCanvas::draw(core::position2di at)
{
	Impl &m = *this->m;
	settled(); // (a paint the GPU's finished since the frame began shows now)
	bool fresh;
	GLuint texture;
	u32 w, h;
	{
		std::lock_guard<std::mutex> guard(m.lock);
		if (m.latest < 0)
			return false;
		fresh = m.fresh;
		if (fresh)
			m.shown = m.latest;
		m.fresh = false;
		texture = m.slots[m.shown].texture;
		w = m.slots[m.shown].width;
		h = m.slots[m.shown].height;
	}
	{
		Restore keep;
		GL.Disable(GL_DEPTH_TEST);
		GL.Disable(GL_CULL_FACE);
		GL.Disable(GL_SCISSOR_TEST);
		GL.Disable(GL_STENCIL_TEST);
		GL.Enable(GL_BLEND);
		GL.BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		GL.BlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // (premultiplied)
		GL.BindVertexArray(m.main_vao);
		const float vw = (float)keep.viewport[2], vh = (float)keep.viewport[3];
		m.programs.show(texture, at.X / vw * 2 - 1, 1 - at.Y / vh * 2,
				(at.X + w) / vw * 2 - 1, 1 - (at.Y + h) / vh * 2);
	}
	GLsync drawn = GL.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	GL.Flush(); // (another context waits on it)
	std::lock_guard<std::mutex> guard(m.lock);
	GLsync &read = m.slots[m.shown].read;
	if (read) // (the newer fence covers it)
		GL.DeleteSync(read);
	read = drawn;
	return fresh;
}

u8 GpuCanvas::alphaAt(s32 x, s32 y) const
{
	std::lock_guard<std::mutex> guard(m->lock);
	if (x < 0 || y < 0)
		return 0;
	const u32 cx = (u32)x / MASK_CELL, cy = (u32)y / MASK_CELL;
	if (cx >= m->mask_stride || cy >= m->mask_rows)
		return 0;
	return m->mask[((size_t)cy * m->mask_stride + cx) * 4];
}

Canvas::Painted GpuCanvas::taken()
{
	std::lock_guard<std::mutex> guard(m->lock);
	const Painted out = m->painted;
	m->painted = {};
	return out;
}

Restore::Restore()
{
	GL.GetIntegerv(GL_CURRENT_PROGRAM, &program);
	GL.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
	GL.GetIntegerv(GL_ACTIVE_TEXTURE, &active);
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

Restore::~Restore()
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
	GL.BindTexture(GL_TEXTURE_2D, texture);
	GL.ActiveTexture(active);
	GL.BindVertexArray(vao);
	GL.UseProgram(program);
}

} // namespace shell

#endif
