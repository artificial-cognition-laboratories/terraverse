// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/reach.h"
#include "physics/skeleton.h"
#include "physics/world.h"
#include <vector>

/*
	A figure's body (sprint 105): its bones as jointed parts in the physics
	world, going where the pose asks as far as what's in the way lets them. How
	stiff it holds itself goes from following the pose to limp, so a fall and a
	ragdoll are the same body as a walk. What's drawn is what the parts did.
*/
namespace physics {

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
	// Hands taking hold (world), or let go: each reaches, then holds where it got to.
	void hold(const std::optional<v3f> (&hands)[2], v3f over);
	bool holding(int side) const;
	void launch(v3f by) { m_world.launchLimbs(m_id, by); }
	// Where the parts got to. Call after the step: keeps this and the last, to draw between.
	void read();
	// As the figure at this foot and yaw draws them, `t` (0..1) between the last two steps.
	Bones drawn(const v3d &foot, float yaw, float t) const;
	float guided() const { return m_world.limbsGuided(m_id); }
	Felt felt() const; // where its hands and feet are now
	std::vector<v3f> parts() const; // each part's middle, now
	v3d hips() const;
	float upright() const; // its chest's up against the world's up: 1 stood, 0 lying // where they are now, in the world
	// How deep the deepest limb's middle or end is in its chest, hips or head (nodes): it should keep out.
	float inItself() const;

private:
	World &m_world;
	World::LimbsId m_id;
	Bones m_asked;
	std::vector<Pose> m_was, m_now;
};

} // namespace physics
