// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "trace/trace.h"
#include "surface.h"
#include "pixels.h"
#include "exceptions.h"
#include <IVideoDriver.h>
#include <cstring>
#include <mutex>

namespace shell {

Surface::Surface(video::IVideoDriver *driver) : m_driver(driver)
{
}

Surface::~Surface()
{
	if (m_texture)
		m_driver->removeTexture(m_texture);
}

bool Surface::draw(Pixels &pixels, core::position2di at)
{
	// (Chromium mid-paint: last frame's page, and this one next frame)
	bool fresh = false;
	if (std::unique_lock<std::mutex> guard(pixels.lock, std::try_to_lock); guard && pixels.dirty) {
		upload(pixels);
		fresh = true;
	}
	if (!m_texture)
		return fresh;

	core::dimension2du size = m_texture->getOriginalSize();
	core::rect<s32> rect(0, 0, size.Width, size.Height);
	m_driver->draw2DImage(m_texture, rect + at, rect, nullptr, nullptr, true);
	return fresh;
}

void Surface::upload(Pixels &pixels)
{
	TF_ZONE("Page upload");
	core::dimension2du size(pixels.width, pixels.height);
	if (m_texture && m_texture->getOriginalSize() != size) {
		m_driver->removeTexture(m_texture);
		m_texture = nullptr;
	}
	if (!m_texture) {
		bool mipmaps = m_driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
		m_driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
		m_texture = m_driver->addTexture(size, "shell_page", video::ECF_A8R8G8B8);
		m_driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmaps);
		if (!m_texture)
			throw BaseException("shell: could not create the page texture");
	}

	// Whole frame each time: a write-only lock leaves untouched texels undefined.
	void *dst = m_texture->lock(video::ETLM_WRITE_ONLY, 0, 0, video::ETLF_NONE);
	if (!dst)
		throw BaseException("shell: could not lock the page texture");
	std::memcpy(dst, pixels.bgra.data(), pixels.bgra.size());
	m_texture->unlock();
	pixels.dirty = false;
}

} // namespace shell
