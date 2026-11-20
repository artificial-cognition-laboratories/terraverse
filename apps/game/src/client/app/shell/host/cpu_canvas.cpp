// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/cpu_canvas.h"
#include "src/client/app/shell/host/gpu_canvas.h"
#include "src/shared/platform/log.h"
#include "library/trace/trace.h"
#include <mutex>
#include <stdexcept>

namespace shell {

void CpuCanvas::paint(const void *premultiplied, u32 width, u32 height, const std::vector<CefRect> &dirty)
{
	TF_ZONE("Page paint"); // unpremultiplying what changed, on Chromium's thread
	m_pixels.write(premultiplied, width, height, dirty);
}

void CpuCanvas::paint(const CefAcceleratedPaintInfo &, const std::vector<CefRect> &)
{
	throw std::logic_error("shell: a shared texture painted to the CPU's canvas (it never asked for them)");
}

bool CpuCanvas::draw(core::position2di at)
{
	return m_surface.draw(m_pixels, at);
}

u8 CpuCanvas::alphaAt(s32 x, s32 y) const
{
	return m_pixels.alphaAt(x, y);
}

Canvas::Painted CpuCanvas::taken()
{
	// (Chromium mid-paint: nothing this frame, all of it next)
	std::unique_lock<std::mutex> guard(m_pixels.lock, std::try_to_lock);
	if (!guard)
		return {};
	const Painted out = m_pixels.painted;
	m_pixels.painted = {};
	return out;
}

std::unique_ptr<Canvas> makeCanvas(video::IVideoDriver *driver, bool gpu, const char *page)
{
	if (!gpu) {
		infostream << "shell: " << page << " drawn in software (shell_gpu off)" << std::endl;
		return std::make_unique<CpuCanvas>(driver);
	}
	GpuCanvas::Boot boot = GpuCanvas::boot();
	if (boot.canvas) {
		infostream << "shell: " << page << "'s frames shared from Chromium's GPU" << std::endl;
		return std::move(boot.canvas);
	}
	// Chromium still draws on the GPU; each frame is read back for us instead
	warningstream << "shell: " << page << "'s frames copied through the CPU: " << boot.why << std::endl;
	return std::make_unique<CpuCanvas>(driver);
}

} // namespace shell
