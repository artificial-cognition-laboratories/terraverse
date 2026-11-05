// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "actor.h"
#include <algorithm>
#include <cmath>

namespace physics {

namespace {

// A person, as the game's player box and tf_player's sprint have it.
constexpr float RADIUS = 0.3f, HEIGHT = 1.77f, SPRINT = 1.6f, GRAVITY = 9.81f, AIR = 1.2f;

} // namespace

const char *stanceName(Stance stance)
{
	static const char *const NAMES[] = { "stand", "crouch", "prone", "slide", "roll", "vault", "mantle", "hang", "wall run", "down" };
	return NAMES[(size_t)stance];
}

Actor::Actor(Boxes boxes, const v3d &foot, std::vector<Cue> cues) : m_boxes(std::move(boxes)), m_cues(std::move(cues))
{
	World::Opts opts;
	opts.field = [](const v3d &, const v3f &) { return Here{ v3f(0, -GRAVITY, 0), AIR }; };
	opts.ground = [this](v3s16 block, std::vector<aabb3f> &out) {
		std::vector<aabb3f> node;
		for (s16 x = 0; x < BLOCK; x++)
			for (s16 y = 0; y < BLOCK; y++)
				for (s16 z = 0; z < BLOCK; z++) {
					const v3s16 at(block.X * BLOCK + x, block.Y * BLOCK + y, block.Z * BLOCK + z);
					node.clear();
					m_boxes(at, node);
					const v3f by(x, y, z);
					for (aabb3f b : node) {
						b.MinEdge += by;
						b.MaxEdge += by;
						out.push_back(b);
					}
				}
		return true;
	};
	m_world = std::make_unique<World>(opts);
	const auto solidAt = [this](v3s16 n) { return solid(n); };
	Mover::Opts mover{ *m_world, solidAt, solidAt,
		[](const v3d &, v3f) { return Here{ v3f(0, -GRAVITY, 0), AIR }; },
		[](const v3d &, const v3d &, v3f velocity) { return velocity; },
		nullptr,
		[this](const std::string &what) { note(what.substr(0, what.find(" at "))); } };
	m_mover = std::make_unique<Mover>(std::move(mover), RADIUS, HEIGHT, foot);
	m_drawn = v3f::from(foot);
	m_doing = stanceName(Stance::STAND);
	poseAndDrive(STEP, 0);
	m_bones = m_limbs->drawn(foot, m_yaw, 1);
}

Actor::~Actor() { m_limbs.reset(); }

bool Actor::solid(v3s16 n) const
{
	std::vector<aabb3f> boxes;
	m_boxes(n, boxes);
	float volume = 0;
	for (const aabb3f &b : boxes) {
		const v3f e = b.getExtent();
		volume += e.X * e.Y * e.Z;
	}
	return volume >= 0.5f; // as Map::solidShape's half
}

void Actor::note(const std::string &what)
{
	m_times[what]++;
	m_did.push_back(what);
	m_doing = what;
}

float Actor::pace() const
{
	const v3f v = m_world->character(m_mover->id()).velocity;
	return std::hypot(v.X, v.Z);
}

float Actor::groundAt(v3f at) const
{
	// as GenericCAO::groundUnder: from a node up, two down
	const v3s16 from(std::floor(at.X + 0.5f), std::floor(at.Y + 0.5f), std::floor(at.Z + 0.5f));
	std::vector<aabb3f> boxes;
	for (s16 y = from.Y + 1; y >= from.Y - 2; y--) {
		boxes.clear();
		m_boxes(v3s16(from.X, y, from.Z), boxes);
		float top = -1e9f;
		for (const aabb3f &b : boxes)
			if (at.X - from.X >= b.MinEdge.X && at.X - from.X <= b.MaxEdge.X && at.Z - from.Z >= b.MinEdge.Z && at.Z - from.Z <= b.MaxEdge.Z)
				top = std::max(top, b.MaxEdge.Y);
		if (top > -1e8f)
			return y + top;
	}
	return from.Y - 2.5f;
}

v3f Actor::clearOf(v3f p, float r) const
{
	// a ball against the whole nodes round it, as content_cao's
	v3f by;
	const v3s16 c(std::floor(p.X + 0.5f), std::floor(p.Y + 0.5f), std::floor(p.Z + 0.5f));
	for (s16 x = c.X - 1; x <= c.X + 1; x++)
		for (s16 y = c.Y - 1; y <= c.Y + 1; y++)
			for (s16 z = c.Z - 1; z <= c.Z + 1; z++) {
				if (!solid(v3s16(x, y, z)))
					continue;
				const v3f near(std::clamp(p.X, x - 0.5f, x + 0.5f), std::clamp(p.Y, y - 0.5f, y + 0.5f), std::clamp(p.Z, z - 0.5f, z + 0.5f));
				const v3f off = p - near;
				const float d = off.getLength();
				if (d >= r || d < 1e-4f)
					continue;
				const v3f out = off / d * (r - d);
				if (out.getLengthSQ() > by.getLengthSQ())
					by = out;
			}
	return by;
}

float Actor::sunk() const
{
	const float c = std::cos(m_yaw), s = std::sin(m_yaw);
	float deepest = 0;
	std::vector<aabb3f> boxes;
	const auto point = [&](v3f f) {
		const v3f w = m_drawn + v3f(f.X * c - f.Z * s, f.Y, f.X * s + f.Z * c);
		const v3s16 n(std::floor(w.X + 0.5f), std::floor(w.Y + 0.5f), std::floor(w.Z + 0.5f));
		const v3f in = w - v3f(n.X, n.Y, n.Z);
		boxes.clear();
		m_boxes(n, boxes);
		for (const aabb3f &b : boxes)
			if (b.isPointInside(in))
				deepest = std::max(deepest, std::min({ in.X - b.MinEdge.X, b.MaxEdge.X - in.X, in.Y - b.MinEdge.Y, b.MaxEdge.Y - in.Y,
						in.Z - b.MinEdge.Z, b.MaxEdge.Z - in.Z }));
	};
	eachBoneBox(m_bones, [&](const core::matrix4 &m, v3f size, v3f centre, BoneColour) {
		for (const float x : { -0.5f, 0.5f })
			for (const float y : { -0.5f, 0.5f })
				for (const float z : { -0.5f, 0.5f })
					point(m.transformVect(centre + v3f(size.X * x, size.Y * y, size.Z * z)));
	});
	return deepest;
}

v3f Actor::head() const
{
	return (m_bones.lower.matrix() * m_bones.hips.matrix() * m_bones.chest.matrix() * m_bones.head.matrix()).getTranslation();
}

// new presses, as the game's keys send them
void Actor::press(const Cue &next)
{
	if (next.jump && !m_on.jump)
		m_mover->pressJump();
	if (next.crouch && !m_on.crouch)
		m_mover->pressCrouch();
	m_on = next;
}

int Actor::times(const std::string &what) const
{
	const auto found = m_times.find(what);
	return found == m_times.end() ? 0 : found->second;
}

void Actor::update(float dt)
{
	if (dt <= 0)
		return;
	m_behind += dt;
	while (m_behind >= STEP) {
		m_time += STEP;
		m_behind -= STEP;
		while (m_cue < m_cues.size() && m_cues[m_cue].at <= m_time) {
			press(m_cues[m_cue++]);
		}
		const float was_yaw = m_yaw;
		m_yaw = m_on.yaw;
		// what's asked of the body for this step, from where it stands now
		poseAndDrive(STEP, -std::remainder(m_yaw - was_yaw, 2 * core::PI));
		Mover::Input in;
		in.ahead = m_on.ahead;
		in.right = m_on.right;
		in.yaw = m_on.yaw;
		in.jump = m_on.jump;
		in.asked = m_on.prone ? Stance::PRONE : m_on.crouch ? Stance::CROUCH : Stance::STAND;
		in.run = m_on.run ? SPRINT : 1;
		const Stance before = m_mover->stance();
		m_mover->step(in);
		m_limbs->read();
		m_mover->bodyAt(m_limbs->hips());
		m_mover->feel(m_limbs->felt());
		const Stance after = m_mover->stance();
		if (after != before)
			note(stanceName(after));
		if (m_did.empty() && pace() > 0.1f)
			m_doing = "moving";
	}
	const v3d drawn = m_mover->was() + (m_mover->now() - m_mover->was()) * (m_behind / STEP) - v3d(0, m_mover->lift(), 0);
	m_drawn = v3f::from(drawn);
	if (m_limbs)
		m_bones = m_limbs->drawn(drawn, m_yaw, (float)(m_behind / STEP));
}

void Actor::poseAndDrive(float dt, float turned)
{
	const v3d foot = m_mover->now() - v3d(0, m_mover->lift(), 0);
	const v3f at = v3f::from(foot);
	// into the figure's frame: +x its right, +z ahead (Body's axes, as content_cao's turning matrix has them)
	const float c = std::cos(m_yaw), s = std::sin(m_yaw);
	const auto toFigure = [&](v3f w) { return v3f(w.X * c + w.Z * s, w.Y, -w.X * s + w.Z * c); };
	const auto toWorld = [&](v3f f) { return v3f(f.X * c - f.Z * s, f.Y, f.X * s + f.Z * c); };
	const World::CharacterState now = m_world->character(m_mover->id());
	const std::optional<float> face = m_mover->wallFace(m_yaw);
	v3f moving = face ? m_mover->climbSpeed() : now.velocity;
	std::optional<Grip> grip = m_mover->grip();
	if (grip) {
		for (const int side : { 0, 1 })
			grip->wrist[side] = toFigure(grip->wrist[side] - at);
		grip->over = toFigure(grip->over);
	}
	Contacts contacts;
	contacts.grip = grip ? &*grip : nullptr;
	contacts.push = toFigure(m_mover->kickPush());
	if (const std::optional<v3f> on = m_mover->stepOn())
		contacts.step_on = toFigure(*on - at);
	contacts.feet = m_mover->feetUsed();
	contacts.fallen = m_mover->fallenAngle();
	contacts.rising = m_mover->risingAt();
	contacts.stagger = m_mover->stagger();
	contacts.throw_side = m_mover->thrownSide();
	contacts.throw_back = m_mover->thrownBack();
	const Bones asked = m_posing.pose(dt, toFigure(moving), 0, !now.on_ground, m_mover->stance(), turned, face, m_mover->wallLean(m_yaw), contacts,
			[&](v3f f) { return groundAt(at + toWorld(f)) - at.Y; },
			[&](v3f p, float r) { return toFigure(clearOf(at + toWorld(p), r)); });
	// Down it lets go of where the pose is, keeping only its shape, and takes hold again as it gets up.
	constexpr float HELD = 0.9f, TONE = 0.3f;
	const float wanted = m_mover->stance() != Stance::DOWN ? HELD : HELD * contacts.rising;
	m_stiff = wanted < m_stiff ? wanted : std::min(wanted, m_stiff + dt * 4);
	if (!m_limbs) {
		m_limbs = std::make_unique<Limbs>(*m_world, asked, foot, m_yaw, now.velocity);
		m_limbs->read();
	}
	m_limbs->drive(asked, foot, m_yaw, m_stiff, TONE);
}

} // namespace physics
