// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mover.h"
#include "tuning.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace physics {

namespace {

constexpr float MASS = 75, PUSH = 600, SLOPE = 50;
// A person to the air: a drag coefficient times the area they show (m²).
constexpr float DRAG_AREA = 0.7f;

} // namespace

Mover::Mover(Opts opts, float radius, float height, const v3d &foot) :
	m_opts(std::move(opts)), m_radius(radius), m_standing_height(height)
{
	m_me = m_opts.world.addCharacter({ radius, height, MASS, PUSH, SLOPE }, foot);
	m_now = m_was = m_measured = foot;
}

void Mover::place(const v3d &foot, v3f velocity)
{
	m_opts.world.place(m_me, foot, velocity);
	m_now = m_was = m_measured = foot;
}

bool Mover::standUp()
{
	if (m_stance == Stance::STAND || !m_opts.world.resize(m_me, m_standing_height))
		return m_stance == Stance::STAND;
	m_stance = Stance::STAND;
	return true;
}

void Mover::say(const std::string &what) const
{
	if (m_opts.say)
		m_opts.say(what);
}

void Mover::onItsFeet()
{
	m_climb.reset();
	m_hanging.reset();
	m_wall_run.reset();
	m_push.reset();
	m_down.reset();
	m_move_ended = true;
	m_stance = m_opts.world.resize(m_me, m_standing_height) ? Stance::STAND : Stance::CROUCH;
}

std::optional<float> Mover::wallFace(float yaw) const
{
	if (m_down) { // the way it went over: face down its head's going on, on its back it's facing where it came from
		const v3f n = m_down->face_down ? m_down->way * -1.f : m_down->way;
		float to = yaw - std::atan2(n.X, -n.Z);
		to = std::fmod(to + 3 * core::PI, 2 * core::PI) - core::PI;
		return to;
	}
	if (m_push) { // the whole body turned to the wall it's pushing off
		const v3f n = m_push->facing() * -1.f;
		float to = yaw - std::atan2(n.X, -n.Z);
		to = std::fmod(to + 3 * core::PI, 2 * core::PI) - core::PI;
		return to;
	}
	const std::optional<Ledge> &l = m_climb ? std::optional<Ledge>(m_climb->ledge) : m_hanging;
	if (!l)
		return std::nullopt;
	const v3f out = l->inward() * -1.f;
	float a = yaw - std::atan2(out.X, -out.Z);
	a = std::fmod(a + 3 * core::PI, 2 * core::PI) - core::PI;
	return m_climb ? a + m_climb->side : a;
}

float Mover::wallLean(float yaw) const
{
	if (!m_wall_run)
		return 0;
	// the view's right, in the world: the wall's out being to the right, it leans right
	const v3f right(std::cos(yaw), 0, std::sin(yaw));
	return -0.4f * m_wall_run->normal.dotProduct(right);
}

void Mover::handsAt(const Ledge &ledge, v3f &right, v3f &left) const
{
	// shoulder width along the edge, wrists just over it. Facing the wall, +x is the side
	// that's right of someone looking in along -normal.
	constexpr float APART = 0.3f, ABOVE = 0.06f; // (outside the shoulders, clear of the head)
	const v3f along(-ledge.normal.Z, 0, ledge.normal.X);
	const v3f at = ledge.lip + v3f(0, ABOVE, 0);
	right = keepOnEdge(ledge, at + along * APART);
	left = keepOnEdge(ledge, at - along * APART);
}

v3f Mover::keepOnEdge(const Ledge &ledge, v3f hand) const
{
	// in along the edge from there, till there's edge under it and a little beyond
	constexpr float MARGIN = 0.08f, STEP_IN = 0.04f;
	const Solid &solid = m_opts.solid;
	const v3f lip_y(hand.X, ledge.lip.Y, hand.Z);
	v3f in = ledge.lip - lip_y;
	in.Y = 0;
	const float far = in.getLength();
	if (far < 1e-3f)
		return hand;
	in /= far;
	for (float d = 0; d < far; d += STEP_IN)
		if (ledgeGoesOn(solid, lip_y + in * (d - MARGIN), ledge.normal))
			return hand + in * d;
	return v3f(ledge.lip.X, hand.Y, ledge.lip.Z);
}

