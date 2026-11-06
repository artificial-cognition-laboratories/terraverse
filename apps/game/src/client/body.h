// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/terrain.h"
#include "irr_v3d.h"
#include "orbit/field.h"
#include "physics/mover.h"
#include "physics/pose.h"
#include "physics/reach.h"
#include "physics/walk.h"
#include "physics/world.h"
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class Client;
namespace planet { class Atlas; }
class LocalPlayer;
struct CollisionInfo;

/*
	The player's body (sprint 58): this client's own physics. The local
	player is a character in a physics::World of this client's, walking by
	physics::walk (inertia, a run that builds, a jump that's a height),
	standing on and riding what moves. Machines near it are in that world
	too, as the server has them (kinematic: they go where they're drawn), so
	the player meets them, stands on them and is carried. What the player
	carries is this client's to move till they let go: pulled toward their
	hands here as the server pulls it there, and drawn where it is here,
	so it's never a step behind the view.

	Gravity is the planet's field (sprint 59, orbit::Field): toward its
	centre, weaker with height, in the axes of the grid the player's in;
	its air drags. A velocity in the air is carried from place to place as
	the same motion, so a fast enough throw falls around the planet.

	Thrusting (sprint 59): flying, the aux1 key (Ctrl) hands the player to
	the body to fly by Newton: gravity and drag act, and the controls push
	(forward and back along the look, sideways, up and down along the
	grid's), harder the longer they're held; the brake pushes against the
	motion. Landing ends it and puts them on their feet.

	Luanti's movement still does what this doesn't yet: flying (until
	thrust), noclip, swimming and climbing. The body takes over again from
	wherever that left the player.
*/
class Body
{
public:
	explicit Body(Client *client);
	~Body();

	// Whether it moves the player this step (else Luanti's movement does);
	// it starts thrusting here.
	bool moves(LocalPlayer &p);
	bool thrusting() const { return m_thrusting; }
	// A line for the debug text: what the body's doing and how fast (world controls, View).
	std::string readout() const;
	// Flying well above the land: in the open sky, whatever the blocks here say of their light.
	bool inOpenSky() const { return m_clear_of_land; }
	// The near world this flight asks for, of `wanted` nodes.
	float nearRange(float wanted) const;
	// How it holds itself now: what was asked, unless a ceiling won't let it up.
	physics::Stance stance() const { return m_mover ? m_mover->stance() : physics::Stance::STAND; }
	// A slide or roll has finished since last asked: a tapped crouch is used up by it.
	// New presses, not holds: Space takes hold of what's in reach, Ctrl lets go.
	void pressJump() { if (m_mover) m_mover->pressJump(); }
	void pressCrouch() { if (m_mover) m_mover->pressCrouch(); }
	// On a wall: how far its hips are turned from a view at this yaw (radians, Luanti's).
	std::optional<float> wallFace(float yaw) const { return m_mover ? m_mover->wallFace(yaw) : std::nullopt; }
	// Running on a wall: how far to lean out from it (radians, + left of a view at this yaw).
	float wallLean(float yaw) const { return m_mover ? m_mover->wallLean(yaw) : 0; }
	// Its hands on an edge, or going for one in reach as it flies (world, nodes).
	std::optional<physics::Grip> grip() const { return m_mover ? m_mover->grip() : std::nullopt; }
	// The rest of what the figure shows (physics::Mover).
	int feetUsed() const { return m_mover ? m_mover->feetUsed() : -1; }
	float thrownSide() const { return m_mover ? m_mover->thrownSide() : 0; }
	float thrownBack() const { return m_mover ? m_mover->thrownBack() : 0; }
	float stagger() const { return m_mover ? m_mover->stagger() : 0; }
	float fallenAngle() const { return m_mover ? m_mover->fallenAngle() : 0; }
	float risingAt() const { return m_mover ? m_mover->risingAt() : 0; }
	// Off its feet, the way it's going: a landing too bad to take, and later a blow.
	void knockDown(float severity, v3f velocity) { if (m_mover && !m_thrusting) m_mover->knockDown(severity, velocity); }
	std::optional<v3f> stepOn() const { return m_mover ? m_mover->stepOn() : std::nullopt; }
	v3f kickPush() const { return m_mover ? m_mover->kickPush() : v3f(0); }
	v3f climbSpeed() const { return m_mover ? m_mover->climbSpeed() : v3f(0); }
	bool takeMoveEnded() { return m_mover && m_mover->takeMoveEnded(); }
	// The brake held (thrusting: push against the motion).
	void brake(bool held) { m_brake = held; }
	// The orbit the player's on (none till it's walked, or outside every grid).
	std::optional<orbit::Elements> orbit() const;
	// The path it coasts on from here (orbit::Field::path): points in its
	// body's frame (m from the centre); none as orbit().
	std::optional<std::vector<orbit::Vec>> path(int points) const;
	// The player moved by `dtime`: walked by their controls; landing told
	// as a collision (for fall damage).
	void move(LocalPlayer &p, float dtime, std::vector<CollisionInfo> *collisions);
	// Each frame something's carried: if move() didn't step the world
	// this frame (they fly, swim or climb by Luanti's movement), the
	// objects are stepped without them, so what's carried still follows.
	void stepObjects(LocalPlayer &p, float dtime);

