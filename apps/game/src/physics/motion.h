// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include <optional>


/*
	Strides from people (sprint 105): recorded walks, jogs, runs and sprints, ahead, back, to each side and
	crouched (100STYLE's and CMU's, one averaged stride each, physics/motion_cycles.h), in place of the pose's
	own. Each bone of ours is pointed as the recorded one points, so it doesn't matter that the two skeletons
	stand differently at rest. Played at the body's own pace: the stride's lengthened to what it's covering,
	and what's left is how quickly it steps.

	Only on its feet, going, and leaping from a run: everything else is the pose's.
*/
namespace physics {

struct Bones;
namespace cycles {
struct Clip;
}

class Strides
{
public:
	// `asked`: the pose's own answer. `on`: on its feet, on the ground, nothing in its hands. What it should be now.
	// `moving`: the body's velocity in the figure's frame (which way it goes against how it faces picks the
	// strides: ahead, to a side, back), `turned`: how far it turned right this step (radians), so a foot that's
	// down stays where it was put while the body goes on over it. `crouched`: its crouched strides. Off the
	// ground (`airborne`, rising at `up` nodes/s, `above` nodes over the ground) from a run, it leaps.
	Bones step(float dt, const Bones &asked, v3f moving, float turned, bool on, bool crouched, bool airborne = false, float up = 0,
			float above = 0);
	bool leaping() const { return m_leaping; }
	void drop() { m_share = 0; } // lets go at once
	static constexpr float LEAPS_FROM = 2.5f; // nodes/s: from slower, a jump's from standing (Moves)
	float phase() const { return m_phase; }
	float share() const { return m_share; } // how much of what's drawn is this, 0..1
	bool planted(int side) const { return m_planted[side]; } // its foot's down, as recorded, now
	bool turnedRound() const { return m_turned_round; } // this step it went from one way to another at once

	static constexpr float LONGEST = 2; // × the recorded stride at most: past that it steps quicker

	// The most a person's head accelerates going as it last stepped, over how the body's going (nodes/s²): its
	// bob and sway as recorded, played as quickly as the stride is. What's drawn shouldn't shake it more.
	float headShake() const;

private:
	float m_phase = 0;
	float m_share = 0;
	bool m_planted[2]{};
	bool m_locked[2]{};
	float m_held[2]{}; // how much it's still where it was put: 1 down, going to 0 as it comes up
	float m_height = 0; // where its hips are, less their bob: what its legs reach to the ground from
	bool m_height_known = false;
	float m_pace = 0, m_way = 0; // as last stepped: nodes/s; radians against how it faces, + to its right
	float m_body = 0;            // radians its hips are turned toward where it goes
	bool m_turned_round = false;
	bool m_leaping = false;
	float m_leap_air = 0, m_leap_from = 0, m_leap_to = 0; // s up; its phase as it left the ground, and as it'll land
	void leap();
	bool m_crouched = false;
	v3f m_lock[2]; // where a foot that's down was put, in the figure's frame as it's moved since
};

/*
	Moves played once (sprint 105): a standing jump as a person jumps, cut from a take of one (physics/motion_clips.h),
	played as the mover jumps it. Its lead-in from just before it leaves the ground; its flight fitted to however
	long it's really in the air, from how high it is and how fast it's rising, so it comes down as it lands; and
	its landing as recorded, from when it's down. Its hips as the recording's against the ground, but for the
	jump's own rise: that's the mover's.

	And punches and kicks, as a person throws them: each played once, through to where it lands (its hit) and
	back. One thrown before the last's done is next: it follows from partway back from the last's hit, not
	cutting it off. A punch going about is its upper body's only, its legs going on as they are, whenever it
	starts or stops going; stood still, and a kick always, it's the whole of it.
*/
enum class Strike { PUNCH, KICK };
// Where a strike landed, as it landed: which, with which side (0 right, 1 left), and the fist or foot's place in
// the figure's frame (nodes).
struct Hit
{
	Strike strike;
	int side;
	v3f at;
};

class Moves
{
	enum class Part { NONE, LEAD, FLIGHT, LANDING, STRIKE };

public:
	// It's just left the ground in a jump from standing, or near it (from a run it's a leap: Strides).
	void jump();
	bool stop(); // whatever's playing, it lets go of at once: something else has the body. Whether it was playing
	// Throws one: after the one that's being thrown, if there is one.
	void strike(Strike which);
	// A kick thrown at a run: off the ground, as a jump's played, it lands in the air (the mover hops it).
	void flyingKick();
	bool playing() const { return m_part != Part::NONE; }
	std::optional<Hit> takeHit() { const std::optional<Hit> h = m_hit; m_hit.reset(); return h; } // since last asked
	bool striking() const { return m_part == Part::STRIKE; }
	// What it should be now: `asked` when nothing's playing. `up`: nodes/s it's rising; `above`: nodes over the
	// ground under it; `going`: it's going about, its legs striding (a punch leaves them be).
	Bones step(float dt, const Bones &asked, bool airborne, float up, float above, bool going = false);
	bool followedOn() const { return m_followed; } // this step one strike went into the next (Posing eases it)
	float share() const { return m_share; } // how much of what's drawn is this, 0..1

private:
	const cycles::Clip *m_clip = nullptr;
	Strike m_strike = Strike::PUNCH;
	int m_side = 0, m_next[2]{}; // the side it's striking with; the next side each strike takes
	float m_legs = 1;            // how much its legs are the strike's, not its striding's
	std::optional<Strike> m_queued;
	bool m_followed = false;
	void start(Strike which);
	std::optional<Hit> m_hit;
	bool m_landing = false; // it's landed this step: where's from what's drawn
	bool m_been_up = false; // a jump's been off the ground
	Part m_part = Part::NONE;
	float m_frame = 0, m_air = 0, m_share = 0;
	float m_until = 0; // the frame its landing's played to
};

} // namespace physics
