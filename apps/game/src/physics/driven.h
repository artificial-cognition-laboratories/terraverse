// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/actor.h"
#include "physics/world.h"
#include <memory>
#include <string>
#include <vector>

/*
	A machine driven by a list of what the driver does when: the game's physics
	world and a compiled assembly on a few blocks, as physics::Actor is a body on
	them. For the wiki's Vehicles scenes and their test.
*/
namespace physics {

class Driven
{
public:
	struct Cue
	{
		float at = 0;
		Drive drive;
		std::string say;
	};
	Driven(Actor::Boxes boxes, std::function<float(const v3d &)> surface, const Assembly &assembly, const v3d &at, std::vector<Cue> cues);
	~Driven();

	void update(float dt);
	double time() const { return m_time; }
	Pose pose() const { return m_world->pose(m_id); }
	// where to draw it: between its last two steps, as a frame rarely lands on a step
	Pose drawnPose() const;
	std::vector<Relative> drawnRelatives() const;
	float pace() const;
	std::vector<World::Contact> contacts() const { return m_world->contacts(m_id); }
	float mass() const { return m_world->mass(m_id); }
	const std::string &doing() const { return m_doing; }

private:
	Actor::Boxes m_boxes;
	std::vector<Cue> m_cues;
	size_t m_cue = 0;
	std::unique_ptr<World> m_world;
	World::Id m_id;
	double m_time = 0, m_behind = 0;
	Pose m_was_pose;
	std::vector<Relative> m_was_relatives;
	std::string m_doing;
};

} // namespace physics
