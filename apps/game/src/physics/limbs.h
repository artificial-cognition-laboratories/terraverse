// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/reach.h"
#include "physics/skeleton.h"
#include "physics/world.h"
#include <optional>
#include <vector>

/*
	A figure's body (sprint 105): its bones as jointed parts in the physics
	world, going where the pose asks as far as what's in the way lets them. How
	stiff it holds itself goes from following the pose to limp. What's drawn is
	what the parts did.

	Only for a body that's down (sprint 105): on its feet or climbing, the pose
	is drawn as it is, and nothing has to hold it up.
*/
namespace physics {

// A pose's parts in the world, its feet at `foot`, facing `yaw`: where Limbs would put them, exactly.
std::vector<Pose> partsOf(const Bones &bones, const v3d &foot, float yaw);
Felt feltOf(const std::vector<Pose> &parts); // where its hands and feet are
// How deep the deepest limb's middle or end is in its chest, hips or head (nodes): it should keep out.
float inItself(const std::vector<Pose> &parts);

class Limbs
{
public:
	// In the world: its feet at `foot`, facing `yaw` (Luanti's), as `bones` has it.
	Limbs(World &world, const Bones &bones, const v3d &foot, float yaw, v3f velocity);
	~Limbs();
	Limbs(const Limbs &) = delete;
	Limbs &operator=(const Limbs &) = delete;

	// One physics step's asking, made before it runs.
	void drive(const Bones &bones, const v3d &foot, float yaw, float stiff, float tone);
	// Where the parts got to. Call after the step: keeps this and the last, to draw between.
	void read();
	// As the figure at this foot and yaw draws them, `t` (0..1) between the last two steps.
	Bones drawn(const v3d &foot, float yaw, float t) const;
	Felt felt() const { return feltOf(m_now); } // where its hands and feet are now
	const std::vector<Pose> &parts() const { return m_now; } // where they are now, in the world

private:
	World &m_world;
	World::LimbsId m_id;
	Bones m_asked;
	std::vector<Pose> m_was, m_now;
};

} // namespace physics
