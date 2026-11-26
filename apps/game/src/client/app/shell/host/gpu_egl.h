// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <include/internal/cef_types_wrappers.h>
#include <memory>
#include <string>

namespace shell {

/*
	EGL for the GPU canvas (gpu_canvas.h): the game's context, found on the
	main thread; a context of ours on Chromium's thread sharing it; and each
	of Chromium's frames (a dmabuf) taken in there as a texture. Linux only.
*/
class GpuEgl
{
public:
	struct Boot
	{
		std::unique_ptr<GpuEgl> egl; // or none, and
		std::string why;             // why it can't run here
	};
	// On the main thread, the game's context current.
	static Boot boot();
	~GpuEgl(); // on the main thread, Chromium still up (ours goes on its thread)

	bool es() const;

	// Chromium's thread: ours current there, made the first time.
	void enter();
	// A frame as a texture of ours (`target`: GL_TEXTURE_2D, or the external
	// target where the driver's layout needs it), till it's let go.
	struct Frame
	{
		unsigned texture = 0, target = 0;
		bool external = false;
		void *image = nullptr;
	};
	Frame take(const CefAcceleratedPaintInfo &info);
	void letGo(const Frame &frame);

	struct State;

private:
	explicit GpuEgl(std::unique_ptr<State> state);
	std::unique_ptr<State> s;
};

} // namespace shell