	// Knocked (nodes/s added to how it's moving).
	void nudge(v3f by);
	// A map block of this client's changed: its ground's built again.
	void groundChanged(v3s16 block);

	// Carrying (an object's id): pulled toward `position` (BS) turned so
	// (degrees, as the server sets them) as hard as `strength` (N).
	void carry(u16 object, float strength);
	void pull(u16 object, v3f position, v3f rotation);
	void letGo(u16 object);
	// Where what's carried is now (BS, degrees); nothing if it isn't here.
	std::optional<std::pair<v3f, v3f>> carried(u16 object) const;

	// Grabbing (sprint 68): an object's point (BS, in its frame) pulled
	// toward `hand` (BS) as hard as `strength` (N), here as the server
	// does there. What's grabbed is moved here, drawn where it is here, and
	// brought toward where the server says it is a little each frame (put
	// there if they're far apart): the hand's felt at once, the server's
	// word still holds.
	void grab(u16 object, v3f point, float strength);
	void grabTo(u16 object, v3f hand);
	void release(u16 object);
	// Where what's grabbed or driven is now (BS, degrees); nothing if it isn't here.
	std::optional<std::pair<v3f, v3f>> grabbed(u16 object) const;
	// What's been let go and still rolls here (drawn as grabbed() has it).
	std::vector<u16> coasting() const;
	// Where the grip on what's grabbed is now (BS): on the part it's on,
	// turned with it; nothing if it isn't here.
	std::optional<v3f> gripAt(u16 object) const;
	// The object the player stands on, if they stand on one.
	std::optional<u16> standingOn() const;

	// Driving (sprint 69): sat in an object's seat, facing its front
	// (sprint 73); false if it has no seat here. Sat, the player is where
	// the seat is, the machine is moved here by the driver's hands and feet
	// as the server moves it there, and brought toward the server's word as
	// what's grabbed is.
	bool hasSeat(u16 object) const;
	bool sit(u16 object);
	void steer(u16 object, const physics::Drive &drive);
	void getUp(u16 object);
	std::optional<u16> driving() const { return m_driving; }
	// The wheels of what's driven here on the ground (sprint 84: they press
	// loose ground, predicted here); none if nothing's driven.
	std::vector<physics::World::Contact> drivenWheels() const;

private:
	float landBelow(const v3f &at, const v3f &velocity); // nodes, the least of here and a second on
	std::map<int, std::unique_ptr<far::Generator>> m_lands; // the land under a flight, by body
	std::future<float> m_sky_answer;
	float m_above_land = 0;
	int m_tier = 0; // physics::FLIGHT_TIERS, from 1
	float m_sky_check = 0;
	bool m_clear_of_land = false;
	struct Mirror
	{
		physics::World::Id id;
		size_t text; // the machine's text's hash: changed, it's rebuilt
		std::optional<v3f> seat; // its driver's (nodes, its frame)
		v3f facing{ 0, 0, 1 };   // the seat's front (its frame)
	};

	// false until the world's bodies are known (its atlas came, its code not yet)
	bool ensureWorld();
	void mirror(LocalPlayer &p);
	// The pull where it is now, as mods scale it (physics_override.gravity).
	physics::Here here(const v3d &foot, v3f velocity) const;
	// Thrusting's velocity after a step: pull, drag, and the controls' push.
	v3f thrust(LocalPlayer &p, v3f velocity, const physics::Here &h, float dt);

	Client *m_client;
	const planet::Atlas *m_atlas = nullptr; // the field's: a new atlas, a new world
	std::unique_ptr<orbit::Field> m_field;
	bool solidAt(v3s16 p) const;
	physics::Field m_pull; // m_field's, as the physics asks it
	float m_scale = 1; // mods' gravity override
	std::unique_ptr<physics::World> m_world;
	bool m_thrusting = false, m_brake = false;
	float m_held = 0; // s the thrust's been held: it grows
	std::unique_ptr<physics::Mover> m_mover; // the player as a character, once walked
	LocalPlayer *m_player = nullptr; // this move()'s: a seam turns its view
	double m_behind = 0;   // s not yet stepped
	bool m_was_place = false; // right click held last step: each press is a kick
	v3f m_set{ 0 };        // where it last put the player (BS): moved since, they were put elsewhere
	std::map<u16, Mirror> m_mirrors;
	std::map<u16, size_t> m_unweighed; // machines it can't weigh (its text's hash): said once
	std::optional<std::pair<u16, float>> m_carrying;
	std::optional<u16> m_grabbing;
	// let go and still rolling: predicted here, corrected toward the server,
	// till it's been still a while (when it went still, ms; 0: moving)
	std::map<u16, u64> m_coasting;
	u64 m_parted_said = 0; // when the client's and server's poses parting was last logged (ms)
	std::optional<u16> m_driving;
	std::optional<f32> m_ride_yaw; // the seat's facing last frame (radians, as the view's yaw)
	// Sat: the machine moved, the player put in its seat.
	void ride(LocalPlayer &p, float dtime);
	// What the player's walked into (held here where the server has
	// them): sent for the server to push, every so often (sprint 61)
	void push(float dtime);
	std::map<u16, v3f> m_pushed;
	float m_push_age = 0;
	bool m_moved = false; // move() stepped the world since stepObjects() last looked
	double m_idle = 0;    // s not yet stepped without them
};
