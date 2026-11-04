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
constexpr float SMALL_MIRRORED = 6;
constexpr int NEW_MIRRORS_A_FRAME = 2;
// the player's key for what they grab (one world, one grabber)
constexpr unsigned ME = 1;
// A person's weight (physics::Mover's body), against the air when thrusting.
constexpr float MASS = 75;
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
	const bool on_wall = m_mover && m_mover->busy();
	const bool walks = !p.getParent() && !fly && !noclip && !p.in_liquid && !p.is_climbing;
	// Luanti's movement has no stances: on its feet as it's handed over
	if (!walks && on_wall)
		m_mover->onItsFeet();
	if (!walks && m_mover)
		m_mover->standUp();
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
	m_mover.reset();
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
	Client *client = m_client;
	m_world = std::make_unique<physics::World>(physics::World::Opts{
			[this](const v3d &at, const v3f &velocity) { return here(at, velocity); },
			[client](v3s16 block, std::vector<aabb3f> &boxes) {
				return workshop::groundBoxes(client->getEnv().getMap(), client->ndef(), block, boxes);
			},
			[client](const v3d &at) { return workshop::surfaceGrip(client->getEnv().getMap(), client->ndef(), client->getMaterials(), at); } });
	if (m_carrying)
		m_carrying.reset(); // its mirror's gone with the world; Game lets it go
	m_grabbing.reset();
	m_driving.reset();
	return true;
}

void Body::nudge(v3f by)
{
	if (!m_mover)
		return; // not walking yet: Luanti's movement had it
	const physics::World::CharacterState c = m_world->character(m_mover->id());
	m_mover->place(c.foot, c.velocity + by);
	m_set = v3f::from(c.foot * BS);
}

void Body::groundChanged(v3s16 block)
{
	if (m_world)
		m_world->groundChanged(block);
}

