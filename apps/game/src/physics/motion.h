// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"


/*
	Strides from people (sprint 105): a recorded walk and run (100STYLE's, one averaged stride each,
	physics/motion_cycles.h) as what the body's muscles go for, in place of the pose's own stride. Each bone
	of ours is pointed as the recorded one points, so it doesn't matter that the two skeletons stand
	differently at rest. Played at the body's own pace: the stride's lengthened to what it's covering, and
	what's left is how quickly it steps.

	Only on its feet on the ground, going: everything else is the pose's.
*/
namespace physics {

struct Bones;

class Strides
{
public:
	// `asked`: the pose's own answer. `pace`: nodes/s along the ground. `twist`: the hips toward where it goes
	// (Joints::twist). `on`: on its feet, on the ground, nothing in its hands. What it should be now.
	// `moving`: the body's velocity in the figure's frame, `turned`: how far it turned right this step (radians),
	// so a foot that's down stays where it was put while the body goes on over it.
	Bones step(float dt, const Bones &asked, v3f moving, float turned, float twist, bool on);
	float phase() const { return m_phase; }
	float share() const { return m_share; } // how much of what's drawn is this, 0..1
	bool planted(int side) const { return m_planted[side]; } // its foot's down, as recorded, now

	static constexpr float LONGEST = 2; // × the recorded stride at most: past that it steps quicker
	// The recorded stride at this pace, in nodes, as it's lengthened: what goes by under a foot each stride.
	static float strideAt(float pace);

private:
	float m_phase = 0;
	float m_share = 0;
	bool m_planted[2]{};
	bool m_locked[2]{};
	v3f m_lock[2]; // where a foot that's down was put, in the figure's frame as it's moved since
};

} // namespace physics
