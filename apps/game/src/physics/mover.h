// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/pose.h"
#include "physics/reach.h"
#include "physics/walk.h"
#include "physics/world.h"
#include <functional>
#include <optional>
#include <string>

/*
	How a body gets about: its stances, climbs, wall runs, kicks and falls, one
	physics step at a time, from what's asked of it and which nodes are solid.
	Knows nothing of a player or a map, so the wiki's scenes and tests run the
	very moves client/body plays. Flying by thrust and what crosses a seam are
	the caller's.
*/
namespace physics {

class Mover
{
public:
	struct Opts
	{
		World &world;
		Solid solid;  // what a hand or wall can meet
		Solid floor;  // what a crouched body won't walk off
		std::function<Here(const v3d &, v3f)> here;
		// a velocity at one place, as the same motion at another (a planet's grid lines curve)
		std::function<v3f(const v3d &from, const v3d &to, v3f velocity)> carry;
		// over a seam since the last step: where it is now (the caller's placed it). May be empty.
		std::function<std::optional<v3d>(const v3d &was, const v3d &now, const v3d &measured)> crossed;
		std::function<void(const std::string &)> say; // what it's just done, a line. May be empty.
	};
	struct Input
	{
		float ahead = 0, right = 0; // the controls, -1..1
		float yaw = 0;              // radians, Luanti's
		bool jump = false;          // Space held
		Stance asked = Stance::STAND;
		float walk = 4;             // nodes/s
		float run = 1;              // × walking
		float jump_height = 1.25f;  // nodes
		bool thrusting = false;
		std::function<v3f(v3f, const Here &, float)> thrust; // needed if thrusting
	};
	struct Stepped
	{
		v3d foot;
		v3f velocity;
		bool on_ground = false;
		bool landed = false, rolled = false; // came down this step; a roll took the fall
		v3f fell_at;
		bool stopped_flying = false;
	};

	Mover(Opts opts, float radius, float height, const v3d &foot);
	World::CharId id() const { return m_me; }
	Stepped step(const Input &in);
	// Put somewhere, moving so.
	void place(const v3d &foot, v3f velocity);
	// Doing anything but going about on its feet.
	bool busy() const { return m_climb || m_hanging || m_wall_run || m_push || m_down; }
	void onItsFeet();
	bool standUp(); // false: no room
	void knockDown(float severity, v3f velocity);
	v3d was() const { return m_was; }
	v3d now() const { return m_now; }
	float lift() const { return m_lift; }

	Stance stance() const { return m_stance; }
	void pressJump() { m_jump_press = JUMP_MEMORY; m_jump_used = false; }
	void pressCrouch() { m_crouch_press = true; }
	std::optional<float> wallFace(float yaw) const;
	float wallLean(float yaw) const;
	std::optional<Grip> grip() const;
	int feetUsed() const { return m_push ? m_push->nearSide() : m_kick_left > 0 ? m_kick_feet : -1; }
	float thrownSide() const;
	float thrownBack() const;
	float stagger() const { return m_stagger; }
	// Down, its body lying where it went: the capsule keeps under it.
	void bodyAt(const v3d &hips) { m_body_at = hips; }
	// Where its limbs got to: a climb waits on them.
	void feel(const Felt &felt) { m_felt = felt; }
	int gaveUp() const { return m_gave_up; } // contacts a climb went on without
	float fallenAngle() const { return m_down ? m_down->fallen.angle() : 0; }
	float risingAt() const { return m_down ? m_down->fallen.riseAt() : 0; }
	std::optional<v3f> stepOn() const
	{
		if (m_push)
			return m_push->footOn();
		return m_climb && m_climb->haul ? m_climb->haul->stepOn() : std::nullopt;
	}
	v3f kickPush() const { return m_kicked && m_kick_left > 0 ? *m_kicked * -(m_kick_left / KICK_SHOWN) : v3f(0); }
	v3f climbSpeed() const { return v3f::from(m_now - m_was) / (float)STEP; }
	bool takeMoveEnded() { const bool e = m_move_ended; m_move_ended = false; return e; }

private:
	struct Down
	{
		Fallen fallen;
		v3f way;  // along the ground: the way its head goes
		bool face_down;
		v3f hands; // on the ground, where they went down
	};
	struct Climbing
	{
		Climb kind;
		Ledge ledge;
		ClimbPath path;
		float t = 0;
		v3f exit{ 0 };
		std::optional<Haul> haul; // everything but a vault
		bool dive = false;
		float side = 0; // a side vault: turned this far from square (radians)
	};
	struct WallRun
	{
		v3f normal;
		v3f along;      // nodes/s
		float up;       // nodes/s
		float left;     // s
	};
	static constexpr float GRAB_MEMORY = 0.35f; // s, and as long as an edge stays ahead
	static constexpr float JUMP_MEMORY = 0.2f; // s a press is remembered: slightly early still counts
	static constexpr float KICK_SHOWN = 0.35f; // s the kicking leg takes to come away

