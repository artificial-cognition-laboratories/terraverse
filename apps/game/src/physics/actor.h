// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/limbs.h"
#include "physics/mover.h"
#include "physics/skeleton.h"
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
	int gaveUp() const { return m_mover->gaveUp(); }
	float inItself() const { return m_limbs->inItself(); }
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
	void poseAndDrive(float dt, float turned);
	float groundAt(v3f at) const;
	v3f clearOf(v3f at, float r) const;

	Boxes m_boxes;
	std::vector<Cue> m_cues;
	size_t m_cue = 0;
	std::unique_ptr<World> m_world;
	std::unique_ptr<Mover> m_mover;
	std::unique_ptr<Limbs> m_limbs;
	float m_stiff = 1;
	float m_rush = 0, m_pop = 0;
	Posing m_posing;
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
