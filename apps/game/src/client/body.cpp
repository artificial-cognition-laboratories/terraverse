// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "body.h"
#include "physics/tuning.h"
#include "profiler.h"
#include <sstream>

#include "client.h"
#include "collision.h"
#include "content_cao.h"
#include "localplayer.h"
#include "log.h"
#include "porting.h"
#include "map.h"
#include "nodedef.h"
#include "piece/machine.h"
#include "planet/atlas.h"
#include "settings.h"
#include "terragen/planet.h"
#include "terragen/worlds.h"
#include "workshop/motion.h"
#include <cmath>
#include <functional>

namespace {

// How far round the player machines are in its world (nodes).
constexpr float MIRRORED = 48;
// the player's key for what they grab (one world, one grabber)
constexpr unsigned ME = 1;
// The player as a character: as Luanti's box is wide and tall, a person's
// weight, and pushing as a person can.
constexpr float MASS = 75, PUSH = 600, SLOPE = 50;
// Jumping clears a block (nodes), as Luanti's jump did under its doubled gravity.
constexpr float JUMP = 1.25f;
// A person to the air: a drag coefficient times the area they show (m²).
constexpr float DRAG_AREA = 0.7f;
// Thrusting (nodes/s²): from about a g, doubling each few seconds it's
// held, so a hop is gentle and orbit (a hundred-odd m/s) is a short burn.
constexpr float THRUST = 10, DOUBLING = 4, THRUST_MOST = 160;

} // namespace

Body::Body(Client *client) : m_client(client) {}

Body::~Body()
{
	if (m_sky_answer.valid())
		m_sky_answer.wait();
}

bool Body::moves(LocalPlayer &p)
{
	if (m_driving)
		return true; // in their seat, whatever else they were doing
	PlayerSettings &s = p.getPlayerSettings();
	const bool fly = m_client->checkLocalPrivilege("fly") && s.free_move;
	const bool noclip = m_client->checkLocalPrivilege("noclip") && s.noclip;
	// flying, Ctrl thrusts; it lasts till they land or stop flying
	if (!fly || noclip || p.getParent())
		m_thrusting = false;
	else if (p.control.aux1)
		m_thrusting = true;
	if (m_thrusting)
		return true;
	const bool on_wall = m_climb || m_hanging || m_wall_run || m_push || m_down;
	const bool walks = !p.getParent() && !fly && !noclip && !p.in_liquid && !p.is_climbing;
	// Luanti's movement has no stances: on its feet as it's handed over
	if (!walks && on_wall)
		onItsFeet();
	if (!walks && m_me && m_stance != physics::Stance::STAND && m_world->resize(*m_me, m_standing_height))
		m_stance = physics::Stance::STAND;
	return walks;
}

bool Body::solidAt(v3s16 p) const
{
	Map &map = m_client->getEnv().getMap();
	if (const std::optional<shape::Shape> s = map.solidShape(p))
		return s->half();
	const MapNode n = map.getNode(p);
	return n.getContent() == CONTENT_IGNORE || m_client->ndef()->get(n).walkable; // (not loaded: not to be stepped into)
}

void Body::onItsFeet()
{
	m_climb.reset();
	m_hanging.reset();
	m_wall_run.reset();
	m_push.reset();
	m_down.reset();
	m_move_ended = true;
	m_stance = m_world->resize(*m_me, m_standing_height) ? physics::Stance::STAND : physics::Stance::CROUCH;
}

std::optional<float> Body::wallFace(float yaw) const
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
	const std::optional<physics::Ledge> &l = m_climb ? std::optional<physics::Ledge>(m_climb->ledge) : m_hanging;
	if (!l)
		return std::nullopt;
	const v3f out = l->inward() * -1.f;
	float a = yaw - std::atan2(out.X, -out.Z);
	a = std::fmod(a + 3 * core::PI, 2 * core::PI) - core::PI;
	return m_climb ? a + m_climb->side : a;
}

float Body::wallLean(float yaw) const
{
	if (!m_wall_run)
		return 0;
	// the view's right, in the world: the wall's out being to the right, it leans right
	const v3f right(std::cos(yaw), 0, std::sin(yaw));
	return -0.4f * m_wall_run->normal.dotProduct(right);
}

void Body::handsAt(const physics::Ledge &ledge, v3f &right, v3f &left) const
{
	// shoulder width along the edge, wrists just over it. Facing the wall, +x is the side
	// that's right of someone looking in along -normal.
	constexpr float APART = 0.34f, ABOVE = 0.06f; // (outside the shoulders, clear of the head)
	const v3f along(-ledge.normal.Z, 0, ledge.normal.X);
	const v3f at = ledge.lip + v3f(0, ABOVE, 0);
	right = keepOnEdge(ledge, at + along * APART);
	left = keepOnEdge(ledge, at - along * APART);
}

v3f Body::keepOnEdge(const physics::Ledge &ledge, v3f hand) const
{
	// in along the edge from there, till there's edge under it and a little beyond
	constexpr float MARGIN = 0.08f, STEP_IN = 0.04f;
	const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
	const v3f lip_y(hand.X, ledge.lip.Y, hand.Z);
	v3f in = ledge.lip - lip_y;
	in.Y = 0;
	const float far = in.getLength();
	if (far < 1e-3f)
		return hand;
	in /= far;
	for (float d = 0; d < far; d += STEP_IN)
		if (physics::ledgeGoesOn(solid, lip_y + in * (d - MARGIN), ledge.normal))
			return hand + in * d;
	return v3f(ledge.lip.X, hand.Y, ledge.lip.Z);
}

