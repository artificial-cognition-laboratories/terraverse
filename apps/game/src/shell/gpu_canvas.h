// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "canvas.h"
#include <memory>
#include <string>

namespace shell {

/*
	The page's frames straight from Chromium's GPU: each paint is a texture
	of its own (a dmabuf on Linux), taken into a GL context of ours on
	Chromium's thread, shared with the engine's, and copied GPU to GPU into
	one of three slots. The main thread draws the newest slot; nothing is
	read back but a quarter-size mask of where the page is solid (alphaAt).

	Neither thread waits on the GPU: a paint is fenced, and the main thread
	takes it in (settled) once the fence has passed, and only then asks
	Chromium for the next frame. Chromium paints only when asked
	(external begin frames), so the buffer it handed over isn't reused
	while our copy of it is still on the GPU.

	Before it (CpuCanvas) Chromium read every frame back to memory and we
	uploaded all of it again: a frame's latency and up to 15 MB a change.
*/
class GpuCanvas : public Canvas
{
public:
	struct Boot
	{
		std::unique_ptr<GpuCanvas> canvas; // or none, and
		std::string why;                   // why it can't run here
	};
	// On the main thread, its GL context current: everything it needs from
	// the driver and the GL there, checked, its programs compiled.
	static Boot boot();
	~GpuCanvas() override;

	bool shared() const override { return true; }
	void paint(const void *premultiplied, u32 width, u32 height, const std::vector<CefRect> &dirty) override;
	void paint(const CefAcceleratedPaintInfo &info, const std::vector<CefRect> &dirty) override;
	bool settled() override;
	bool draw(core::position2di at) override;
	u8 alphaAt(s32 x, s32 y) const override;
	Painted taken() override;

	struct Impl;

private:
	explicit GpuCanvas(std::unique_ptr<Impl> impl);
	std::unique_ptr<Impl> m;
};

} // namespace shell