std::optional<Grip> Mover::grip() const
{
	if (m_down) {
		if (!m_down->fallen.handsDown())
			return std::nullopt;
		Grip g;
		const v3f across(-m_down->way.Z, 0, m_down->way.X);
		g.held[0] = g.held[1] = true;
		g.wrist[0] = m_down->hands + across * 0.3f;
		g.wrist[1] = m_down->hands - across * 0.3f;
		g.over = m_down->way;
		return g;
	}
	if (m_trip_left > TRIP_TIME - 0.3f) { // hands down on what it went over
		Grip g;
		const v3f across(-m_trip_way.Z, 0, m_trip_way.X);
		g.held[0] = g.held[1] = true;
		g.wrist[0] = m_trip_at + across * 0.25f + v3f(0, 0.06f, 0);
		g.wrist[1] = m_trip_at - across * 0.25f + v3f(0, 0.06f, 0);
		g.over = m_trip_way;
		return g;
	}
	if (m_push) {
		if (!m_push->handsOn())
			return std::nullopt;
		Grip g;
		const v3f along(-m_push->normal().Z, 0, m_push->normal().X);
		const int near = m_push->nearSide();
		g.held[0] = near != 1;
		g.held[1] = near != 0;
		g.wrist[0] = m_push->handsAt() + (near == 2 ? along * 0.3f : m_push->facing() * 0.2f);
		g.wrist[1] = m_push->handsAt() + (near == 2 ? along * -0.3f : m_push->facing() * 0.2f);
		g.over = v3f(0, 1, 0);
		return g;
	}
	const Ledge *ledge = m_hanging ? &*m_hanging : m_climb ? &m_climb->ledge : m_in_reach ? &*m_in_reach : nullptr;
	if (!ledge)
		return std::nullopt;
	Grip g;
	g.over = ledge->inward();
	g.held[0] = g.held[1] = true;
	if (m_hanging) {
		g.wrist[0] = m_hands.at(0);
		g.wrist[1] = m_hands.at(1);
		g.moving = m_hands.moving();
		return g;
	}
	handsAt(*ledge, g.wrist[0], g.wrist[1]);
	if (ledge->in.getLengthSQ() > 0) { // on top, either side of the corner it's going over
		const v3f across(-ledge->in.Z, 0, ledge->in.X);
		const v3f at = ledge->lip + ledge->in * 0.25f + v3f(0, 0.06f, 0);
		g.wrist[0] = at + across * -0.25f;
		g.wrist[1] = at + across * 0.25f;
		if (across.dotProduct(v3f(ledge->in.Z, 0, -ledge->in.X)) > 0)
			std::swap(g.wrist[0], g.wrist[1]);
	}
	if (m_climb && m_climb->haul) {
		// its hands stay till it's rising past them, then they're done
		if (!m_climb->haul->holding())
			return std::nullopt;
	} else if (m_climb) {
		const float p = m_climb->t / m_climb->path.time;
		// thrown round them, the hands stay till it's past; otherwise each comes off once out of arm's reach
		const bool pivots = m_climb->side != 0 || m_climb->dive;
		if (p >= (pivots ? 0.8f : 0.65f))
			return std::nullopt;
		const v3f shoulder = v3f::from(m_now) + v3f(0, 1.2f, 0);
		for (const int side : { 0, 1 }) {
			g.wrist[side] += g.over * 0.15f;
			if (!pivots && (g.wrist[side] - shoulder).getLength() > 0.75f && p > 0.3f)
				g.held[side] = false;
		}
		// a side vault goes past the hand on the wall's side: its hips turned right, that's its left
		if (m_climb->side != 0)
			g.held[m_climb->side > 0 ? 0 : 1] = false;
	}
	return g;
}

float Mover::thrownSide() const
{
	if (!m_climb || m_climb->side == 0 || m_climb->haul)
		return 0;
	// the wall's to its left when turned right: the hand plants there and the body goes round it, feet up and
	// away from where it's going first, then over
	constexpr float THROW = 0.9f;
	return (m_climb->side > 0 ? THROW : -THROW) * std::sin(core::PI * std::min(m_climb->t / m_climb->path.time, 1.f));
}

float Mover::thrownBack() const
{
	if (!m_climb || !m_climb->dive || m_climb->haul)
		return 0;
	// head first, hips up behind it, over the hands
	constexpr float THROW = 1.1f;
	return THROW * std::sin(core::PI * std::min(m_climb->t / m_climb->path.time, 1.f));
}