void Body::mirror(LocalPlayer &p)
{
	ScopeProfiler timed(g_profiler, "Body mirror [us]", SPT_GRAPH_ADD, PRECISION_MICRO);
	int made = 0;
	std::vector<DistanceSortedActiveObject> near;
	m_client->getEnv().getActiveObjects(p.getPosition(), MIRRORED * BS, near);
	std::map<u16, GenericCAO *> seen;
	for (const DistanceSortedActiveObject &d : near) {
		auto *cao = dynamic_cast<GenericCAO *>(d.obj);
		if (!cao || !cao->getMachine())
			continue;
		// a small thing's only in the way underfoot
		if (cao->getMachine()->scale() < 1 && cao->getPosition().getDistanceFrom(p.getPosition()) > SMALL_MIRRORED * BS)
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
		// a crowd come into range is taken a few a frame
		if (it == m_mirrors.end() && made >= NEW_MIRRORS_A_FRAME) {
			seen.erase(id);
			continue;
		}
		made += it == m_mirrors.end();
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
	if (!m_mover) {
		const float radius = std::max({ -box.MinEdge.X, box.MaxEdge.X, -box.MinEdge.Z, box.MaxEdge.Z }) / BS;
		physics::Mover::Opts opts{ *m_world,
			[this](v3s16 n) { return solidAt(n); },
			[this](v3s16 n) {
				const MapNode node = m_client->getEnv().getMap().getNode(n);
				return node.getContent() == CONTENT_IGNORE || m_client->ndef()->get(node).walkable;
			},
			[this](const v3d &foot, v3f velocity) { return here(foot, velocity); },
			[this](const v3d &from, const v3d &to, v3f velocity) { return m_field->carry(from, to, velocity); },
			[this](const v3d &was, const v3d &now, const v3d &measured) -> std::optional<v3d> {
				const std::optional<planet::Atlas::Carried> c = m_atlas ?
						m_atlas->carry(was.X, was.Y, was.Z, now.X, now.Y, now.Z) : std::nullopt;
				if (!c)
					return std::nullopt;
				// over a seam: into the grid it's in now, the same motion in its
				// axes, the view turned with the ground
				const v3d there(c->x, c->y, c->z);
				m_world->place(m_mover->id(), there, m_field->carry(measured, there, m_world->character(m_mover->id()).velocity));
				m_player->turnWithGround(c->angle);
				return there;
			},
			[](const std::string &what) { actionstream << "body: " << what << std::endl; } };
		m_mover = std::make_unique<physics::Mover>(std::move(opts), radius, box.getExtent().Y / BS, v3d::from(at / BS));
		m_set = at;
	}
	m_player = &p;
	const physics::World::CharId me = m_mover->id();
	if (at.getDistanceFrom(m_set) > 0.001f * BS) {
		// put elsewhere since (by the server, Luanti's movement, a seam's
		// crossing): from there
		m_mover->place(v3d::from(at / BS), p.getSpeed() / BS);
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
	m_world->unknownIsAir(me, m_clear_of_land);
	// ... and it asks for only as much of the map as is any use at this speed
	m_tier = m_clear_of_land ? physics::flightTier(p.getSpeed().getLength() / BS, m_above_land, m_tier) : 0;
	m_world->skipsGround(me, m_tier > 0);
	m_world->keepsSpeed(me, m_thrusting); // (a flight held up by the map goes on as it was)
	mirror(p);
	m_moved = true;

	const PlayerControl &c = p.control;
	const PlayerPhysicsOverride &o = p.physics_override;
	physics::Mover::Input in;
	in.ahead = std::cos(c.movement_direction) * c.movement_speed;
	in.right = std::sin(c.movement_direction) * c.movement_speed;
	in.yaw = p.getYaw() * core::DEGTORAD;
	in.jump = c.jump;
	in.asked = c.prone ? physics::Stance::PRONE : c.sneak ? physics::Stance::CROUCH : physics::Stance::STAND;
	in.walk = p.movement_speed_walk * o.speed_walk / BS;
	in.run = o.speed;
	in.jump_height = JUMP * o.jump;
	in.thrust = [this, &p](v3f velocity, const physics::Here &h, float dt) { return thrust(p, velocity, h, dt); };

	m_behind += dtime;
	while (m_behind >= physics::STEP) {
		in.thrusting = m_thrusting;
		const physics::Mover::Stepped stepped = m_mover->step(in);
		m_behind -= physics::STEP;
		if (stepped.stopped_flying) {
			m_thrusting = false;
			g_settings->setBool("free_move", false);
		}
		if (stepped.landed && collisions && !stepped.rolled) { // (a roll takes the fall)
			// landed: as a collision with the ground, so a hard one hurts
			CollisionInfo landed;
			landed.type = COLLISION_NODE;
			landed.axis = COLLISION_AXIS_Y;
			landed.node_p = floatToInt(v3f::from(stepped.foot) * BS - v3f(0, 0.05f * BS, 0), BS);
			landed.old_speed = stepped.fell_at * BS;
			landed.new_speed = stepped.velocity * BS;
			collisions->push_back(landed);
		}
	}
	// what others are told is what it really is, and the camera follows it
	const physics::Stance stance = m_mover->stance();
	p.control.sneak = !m_thrusting && stance == physics::Stance::CROUCH;
	p.control.prone = !m_thrusting && stance == physics::Stance::PRONE;
	p.control.slide = stance == physics::Stance::SLIDE;
	p.control.roll = stance == physics::Stance::ROLL;
	p.control.climb = stance == physics::Stance::VAULT ? 1 : stance == physics::Stance::MANTLE ? 2 :
			stance == physics::Stance::HANG ? 3 : stance == physics::Stance::WALL_RUN ? 4 : stance == physics::Stance::DOWN ? 5 : 0;
	push(dtime);
	const physics::World::CharacterState now = m_world->character(me);
	const v3d drawn = m_mover->was() + (m_mover->now() - m_mover->was()) * (m_behind / physics::STEP) - v3d(0, m_mover->lift(), 0);
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
	if (!m_mover)
		return std::nullopt;
	const physics::World::CharacterState c = m_world->character(m_mover->id());
	return m_field->elements(c.foot, c.velocity);
}

std::optional<std::vector<orbit::Vec>> Body::path(int points) const
{
	if (!m_mover)
		return std::nullopt;
	const physics::World::CharacterState c = m_world->character(m_mover->id());
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
	for (const auto &[id, impulse] : m_world->pushes(m_mover->id()))
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
	if (m_mover) {
		m_world->removeCharacter(m_mover->id());
		m_mover.reset();
	}
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
	if (!m_world || !m_mover)
		return std::nullopt;
	const std::optional<physics::World::Id> on = m_world->character(m_mover->id()).standing_on;
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
	os << "body: " << STANCES[(size_t)stance()];
	if (m_mover) {
		const v3f v = m_world->velocity(m_mover->id());
		os << " | speed: " << std::hypot(v.X, v.Z) << " nodes/s | rising: " << v.Y << " nodes/s";
	}
	os << " | stagger: " << stagger() << " | flight tier: " << m_tier << ", " << m_above_land << " over the land";
	return os.str();
}