	void say(const std::string &what) const;
	v3f wallStep(const World::CharacterState &was, const Here &h, float jump);
	void airWall(const World::CharacterState &was, v3f velocity, float jump);
	void handsAt(const Ledge &ledge, v3f &right, v3f &left) const;
	v3f keepOnEdge(const Ledge &ledge, v3f hand) const;
	// Takes hold if it's asked and something's in reach.
	void startClimb(const World::CharacterState &was, v3f velocity, float yaw);
	// A step on a wall: where it goes, and the velocity it's left with.
	v3f climbStep(const Input &in, const v3d &foot);
	// Up or down to what the controls ask and how it's moving, if it fits.
	void holdStance(const World::CharacterState &now, bool thrusting);
	// Crouched: what's left of `next` keeping it from walking off the edge it stands on.
	v3f keepOnEdge(const v3d &foot, v3f next) const;

	Opts m_opts;
	World::CharId m_me;
	float m_radius, m_standing_height;
	Stance m_stance = Stance::STAND;
	Stance m_asked = Stance::STAND; // what the controls ask, this frame
	float m_roll_left = 0;    // s
	float m_keep = 1;         // × its speed, next step: a landing or a step it paid for
	float m_stagger = 0;      // off balance, 0..1
	float m_lift = 0;         // nodes: gone up a step in one physics step, drawn below that, catching up
	std::optional<Down> m_down;
	std::optional<v3d> m_body_at;
	std::optional<Felt> m_felt;
	int m_gave_up = 0;
	float m_trip_left = 0;    // s: tripped, scrambling
	v3f m_trip_at, m_trip_way; // where its hands went down, and which way it was going
	bool m_move_ended = false;
	bool m_rising = false;    // jumped, Space still down: letting go now cuts it short
	bool m_said_stuck = false; // a refused stance is logged once
	bool m_grab_wait = false;
	float m_jump_press = 0;
	bool m_crouch_press = false;
	bool m_jump_used = false; // Space went to a climb: it isn't a jump as well until let go
	float m_base = 0;         // y it left the ground at
	std::optional<WallRun> m_wall_run;
	v3f m_look;  // flat, where the view goes
	std::optional<WallPush> m_push; // kicking off a wall
	float m_kick_left = 0;
	int m_kick_feet = -1;
	std::optional<v3f> m_kicked;  // the wall last kicked off: not the same one twice before the ground
	std::optional<Climbing> m_climb;
	std::optional<Ledge> m_hanging;
	std::optional<Ledge> m_in_reach; // in the air: what Space would take hold of
	HandOverHand m_hands;
	float m_late_roll = 0;    // s: landed hard, a crouch now still rolls
	v3d m_was, m_now;         // its foot at the last two steps: drawn between
	v3d m_measured;           // where its velocity was last given: carried from there
};

} // namespace physics
