// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_ptr.h"
#include "src/shared/base/irrlichttypes_bloated.h"
#include "library/space/milkyway/milkyway.h"
#include <CMeshBuffer.h>
#include <SMaterial.h>
#include <S3DVertex.h>
#include <chrono>
#include <memory>
#include <vector>

class IShaderSource;
namespace scene { class IMeshBuffer; }
namespace video { class ITexture; class IVideoDriver; }

namespace planetarium {

/*
	What's behind everything: a galaxy seen from inside (milkyway::), one
	picture of the whole sky, made on workers once and kept on disk, so a
	frame only pays to draw one textured sphere. The stars
	over it are dots a set number of pixels across, placed by a shader, so
	they stay round where a wide view would stretch them. Visual only: not
	part of any world's determinism.
*/
class Galaxy
{
public:
	Galaxy(IShaderSource *ssrc, scene::IMeshBuffer *sphere);
	~Galaxy();
	struct Frame
	{
		float aspect, fov; // the view's: width / height, radians (vertical)
		u32 height;        // pixels
		float level;       // how much of the sky shows at all (the page's)
		float open;        // how adjusted to the dark the eye is (Eye), 1 wide open
		float art;         // drawn to be pixel art (Pixels): the band alone, its contrast up; 0 not
	};
	// The view already set (turned, not moved: the sky's at infinity).
	void draw(video::IVideoDriver *driver, const Frame &frame);

private:
	struct SkyUniforms;
	struct StarUniforms;
	irr_ptr<SkyUniforms> m_skyUniforms;
	irr_ptr<StarUniforms> m_starUniforms;
	scene::IMeshBuffer *m_sphere;
	video::SMaterial m_skyMaterial, m_starMaterial;
	// On the GPU, shared by every Galaxy alive: a new world's view (each edit
	// on the create screen) shouldn't stall a frame uploading the same sky.
	struct Shared;
	std::shared_ptr<Shared> m_shared;
};

} // namespace planetarium