void Mover::knockDown(float severity, v3f velocity)
{
	if (m_down)
		return;
	const v3f across(velocity.X, 0, velocity.Z);
	const float pace = across.getLength();
	const v3f way = pace > 0.5f ? across / pace : m_look;
	// going the way it faced it goes on over its front; carried backward it goes down on its back
	const bool face_down = way.dotProduct(m_look) >= -0.1f;
	const v3f foot = v3f::from(m_now);
	m_climb.reset();
	m_hanging.reset();
	m_wall_run.reset();
	m_push.reset();
	m_lift = 0;
	m_trip_left = 0;
	m_down = Down{ Fallen(severity, face_down), way, face_down, foot + way * 0.45f + v3f(0, 0.06f, 0), foot };
	m_stance = Stance::DOWN;
	m_opts.world.resize(m_me, stanceInfo(m_stance).height);
	say(std::string("down at ") + std::to_string(pace) + " nodes/s, " + (face_down ? "on its front" : "on its back"));
}

void Mover::airWall(const World::CharacterState &was, v3f velocity, float jump)
{
	const Solid &solid = m_opts.solid;
	const std::optional<v3f> wall = findWall(solid, v3f::from(was.foot), m_radius);
	if (!wall || (m_kicked && m_kicked->dotProduct(*wall) > 0.5f))
		return;
	m_jump_press = 0;
	m_jump_used = true;
	m_kicked = *wall;
	if (runsAlong(velocity, *wall)) {
		const v3f across(velocity.X, 0, velocity.Z);
		m_wall_run = WallRun{ *wall, across - *wall * across.dotProduct(*wall), tuning().wall_run_lift, tuning().wall_run_time };
		m_stance = Stance::WALL_RUN;
		m_opts.world.resize(m_me, stanceInfo(m_stance).height);
	} else {
		m_push = WallPush(*wall, velocity, jump, wallFaceAt(v3f::from(was.foot), *wall, m_radius), m_look);
		say("wall kick");
	}
}

v3f Mover::wallStep(const World::CharacterState &was, const Here &h, float jump)
{
	WallRun &run = *m_wall_run;
	const float dt = (float)STEP;
	const Solid &solid = m_opts.solid;
	const std::optional<v3f> wall = findWall(solid, v3f::from(was.foot), m_radius);
	run.left -= dt;
	run.up += h.gravity.Y * tuning().wall_run_gravity * dt;
	const v3f velocity = run.along + v3f(0, run.up, 0);
	if (m_jump_press > 0) { // off it, and on
		m_jump_press = 0;
		m_jump_used = true;
		const v3f normal = run.normal; // (the run's over with the next line)
		onItsFeet();
		m_push = WallPush(normal, velocity, jump, wallFaceAt(v3f::from(was.foot), normal, m_radius), m_look);
		say("wall kick");
		return velocity;
	}
	if (m_crouch_press || run.left <= 0 || was.on_ground || !wall || wall->dotProduct(run.normal) < 0.5f) {
		m_crouch_press = false;
		onItsFeet();
		return velocity;
	}
	// held to it, not pressed into it
	return velocity - run.normal * 0.5f;
}

void Mover::startClimb(const World::CharacterState &was, v3f velocity, float yaw)
{
	const Solid &solid = m_opts.solid;
	const v3f foot = v3f::from(was.foot);
	std::optional<Ledge> ledge = findLedge(solid, foot, yaw, was.on_ground ? VAULT_REACH : GRAB_REACH);
	if (ledge)
		ledge = overCorner(solid, *ledge, velocity);
	std::optional<Move> move = ledge ? moveFor(*ledge, m_base, foot.Y, velocity) : std::nullopt;
	if (was.on_ground) {
		// from the ground, only a vault, and only on the way to it: stood beside a wall,
		// Space is a jump. The press is spent either way: the next is what grabs.
		const v3f across(velocity.X, 0, velocity.Z);
		const bool heading = ledge && across.dotProduct(ledge->normal * -1.f) > 1;
		if (!move || move->climb != Climb::VAULT || !heading)
			move = std::nullopt;
		m_jump_press = 0;
	}
	if (!move)
		return;
	m_jump_press = 0;
	m_jump_used = true;
	const v3f across(velocity.X, 0, velocity.Z);
	const float pace = across.getLength();
	Climbing c{ move->climb, *ledge, {} };
	if (move->climb == Climb::VAULT)
		c.path = climbPath(*move, *ledge, foot, m_radius, pace);
	else
		c.haul = Haul(*ledge, foot, velocity, m_radius, move->climb == Climb::MANTLE, m_base);
	const v3f in = ledge->inward();
	c.dive = move->dive;
	if (move->climb == Climb::VAULT) {
		// it goes on as it came in: the way it was going, not turned square to the wall
		c.exit = pace > 2 ? across * VAULT_KEEPS : in * 2.f;
		const v3f along = across - in * across.dotProduct(in);
		const v3f drifted = c.path.to + along * c.path.time;
		const auto clear = [&](float up) {
			return !m_opts.solid(v3s16(std::floor(drifted.X + 0.5f), std::floor(drifted.Y + up + 0.5f), std::floor(drifted.Z + 0.5f)));
		};
		if (clear(0.5f) && clear(1.5f))
			c.path.to = drifted;
		if (move->side) // turned toward the hand it goes past
			c.side = (across.X * in.Z - across.Z * in.X) > 0 ? 1.2f : -1.2f;
	}
	say(std::string(move->name) + " at " + std::to_string(pace) + " nodes/s");
	// (caught first, whichever it goes on to)
	m_stance = move->climb == Climb::VAULT ? Stance::VAULT : Stance::HANG;
	m_opts.world.resize(m_me, stanceInfo(m_stance).height);
	m_climb = c;
}

