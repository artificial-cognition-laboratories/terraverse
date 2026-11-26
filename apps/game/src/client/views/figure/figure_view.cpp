// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/figure/figure_view.h"
#include "src/client/views/figure/figure_mesh.h"
#include "src/client/views/planetarium/camera.h"
#include "src/shared/platform/system/porting.h"
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace {

constexpr float FOV = 0.5f;     // radians, up and down: long, so it isn't distorted close up
constexpr float PITCH = 0.12f;  // looked at from a little above its middle
constexpr float MIDDLE = 0.9f;  // nodes up from its feet: what the camera turns about
constexpr float TALLEST = 2.2f; // nodes of it kept in view, its height and a margin
constexpr float TURN = 0.012f;  // radians a pixel dragged
// the inventory's well behind it (theme.css: --night-0)
const video::SColor WELL(255, 7, 11, 17);

} // namespace

void FigureView::hide()
{
	m_shown = false;
	m_dragging = false;
	m_last = 0;
}

void FigureView::draw(video::IVideoDriver *driver)
{
	if (!m_shown || m_rect.getArea() <= 0)
		return;
	const physics::Bones bones = posed();
	const core::dimension2du window = driver->getScreenSize();
	driver->draw2DRectangle(WELL, m_rect);
	driver->setViewPort(m_rect);
	driver->clearBuffers(video::ECBF_DEPTH);
	const float aspect = (float)m_rect.getWidth() / std::max(1, m_rect.getHeight());
	// far enough for its height, or in a narrow box its width, to fit
	const float distance = (TALLEST / 2) / std::tan(FOV / 2) * std::max(1.f, 0.55f / aspect);
	const v3f target(0, MIDDLE, 0);
	const v3f eye = target + v3f(std::cos(PITCH) * std::sin(m_yaw), std::sin(PITCH), std::cos(PITCH) * std::cos(m_yaw)) * distance;
	core::matrix4 view;
	view.buildCameraLookAtMatrixLH(eye, target, v3f(0, 1, 0));
	driver->setTransform(video::ETS_PROJECTION, planetarium::OrbitCamera::perspective(aspect, 0.1f, 50.f, FOV));
	driver->setTransform(video::ETS_VIEW, view);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	figure_mesh::draw(driver, figure_mesh::build(bones, core::IdentityMatrix));
	driver->setViewPort(core::recti(0, 0, window.Width, window.Height));
}

bool FigureView::handle(const SEvent &event)
{
	if (!m_shown || event.EventType != EET_MOUSE_INPUT_EVENT)
		return false;
	const s32 x = event.MouseInput.X;
	switch (event.MouseInput.Event) {
	case EMIE_LMOUSE_PRESSED_DOWN:
		if (!m_rect.isPointInside({ x, event.MouseInput.Y }))
			return false;
		m_dragging = true;
		m_mouse_x = x;
		return true;
	case EMIE_MOUSE_MOVED:
		if (!m_dragging)
			return false;
		m_yaw += (x - m_mouse_x) * TURN;
		m_mouse_x = x;
		return true;
	case EMIE_LMOUSE_LEFT_UP:
		if (!m_dragging)
			return false;
		m_dragging = false;
		return true;
	default:
		return false;
	}
}

// Standing still on level ground, nothing near it: the game's own pose for that, moved on by the time since.
physics::Bones FigureView::posed()
{
	const u64 now = porting::getTimeMs();
	const float dt = m_last ? std::clamp((now - m_last) / 1000.f, 0.001f, 0.1f) : 1 / 60.f;
	m_last = now;
	return m_posing.pose(dt, v3f(0), 0, false, physics::Stance::STAND, 0, std::nullopt, 0, physics::Contacts{},
			[](v3f) { return 0.f; }, [](v3f, float) { return v3f(0); });
}
