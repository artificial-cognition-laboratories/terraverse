// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/limbs.h"
#include "physics/mover.h"
#include "physics/skeleton.h"
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/*
	A body playing keys on a few blocks: its own physics world, the game's
	Mover and the game's pose, and a list of what's pressed when. No player and
	no map, so the wiki's Movement scenes and their test see exactly what a
	player would do with those keys there.
*/
namespace physics {

class Actor
{
public:
	// From this moment, these controls.
	struct Cue
	{
		float at = 0;       // s
		float ahead = 0, right = 0;
		float yaw = 0;      // radians, Luanti's: 0 goes +z, + turns toward -x
		bool run = false, jump = false, crouch = false, prone = false;
	};
	// A node's boxes, about its centre (nodes). None: it's air.
	using Boxes = std::function<void(v3s16, std::vector<aabb3f> &)>;
	Actor(Boxes boxes, const v3d &foot, std::vector<Cue> cues);
	~Actor();

	void update(float dt);
	// These controls from now, whatever's cued (a scene that plays by where it is, not when).
	void press(const Cue &cue);
	// How many times it's done that, by name, as did() has them.
	int times(const std::string &what) const;
	double time() const { return m_time; }
	v3f at() const { return m_drawn; }   // its feet (nodes)
	float yaw() const { return m_yaw; }
	const Bones &bones() const { return m_bones; }
	// How far its deepest bone corner is inside something solid, as drawn now (nodes).
	float sunk() const;
	// The fastest the body went since last asked, and any part against it (nodes/s): a jump's a pop.
	float takeRush() { const float r = m_rush; m_rush = 0; return r; }
	float takePop() { const float p = m_pop; m_pop = 0; return p; }
	// How fast a foot that's down slides along the ground, on average (nodes/s): a planted foot shouldn't.
	float skate() const { return m_planted_steps ? m_skate / (m_planted_steps * (float)STEP) : 0; }
	// The sharpest each part (Limbs' parts) changed how it was going, over how the whole body did (nodes/s²):
	// a jerk's a step far past what a person's does. And its head's, striding, against a person's at that pace
	// (Strides::headShake): past 1, it's shaking more than one does.
	struct Shake
	{
		float most = 0;
		double at = 0;
		std::string doing;
	};
	const std::vector<Shake> &shakes() const { return m_shakes; }
	const Shake &headShake() const { return m_head_shake; }
	int holdingSteps() const { return m_holding_steps; }
	int gaveUp() const { return m_mover->gaveUp(); }
	float inItself() const { return physics::inItself(m_parts); }
	v3f head() const; // its head's place against its feet, as drawn
	Stance stance() const { return m_mover->stance(); }
	float pace() const;
	// Everything it's done so far: moves by name, and stances.
	const std::vector<std::string> &did() const { return m_did; }
	// What it's doing now, in words.
	const std::string &doing() const { return m_doing; }

private:
	bool solid(v3s16 n) const;
	void note(const std::string &what);
	void pose(float dt, float turned);
	void shake(const std::vector<Pose> &parts, const v3d &foot);
	void trace(const Felt &felt);
	float groundAt(v3f at) const;
	v3f clearOf(v3f at, float r) const;

	Boxes m_boxes;
	std::vector<Cue> m_cues;
	size_t m_cue = 0;
	std::unique_ptr<World> m_world;
	std::unique_ptr<Mover> m_mover;
	std::unique_ptr<Limbs> m_limbs; // only while it's down
	Posing m_posing;
	Bones m_posed, m_was_posed; // what the pose has it doing, this step and the last
	std::vector<Pose> m_parts, m_parts_was; // where its parts are now, in the world, and a step ago
	v3d m_foot_at[2];                       // where the mover had it a step ago, and two
	int m_steps_seen = 0;
	std::vector<Shake> m_shakes;
	Shake m_head_shake;
	float m_head_now = 0;
	std::unique_ptr<std::ofstream> m_trace;
	float m_rush = 0, m_pop = 0;
	bool m_foot_was_down[2]{};
	v3f m_foot_was[2];
	float m_skate = 0;
	int m_planted_steps = 0;
	int m_holding_steps = 0;
	Bones m_bones;
	Cue m_on;
	double m_time = 0, m_behind = 0;
	v3f m_drawn;
	float m_yaw = 0;
	std::vector<std::string> m_did;
	std::map<std::string, int> m_times;
	std::string m_doing;
};

const char *stanceName(Stance stance);

} // namespace physics
