// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "canvas.h"
#include "pixels.h"
#include "surface.h"

namespace shell {

// The page's frames as pixels: Chromium reads each back from its GPU (or
// draws it in software), and the changed part is uploaded to a texture.
class CpuCanvas : public Canvas
{
public:
	explicit CpuCanvas(video::IVideoDriver *driver) : m_surface(driver) {}

	bool shared() const override { return false; }
	void paint(const void *premultiplied, u32 width, u32 height, const std::vector<CefRect> &dirty) override;
	void paint(const CefAcceleratedPaintInfo &info, const std::vector<CefRect> &dirty) override;
	bool settled() override { return true; } // (a paint's taken in before OnPaint returns)
	bool draw(core::position2di at) override;
	u8 alphaAt(s32 x, s32 y) const override;
	Painted taken() override;

private:
	Pixels m_pixels;
	Surface m_surface;
};

} // namespace shell
