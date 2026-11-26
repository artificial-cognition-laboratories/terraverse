// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/physics/skeleton.h"
#include <IEventReceiver.h>
#include <rect.h>

namespace video { class IVideoDriver; }

/*
	A figure standing in a box of the window, as the page leaves room for it (the inventory's well):
	posed as the game poses one standing still, so it breathes and settles as yours does, on its own
	night, and turned round by dragging across it.
*/
class FigureView
{
public:
	// Where it draws (window pixels).
	void place(core::recti rect) { m_rect = rect; }
	void show() { m_shown = true; }
	void hide();
	bool shown() const { return m_shown; }

	void draw(video::IVideoDriver *driver);
	// The mouse the page left: a drag across it turns it. Whether it was ours.
	bool handle(const SEvent &event);

private:
	physics::Bones posed();

	physics::Posing m_posing;
	core::recti m_rect;
	bool m_shown = false;
	float m_yaw = 0.45f; // the camera's way round it (radians): a little to its left, so it reads as solid
	bool m_dragging = false;
	s32 m_mouse_x = 0;
	u64 m_last = 0; // ms, when it was last posed
};
