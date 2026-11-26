// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <position2d.h>

namespace video { class IVideoDriver; class ITexture; }

namespace shell {

struct Pixels;

// Owns the GPU texture the page is drawn from, and draws it over the frame.
class Surface
{
public:
	explicit Surface(video::IVideoDriver *driver);
	~Surface();

	// With its top-left corner here on the screen. Whether it took a newly
	// painted page to the GPU first.
	bool draw(Pixels &pixels, core::position2di at = { 0, 0 });

private:
	void upload(Pixels &pixels); // holding its lock

	video::IVideoDriver *m_driver;
	video::ITexture *m_texture = nullptr;
};

} // namespace shell