v3f Mover::climbStep(const Input &in, const v3d &foot)
{
	const float dt = (float)STEP;
	if (m_climb) {
		m_climb->t += dt;
		if (m_climb->haul) {
			Haul &haul = *m_climb->haul;
			if (m_felt)
				haul.feel(*m_felt);
			const bool had_given_up = haul.gaveUp();
			haul.step(dt);
			m_gave_up += haul.gaveUp() && !had_given_up;
			if (m_stance == Stance::HANG && haul.phase() != Haul::Phase::CATCH && haul.phase() != Haul::Phase::DONE) {
				m_stance = Stance::MANTLE;
				m_opts.world.resize(m_me, stanceInfo(m_stance).height);
			}
			const v3f out = haul.done() ? haul.exit() : v3f(0);
			if (haul.done() && m_climb->kind == Climb::HANG) {
				m_opts.world.place(m_me, v3d::from(haul.at()), v3f(0));
				m_hanging = m_climb->ledge;
				v3f right, left;
				handsAt(*m_hanging, right, left);
				m_hands.place(right, left);
				m_climb.reset();
				return v3f(0);
			}
			m_opts.world.place(m_me, v3d::from(haul.at()), out);
			if (haul.done())
				onItsFeet();
			return out;
		}
		const bool done = m_climb->t >= m_climb->path.time;
		const v3f at = m_climb->path.at(m_climb->t);
		const v3f exit = done ? m_climb->exit : v3f(0);
		m_opts.world.place(m_me, v3d::from(at), exit);
		if (done) {
			onItsFeet();
		}
		return exit;
	}
	const Ledge ledge = *m_hanging;
	// Ctrl or S lets go
	const bool backing = in.ahead < -0.5f;
	if (m_crouch_press || backing) {
		m_crouch_press = false;
		onItsFeet();
		return v3f(0);
	}
	if (m_jump_press > 0) {
		m_jump_press = 0;
		m_jump_used = true;
		// turned away from the wall: off it, the way it's looking, not up it
		if (m_look.dotProduct(ledge.normal) > 0.3f) {
			const v3f face = wallFaceAt(v3f::from(foot), ledge.normal, m_radius);
			const float up = std::sqrt(2 * std::max(0.f, -m_opts.here(foot, v3f(0)).gravity.Y) * in.jump_height);
			onItsFeet();
			m_push = WallPush(ledge.normal, v3f(0), up, face, m_look);
			return v3f(0);
		}
		const v3s16 top(std::floor(ledge.lip.X - ledge.normal.X * 0.25f + 0.5f), std::floor(ledge.lip.Y + 1), std::floor(ledge.lip.Z - ledge.normal.Z * 0.25f + 0.5f));
		if (!m_opts.solid(top) && !m_opts.solid(top + v3s16(0, 1, 0))) {
			m_stance = Stance::MANTLE;
			m_opts.world.resize(m_me, stanceInfo(m_stance).height);
			Climbing c{ Climb::MANTLE, ledge, {} };
			c.haul = Haul::pullUp(ledge, v3f::from(foot), m_radius);
			m_hanging.reset();
			m_climb = c;
			return v3f(0);
		}
	}
	// along the edge: A and D as the body faces it, wherever the view has gone. As far as there's edge for its hands
	constexpr float HANDS_OUT = 0.42f;
	const float right = in.right;
	const v3f along(-ledge.normal.Z, 0, ledge.normal.X);
	// It's the arms that move it: pulled along on both hands, hardly going with one in the air.
	const float pull = m_hands.moving() >= 0 ? 0.5f : 1.3f;
	const v3f moved = ledge.lip + along * (right * tuning().shimmy * pull * dt);
	const v3f lookout = moved + along * (right > 0 ? HANDS_OUT : -HANDS_OUT);
	const Solid &solid = m_opts.solid;
	if (ledgeGoesOn(solid, lookout, ledge.normal))
		m_hanging->lip = moved;
	const v3f hung(m_hanging->lip.X + ledge.normal.X * (m_radius + 0.05f), ledge.lip.Y - HANG_DROP,
			m_hanging->lip.Z + ledge.normal.Z * (m_radius + 0.05f));
	m_opts.world.place(m_me, v3d::from(hung), v3f(0));
	v3f hand_right, hand_left;
	handsAt(*m_hanging, hand_right, hand_left);
	const Ledge held = *m_hanging;
	m_hands.step(dt, hand_right, hand_left, [&](v3f to) { return keepOnEdge(held, to); });
	return v3f(0);
}

