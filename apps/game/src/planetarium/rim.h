// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "air/tables.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include "terragen/system.h"
#include <SMaterial.h>
#include <future>
#include <map>
#include <memory>

class IShaderSource;
namespace scene { class IMeshBuffer; }
namespace video { class ITexture; class IVideoDriver; }

/*
	A body's air from outside: the glowing edge, reddening to the night side, and a
	little haze over the land near it. Colours from the body's real air (air::rimImage,
	made on a worker the first time it's seen); its height on the drawn planet is
	a share of the radius, chosen by eye (real air would be a hairline: under 1%).
*/
namespace planetarium {

class Rim
{
public:
	static constexpr double SHARE = 0.03; // of the radius

	Rim(IShaderSource *ssrc, const terragen::System &system);
	~Rim();

	// Over a body already drawn, the camera at the origin: `at` its centre (BS), `ground`
	// its radius (BS), `sun` the way to its star, `mesh` a unit sphere. Nothing if it has no air.
	void draw(video::IVideoDriver *driver, size_t body, const v3f &at, float ground, const v3f &sun, scene::IMeshBuffer *mesh);

private:
	struct Part
	{
		std::future<air::RimImage> making;
		video::ITexture *texture = nullptr;
		bool none = false;
	};
	const terragen::System &m_system;
	std::map<size_t, Part> m_parts;
	video::SMaterial m_material;
	struct Uniforms;
	irr_ptr<Uniforms> m_uniforms;
	video::IVideoDriver *m_driver = nullptr;
};

} // namespace planetarium
