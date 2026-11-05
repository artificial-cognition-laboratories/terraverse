// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include <functional>
#include <optional>

/*
	Reach (arc-movement, sprint 118): what a body can get hold of from where it
	is. A pure question of which nodes are solid. How high a ledge is above where
	the body left the ground says what to do with it: go over, climb on, or hang.
	Whole nodes for now: a cut edge counts as its node, solid if at least half's there.
*/
namespace physics {

using Solid = std::function<bool(v3s16)>;

struct Ledge
{
	v3f normal;   // out of the wall, along an axis
	v3f lip;      // on the wall's face at its top (x, z); y is the top
	bool thin;    // open just past the wall: over it, there's ground lower down
	bool room;    // a body can stand on top
	float far = 0; // thin: the ground's height just past it (the lip's, if it's further down than a vault goes)
	v3f in{ 0 };   // over a corner: the way across it. Otherwise straight in
	v3f inward() const { return in.getLengthSQ() > 0 ? in : normal * -1.f; }
};
enum class Climb : unsigned char { VAULT, MANTLE, HANG };

constexpr float MOST_REACH = 2.05f;      // nodes: hands above the feet
constexpr float VAULT_TO = 1.3f, MANTLE_TO = 2.3f; // nodes above where it left the ground
constexpr float COMING_DOWN = 0.9f;      // nodes above the feet: higher than this, falling, it can't just go over
constexpr float STEPS_OVER = 1.1f;       // nodes above the feet as they are: waist high by now, it goes over whatever it jumped from
constexpr float HANG_DROP = 1.55f;      // the feet below the lip, hanging: arms all but straight, hands on it (the figure leans in to the wall)
constexpr float SHIMMY = 1.2f;           // nodes/s

// The best edge to hand from a foot, looking along a yaw (Luanti's: radians, 0 is +z): straight
// ahead first, then further round.
constexpr float GRAB_REACH = 0.5f, VAULT_REACH = 1.2f; // nodes from the feet: hands can get there; a run's last stride can
std::optional<Ledge> findLedge(const Solid &solid, v3f foot, float yaw, float reach = GRAB_REACH);
// The same edge further along, if it's still there. `body` is the hanging foot.
bool ledgeGoesOn(const Solid &solid, v3f lip, v3f normal);
// What can be done with this ledge, by how the body is moving: the first row of a table that fits
// (MOVES in reach.cpp). `base`: where it left the ground, or lower if it's fallen since.
struct Move
{
	const char *name;
	Climb climb;
	bool over; // a vault that lands past the wall, not on it
	bool side; // legs swung past one hand
	bool dive = false; // head first over both hands
};
// Coming in on the slant near the end of an edge, it goes over the corner, the way it's going: `ledge` with that way
// set, and to climb onto, not vault across. As it is otherwise.
Ledge overCorner(const Solid &solid, Ledge ledge, v3f velocity);
std::optional<Move> moveFor(const Ledge &ledge, float base, float foot_y, v3f velocity);

// A body going from one place to another by way of over a lip: up, then across.
struct ClimbPath
{
	v3f from, to;
	float top = 0;   // y it clears
	float time = 1;  // s
	bool arc = false; // across at a steady speed, over and down, not up and then across
	v3f at(float t) const; // t in s
};
// `pace`: how fast it's going in (nodes/s). Its hands have to get down and take its weight, so it crosses
// no faster than they can, and what that costs it comes out of what it leaves with.
constexpr float VAULT_PACE = 4.5f, VAULT_KEEPS = 0.85f; // nodes/s; × what it came in with
ClimbPath climbPath(const Move &move, const Ledge &ledge, v3f foot, float radius, float pace = 4);

// Where a body's hands and feet really are (sprint 105): what the moves wait on. Without it they go by the clock.
struct Felt
{
	v3f wrist[2], foot[2];
};

// A body taking hold of an edge, moved by forces rather than along a curve (sprint 123), so how it
// arrives decides how it goes. Caught: its hands stay, and what it arrived with is taken up by its
// arms, sagging under them if it was falling. Pulled: its back and arms throw it up, slow and then
// fast. Let go once it's rising: one leg up on the top, then driving off it.
class Haul
{
public:
	enum class Phase { CATCH, PULL, LEG_UP, DRIVE, DONE };
	// `pull`: it goes on up. Otherwise it's done when it hangs still. `floor`: it doesn't sag through this.
	Haul(const Ledge &ledge, v3f foot, v3f velocity, float radius, bool pull, float floor);
	// From hanging still, straight into the pull.
	static Haul pullUp(const Ledge &ledge, v3f foot, float radius);
	void step(float dt);
	// What its limbs did, for the next step: its hands have to be on before it pulls, a foot on top before it drives.
	void feel(const Felt &felt) { m_felt = felt; }
	bool handsOn() const;
	bool footOn() const;
	// A contact it waited for and never made: it went on without.
	bool gaveUp() const { return m_gave_up; }
	Phase phase() const { return m_phase; }
	v3f at() const { return m_at; }
	v3f velocity() const { return m_velocity; }
	bool done() const { return m_phase == Phase::DONE; }
	// Thrown hard enough, the hands have done their work and leave, part way up (after long enough to have touched).
	bool holding() const
	{
		return m_phase == Phase::CATCH || (m_phase == Phase::PULL && (m_time < 0.12f || m_velocity.Y < 3));
	}
	// Where the leg going up is put: on the top, just past the edge.
	std::optional<v3f> stepOn() const;
	v3f exit() const;
	constexpr static float HARD_CATCH = -7; // nodes/s: falling faster, it hangs, and goes on only when asked

private:
	Ledge m_ledge;
	v3f m_at, m_velocity, m_hung, m_on;
	float m_floor, m_pace, m_time = 0, m_waited = 0;
	bool m_pull, m_gave_up = false;
	std::optional<Felt> m_felt;
	bool waiting(float dt); // for a contact that isn't made: true while it's worth it
	Phase m_phase = Phase::CATCH;
};

// A wall beside or ahead of a body, at its chest: which way is out of it, the nearest if there are several.
std::optional<v3f> findWall(const Solid &solid, v3f foot, float radius);
// Whether going this way past that wall is along it, enough to run on it.
bool runsAlong(v3f velocity, v3f normal);
// Kicking off it: most of what's along the wall kept, thrown out from it and up.
// `look`: flat, where it's looking: it goes that way as far as a wall behind it lets it.
v3f wallKick(v3f velocity, v3f normal, float up, v3f look = v3f(0));
// Kicking off a wall, as a body does it (sprint 124): it goes on in till a foot, and at speed its hands,
// are planted and bent, the harder the faster it came; then they straighten, and the kick builds.
class WallPush
{
public:
	WallPush(v3f normal, v3f velocity, float up, v3f foot, v3f look = v3f(0));
	v3f step(float dt); // its velocity now
	bool done() const { return m_t >= m_press + SPRING; }
	v3f footOn() const { return m_foot_on; }
	bool handsOn() const { return m_t < m_press + SPRING / 2; }
	v3f handsAt() const { return m_hands_at; }
	v3f normal() const { return m_normal; }
	// Square on it's the whole body. Side on, only what's touching: the near foot and hand, and it keeps facing its way.
	float square() const { return m_square; }
	v3f facing() const;
	int nearSide() const; // 0 right, 1 left; 2: both
	static constexpr float BOTH_FROM = 0.65f; // of square: both feet and hands
	float pressTime() const { return m_press; }
	static constexpr float SPRING = 0.09f;
private:
	v3f m_normal, m_along, m_into, m_out, m_foot_on, m_hands_at;
	float m_fall, m_press, m_t = 0, m_square = 1;
};
// Where a wall found by findWall has its face, level with a foot.
v3f wallFaceAt(v3f foot, v3f normal, float radius);
constexpr float WALL_RUN_TIME = 1.2f;     // s at most
constexpr float WALL_RUN_LIFT = 2;        // nodes/s up, as it takes to the wall
constexpr float WALL_RUN_GRAVITY = 0.35f; // × the pull, while it's running on one

} // namespace physics