void Mover::holdStance(const World::CharacterState &now, bool thrusting)
{
	if (thrusting) { // flying: on its feet
		if (m_stance != Stance::STAND && m_opts.world.resize(m_me, m_standing_height))
			m_stance = Stance::STAND;
		return;
	}
	const float pace = v3f(now.velocity.X - now.ground_velocity.X, 0, now.velocity.Z - now.ground_velocity.Z).getLength();
	const Stance next = nextStance(m_stance, m_asked, pace, now.on_ground, m_roll_left);
	if (next == m_stance)
		return;
	// tallest first that's no taller than asked, so a low roof stops at a crouch
	for (const Stance s : { next, Stance::CROUCH, Stance::PRONE }) {
		if (s != next && stanceInfo(s).height <= stanceInfo(m_stance).height)
			continue;
		const float h = stanceInfo(s).height;
		if (m_opts.world.resize(m_me, h > 0 ? h : m_standing_height)) {
			if (m_stance == Stance::ROLL) // (a slide ends crouched, till asked otherwise)
				m_move_ended = true;
			m_stance = s;
			m_said_stuck = false;
			return;
		}
	}
	// (hunting a crouch that won't undo: says whether the capsule's what refuses)
	if (!m_said_stuck) {
		std::ostringstream os;
		os << "stance " << (int)m_stance << " can't become " << (int)next << " at " << now.foot.X << "," << now.foot.Y << "," << now.foot.Z << ", on ground " << now.on_ground;
		say(os.str());
	}
	m_said_stuck = true;
}

v3f Mover::keepOnEdge(const v3d &foot, v3f next) const
{
	constexpr float STEP_LOOK = 0.25f, DEEPEST = 0.7f; // nodes: ahead of the body, and a drop it'd not take
	const auto floorAt = [&](double x, double z) {
		for (const double y : { foot.Y - 0.1, foot.Y - DEEPEST }) {
			const v3s16 n(std::floor(x + 0.5), std::floor(y + 0.5), std::floor(z + 0.5));
			if (m_opts.floor(n))
				return true;
		}
		return false;
	};
	// each way on its own, so it can still walk along the edge
	if (next.X != 0 && !floorAt(foot.X + (next.X > 0 ? STEP_LOOK : -STEP_LOOK), foot.Z))
		next.X = 0;
	if (next.Z != 0 && !floorAt(foot.X, foot.Z + (next.Z > 0 ? STEP_LOOK : -STEP_LOOK)))
		next.Z = 0;
	return next;
}

