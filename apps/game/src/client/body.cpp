// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "body.h"

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

Body::~Body() = default;

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
	const bool on_wall = m_climb || m_hanging || m_wall_run;
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
	m_move_ended = true;
	m_stance = m_world->resize(*m_me, m_standing_height) ? physics::Stance::STAND : physics::Stance::CROUCH;
}

std::optional<float> Body::wallFace(float yaw) const
{
	const std::optional<physics::Ledge> &l = m_climb ? std::optional<physics::Ledge>(m_climb->ledge) : m_hanging;
	if (!l)
		return std::nullopt;
	float a = yaw - std::atan2(l->normal.X, -l->normal.Z);
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
		m_wall_run = WallRun{ *wall, across - *wall * across.dotProduct(*wall), physics::WALL_RUN_LIFT, physics::WALL_RUN_TIME };
		m_stance = physics::Stance::WALL_RUN;
		m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
	} else {
		m_kick = physics::wallKick(velocity, *wall, jump);
	}
}

v3f Body::wallStep(const physics::World::CharacterState &was, const physics::Here &h, float jump)
{
	WallRun &run = *m_wall_run;
	const float dt = (float)physics::STEP;
	const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
	const std::optional<v3f> wall = physics::findWall(solid, v3f::from(was.foot), m_radius);
	run.left -= dt;
	run.up += h.gravity.Y * physics::WALL_RUN_GRAVITY * dt;
	const v3f velocity = run.along + v3f(0, run.up, 0);
	if (m_jump_press > 0) { // off it, and on
		m_jump_press = 0;
		m_jump_used = true;
		onItsFeet();
		return physics::wallKick(velocity, m_kicked.value_or(v3f(0, 0, 0)), jump);
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
	const std::optional<physics::Ledge> ledge = physics::findLedge(solid, foot, yaw);
	std::optional<physics::Climb> kind = ledge ? physics::climbFor(*ledge, m_base, foot.Y) : std::nullopt;
	if (was.on_ground) {
		// from the ground, only a vault, and only on the way to it: stood beside a wall,
		// Space is a jump. The press is spent either way: the next is what grabs.
		const v3f across(velocity.X, 0, velocity.Z);
		const bool heading = ledge && across.dotProduct(ledge->normal * -1.f) > 1;
		if (kind != physics::Climb::VAULT || !heading)
			kind = std::nullopt;
		m_jump_press = 0;
	}
	if (!kind)
		return;
	m_jump_press = 0;
	m_jump_used = true;
	const float pace = v3f(velocity.X, 0, velocity.Z).getLength();
	Climbing c{ *kind, *ledge, physics::climbPath(*kind, *ledge, foot, m_radius, pace) };
	// a vault runs on as it came in; a mantle ends standing
	const v3f in = ledge->normal * -1.f;
	if (*kind == physics::Climb::VAULT)
	{
		c.exit = in * std::max(pace, 2.f);
		// coming in on the slant, it goes over sideways, legs swung past a hand
		if (pace > 0.1f) {
			const v3f way = v3f(velocity.X, 0, velocity.Z) / pace;
			const float off = std::acos(std::clamp(way.dotProduct(in), -1.f, 1.f));
			if (off > 0.6f) // (about 35°)
				c.side = (way.X * in.Z - way.Z * in.X) > 0 ? 1.2f : -1.2f;
		}
	}
	m_stance = *kind == physics::Climb::VAULT ? physics::Stance::VAULT :
			*kind == physics::Climb::MANTLE ? physics::Stance::MANTLE : physics::Stance::HANG;
	m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
	m_climb = c;
}

v3f Body::climbStep(LocalPlayer &p, const v3d &foot)
{
	const float dt = (float)physics::STEP;
	if (m_climb) {
		m_climb->t += dt;
		const bool done = m_climb->t >= m_climb->path.time;
		const v3f at = m_climb->path.at(m_climb->t);
		const v3f exit = done ? m_climb->exit : v3f(0);
		m_world->place(*m_me, v3d::from(at), exit);
		if (done) {
			if (m_climb->kind == physics::Climb::HANG) {
				m_hanging = m_climb->ledge;
				m_climb.reset();
			} else {
				onItsFeet();
			}
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
		const v3s16 top(std::floor(ledge.lip.X - ledge.normal.X * 0.25f + 0.5f), std::floor(ledge.lip.Y + 1), std::floor(ledge.lip.Z - ledge.normal.Z * 0.25f + 0.5f));
		if (!solidAt(top) && !solidAt(top + v3s16(0, 1, 0))) {
			m_stance = physics::Stance::MANTLE;
			m_world->resize(*m_me, physics::stanceInfo(m_stance).height);
			Climbing c{ physics::Climb::MANTLE, ledge, physics::climbPath(physics::Climb::MANTLE, ledge, v3f::from(foot), m_radius) };
			c.path.time = 0.65f; // the whole body's weight, from straight arms
			m_hanging.reset();
			m_climb = c;
			return v3f(0);
		}
	}
	// along the edge, as far as it goes: A and D as they are from where it looks
	const PlayerControl &c = p.control;
	const float yaw = p.getYaw() * core::DEGTORAD;
	const float sc = std::cos(yaw), ss = std::sin(yaw);
	const float right = std::sin(c.movement_direction) * c.movement_speed, ahead = std::cos(c.movement_direction) * c.movement_speed;
	const v3f ask(right * sc - ahead * ss, 0, right * ss + ahead * sc);
	const v3f along(ledge.normal.Z, 0, -ledge.normal.X);
	const v3f moved = ledge.lip + along * (ask.dotProduct(along) * physics::SHIMMY * dt);
	const v3f lookout = moved + along * (ask.dotProduct(along) > 0 ? m_radius : -m_radius);
	const physics::Solid solid = [this](v3s16 n) { return solidAt(n); };
	if (physics::ledgeGoesOn(solid, lookout, ledge.normal))
		m_hanging->lip = moved;
	const v3f hung(m_hanging->lip.X + ledge.normal.X * (m_radius + 0.05f), ledge.lip.Y - physics::HANG_DROP,
			m_hanging->lip.Z + ledge.normal.Z * (m_radius + 0.05f));
	m_world->place(*m_me, v3d::from(hung), v3f(0));
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
			return;
		}
	}
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
bool Body::aboveAllGround(const v3f &at) const
{
	if (!m_atlas)
		return true;
	const std::optional<planet::Atlas::Cell> c = m_atlas->cell(planet::Atlas::moverNode(at.X, at.Y, at.Z));
	if (!c)
		return true; // seam, or over the sky's top: no ground to meet
	const double radius = m_atlas->body(c->body).params().radius;
	return c->k > 25000 * std::sqrt(radius / 6371000);
}

void Body::move(LocalPlayer &p, float dtime, std::vector<CollisionInfo> *collisions)
{
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
	// flying through the sky faster than the map arrives: what isn't here
	// yet is air (walking, it's waited for)
	// (only above where any ground could be: lower, flying waits for it too,
	// or the floor is flown through before it arrives)
	m_world->unknownIsAir(*m_me, m_thrusting && aboveAllGround(at / BS));
	mirror(p);
	m_moved = true;

	const PlayerControl &c = p.control;
	const PlayerPhysicsOverride &o = p.physics_override;
	m_asked = c.prone ? physics::Stance::PRONE : c.sneak ? physics::Stance::CROUCH : physics::Stance::STAND;

	m_behind += dtime;
	while (m_behind >= physics::STEP) {
		const physics::World::CharacterState was = m_world->character(*m_me);
		m_roll_left = std::max(0.f, m_roll_left - (float)physics::STEP);
		m_jump_press = std::max(0.f, m_jump_press - (float)physics::STEP);
		if (was.on_ground)
			m_base = (float)was.foot.Y;
		else
			m_base = std::min(m_base, (float)was.foot.Y);
		if (!c.jump)
			m_jump_used = false;
		if (was.on_ground)
			m_kicked.reset();
		const bool climbing = m_climb || m_hanging || m_wall_run;
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
			std::sin(c.movement_direction) * c.movement_speed, p.getYaw() * core::DEGTORAD, c.jump && held.jumps && !m_jump_used };
		// in the air, its motion carried to where it is now (the grid's axes turn)
		const v3f v = was.on_ground ? was.velocity : m_field->carry(m_measured, was.foot, was.velocity);
		m_measured = was.foot;
		const physics::Here h = here(was.foot, v);
		if (!m_thrusting && !climbing && m_stance == physics::Stance::STAND && m_jump_press > 0) {
			startClimb(was, v, p.getYaw() * core::DEGTORAD);
			// nothing to take hold of, in the air: a wall to run on or kick off
			if (m_jump_press > 0 && !was.on_ground)
				airWall(was, v, std::sqrt(2 * std::max(0.f, -h.gravity.Y) * JUMP * o.jump));
		}
		v3f next;
		if (m_wall_run) {
			next = wallStep(was, h, std::sqrt(2 * std::max(0.f, -h.gravity.Y) * JUMP * o.jump));
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
			if (m_stance == physics::Stance::SLIDE && stance_was != physics::Stance::SLIDE) {
				const float pace = std::hypot(next.X, next.Z);
				if (pace > 0.01f && pace < physics::SLIDE_KICK) {
					next.X *= physics::SLIDE_KICK / pace;
					next.Z *= physics::SLIDE_KICK / pace;
				}
			}
			if (m_stumble) { // a hard landing with no roll: most of its speed is gone
				next.X *= physics::STUMBLE;
				next.Z *= physics::STUMBLE;
				m_stumble = false;
			}
			if (edged && was.on_ground && !was.standing_on && !intent.jump)
				next = keepOnEdge(was.foot, next);
			if (!was.on_ground) // falling: the air holds it back
				next -= v * (0.5f * h.air * v.getLength() * DRAG_AREA / MASS * (float)physics::STEP);
		}
		if (m_kick) { // off a wall: this, not what walking made of the old motion
			next = *m_kick;
			m_kick.reset();
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
		if (!was.on_ground && now.on_ground && !climbing && !m_climb && !m_hanging && !m_wall_run) {
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
			} else if (fall >= physics::HARD_LANDING) {
				m_stumble = true;
				m_late_roll = physics::ROLL_LATE;
			}
			if (fall >= physics::HARD_LANDING) // (no roll yet in play: this says why)
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
			m_stance == physics::Stance::HANG ? 3 : m_stance == physics::Stance::WALL_RUN ? 4 : 0;
	m_crouch_press = false;
	push(dtime);
	const physics::World::CharacterState now = m_world->character(*m_me);
	const v3d drawn = m_was + (m_now - m_was) * (m_behind / physics::STEP);
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
