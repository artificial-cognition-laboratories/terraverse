// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/physics/skeleton.h"
#include <json/json.h>
#include <vector>

namespace headless {

/*
	A body's skeleton, as the body contract has one (axon.body/1's frames, URDF's links and
	joints): the figure the game draws (physics::eachBone), every bone a link with its box. A bone
	turns three ways (Irrlicht's Rz·Ry·Rx), so it hangs from its parent by three joints that each
	turn about one axis, named `<bone>.z`, `<bone>.y` and `<bone>`; `/joints` reports their angles,
	read off the figure as it's posed, so what a mind (or Fleet) draws is the figure exactly.

	The engine's axes (x right, y up, z ahead) become the contract's flu (x ahead, y left, z up).
	A bone's place on its parent is its resting one: a crouch's lowered hips turn, they don't move.
*/
namespace rig {

// The frames: `base` at the feet, the bones, and `eye` on the head.
Json::Value frames(const physics::Bones &rest);
// The `/joints` sense (sensor_msgs/JointState): a component a joint, in frames' order.
Json::Value sense();
// Each joint's angle now, rad, in the sense's order.
std::vector<float> angles(const physics::Bones &now);
// A turn's three angles as the joints `.z`, `.y` and the bone's own take them: what the
// contract's chain turns to give Irrlicht's matrix (tested: test_body_rig.cpp).
v3f jointsOf(v3f turn_degrees);
// Where a point of a bone's (engine axes) is in the contract's.
v3f flu(v3f engine);

} // namespace rig

} // namespace headless