Mover::Stepped Mover::step(const Input &in)
{
	Stepped out;
	bool thrusting = in.thrusting;
	m_asked = in.asked;
	const World::CharacterState was = m_opts.world.character(m_me);
	m_roll_left = std::max(0.f, m_roll_left - (float)STEP);
	m_kick_left = std::max(0.f, m_kick_left - (float)STEP);
	// (pressed during a vault or a climb, it's the next move: the press waits for it)
	if (!m_climb)
		m_jump_press = std::max(0.f, m_jump_press - (float)STEP);
	if (was.on_ground)
		m_base = (float)was.foot.Y;
	else
		m_base = std::min(m_base, (float)was.foot.Y);
	if (!in.jump)
		m_jump_used = false;
	if (was.on_ground)
		m_kicked.reset();
	const bool climbing = m_climb || m_hanging || m_wall_run || m_push || m_down;
	m_late_roll = std::max(0.f, m_late_roll - (float)STEP);
	const Stance stance_was = m_stance;
	if (m_late_roll > 0 && m_asked != Stance::STAND && m_stance == Stance::STAND && was.on_ground) {
		m_stance = Stance::ROLL;
		m_roll_left = ROLL_TIME;
		m_late_roll = 0;
		m_opts.world.resize(m_me, stanceInfo(Stance::ROLL).height);
		say("rolled late");
	}
	if (!climbing)
		holdStance(was, thrusting);
	const StanceInfo &held = stanceInfo(m_stance);
	const bool low = m_stance != Stance::STAND;
	const bool edged = m_stance == Stance::CROUCH || m_stance == Stance::PRONE;

	Gait gait;
	gait.walk = in.walk * held.speed;
	gait.run = low ? 1 : in.run;
	gait.flow = in.run > 1 ? 1.1f : 1; // only a run builds (to a sprint about 6 nodes/s: a person's, sprint 105)
	gait.jump = in.jump_height;
	const Intent intent{ in.ahead,
		in.right, in.yaw, (in.jump || m_jump_press > 0) && held.jumps && !m_jump_used && m_trip_left <= 0 };
	// in the air, its motion carried to where it is now (the grid's axes turn)
	const v3f v = was.on_ground ? was.velocity : m_opts.carry(m_measured, was.foot, was.velocity);
	m_measured = was.foot;
	const Here h = m_opts.here(was.foot, v);
	m_in_reach.reset();
	m_look = v3f(-std::sin(in.yaw), 0, std::cos(in.yaw));
	const float wall_jump = std::sqrt(2 * std::max(0.f, -h.gravity.Y) * in.jump_height);
	const Solid &solid = m_opts.solid;
	if (!thrusting && !climbing && m_stance == Stance::STAND && !was.on_ground) {
		// the hands go for what's in reach before it's asked: that's how it's known it can be
		// (from further than it can take hold: going for it is what shows it can be had)
		const std::optional<Ledge> ledge = findLedge(solid, v3f::from(was.foot), in.yaw, VAULT_REACH);
		if (ledge && moveFor(*ledge, m_base, (float)was.foot.Y, v))
			m_in_reach = ledge;
		// Space (or still held from the jump) going along a wall runs on it, ledge or no:
		// what's being passed isn't what it's trying to climb
		if (m_jump_press > 0 || in.jump) {
			const std::optional<v3f> wall = findWall(solid, v3f::from(was.foot), m_radius);
			if (wall && runsAlong(v, *wall) && (!m_kicked || m_kicked->dotProduct(*wall) < 0.5f))
				airWall(was, v, wall_jump);
		}
	}
	if (was.on_ground && m_grab_wait) { // never got there: not a jump on landing
		m_jump_press = 0;
		m_grab_wait = false;
	}
	const bool free_to_climb = !thrusting && !climbing && !m_wall_run && m_stance == Stance::STAND;
	// on the way to an edge, in reach or nearly, or about to be as it rises: Space is for that, not for bouncing off
	const auto edge_coming = [&] {
		const float yaw = in.yaw;
		const v3f foot = v3f::from(was.foot);
		for (const float up : { 0.f, v.Y > 0 ? 0.6f : 0.f }) {
			const v3f at = foot + v3f(0, up, 0);
			const std::optional<Ledge> l = findLedge(solid, at, yaw, VAULT_REACH);
			if (l && moveFor(*l, m_base, at.Y, v) && (v.dotProduct(l->normal * -1.f) > 0.5f || m_look.dotProduct(l->normal * -1.f) > 0.5f))
				return true;
		}
		return false;
	};
	if (free_to_climb && m_jump_press > 0) {
		startClimb(was, v, in.yaw);
		// nothing to take hold of, in the air: a wall to run on or kick off
		if (m_jump_press > 0 && !was.on_ground) {
			if (edge_coming()) {
				m_jump_press = GRAB_MEMORY;
				m_grab_wait = true;
			} else {
				airWall(was, v, wall_jump);
			}
		}
	} else if (free_to_climb && !was.on_ground && in.jump && !m_jump_used) {
		// Space held from the jump takes hold of what it comes to, heading for it. (A hop, let go, doesn't.)
		const std::optional<Ledge> l = findLedge(solid, v3f::from(was.foot), in.yaw);
		if (l && v.dotProduct(l->normal * -1.f) > 0.5f)
			startClimb(was, v, in.yaw);
	}
	v3f next;
	if (m_down) {
		// sliding to a stop where it went down. A direction, or Space, gets it up sooner
		constexpr float FRICTION = 7, UP_AND_OFF = 3;
		const float ahead = in.ahead, right = in.right;
		const float yaw_now = in.yaw;
		const v3f asked(right * std::cos(yaw_now) - ahead * std::sin(yaw_now), 0, right * std::sin(yaw_now) + ahead * std::cos(yaw_now));
		const bool wants_up = asked.getLengthSQ() > 0.01f || in.jump;
		m_down->fallen.step((float)STEP, wants_up);
		// till it pushes up, its hands go under its shoulders as they lie: that's where they push from
		if (m_felt && !m_down->fallen.rising())
			m_down->hands = v3f(m_felt->shoulders.X, (float)was.foot.Y + 0.06f, m_felt->shoulders.Z) + m_down->way * 0.1f;
		const float pace = std::hypot(v.X, v.Z);
		const float kept = pace > 0.01f ? std::max(0.f, pace - FRICTION * (float)STEP) / pace : 0;
		next = v3f(v.X * kept, was.on_ground ? 0 : v.Y + h.gravity.Y * (float)STEP, v.Z * kept);
		if (m_body_at) {
			// under its body, along the way it went over, as far as a body slides falling: what it sprawls past that
			// sideways is the body's, and comes back as it gets up
			constexpr float KEEPS_UNDER = 8, GOES = 4; // (at a run it slides a good way, as a body does)
			const v3f way = m_down->way;
			const v3f body = v3f::from(*m_body_at);
			const float along = std::clamp((body - m_down->from).dotProduct(way), 0.f, GOES);
			const v3f off = m_down->from + way * along - v3f::from(was.foot);
			next.X = off.X * KEEPS_UNDER;
			next.Z = off.Z * KEEPS_UNDER;
		}
		if (m_down->fallen.done()) {
			onItsFeet();
			// off in the way it's asked, not from nothing
			next += asked * UP_AND_OFF;
			m_jump_used = in.jump;
		}
	} else if (m_push) {
		next = m_push->step((float)STEP);
		if (m_push->done()) {
			m_kick_feet = m_push->nearSide();
			m_push.reset();
			m_kick_left = KICK_SHOWN;
		} else if (was.on_ground) {
			m_push.reset(); // came down on something first
		}
	} else if (m_wall_run) {
		next = wallStep(was, h, wall_jump);
	} else if (m_climb || m_hanging) {
		next = climbStep(in, was.foot);
	} else if (thrusting) {
		next = in.thrust(v, h, (float)STEP);
	} else {
		gait.gravity = h.gravity;
		const Footing footing{ v, was.on_ground, was.ground_velocity, was.ground_normal };
		if (m_stance == Stance::SLIDE)
			next = slide(gait, intent, footing, (float)STEP);
		else if (m_stance == Stance::ROLL)
			next = roll(gait, intent, footing, (float)STEP);
		else
			next = walk(gait, intent, footing, (float)STEP);
		// let go of Space on the way up and it's a hop: the jump's made on the press, so nothing waits
		if (was.on_ground && intent.jump && next.Y > 0)
			m_rising = true;
		else if (m_rising && !was.on_ground && !in.jump) {
			if (next.Y > 0)
				next.Y *= HOP;
			m_rising = false;
		} else if (was.on_ground)
			m_rising = false;
		if (m_stance == Stance::SLIDE && stance_was != Stance::SLIDE) {
			const float pace = std::hypot(next.X, next.Z);
			if (pace > 0.01f && pace < SLIDE_KICK) {
				next.X *= SLIDE_KICK / pace;
				next.Z *= SLIDE_KICK / pace;
			}
		}
		if (m_keep < 1) { // what a bad landing or a caught shin cost
			next.X *= m_keep;
			next.Z *= m_keep;
			m_keep = 1;
		}
		if (edged && was.on_ground && !was.standing_on && !intent.jump)
			next = keepOnEdge(was.foot, next);
		if (!was.on_ground) // falling: the air holds it back
			next -= v * (0.5f * h.air * v.getLength() * DRAG_AREA / MASS * (float)STEP);
	}
		if (!was.on_ground) {
		// it goes by its velocity as it is halfway there (the grid's
		// lines curve round the planet), and it's measured there
		m_measured = was.foot + v3d::from(next) * (STEP / 2);
		next = m_opts.carry(was.foot, m_measured, next);
	}
	m_opts.world.want(m_me, next);
	m_opts.world.holdsStill(m_me, m_climb.has_value() || m_hanging.has_value());
	m_opts.world.step(STEP);
	World::CharacterState now = m_opts.world.character(m_me);
	// Up a step in one physics step (the capsule's way): it isn't drawn there at once. It's drawn where
	// it was, catching up, and how far and how fast it was going decides what that cost.
	{
		constexpr float SHOWS_FROM = 0.12f, CATCHES_UP = 5, SCRAMBLES_UP = 1.5f, RECOVERS = 2;
		const float s = (float)STEP;
		const float rise = (float)now.foot.Y - ((float)was.foot.Y + next.Y * s);
		if (!climbing && !m_climb && !m_hanging && !m_wall_run && !thrusting && !now.standing_on &&
				(was.on_ground || now.on_ground) && rise >= SHOWS_FROM && rise <= 1) {
			const v3f across(was.velocity.X, 0, was.velocity.Z);
			const float pace = across.getLength();
			m_lift += rise;
			if (trips(rise, pace) && m_trip_left <= 0) {
				m_trip_left = TRIP_TIME;
				m_stagger = 1;
				m_keep = 0.45f;
				m_trip_way = across / pace;
				m_trip_at = v3f::from(now.foot) + m_trip_way * 0.5f;
				say("tripped at " + std::to_string(pace) + " nodes/s, " + std::to_string(rise) + " up");
			} else {
				m_stagger = std::max(m_stagger, rise);
				m_keep = std::min(m_keep, 1 - rise * 0.4f);
			}
		}
		m_lift = std::max(0.f, m_lift - (m_trip_left > 0 ? SCRAMBLES_UP : CATCHES_UP) * s);
		m_trip_left = std::max(0.f, m_trip_left - s);
		m_stagger = std::max(0.f, m_stagger - RECOVERS * s);
	}
	m_was = m_now;
	m_now = now.foot;
	if (m_opts.crossed) if (const std::optional<v3d> there = m_opts.crossed(was.foot, now.foot, m_measured)) {
		m_was = m_now = m_measured = *there;
		now = m_opts.world.character(m_me);
	}
	if (!was.on_ground && now.on_ground && !climbing && !m_climb && !m_hanging && !m_wall_run && !m_push) {
		if (thrusting) {
			// landed: on their feet, flying off (as a double-tap would)
			thrusting = false;
			out.stopped_flying = true;
		}
		const float fall = -was.velocity.Y;
		const bool rolled = !thrusting && rollsOut(fall, m_asked);
		if (rolled) {
			// it rolls whether or not the small capsule fits this instant (coming down,
			// it can be pressed into the ground); holdStance sets it right after
			m_opts.world.resize(m_me, stanceInfo(Stance::ROLL).height);
			m_stance = Stance::ROLL;
			m_roll_left = ROLL_TIME;
		} else if (fall >= tuning().hard_landing) {
			m_late_roll = tuning().roll_late;
		}
		if (!rolled && !thrusting) {
			// its speed goes as far as its balance did, and it shows till it's back
			const float yaw_now = in.yaw;
			const float ahead = in.ahead, right = in.right;
			const v3f asked = right != 0 || ahead != 0 ?
					v3f(right * std::cos(yaw_now) - ahead * std::sin(yaw_now), 0, right * std::sin(yaw_now) + ahead * std::cos(yaw_now)) : m_look;
			const float off = fall >= tuning().hard_landing ? 1 : landingStagger(fall, was.velocity, asked);
			if (off >= tuning().falls_from && (landingStagger(fall, was.velocity, asked) >= tuning().falls_from || fall >= tuning().falls_at))
				knockDown(std::clamp((fall - 4) / 12, 0.f, 1.f), was.velocity);
			if (off > 0.15f) {
				m_stagger = std::max(m_stagger, off);
				m_keep = std::min(m_keep, 1 - (1 - STUMBLE) * off);
			}
		}
		out.landed = true;
		out.rolled = rolled;
		out.fell_at = was.velocity;
	}
	m_crouch_press = false;
	const World::CharacterState end = m_opts.world.character(m_me);
	out.foot = end.foot;
	out.velocity = end.velocity;
	out.on_ground = end.on_ground;
	return out;
}

} // namespace physics
