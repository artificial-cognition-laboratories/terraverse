// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/sky.h"
#include "planetarium/camera.h"
#include "planetarium/planetarium.h"
#include "client/atlas_view.h"
#include "client/camera.h"
#include "client/clientmap.h"
#include "constants.h"
#include "terragen/worlds.h"
#include <ICameraSceneNode.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <cmath>

namespace planetarium {

static SystemLayer::Opts layerFor(const planet::Atlas &atlas, const far::Horizon &horizon, int body)
{
	auto system = std::make_shared<const terragen::System>(
			terragen::SystemSpec::forWorld(terragen::WorldCode::decode(horizon.code)));
	const size_t focus = systemIndex(atlas, horizon, *system, body);
	return { system, focus, atlas.body(body).params().radius / system->radius(focus), false };
}

Sky::Sky(scene::ISceneManager *smgr, const planet::Atlas &atlas, const far::Horizon &horizon, int body) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1),
	m_system(layerFor(atlas, horizon, body)),
	m_year(terragen::WorldCode::decode(horizon.code).year)
{
	setAutomaticCulling(scene::EAC_OFF); // all round the camera
}

// a direction in the scene (the eye's grid) on the planet, unit
static OrbitCamera::Vec onPlanet(const planet::Atlas::Frame &f, const v3f &d)
{
	OrbitCamera::Vec v{ 0, 0, 0 };
	const float c[3] = { d.X, d.Y, d.Z };
	for (int i = 0; i < 3; i++) {
		v.x += f.axes[i].x * c[i];
		v.y += f.axes[i].y * c[i];
		v.z += f.axes[i].z * c[i];
	}
	const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return { v.x / l, v.y / l, v.z / l };
}

void Sky::update(const Camera &camera, ClientMap &map, double worldDays)
{
	m_eye.reset();
	const AtlasView *view = map.atlasView();
	const std::optional<planet::Atlas::Frame> &frame = view ? view->eye() : std::nullopt;
	if (!frame)
		return;
	const double day = m_system.system().dayFor(worldDays, m_year);
	const double spin = m_system.spin(day, worldDays - std::floor(worldDays));
	// the camera's look and up in the planet's frame (its ground's), then
	// in space as the planet has turned
	scene::ICameraSceneNode *node = camera.getCameraNode();
	const v3f ahead = node->getTarget() - node->getAbsolutePosition();
	const OrbitCamera::Vec a = OrbitCamera::turn(onPlanet(*frame, ahead), spin);
	const OrbitCamera::Vec u = OrbitCamera::turn(onPlanet(*frame, node->getUpVector()), spin);
	const OrbitCamera::Vec at = OrbitCamera::turn({ frame->origin.x, frame->origin.y, frame->origin.z }, spin);
	core::matrix4 look;
	look.buildCameraLookAtMatrixLH(v3f(0), v3f((f32)a.x, (f32)a.y, (f32)a.z), v3f((f32)u.x, (f32)u.y, (f32)u.z));
	m_eye = Eye{ { at.x, at.y, at.z }, look, camera.getFovY(), day };
}

void Sky::OnRegisterSceneNode()
{
	if (IsVisible && m_eye)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SKY_BOX);
	ISceneNode::OnRegisterSceneNode();
}

void Sky::render()
{
	scene::ICameraSceneNode *camera = SceneManager->getActiveCamera();
	if (!m_eye || !camera || SceneManager->getSceneNodeRenderPass() != scene::ESNRP_SKY_BOX)
		return;
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	const core::matrix4 projection = driver->getTransform(video::ETS_PROJECTION);
	const core::matrix4 view = driver->getTransform(video::ETS_VIEW);
	const core::dimension2du screen = driver->getCurrentRenderTargetSize();
	const float aspect = (float)screen.Width / std::max(1u, screen.Height);
	// the planet itself is the far view's, drawn next
	m_system.draw(driver, SystemLayer::Camera{ m_eye->at, m_eye->view, m_eye->fov, aspect, v2f(), screen.Height },
			m_eye->day, [](float, float) {});
	// what follows draws in the camera's own depth
	driver->clearBuffers(video::ECBF_DEPTH);
	driver->setTransform(video::ETS_PROJECTION, projection);
	driver->setTransform(video::ETS_VIEW, view);
}

} // namespace planetarium