std::optional<physics::Grip> Body::grip() const
{
	if (m_down) {
		if (!m_down->fallen.handsDown())
			return std::nullopt;
		physics::Grip g;
		const v3f across(-m_down->way.Z, 0, m_down->way.X);
		g.held[0] = g.held[1] = true;
		g.wrist[0] = m_down->hands + across * 0.3f;
		g.wrist[1] = m_down->hands - across * 0.3f;
		g.over = m_down->way;
		return g;
	}
	if (m_trip_left > physics::TRIP_TIME - 0.3f) { // hands down on what it went over
		physics::Grip g;
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
		physics::Grip g;
		const v3f along(-m_push->normal().Z, 0, m_push->normal().X);
		const int near = m_push->nearSide();
		g.held[0] = near != 1;
		g.held[1] = near != 0;
		g.wrist[0] = m_push->handsAt() + (near == 2 ? along * 0.3f : m_push->facing() * 0.2f);
		g.wrist[1] = m_push->handsAt() + (near == 2 ? along * -0.3f : m_push->facing() * 0.2f);
		g.over = v3f(0, 1, 0);
		return g;
	}
	const physics::Ledge *ledge = m_hanging ? &*m_hanging : m_climb ? &m_climb->ledge : m_in_reach ? &*m_in_reach : nullptr;
	if (!ledge)
		return std::nullopt;
	physics::Grip g;
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

float Body::thrownSide() const
{
	if (!m_climb || m_climb->side == 0 || m_climb->haul)
		return 0;
	// the wall's to its left when turned right: the hand plants there and the body goes round it, feet up and
	// away from where it's going first, then over
	constexpr float THROW = 0.9f;
	return (m_climb->side > 0 ? THROW : -THROW) * std::sin(core::PI * std::min(m_climb->t / m_climb->path.time, 1.f));
}

float Body::thrownBack() const
{
	if (!m_climb || !m_climb->dive || m_climb->haul)
		return 0;
	// head first, hips up behind it, over the hands
	constexpr float THROW = 1.1f;
	return THROW * std::sin(core::PI * std::min(m_climb->t / m_climb->path.time, 1.f));
}

void Body::knockDown(float severity, v3f velocity)
{
	if (!m_me || m_down || m_thrusting)
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
	m_down = Down{ physics::Fallen(severity, face_down), way, face_down, foot + way * 0.45f + v3f(0, 0.06f, 0) };
	m_stance = physics::Stance::DOWN;
	m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
	actionstream << "body: down at " << pace << " nodes/s, " << (face_down ? "on its front" : "on its back") << std::endl;
}

void Body::airWall(const physics::World::CharacterState &was, v3f velocity, float jump)
{
	const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
	const std::optional<v3f> wall = physics::findWall(solid, v3f::from(was.foot), m_radius);
	if (!wall || (m_kicked && m_kicked->dotProduct(*wall) > 0.5f))
		return;
	m_jump_press = 0;
	m_jump_used = true;
	m_kicked = *wall;
	if (physics::runsAlong(velocity, *wall)) {
		const v3f across(velocity.X, 0, velocity.Z);
		m_wall_run = WallRun{ *wall, across - *wall * across.dotProduct(*wall), physics::tuning().wall_run_lift, physics::tuning().wall_run_time };
		m_stance = physics::Stance::WALL_RUN;
		m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
	} else {
		m_push = physics::WallPush(*wall, velocity, jump, physics::wallFaceAt(v3f::from(was.foot), *wall, m_radius), m_look);
	}
}

v3f Body::wallStep(const physics::World::CharacterState &was, const physics::Here &h, float jump)
{
	WallRun &run = *m_wall_run;
	const float dt = (float)physics::STEP;
	const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
	const std::optional<v3f> wall = physics::findWall(solid, v3f::from(was.foot), m_radius);
	run.left -= dt;
	run.up += h.gravity.Y * physics::tuning().wall_run_gravity * dt;
	const v3f velocity = run.along + v3f(0, run.up, 0);
	if (m_jump_press > 0) { // off it, and on
		m_jump_press = 0;
		m_jump_used = true;
		const v3f normal = run.normal; // (the run's over with the next line)
		onItsFeet();
		m_push = physics::WallPush(normal, velocity, jump, physics::wallFaceAt(v3f::from(was.foot), normal, m_radius), m_look);
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

void Body::startClimb(const physics::World::CharacterState &was, v3f velocity, float yaw)
{
	const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
	const v3f foot = v3f::from(was.foot);
	std::optional<physics::Ledge> ledge = physics::findLedge(solid, foot, yaw, was.on_ground ? physics::VAULT_REACH : physics::GRAB_REACH);
	if (ledge)
		ledge = physics::overCorner(solid, *ledge, velocity);
	std::optional<physics::Move> move = ledge ? physics::moveFor(*ledge, m_base, foot.Y, velocity) : std::nullopt;
	if (was.on_ground) {
		// from the ground, only a vault, and only on the way to it: stood beside a wall,
		// Space is a jump. The press is spent either way: the next is what grabs.
		const v3f across(velocity.X, 0, velocity.Z);
		const bool heading = ledge && across.dotProduct(ledge->normal * -1.f) > 1;
		if (!move || move->climb != physics::Climb::VAULT || !heading)
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
	if (move->climb == physics::Climb::VAULT)
		c.path = physics::climbPath(*move, *ledge, foot, m_radius, pace);
	else
		c.haul = physics::Haul(*ledge, foot, velocity, m_radius, move->climb == physics::Climb::MANTLE, m_base);
	const v3f in = ledge->inward();
	c.dive = move->dive;
	if (move->climb == physics::Climb::VAULT) {
		// it goes on as it came in: the way it was going, not turned square to the wall
		c.exit = pace > 2 ? across : in * 2.f;
		const v3f along = across - in * across.dotProduct(in);
		const v3f drifted = c.path.to + along * c.path.time;
		const auto clear = [&](float up) {
			return !solidAt(v3s16(std::floor(drifted.X + 0.5f), std::floor(drifted.Y + up + 0.5f), std::floor(drifted.Z + 0.5f)));
		};
		if (clear(0.5f) && clear(1.5f))
			c.path.to = drifted;
		if (move->side) // turned toward the hand it goes past
			c.side = (across.X * in.Z - across.Z * in.X) > 0 ? 1.2f : -1.2f;
	}
	actionstream << "body: " << move->name << " at " << pace << " nodes/s" << std::endl;
	// (caught first, whichever it goes on to)
	m_stance = move->climb == physics::Climb::VAULT ? physics::Stance::VAULT : physics::Stance::HANG;
	m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
	m_climb = c;
}

v3f Body::climbStep(LocalPlayer &p, const v3d &foot)
{
	const float dt = (float)physics::STEP;
	if (m_climb) {
		m_climb->t += dt;
		if (m_climb->haul) {
			physics::Haul &haul = *m_climb->haul;
			haul.step(dt);
			if (m_stance == physics::Stance::HANG && haul.phase() != physics::Haul::Phase::CATCH && haul.phase() != physics::Haul::Phase::DONE) {
				m_stance = physics::Stance::MANTLE;
				m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
			}
			const v3f out = haul.done() ? haul.exit() : v3f(0);
			if (haul.done() && m_climb->kind == physics::Climb::HANG) {
				m_world->place(*m_me, v3d::from(haul.at()), v3f(0));
				m_hanging = m_climb->ledge;
				v3f right, left;
				handsAt(*m_hanging, right, left);
				m_hands.place(right, left);
				m_climb.reset();
				return v3f(0);
			}
			m_world->place(*m_me, v3d::from(haul.at()), out);
			if (haul.done())
				onItsFeet();
			return out;
		}
		const bool done = m_climb->t >= m_climb->path.time;
		const v3f at = m_climb->path.at(m_climb->t);
		const v3f exit = done ? m_climb->exit : v3f(0);
		m_world->place(*m_me, v3d::from(at), exit);
		if (done) {
			onItsFeet();
		}
		return exit;
	}
	const physics::Ledge ledge = *m_hanging;
	// Ctrl or S lets go
	const bool backing = std::cos(p.control.movement_direction) * p.control.movement_speed < -0.5f;
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
			const v3f face = physics::wallFaceAt(v3f::from(foot), ledge.normal, m_radius);
			const float up = std::sqrt(2 * std::max(0.f, -here(foot, v3f(0)).gravity.Y) * JUMP * p.physics_override.jump);
			onItsFeet();
			m_push = physics::WallPush(ledge.normal, v3f(0), up, face, m_look);
			return v3f(0);
		}
		const v3s16 top(std::floor(ledge.lip.X - ledge.normal.X * 0.25f + 0.5f), std::floor(ledge.lip.Y + 1), std::floor(ledge.lip.Z - ledge.normal.Z * 0.25f + 0.5f));
		if (!solidAt(top) && !solidAt(top + v3s16(0, 1, 0))) {
			m_stance = physics::Stance::MANTLE;
			m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
			Climbing c{ physics::Climb::MANTLE, ledge, {} };
			c.haul = physics::Haul::pullUp(ledge, v3f::from(foot), m_radius);
			m_hanging.reset();
			m_climb = c;
			return v3f(0);
		}
	}
	// along the edge: A and D as the body faces it, wherever the view has gone. As far as there's edge for its hands
	constexpr float HANDS_OUT = 0.42f;
	const PlayerControl &c = p.control;
	const float right = std::sin(c.movement_direction) * c.movement_speed;
	const v3f along(-ledge.normal.Z, 0, ledge.normal.X);
	const v3f moved = ledge.lip + along * (right * physics::tuning().shimmy * dt);
	const v3f lookout = moved + along * (right > 0 ? HANDS_OUT : -HANDS_OUT);
	const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
	if (physics::ledgeGoesOn(solid, lookout, ledge.normal))
		m_hanging->lip = moved;
	const v3f hung(m_hanging->lip.X + ledge.normal.X * (m_radius + 0.05f), ledge.lip.Y - physics::HANG_DROP,
			m_hanging->lip.Z + ledge.normal.Z * (m_radius + 0.05f));
	m_world->place(*m_me, v3d::from(hung), v3f(0));
	v3f hand_right, hand_left;
	handsAt(*m_hanging, hand_right, hand_left);
	const physics::Ledge held = *m_hanging;
	m_hands.step(dt, hand_right, hand_left, [&](v3f to) { return keepOnEdge(held, to); });
	return v3f(0);
}

void Body::holdStance(const physics::World::CharacterState &now)
{
	using physics::Stance;
	if (m_thrusting) { // flying: on its feet
		if (m_stance != Stance::STAND && m_world->resize(*m_me, m_standing_height))
			m_stance = Stance::STAND;
		return;
	}
	const float pace = v3f(now.velocity.X - now.ground_velocity.X, 0, now.velocity.Z - now.ground_velocity.Z).getLength();
	const Stance next = physics::nextStance(m_stance, m_asked, pace, now.on_ground, m_roll_left);
	if (next == m_stance)
		return;
	// tallest first that's no taller than asked, so a low roof stops at a crouch
	for (const Stance s : { next, Stance::CROUCH, Stance::PRONE }) {
		if (s != next && physics::stanceInfo(s).height <= physics::stanceInfo(m_stance).height)
			continue;
		const float h = physics::stanceInfo(s).height;
		if (m_world->resize(*m_me, h > 0 ? h : m_standing_height)) {
			if (m_stance == Stance::ROLL) // (a slide ends crouched, till asked otherwise)
				m_move_ended = true;
			m_stance = s;
			m_said_stuck = false;
			return;
		}
	}
	// (hunting a crouch that won't undo: says whether the capsule's what refuses)
	if (!m_said_stuck)
		actionstream << "body: stance " << (int)m_stance << " can't become " << (int)next << " at " << now.foot.X << ","
				<< now.foot.Y << "," << now.foot.Z << ", on ground " << now.on_ground << std::endl;
	m_said_stuck = true;
}

v3f Body::keepOnEdge(const v3d &foot, v3f next) const
{
	constexpr float STEP_LOOK = 0.25f, DEEPEST = 0.7f; // nodes: ahead of the body, and a drop it'd not take
	Map &map = m_client->getEnv().getMap();
	const NodeDefManager *ndef = m_client->ndef();
	const auto floorAt = [&](double x, double z) {
		for (const double y : { foot.Y - 0.1, foot.Y - DEEPEST }) {
			const v3s16 n(std::floor(x + 0.5), std::floor(y + 0.5), std::floor(z + 0.5));
			const MapNode node = map.getNode(n);
			if (node.getContent() == CONTENT_IGNORE || ndef->get(node).walkable)
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

physics::Here Body::here(const v3d &foot, v3f velocity) const
{
	physics::Here h = m_pull(foot, velocity);
	h.gravity *= m_scale;
	return h;
}

bool Body::ensureWorld()
{
	const planet::Atlas *atlas = m_client->getAtlas();
	if (m_world && atlas == m_atlas)
		return true;
	// each body's pull and air come from the world code's system, which the
	// horizon carries (sent just after the atlas)
	const std::optional<far::Horizon> &horizon = m_client->getHorizon();
	if (atlas && !horizon)
		return false;
	// a new planet (or the first): a new field, a new world
	m_atlas = atlas;
	m_world.reset();
	if (atlas) {
		std::vector<terragen::Planet> planets;
		for (const terragen::BodyWorld &b : terragen::bodyWorlds(terragen::WorldCode::decode(horizon->code), atlas->body(0).params()))
			planets.push_back(terragen::planetOf(b));
		m_field = std::make_unique<orbit::Field>(*atlas, planets);
	} else {
		m_field = std::make_unique<orbit::Field>(terragen::home());
	}
	m_pull = workshop::fieldOf(*m_field);
	m_mirrors.clear();
	if (m_sky_answer.valid())
		m_sky_answer.wait(); // (it reads what's replaced here)
	m_lands.clear();
	m_me.reset();
	Client *client = m_client;
	m_world = std::make_unique<physics::World>(physics::World::Opts{
			[this](const v3d &at, const v3f &velocity) { return here(at, velocity); },
			[client](v3s16 block, std::vector<aabb3f> &boxes) {
				return workshop::groundBoxes(client->getEnv().getMap(), client->ndef(), block, boxes);
			} });
	if (m_carrying)
		m_carrying.reset(); // its mirror's gone with the world; Game lets it go
	m_grabbing.reset();
	m_driving.reset();
	return true;
}

void Body::nudge(v3f by)
{
	if (!m_me)
		return; // not walking yet: Luanti's movement had it
	const physics::World::CharacterState c = m_world->character(*m_me);
	m_world->place(*m_me, c.foot, c.velocity + by);
	m_now = m_was = m_measured = c.foot;
	m_set = v3f::from(c.foot * BS);
}

void Body::groundChanged(v3s16 block)
{
	if (m_world)
		m_world->groundChanged(block);
}

void Body::mirror(LocalPlayer &p)
{
	std::vector<DistanceSortedActiveObject> near;
	m_client->getEnv().getActiveObjects(p.getPosition(), MIRRORED * BS, near);
	std::map<u16, GenericCAO *> seen;
	for (const DistanceSortedActiveObject &d : near) {
		auto *cao = dynamic_cast<GenericCAO *>(d.obj);
		if (!cao || !cao->getMachine())
			continue;
		const u16 id = cao->getId();
		const size_t text = std::hash<std::string>{}(cao->getProperties().piece);
		auto unweighed = m_unweighed.find(id);
		if (unweighed != m_unweighed.end() && unweighed->second == text)
			continue;
		seen[id] = cao;
		auto it = m_mirrors.find(id);
		if (it != m_mirrors.end() && it->second.text == text)
			continue;
		try {
			// from its text as it is now, not the machine it's drawn as: that's
			// read when it's next drawn, a frame or more behind (sprint 72: a
			// wheel just made was missing here, and sank while it was dragged)
			piece::Machine machine(m_client);
			machine.read(cao->getProperties().piece);
			const physics::Assembly a = workshop::compile(machine, m_client->getMaterials(), m_client);
			if (it != m_mirrors.end()) {
				m_world->rebuild(it->second.id, a);
				it->second.text = text;
				it->second.seat = a.seat;
				it->second.facing = a.facing;
			} else {
				const physics::Pose at{ v3d::from(cao->getPosition() / BS), workshop::quatOf(cao->getRotation()) };
				const physics::World::Id w = m_world->add(a, at);
				m_world->hold(w, true); // the server's: it goes where it's drawn
				m_mirrors[id] = { w, text, a.seat, a.facing };
			}
			m_unweighed.erase(id);
		} catch (const std::runtime_error &e) {
			// the server can't move it either (it says so in its log): here
			// it's no obstacle, as it isn't there
			infostream << "Body: machine " << id << " isn't in the player's world: " << e.what() << std::endl;
			m_unweighed[id] = text;
			seen.erase(id);
			if (it != m_mirrors.end()) {
				m_world->remove(it->second.id);
				m_mirrors.erase(it);
			}
		}
	}
	for (auto it = m_mirrors.begin(); it != m_mirrors.end();) {
		const bool carried = m_carrying && m_carrying->first == it->first;
		const bool coasting = !carried && m_coasting.count(it->first) > 0;
		const bool grabbed = (m_grabbing && *m_grabbing == it->first) || (m_driving && *m_driving == it->first)
				|| coasting;
		auto s = seen.find(it->first);
		if (s == seen.end()) {
			m_world->remove(it->second.id);
			if (carried)
				m_carrying.reset();
			if (m_grabbing == it->first)
				m_grabbing.reset();
			if (m_driving == it->first)
				m_driving.reset(); // Game sees it's gone and gets them up
			m_coasting.erase(it->first);
			it = m_mirrors.erase(it);
			continue;
		}
		if (grabbed) {
			// toward where the server has it, as far on as the time its word
			// took to come; a share each frame, or put there if far off
			constexpr float LATE = 0.1f, SHARE = 0.05f, SNAP = 1.5f; // s; of the gap; nodes
			const GenericCAO::Served &s_ = s->second->served();
			// what's wrong when they part (sprint 72: a car sank on one side
			// while grabbed): both poses, its wheels and the ground under it,
			// to the log, at most twice a second
			const v3d here = m_world->pose(it->second.id).position, there = v3d::from(s_.position / BS);
			if (!coasting && (here - there).getLength() > 0.2 && porting::getTimeMs() - m_parted_said > 500) {
				m_parted_said = porting::getTimeMs();
				warningstream << "physics: the client's " << it->first << " is " << (here - there).getLength()
						<< " nodes off the server's (" << there.X << "," << there.Y << "," << there.Z << "): "
						<< m_world->describe(it->second.id) << std::endl;
			}
			m_world->correct(it->second.id, { v3d::from((s_.position + s_.velocity * LATE) / BS),
					workshop::quatOf(s_.rotation) }, SHARE, SNAP);
			// let go, it's this client's till the server has it still a while
			// (sprint 72: drawn from the server's word as it rolled, it
			// jittered); then the server's again
			if (coasting) {
				constexpr float STILL = 0.05f * BS; // BS/s
				constexpr u64 SETTLED = 500;        // ms
				u64 &since = m_coasting.at(it->first);
				if (s_.velocity.getLength() > STILL)
					since = 0;
				else if (since == 0)
					since = porting::getTimeMs();
				else if (porting::getTimeMs() - since > SETTLED) {
					m_coasting.erase(it->first);
					m_world->hold(it->second.id, true);
				}
			}
		} else if (!carried) // what's carried is this client's to move
			m_world->moveTo(it->second.id, { v3d::from(s->second->getPosition() / BS),
					workshop::quatOf(s->second->getRotation()) });
		++it;
	}
}

// Above the highest ground any body could have here: 25 km of mountain, at
// the planet's scale (terragen::blocksPerMetre's √(R / R⊕)).
// Flying through the sky faster than the map arrives. Clear of the land under
// it and the land it's about to reach, what isn't loaded yet is air: waiting for
// it stopped the body dead at every block's edge. Near the land it's still waited
// for, or the floor is flown through before it arrives.
float Body::landBelow(const v3f &at, const v3f &velocity)
{
	constexpr float LOOK_AHEAD = 1; // s
	constexpr float NO_LAND = 1e9f; // a seam, or over the atlas's top
	if (!m_atlas)
		return NO_LAND;
	const std::optional<far::Horizon> &horizon = m_client->getHorizon();
	const auto below = [&](const v3f &there) {
		const std::optional<planet::Atlas::Cell> c = m_atlas->cell(planet::Atlas::moverNode(there.X, there.Y, there.Z));
		if (!c)
			return NO_LAND;
		std::unique_ptr<far::Generator> &land = m_lands[c->body];
		if (!land)
			land = std::make_unique<far::Generator>(*horizon, *m_atlas, c->body);
		return (float)(c->k - land->surfaceAt(planet::Atlas::Cell(*c)));
	};
	// (a place in another grid isn't charted from this one's numbers: it's asked again from there)
	return std::min(below(at), below(at + velocity * LOOK_AHEAD));
}

float Body::nearRange(float wanted) const
{
	return m_tier ? std::min(wanted, physics::FLIGHT_TIERS[m_tier - 1].near) : wanted;
}

void Body::move(LocalPlayer &p, float dtime, std::vector<CollisionInfo> *collisions)
{
	ScopeProfiler timed(g_profiler, "Body move [us]", SPT_GRAPH_ADD, PRECISION_MICRO);
	if (!ensureWorld())
		return; // held a frame till the world's bodies are known
	m_scale = p.physics_override.gravity;
	if (m_driving) {
		ride(p, dtime);
		return;
	}
	const aabb3f &box = p.getCollisionbox();
	const v3f at = p.getPosition();
	if (!m_me) {
		const float radius = std::max({ -box.MinEdge.X, box.MaxEdge.X, -box.MinEdge.Z, box.MaxEdge.Z }) / BS;
		m_standing_height = box.getExtent().Y / BS;
		m_radius = radius;
		m_stance = physics::Stance::STAND;
		m_me = m_world->addCharacter({ radius, m_standing_height, MASS, PUSH, SLOPE }, v3d::from(at / BS));
		m_now = m_was = m_measured = v3d::from(at / BS);
		m_set = at;
	}
	if (at.getDistanceFrom(m_set) > 0.001f * BS) {
		// put elsewhere since (by the server, Luanti's movement, a seam's
		// crossing): from there
		m_world->place(*m_me, v3d::from(at / BS), p.getSpeed() / BS);
		m_now = m_was = m_measured = v3d::from(at / BS);
		m_behind = 0;
	}
	// (a column's asked for only a few times a second: it's generated)
	m_sky_check -= dtime;
	if (!m_thrusting) {
		m_clear_of_land = false;
		m_above_land = 0;
	} else {
		// (asked on a worker: a column in new country took long enough to drop frames)
		if (m_sky_answer.valid() && m_sky_answer.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			constexpr float CLEAR = 16; // nodes: over trees
			m_above_land = m_sky_answer.get();
			m_clear_of_land = m_above_land > CLEAR;
		}
		if (m_sky_check <= 0 && !m_sky_answer.valid()) {
			m_sky_check = 0.25f;
			m_sky_answer = std::async(std::launch::async, [this, here = at / BS, v = p.getSpeed() / BS] { return landBelow(here, v); });
		}
	}
	m_world->unknownIsAir(*m_me, m_clear_of_land);
	// ... and it asks for only as much of the map as is any use at this speed
	m_tier = m_clear_of_land ? physics::flightTier(p.getSpeed().getLength() / BS, m_above_land, m_tier) : 0;
	m_world->skipsGround(*m_me, m_tier > 0);
	m_world->keepsSpeed(*m_me, m_thrusting); // (a flight held up by the map goes on as it was)
	mirror(p);
	m_moved = true;

	const PlayerControl &c = p.control;
	const PlayerPhysicsOverride &o = p.physics_override;
	m_asked = c.prone ? physics::Stance::PRONE : c.sneak ? physics::Stance::CROUCH : physics::Stance::STAND;

	m_behind += dtime;
	while (m_behind >= physics::STEP) {
		const physics::World::CharacterState was = m_world->character(*m_me);
		m_roll_left = std::max(0.f, m_roll_left - (float)physics::STEP);
		m_kick_left = std::max(0.f, m_kick_left - (float)physics::STEP);
		// (pressed during a vault or a climb, it's the next move: the press waits for it)
		if (!m_climb)
			m_jump_press = std::max(0.f, m_jump_press - (float)physics::STEP);
		if (was.on_ground)
			m_base = (float)was.foot.Y;
		else
			m_base = std::min(m_base, (float)was.foot.Y);
		if (!c.jump)
			m_jump_used = false;
		if (was.on_ground)
			m_kicked.reset();
		const bool climbing = m_climb || m_hanging || m_wall_run || m_push || m_down;
		m_late_roll = std::max(0.f, m_late_roll - (float)physics::STEP);
		const physics::Stance stance_was = m_stance;
		if (m_late_roll > 0 && m_asked != physics::Stance::STAND && m_stance == physics::Stance::STAND && was.on_ground) {
			m_stance = physics::Stance::ROLL;
			m_roll_left = physics::ROLL_TIME;
			m_late_roll = 0;
			m_world->resize(*m_me, physics::stanceInfo(physics::Stance::ROLL).height);
			infostream << "body: rolled late" << std::endl;
		}
		if (!climbing)
			holdStance(was);
		const physics::StanceInfo &held = physics::stanceInfo(m_stance);
		const bool low = m_stance != physics::Stance::STAND;
		const bool edged = m_stance == physics::Stance::CROUCH || m_stance == physics::Stance::PRONE;

		physics::Gait gait;
		gait.walk = p.movement_speed_walk * o.speed_walk * held.speed / BS;
		gait.run = low ? 1 : o.speed;
		gait.flow = o.speed > 1 ? 1.25f : 1; // only a run builds
		gait.jump = JUMP * o.jump;
		const physics::Intent intent{ std::cos(c.movement_direction) * c.movement_speed,
			std::sin(c.movement_direction) * c.movement_speed, p.getYaw() * core::DEGTORAD, (c.jump || m_jump_press > 0) && held.jumps && !m_jump_used && m_trip_left <= 0 };
		// in the air, its motion carried to where it is now (the grid's axes turn)
		const v3f v = was.on_ground ? was.velocity : m_field->carry(m_measured, was.foot, was.velocity);
		m_measured = was.foot;
		const physics::Here h = here(was.foot, v);
		m_in_reach.reset();
		m_look = v3f(-std::sin(p.getYaw() * core::DEGTORAD), 0, std::cos(p.getYaw() * core::DEGTORAD));
		const float wall_jump = std::sqrt(2 * std::max(0.f, -h.gravity.Y) * JUMP * o.jump);
		const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
		if (!m_thrusting && !climbing && m_stance == physics::Stance::STAND && !was.on_ground) {
			// the hands go for what's in reach before it's asked: that's how it's known it can be
			// (from further than it can take hold: going for it is what shows it can be had)
			const std::optional<physics::Ledge> ledge = physics::findLedge(solid, v3f::from(was.foot), p.getYaw() * core::DEGTORAD, physics::VAULT_REACH);
			if (ledge && physics::moveFor(*ledge, m_base, (float)was.foot.Y, v))
				m_in_reach = ledge;
			// Space (or still held from the jump) going along a wall runs on it, ledge or no:
			// what's being passed isn't what it's trying to climb
			if (m_jump_press > 0 || c.jump) {
				const std::optional<v3f> wall = physics::findWall(solid, v3f::from(was.foot), m_radius);
				if (wall && physics::runsAlong(v, *wall) && (!m_kicked || m_kicked->dotProduct(*wall) < 0.5f))
					airWall(was, v, wall_jump);
			}
		}
		if (was.on_ground && m_grab_wait) { // never got there: not a jump on landing
			m_jump_press = 0;
			m_grab_wait = false;
		}
		const bool free_to_climb = !m_thrusting && !climbing && !m_wall_run && m_stance == physics::Stance::STAND;
		// on the way to an edge, in reach or nearly, or about to be as it rises: Space is for that, not for bouncing off
		const auto edge_coming = [&] {
			const float yaw = p.getYaw() * core::DEGTORAD;
			const v3f foot = v3f::from(was.foot);
			for (const float up : { 0.f, v.Y > 0 ? 0.6f : 0.f }) {
				const v3f at = foot + v3f(0, up, 0);
				const std::optional<physics::Ledge> l = physics::findLedge(solid, at, yaw, physics::VAULT_REACH);
				if (l && physics::moveFor(*l, m_base, at.Y, v) && (v.dotProduct(l->normal * -1.f) > 0.5f || m_look.dotProduct(l->normal * -1.f) > 0.5f))
					return true;
			}
			return false;
		};
		if (free_to_climb && m_jump_press > 0) {
			startClimb(was, v, p.getYaw() * core::DEGTORAD);
			// nothing to take hold of, in the air: a wall to run on or kick off
			if (m_jump_press > 0 && !was.on_ground) {
				if (edge_coming()) {
					m_jump_press = GRAB_MEMORY;
					m_grab_wait = true;
				} else {
					airWall(was, v, wall_jump);
				}
			}
		} else if (free_to_climb && !was.on_ground && c.jump && !m_jump_used) {
			// Space held from the jump takes hold of what it comes to, heading for it. (A hop, let go, doesn't.)
			const std::optional<physics::Ledge> l = physics::findLedge(solid, v3f::from(was.foot), p.getYaw() * core::DEGTORAD);
			if (l && v.dotProduct(l->normal * -1.f) > 0.5f)
				startClimb(was, v, p.getYaw() * core::DEGTORAD);
		}
		v3f next;
		if (m_down) {
			// sliding to a stop where it went down. A direction, or Space, gets it up sooner
			constexpr float FRICTION = 7, UP_AND_OFF = 3;
			const float ahead = std::cos(c.movement_direction) * c.movement_speed, right = std::sin(c.movement_direction) * c.movement_speed;
			const float yaw_now = p.getYaw() * core::DEGTORAD;
			const v3f asked(right * std::cos(yaw_now) - ahead * std::sin(yaw_now), 0, right * std::sin(yaw_now) + ahead * std::cos(yaw_now));
			const bool wants_up = asked.getLengthSQ() > 0.01f || c.jump;
			m_down->fallen.step((float)physics::STEP, wants_up);
			const float pace = std::hypot(v.X, v.Z);
			const float kept = pace > 0.01f ? std::max(0.f, pace - FRICTION * (float)physics::STEP) / pace : 0;
			next = v3f(v.X * kept, was.on_ground ? 0 : v.Y + h.gravity.Y * (float)physics::STEP, v.Z * kept);
			if (m_down->fallen.done()) {
				onItsFeet();
				// off in the way it's asked, not from nothing
				next += asked * UP_AND_OFF;
				m_jump_used = c.jump;
			}
		} else if (m_push) {
			next = m_push->step((float)physics::STEP);
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
			next = climbStep(p, was.foot);
		} else if (m_thrusting) {
			next = thrust(p, v, h, (float)physics::STEP);
		} else {
			gait.gravity = h.gravity;
			const physics::Footing footing{ v, was.on_ground, was.ground_velocity, was.ground_normal };
			if (m_stance == physics::Stance::SLIDE)
				next = physics::slide(gait, intent, footing, (float)physics::STEP);
			else if (m_stance == physics::Stance::ROLL)
				next = physics::roll(gait, intent, footing, (float)physics::STEP);
			else
				next = physics::walk(gait, intent, footing, (float)physics::STEP);
			// let go of Space on the way up and it's a hop: the jump's made on the press, so nothing waits
			if (was.on_ground && intent.jump && next.Y > 0)
				m_rising = true;
			else if (m_rising && !was.on_ground && !c.jump) {
				if (next.Y > 0)
					next.Y *= physics::HOP;
				m_rising = false;
			} else if (was.on_ground)
				m_rising = false;
			if (m_stance == physics::Stance::SLIDE && stance_was != physics::Stance::SLIDE) {
				const float pace = std::hypot(next.X, next.Z);
				if (pace > 0.01f && pace < physics::SLIDE_KICK) {
					next.X *= physics::SLIDE_KICK / pace;
					next.Z *= physics::SLIDE_KICK / pace;
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
				next -= v * (0.5f * h.air * v.getLength() * DRAG_AREA / MASS * (float)physics::STEP);
		}
			if (!was.on_ground) {
			// it goes by its velocity as it is halfway there (the grid's
			// lines curve round the planet), and it's measured there
			m_measured = was.foot + v3d::from(next) * (physics::STEP / 2);
			next = m_field->carry(was.foot, m_measured, next);
		}
		m_world->want(*m_me, next);
		m_world->step(physics::STEP);
		m_behind -= physics::STEP;
		physics::World::CharacterState now = m_world->character(*m_me);
		// Up a step in one physics step (the capsule's way): it isn't drawn there at once. It's drawn where
		// it was, catching up, and how far and how fast it was going decides what that cost.
		{
			constexpr float SHOWS_FROM = 0.12f, CATCHES_UP = 5, SCRAMBLES_UP = 1.5f, RECOVERS = 2;
			const float s = (float)physics::STEP;
			const float rise = (float)now.foot.Y - ((float)was.foot.Y + next.Y * s);
			if (!climbing && !m_climb && !m_hanging && !m_wall_run && !m_thrusting && !now.standing_on &&
					(was.on_ground || now.on_ground) && rise >= SHOWS_FROM && rise <= 1) {
				const v3f across(was.velocity.X, 0, was.velocity.Z);
				const float pace = across.getLength();
				m_lift += rise;
				if (physics::trips(rise, pace) && m_trip_left <= 0) {
					m_trip_left = physics::TRIP_TIME;
					m_stagger = 1;
					m_keep = 0.45f;
					m_trip_way = across / pace;
					m_trip_at = v3f::from(now.foot) + m_trip_way * 0.5f;
					actionstream << "body: tripped at " << pace << " nodes/s, " << rise << " up" << std::endl;
				} else {
					m_stagger = std::max(m_stagger, rise);
					m_keep = std::min(m_keep, 1 - rise * 0.4f);
				}
			}
			m_lift = std::max(0.f, m_lift - (m_trip_left > 0 ? SCRAMBLES_UP : CATCHES_UP) * s);
			m_trip_left = std::max(0.f, m_trip_left - s);
			m_stagger = std::max(0.f, m_stagger - RECOVERS * s);
		}
		// (what stops a flight is worth a line: nothing else says which it was)
		if (m_thrusting && was.velocity.getLength() > 20 && now.velocity.getLength() < was.velocity.getLength() * 0.5f)
			actionstream << "body: flight stopped at " << now.foot.X << "," << now.foot.Y << "," << now.foot.Z << " from "
					<< was.velocity.getLength() << " nodes/s, clear of land: " << m_clear_of_land << ", on ground: " << now.on_ground << std::endl;
		m_was = m_now;
		m_now = now.foot;
		if (const std::optional<planet::Atlas::Carried> c = m_atlas ?
				m_atlas->carry(was.foot.X, was.foot.Y, was.foot.Z, now.foot.X, now.foot.Y, now.foot.Z) : std::nullopt) {
			// over a seam: into the grid it's in now, the same motion in its
			// axes, the view turned with the ground
			const v3d there(c->x, c->y, c->z);
			m_world->place(*m_me, there, m_field->carry(m_measured, there, now.velocity));
			m_was = m_now = m_measured = there;
			p.turnWithGround(c->angle);
			now = m_world->character(*m_me);
		}
		if (!was.on_ground && now.on_ground && !climbing && !m_climb && !m_hanging && !m_wall_run && !m_push) {
			if (m_thrusting) {
				// landed: on their feet, flying off (as a double-tap would)
				m_thrusting = false;
				g_settings->setBool("free_move", false);
			}
			const float fall = -was.velocity.Y;
			const bool rolled = !m_thrusting && physics::rollsOut(fall, m_asked);
			if (rolled) {
				// it rolls whether or not the small capsule fits this instant (coming down,
				// it can be pressed into the ground); holdStance sets it right after
				m_world->resize(*m_me, physics::stanceInfo(physics::Stance::ROLL).height);
				m_stance = physics::Stance::ROLL;
				m_roll_left = physics::ROLL_TIME;
			} else if (fall >= physics::tuning().hard_landing) {
				m_late_roll = physics::tuning().roll_late;
			}
			if (!rolled && !m_thrusting) {
				// its speed goes as far as its balance did, and it shows till it's back
				const float yaw_now = p.getYaw() * core::DEGTORAD;
				const float ahead = std::cos(c.movement_direction) * c.movement_speed, right = std::sin(c.movement_direction) * c.movement_speed;
				const v3f asked = right != 0 || ahead != 0 ?
						v3f(right * std::cos(yaw_now) - ahead * std::sin(yaw_now), 0, right * std::sin(yaw_now) + ahead * std::cos(yaw_now)) : m_look;
				const float off = fall >= physics::tuning().hard_landing ? 1 : physics::landingStagger(fall, was.velocity, asked);
				if (off >= physics::tuning().falls_from && (physics::landingStagger(fall, was.velocity, asked) >= physics::tuning().falls_from || fall >= physics::tuning().falls_at))
					knockDown(std::clamp((fall - 4) / 12, 0.f, 1.f), was.velocity);
				if (off > 0.15f) {
					m_stagger = std::max(m_stagger, off);
					m_keep = std::min(m_keep, 1 - (1 - physics::STUMBLE) * off);
				}
			}
			if (fall >= physics::tuning().hard_landing) // (no roll yet in play: this says why)
				actionstream << "body: landed at " << fall << " nodes/s, asking stance " << (int)m_asked
						<< (rolled ? ", rolled" : ", no roll") << std::endl;
			if (collisions && !rolled) { // (a roll takes the fall)
				// landed: as a collision with the ground, so a hard one hurts
				CollisionInfo landed;
				landed.type = COLLISION_NODE;
				landed.axis = COLLISION_AXIS_Y;
				landed.node_p = floatToInt(v3f::from(now.foot) * BS - v3f(0, 0.05f * BS, 0), BS);
				landed.old_speed = was.velocity * BS;
				landed.new_speed = now.velocity * BS;
				collisions->push_back(landed);
			}
		}
	}
	// what others are told is what it really is, and the camera follows it
	p.control.sneak = !m_thrusting && m_stance == physics::Stance::CROUCH;
	p.control.prone = !m_thrusting && m_stance == physics::Stance::PRONE;
	p.control.slide = m_stance == physics::Stance::SLIDE;
	p.control.roll = m_stance == physics::Stance::ROLL;
	p.control.climb = m_stance == physics::Stance::VAULT ? 1 : m_stance == physics::Stance::MANTLE ? 2 :
			m_stance == physics::Stance::HANG ? 3 : m_stance == physics::Stance::WALL_RUN ? 4 : m_stance == physics::Stance::DOWN ? 5 : 0;
	m_crouch_press = false;
	push(dtime);
	const physics::World::CharacterState now = m_world->character(*m_me);
	const v3d drawn = m_was + (m_now - m_was) * (m_behind / physics::STEP) - v3d(0, m_lift, 0);
	m_set = v3f::from(drawn * BS);
	p.setPosition(m_set);
	p.setSpeed(now.velocity * BS);
	p.touching_ground = now.on_ground;
}

v3f Body::thrust(LocalPlayer &p, v3f velocity, const physics::Here &h, float dt)
{
	const PlayerControl &c = p.control;
	// where they look (Luanti's: yaw turns +z toward −x, pitch up is negative)
	const float yaw = c.yaw * core::DEGTORAD, pitch = c.pitch * core::DEGTORAD;
	const v3f look(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
	const v3f right(std::cos(yaw), 0, std::sin(yaw));
	v3f ask = look * (std::cos(c.movement_direction) * c.movement_speed) +
			right * (std::sin(c.movement_direction) * c.movement_speed) +
			v3f(0, (c.jump ? 1.0f : 0.0f) - (c.sneak ? 1.0f : 0.0f), 0);
	if (ask.getLengthSQ() > 1)
		ask.normalize();
	const float speed = velocity.getLength();
	const bool pushing = ask.getLengthSQ() > 0 || (m_brake && speed > 0);
	m_held = pushing ? m_held + dt : 0;
	const float push = std::min(THRUST_MOST, THRUST * std::exp2(m_held / DOUBLING));

	const v3f drag = velocity * (-0.5f * h.air * speed * DRAG_AREA / MASS);
	v3f next = velocity + (h.gravity + drag + ask * push) * dt;
	if (m_brake) {
		// against the motion, as hard: to a stop, then holding it there
		const float most = push * dt, left = next.getLength();
		next = left <= most ? v3f(0) : next * ((left - most) / left);
	}
	return next;
}

std::optional<orbit::Elements> Body::orbit() const
{
	if (!m_me)
		return std::nullopt;
	const physics::World::CharacterState c = m_world->character(*m_me);
	return m_field->elements(c.foot, c.velocity);
}

std::optional<std::vector<orbit::Vec>> Body::path(int points) const
{
	if (!m_me)
		return std::nullopt;
	const physics::World::CharacterState c = m_world->character(*m_me);
	return m_field->path(c.foot, c.velocity, points);
}

void Body::stepObjects(LocalPlayer &p, float dtime)
{
	if (m_moved || !m_world) {
		m_moved = false;
		m_idle = 0;
		return;
	}
	mirror(p);
	m_idle += dtime;
	while (m_idle >= physics::STEP) {
		m_world->step(physics::STEP);
		m_idle -= physics::STEP;
	}
}

void Body::push(float dtime)
{
	constexpr float SEND = 0.05f; // s between pushes sent
	for (const auto &[id, impulse] : m_world->pushes(*m_me))
		for (const auto &[object, mirror] : m_mirrors)
			if (mirror.id == id)
				m_pushed[object] += impulse;
	m_push_age += dtime;
	if (m_push_age < SEND)
		return;
	m_push_age = 0;
	for (const auto &[object, impulse] : m_pushed)
		m_client->sendPush(object, impulse);
	m_pushed.clear();
}

void Body::carry(u16 object, float strength)
{
	auto it = m_mirrors.find(object);
	if (!m_world || it == m_mirrors.end())
		return; // not in its world (it can't be weighed): the server's drawing is all there is
	// a coast's settling would hold it again, ending the carry under the
	// hand (the next pull threw: "only what's carried is pulled")
	m_coasting.erase(object);
	m_world->hold(it->second.id, false);
	m_world->carry(it->second.id, strength);
	m_carrying = { object, strength };
}

void Body::pull(u16 object, v3f position, v3f rotation)
{
	if (!m_carrying || m_carrying->first != object)
		return;
	const physics::World::Id id = m_mirrors.at(object).id;
	m_world->pullTo(id, { v3d::from(position / BS), workshop::quatOf(rotation) });
}

void Body::letGo(u16 object)
{
	if (!m_carrying || m_carrying->first != object)
		return;
	m_carrying.reset();
	auto it = m_mirrors.find(object);
	if (it == m_mirrors.end())
		return;
	m_world->carry(it->second.id, std::nullopt);
	m_world->hold(it->second.id, true); // the server's again: where it's drawn
}

void Body::grab(u16 object, v3f point, float strength)
{
	auto it = m_mirrors.find(object);
	if (!m_world || it == m_mirrors.end())
		return; // not in its world: the server's drawing is all there is
	m_world->hold(it->second.id, false);
	m_coasting.erase(object);
	m_world->grab(it->second.id, ME, point / BS, strength);
	m_grabbing = object;
}

void Body::grabTo(u16 object, v3f hand)
{
	if (!m_grabbing || *m_grabbing != object)
		return;
	m_world->grabTo(m_mirrors.at(object).id, ME, v3d::from(hand / BS));
}

void Body::release(u16 object)
{
	if (!m_grabbing || *m_grabbing != object)
		return;
	m_grabbing.reset();
	auto it = m_mirrors.find(object);
	if (it == m_mirrors.end())
		return;
	m_world->release(it->second.id, ME);
	m_coasting[object] = 0; // it rolls on here till it's still (stepMirrors)
}

std::vector<u16> Body::coasting() const
{
	std::vector<u16> out;
	for (const auto &[object, since] : m_coasting)
		out.push_back(object);
	return out;
}

bool Body::hasSeat(u16 object) const
{
	auto it = m_mirrors.find(object);
	return it != m_mirrors.end() && it->second.seat;
}

bool Body::sit(u16 object)
{
	auto it = m_mirrors.find(object);
	if (!m_world || it == m_mirrors.end() || !it->second.seat)
		return false;
	if (m_grabbing)
		release(*m_grabbing);
	m_world->hold(it->second.id, false);
	m_world->drive(it->second.id, physics::Drive{});
	// the player's body isn't in the way of their own machine: it's made
	// again where they get up
	if (m_me) {
		m_world->removeCharacter(*m_me);
		m_me.reset();
	}
	m_stance = physics::Stance::STAND;
	m_driving = object;
	m_ride_yaw.reset(); // the view's turned to the seat's front as they sit
	return true;
}

void Body::steer(u16 object, const physics::Drive &drive)
{
	if (m_driving != object)
		return;
	m_world->drive(m_mirrors.at(object).id, drive);
}

void Body::getUp(u16 object)
{
	if (m_driving != object)
		return;
	m_driving.reset();
	auto it = m_mirrors.find(object);
	if (it == m_mirrors.end())
		return;
	m_world->drive(it->second.id, std::nullopt);
	m_world->hold(it->second.id, true); // the server's again: where it's drawn
}

void Body::ride(LocalPlayer &p, float dtime)
{
	mirror(p);
	if (!m_driving)
		return; // its machine's gone
	m_moved = true;
	m_behind += dtime;
	while (m_behind >= physics::STEP) {
		m_world->step(physics::STEP);
		m_behind -= physics::STEP;
	}
	const Mirror &m = m_mirrors.at(*m_driving);
	const physics::Pose pose = m_world->pose(m.id);
	// sat: their hips on the seat, not their feet (sprint 73)
	constexpr float SIT = 0.5f; // nodes the foot is below the seat's top
	const v3f seat = v3f::from(pose.position) + workshop::turned(pose.rotation, *m.seat) - v3f(0, SIT, 0);
	p.setPosition(seat * BS);
	p.setSpeed(m_world->velocity(m.id) * BS);
	m_set = seat * BS;
	// the view turned to the seat's front as they sit, then with the
	// machine as it turns (the mouse still looks about)
	const v3f facing = workshop::turned(pose.rotation, m.facing);
	const f32 yaw = std::atan2(-facing.X, facing.Z);
	const f32 from = m_ride_yaw ? *m_ride_yaw : p.getYaw() * core::DEGTORAD;
	p.turnWithGround(std::remainder(yaw - from, 2 * core::PI));
	m_ride_yaw = yaw;
}

std::optional<u16> Body::standingOn() const
{
	if (!m_world || !m_me)
		return std::nullopt;
	const std::optional<physics::World::Id> on = m_world->character(*m_me).standing_on;
	if (on)
		for (const auto &[object, mirror] : m_mirrors)
			if (mirror.id == *on)
				return object;
	return std::nullopt;
}

std::vector<physics::World::Contact> Body::drivenWheels() const
{
	if (!m_driving || !m_world)
		return {};
	auto it = m_mirrors.find(*m_driving);
	if (it == m_mirrors.end())
		return {};
	return m_world->contacts(it->second.id);
}

std::optional<std::pair<v3f, v3f>> Body::grabbed(u16 object) const
{
	if (m_grabbing != object && m_driving != object && !m_coasting.count(object))
		return std::nullopt;
	const physics::Pose p = m_world->pose(m_mirrors.at(object).id);
	return std::make_pair(v3f::from(p.position * BS), workshop::rotationOf(p.rotation));
}

std::optional<v3f> Body::gripAt(u16 object) const
{
	if (m_grabbing != object)
		return std::nullopt;
	const std::optional<v3d> at = m_world->gripAt(m_mirrors.at(object).id, ME);
	return at ? std::optional<v3f>(v3f::from(*at * BS)) : std::nullopt;
}

std::optional<std::pair<v3f, v3f>> Body::carried(u16 object) const
{
	if (!m_carrying || m_carrying->first != object)
		return std::nullopt;
	const physics::Pose p = m_world->pose(m_mirrors.at(object).id);
	return std::make_pair(v3f::from(p.position * BS), workshop::rotationOf(p.rotation));
}

std::string Body::readout() const
{
	static const char *const STANCES[] = { "stand", "crouch", "prone", "slide", "roll", "vault", "mantle", "hang", "wall run", "down" };
	std::ostringstream os;
	os << "body: " << STANCES[(size_t)m_stance];
	if (m_me) {
		const v3f v = m_world->velocity(*m_me);
		os << " | speed: " << std::hypot(v.X, v.Z) << " nodes/s | rising: " << v.Y << " nodes/s";
	}
	os << " | stagger: " << m_stagger << " | flight tier: " << m_tier << ", " << m_above_land << " over the land";
	return os.str();
}
