// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "physics/motion.h"
#include "physics/skeleton.h"
#include "physics/motion_cycles.h"
#include "physics/motion_clips.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace physics {

namespace {

using namespace cycles;
constexpr float S = FIGURE_SCALE;
constexpr float LEG = (THIGH + SHIN) * S; // nodes: ours, what a recorded leg's length is made

// The recorded strides a body's going between, and how much of each: by its pace (each is all there is at its own,
// nodes/s, and the two either side shared out between), and by which way it's going against the way it faces
// (ahead, to a side, back: the two either side of it). A jog from a stroll up: a person going at the pace we call
// walking is jogging; faster, a run, then a sprint, each off the ground longer and further each step, not just
// quicker (sprint 105: one jog played faster was nine steps a second). Every stride's cut where its right foot
// comes down, so any two are at the same point at the same phase.
struct Mix
{
	const Cycle *c[8];
	float w[8];
	int n = 0;
	void add(const Cycle *k, float weight)
	{
		if (weight <= 1e-4f)
			return;
		for (int i = 0; i < n; i++)
			if (c[i] == k) {
				w[i] += weight;
				return;
			}
		c[n] = k;
		w[n++] = weight;
	}
	template <typename F> float of(F f) const
	{
		float sum = 0;
		for (int i = 0; i < n; i++)
			sum += w[i] * f(*c[i]);
		return sum;
	}
	const Cycle &most() const
	{
		int best = 0;
		for (int i = 1; i < n; i++)
			if (w[i] > w[best])
				best = i;
		return *c[best];
	}
};

struct Key
{
	const Cycle *c;
	float at; // nodes/s
};
struct Ways
{
	std::vector<Key> ahead, right, back, left;
};
const Ways &waysFor(bool crouched)
{
	static const Ways STOOD{ { { &WALK, 0.8f }, { &JOG, 2.3f }, { &JOG, 3.5f }, { &RUN, 4.6f }, { &SPRINT, 5.8f } },
		{ { &WALK_RIGHT, 0.8f }, { &JOG_RIGHT, 2.3f } }, { { &WALK_BACK, 0.8f }, { &JOG_BACK, 2.3f } },
		{ { &WALK_LEFT, 0.8f }, { &JOG_LEFT, 2.3f } } };
	static const Ways CROUCHED{ { { &CROUCH, 0.5f }, { &CROUCH_RUN, 1.5f } }, { { &CROUCH_RIGHT, 0.5f } }, { { &CROUCH_BACK, 0.5f } },
		{ { &CROUCH_LEFT, 0.5f } } };
	return crouched ? CROUCHED : STOOD;
}

void addPaced(Mix &m, const std::vector<Key> &keys, float pace, float weight)
{
	size_t k = 0;
	while (k + 2 < keys.size() && pace > keys[k + 1].at)
		k++;
	if (keys.size() == 1) {
		m.add(keys[0].c, weight);
		return;
	}
	const float t = std::clamp((pace - keys[k].at) / (keys[k + 1].at - keys[k].at), 0.f, 1.f);
	m.add(keys[k].c, weight * (1 - t));
	m.add(keys[k + 1].c, weight * t);
}

// `way`: radians it's going, against how it faces (0 ahead, + to its right).
Mix mixAt(float pace, float way, bool crouched)
{
	const Ways &ways = waysFor(crouched);
	const std::vector<Key> *round[4] = { &ways.ahead, &ways.right, &ways.back, &ways.left };
	const float quarter = std::fmod(way / (core::PI / 2) + 4, 4.f);
	const int i = (int)quarter % 4;
	const float t = quarter - std::floor(quarter);
	Mix m;
	addPaced(m, *round[i], pace, 1 - t);
	addPaced(m, *round[(i + 1) % 4], pace, t);
	return m;
}

// The hips' height, as recorded, at a phase (legs); and over the whole stride.
v3f sample(const Cycle &c, float phase, int joint);

float hipsAt(const Cycle &c, float phase)
{
	return sample(c, phase, HIPS).Y;
}

float meanHips(const Cycle &c)
{
	float sum = 0;
	for (int p = 0; p < PHASES; p++)
		sum += c.at[p][HIPS][1];
	return sum / PHASES;
}

// A smooth curve through the recorded phases (Catmull-Rom), not straight between them: straight, it changed how
// it was going all at once at each one, and played quickly that's a jolt (sprint 105).
float through(float a, float b, float c, float d, float t)
{
	return b + 0.5f * t * (c - a + t * (2 * a - 5 * b + 4 * c - d + t * (3 * (b - c) + d - a)));
}

v3f sample(const Cycle &c, float phase, int joint)
{
	const float f = std::fmod(phase * PHASES, (float)PHASES);
	const int i = (int)f;
	const float t = f - i;
	const auto at = [&](int k) { return c.at[(k + PHASES) % PHASES][joint]; };
	v3f out;
	for (int k = 0; k < 3; k++)
		(&out.X)[k] = through(at(i - 1)[k], at(i)[k], at(i + 1)[k], at(i + 2)[k], t);
	return out;
}

core::matrix4 axes(v3f x, v3f y, v3f z)
{
	core::matrix4 m;
	m[0] = x.X; m[1] = x.Y; m[2] = x.Z;
	m[4] = y.X; m[5] = y.Y; m[6] = y.Z;
	m[8] = z.X; m[9] = z.Y; m[10] = z.Z;
	return m;
}

// A bone that hangs along -y, pointed from its joint toward `to`, its x as near `across` as that allows.
core::matrix4 aim(v3f along, v3f across)
{
	v3f y = along * -1.f;
	y.normalize();
	v3f x = across - y * across.dotProduct(y);
	if (x.getLengthSQ() < 1e-6f)
		x = v3f(1, 0, 0) - y * y.X;
	x.normalize();
	return axes(x, y, x.crossProduct(y));
}

// The way a two-bone limb bends, as its first bone's x: square to both, toward `side`.
v3f bendAxis(v3f first, v3f second, v3f side)
{
	v3f n = first.crossProduct(second);
	if (n.getLengthSQ() < 1e-6f)
		return side;
	n.normalize();
	return n.dotProduct(side) < 0 ? n * -1.f : n;
}

core::matrix4 against(const core::matrix4 &parent, const core::matrix4 &child)
{
	core::matrix4 inv;
	parent.getInverse(inv);
	return inv * child;
}

struct Q
{
	float x, y, z, w;
};

Q toQ(const core::matrix4 &m)
{
	const float xx = m[0], xy = m[1], xz = m[2], yx = m[4], yy = m[5], yz = m[6], zx = m[8], zy = m[9], zz = m[10];
	const float trace = xx + yy + zz;
	if (trace > 0) {
		const float r = 2 * std::sqrt(1 + trace);
		return { (yz - zy) / r, (zx - xz) / r, (xy - yx) / r, r / 4 };
	} else if (xx >= yy && xx >= zz) {
		const float r = 2 * std::sqrt(1 + xx - yy - zz);
		return { r / 4, (xy + yx) / r, (zx + xz) / r, (yz - zy) / r };
	} else if (yy >= zz) {
		const float r = 2 * std::sqrt(1 + yy - xx - zz);
		return { (xy + yx) / r, r / 4, (yz + zy) / r, (zx - xz) / r };
	}
	const float r = 2 * std::sqrt(1 + zz - xx - yy);
	return { (zx + xz) / r, (yz + zy) / r, r / 4, (xy - yx) / r };
}

core::matrix4 fromQ(Q q)
{
	core::matrix4 m;
	m[0] = 1 - 2 * (q.y * q.y + q.z * q.z);
	m[1] = 2 * (q.x * q.y + q.z * q.w);
	m[2] = 2 * (q.x * q.z - q.y * q.w);
	m[4] = 2 * (q.x * q.y - q.z * q.w);
	m[5] = 1 - 2 * (q.x * q.x + q.z * q.z);
	m[6] = 2 * (q.y * q.z + q.x * q.w);
	m[8] = 2 * (q.x * q.z + q.y * q.w);
	m[9] = 2 * (q.y * q.z - q.x * q.w);
	m[10] = 1 - 2 * (q.x * q.x + q.y * q.y);
	return m;
}

BonePlace blend(const BonePlace &a, const BonePlace &b, float t)
{
	if (t <= 0)
		return a;
	if (t >= 1)
		return b;
	core::matrix4 ma, mb;
	ma.setRotationDegrees(a.turn);
	mb.setRotationDegrees(b.turn);
	Q qa = toQ(ma), qb = toQ(mb);
	if (qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w < 0)
		qb = { -qb.x, -qb.y, -qb.z, -qb.w };
	Q q{ qa.x + (qb.x - qa.x) * t, qa.y + (qb.y - qa.y) * t, qa.z + (qb.z - qa.z) * t, qa.w + (qb.w - qa.w) * t };
	const float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	q = { q.x / l, q.y / l, q.z / l, q.w / l };
	return { a.at + (b.at - a.at) * t, fromQ(q).getRotationDegrees() };
}

// A body as recorded joints have it (`at`: each joint against the hips, figure-frame axes, in our nodes), turned
// by `turn`: each bone pointed as the recorded one points, so it doesn't matter that the two skeletons stand
// differently at rest. Its hips at their height stood straight; the rest's `asked`'s.
template <typename At> Bones pointed(const At &at, const Bones &asked, const core::matrix4 &turn)
{
	const v3f hips(0, 0, 0);
	const v3f pelvis_x = at(RIGHTHIP) - at(LEFTHIP), chest_x = at(RIGHTSHOULDER) - at(LEFTSHOULDER);
	const core::matrix4 pelvis = aim(hips - at(CHEST), pelvis_x);
	const core::matrix4 chest = aim(at(CHEST) - at(NECK), chest_x);
	const core::matrix4 head = aim(at(NECK) - at(HEAD_END), chest_x);
	core::matrix4 thigh[2], shin[2], upper[2], fore[2];
	const int HIP[2] = { RIGHTHIP, LEFTHIP }, KNEE[2] = { RIGHTKNEE, LEFTKNEE }, ANKLE[2] = { RIGHTANKLE, LEFTANKLE };
	const int SHOULDER[2] = { RIGHTSHOULDER, LEFTSHOULDER }, ELBOW[2] = { RIGHTELBOW, LEFTELBOW }, WRIST[2] = { RIGHTWRIST, LEFTWRIST };
	for (int side : { 0, 1 }) {
		const v3f up = at(KNEE[side]) - at(HIP[side]), down = at(ANKLE[side]) - at(KNEE[side]);
		const v3f knee = bendAxis(up, down, pelvis_x);
		thigh[side] = aim(up, knee);
		shin[side] = aim(down, knee);
		const v3f arm = at(ELBOW[side]) - at(SHOULDER[side]), forearm = at(WRIST[side]) - at(ELBOW[side]);
		const v3f elbow = bendAxis(arm, forearm, chest_x);
		upper[side] = aim(arm, elbow);
		fore[side] = aim(forearm, elbow);
	}
	const core::matrix4 lower = turn * pelvis;
	Bones b = asked;
	const float hip_y = HIP_HEIGHT * S;
	b.hips = { v3f(0, hip_y, 0), v3f(0) };
	b.lower.turn = lower.getRotationDegrees();
	b.lower.at = v3f(0) - lower.rotateAndScaleVect(v3f(0, hip_y, 0));
	b.chest.turn = against(lower, turn * chest).getRotationDegrees();
	b.head.turn = against(turn * chest, turn * head).getRotationDegrees();
	for (int side : { 0, 1 }) {
		const float x = side ? -1.f : 1.f;
		b.thigh[side] = { v3f(HIP_WIDE * x, HIP_HEIGHT, 0) * S, against(lower, turn * thigh[side]).getRotationDegrees() };
		b.shin[side].turn = against(thigh[side], shin[side]).getRotationDegrees();
		b.upper[side].turn = against(turn * chest, turn * upper[side]).getRotationDegrees();
		b.fore[side].turn = against(upper[side], fore[side]).getRotationDegrees();
		b.hand[side].turn = v3f(0);
	}
	return b;
}

} // namespace

Bones blend(const Bones &a, const Bones &b, float t)
{
	Bones out;
	out.lower = blend(a.lower, b.lower, t);
	out.hips = blend(a.hips, b.hips, t);
	out.chest = blend(a.chest, b.chest, t);
	out.head = blend(a.head, b.head, t);
	for (int side : { 0, 1 }) {
		out.thigh[side] = blend(a.thigh[side], b.thigh[side], t);
		out.shin[side] = blend(a.shin[side], b.shin[side], t);
		out.upper[side] = blend(a.upper[side], b.upper[side], t);
		out.fore[side] = blend(a.fore[side], b.fore[side], t);
		out.hand[side] = blend(a.hand[side], b.hand[side], t);
	}
	return out;
}

namespace {

// The recorded stride at this pace, in nodes, as it's lengthened: what goes by under a foot each stride.
float strideOf(const Mix &m, float pace)
{
	const float recorded_pace = m.of([](const Cycle &c) { return c.pace; }) * LEG;
	const float recorded_stride = m.of([](const Cycle &c) { return c.pace * c.period; }) * LEG;
	return recorded_stride * std::clamp(pace / std::max(recorded_pace, 0.01f), 1.f, Strides::LONGEST);
}

} // namespace

void Strides::leap()
{
	// From where it is in its stride to the next foot coming down after it's off the ground: the flight that's
	// stretched to however long it's in the air
	const Mix g = mixAt(m_pace, m_way - m_body, m_crouched);
	const Cycle &c = g.most();
	const int now = (int)std::lround(m_phase * PHASES);
	const auto none = [&](int p) { return !c.down[p % PHASES][0] && !c.down[p % PHASES][1]; };
	int p = now;
	while (p < now + PHASES && !none(p)) // (to its toes leaving the ground...)
		p++;
	while (p < now + 2 * PHASES && none(p)) // (...and the other foot down)
		p++;
	m_leaping = true;
	m_leap_air = 0;
	m_leap_from = m_phase;
	m_leap_to = m_phase + (float)(p - now) / PHASES;
}

float Strides::headShake() const
{
	const auto most = [](const Cycle &c) {
		const float dt = c.period / PHASES;
		const auto head = [&](int p) {
			p = (p + PHASES) % PHASES;
			v3f h;
			for (int k = 0; k < 3; k++)
				(&h.X)[k] = c.at[p][HIPS][k] + (c.at[p][HEAD][k] + c.at[p][HEAD_END][k]) / 2;
			return h;
		};
		float a = 0;
		for (int p = 0; p < PHASES; p++)
			a = std::max(a, (head(p + 1) - head(p) * 2.f + head(p - 1)).getLength() / (dt * dt));
		return a * LEG;
	};
	const Mix m = mixAt(m_pace, m_way, m_crouched);
	const float recorded = m.of([](const Cycle &c) { return c.period; });
	const float played = strideOf(m, m_pace) / std::max(m_pace, 0.05f);
	const float quicker = recorded / std::max(played, 0.05f);
	return m.of(most) * quicker * quicker;
}

Bones Strides::step(float dt, const Bones &asked, v3f moving, float turned, bool on, bool crouched, bool airborne, float up, float above)
{
	const float pace = std::hypot(moving.X, moving.Z);
	// Off the ground going at a run, it leaps: a stride of its run, its flight as long as it's in the air
	// (sprint 105: a jump as recorded from a run was a long jump's, its arms swung once it was already up)
	// (jumped: rising. Run off an edge, it's falling, and its legs come under it for the ground)
	if (airborne && !m_leaping && on && !crouched && m_share > 0.5f && pace > LEAPS_FROM && up > 1)
		leap();
	if (!airborne || crouched) // (tucked in the air, for a roll: that's the roll's, not a stride's)
		m_leaping = false;
	on = on && (!airborne || m_leaping);
	constexpr float COMES_IN = 5; // /s: it takes over, and lets go, over a fifth of a second
	const float going = std::clamp(pace / 0.6f, 0.f, 1.f);
	m_share += ((on ? going : 0.f) - m_share) * std::min(1.f, COMES_IN * dt);
	if (m_share < 0.005f) {
		m_share = 0;
		m_planted[0] = m_planted[1] = m_locked[0] = m_locked[1] = false;
		m_held[0] = m_held[1] = 0;
		m_height_known = false;
		m_body = 0;
		m_turned_round = false;
		m_leaping = false;
		return asked;
	}
	// which way it's going, against how it faces: held while it's slowing to a stop, not swung about
	const float was_way = m_way;
	if (pace > 0.3f)
		m_way = std::atan2(moving.X, moving.Z);
	// A person steps to a side slowly; faster, they turn and go, its hips toward where it's going as far as
	// they turn, looking where it was (Posing's look) (sprint 105: a sidestep stretched to a run slid its feet)
	constexpr float TURNS_FROM = 2.5f, TURNS_BY = 4.5f, HIPS_TURN = 1.2f; // nodes/s, nodes/s, radians
	const float turns = std::fabs(m_way) < 2.f ? std::clamp((pace - TURNS_FROM) / (TURNS_BY - TURNS_FROM), 0.f, 1.f) : 0.f;
	m_body += (std::clamp(m_way, -HIPS_TURN, HIPS_TURN) * turns - m_body) * std::min(1.f, 6 * dt);
	const float way = m_way - m_body; // against the way its hips face
	m_pace = pace;
	m_crouched = crouched;
	// gone the other way at once (a sidestep the other way): Posing eases it from how it was
	m_turned_round = std::fabs(std::remainder(m_way - was_way, 2 * core::PI)) > 1.f;
	const Mix g = mixAt(pace, way, crouched);
	const float recorded_pace = g.of([](const Cycle &c) { return c.pace; }) * LEG;
	const float stride = strideOf(g, pace);
	// a leap's legs split wider than a stride's, most at the top of it
	constexpr float SPLITS = 1.35f;
	float longer = std::clamp(pace / std::max(recorded_pace, 0.01f), 1.f, LONGEST);
	if (m_leaping) {
		// as far through the stride's flight as it is through its time in the air, by when it'll be down
		constexpr float GRAVITY = 9.81f;
		m_leap_air += dt;
		const float left = (up + std::sqrt(std::max(0.f, up * up + 2 * GRAVITY * above))) / GRAVITY;
		const float through = std::min(m_leap_air / std::max(m_leap_air + left, 1e-3f), 0.98f);
		m_phase = std::fmod(m_leap_from + (m_leap_to - m_leap_from) * through, 1.f);
		longer *= 1 + (SPLITS - 1) * std::sin(core::PI * through);
	} else {
		m_phase = std::fmod(m_phase + dt * pace / std::max(stride, 0.05f), 1.f);
	}

	const v3f along(std::sin(way), 0, std::cos(way));
	const auto at = [&](int joint) {
		v3f p(0);
		for (int i = 0; i < g.n; i++)
			p += sample(*g.c[i], m_phase, joint) * g.w[i];
		// a longer stride: the legs reach further the way it goes, and back, not quicker
		if (joint == RIGHTKNEE || joint == RIGHTANKLE || joint == RIGHTTOE || joint == LEFTKNEE || joint == LEFTANKLE || joint == LEFTTOE)
			p += along * (p.dotProduct(along) * (longer - 1));
		return p;
	};
	// a foot's down when the recording's is: low and still, heel to toe (and not in the air)
	for (int side : { 0, 1 }) {
		m_planted[side] = !airborne && g.most().down[(int)std::lround(m_phase * PHASES) % PHASES][side];
	}
	// its hips as far toward where it goes as it's turned them; going to a side, or back, is the recording's
	// (the head looks where it's looking, after: Posing)
	core::matrix4 turn;
	turn.setRotationRadians(v3f(0, m_body, 0));
	Bones b = pointed(at, asked, turn);
	// stood on its lower foot: whatever height that leaves the hips at
	float lowest = 1e3f;
	const core::matrix4 l = b.lower.matrix();
	for (int side : { 0, 1 }) {
		v3f foot(0, -SHIN * S, 0);
		(l * b.thigh[side].matrix() * b.shin[side].matrix()).transformVect(foot);
		lowest = std::min(lowest, foot.Y);
	}
	// The hips bob as the recording's do, about the height that puts its feet on the ground at this stride's
	// length, taken over a stride or so. Not stood each moment on its lower foot: a run's in the air between
	// steps, and the lower foot changing over dropped the hips at once (sprint 105: a jolt each step).
	const float bob = g.of([&](const Cycle &c) { return hipsAt(c, m_phase) - meanHips(c); }) * LEG;
	const float stood = b.lower.at.Y - lowest - bob; // where it'd be, stood on its lower foot, less its bob
	const float stride_time = stride / std::max(pace, 0.05f);
	if (!airborne) // (in the air its legs reach for nothing: the hips stay where they were)
		m_height = m_height_known ? m_height + (stood - m_height) * std::min(1.f, dt / stride_time) : stood;
	m_height_known = true;
	b.lower.at.Y = m_height + bob;

	// A foot that's down stays where it was put: the body's going on over it, so it goes back as fast in the
	// figure's frame, turned as the body turns. The leg's solved to it, its knee where the recording's points.
	// Coming up, it comes away from there over a moment, not all at once (sprint 105: drawn as posed, it snapped).
	constexpr float LETS_GO = 0.12f; // s
	core::matrix4 turned_by;
	turned_by.setRotationRadians(v3f(0, -turned, 0));
	const core::matrix4 l2 = b.lower.matrix();
	for (int side : { 0, 1 }) {
		v3f hip = b.thigh[side].at, foot(0, -SHIN * S, 0), knee(0, 0, 0);
		l2.transformVect(hip);
		const core::matrix4 leg = l2 * b.thigh[side].matrix();
		leg.transformVect(knee = v3f(0, -THIGH * S, 0));
		(leg * b.shin[side].matrix()).transformVect(foot);
		const bool down = m_planted[side] && m_share >= 0.5f;
		if (down && !m_locked[side] && m_held[side] <= 0) {
			m_locked[side] = true;
			m_lock[side] = v3f(foot.X, 0, foot.Z);
		} else if (m_locked[side] || m_held[side] > 0) {
			m_lock[side] = turned_by.rotateAndScaleVect(m_lock[side]) - v3f(moving.X, 0, moving.Z) * dt;
		}
		// past what the leg reaches, the foot comes up early: it's left behind, not dragged
		const v3f to = m_lock[side] - hip;
		if (!down || to.getLength() > (THIGH + SHIN) * S * 1.1f) // (a planted leg's about straight: the solve holds it so)
			m_locked[side] = false;
		m_held[side] = m_locked[side] ? 1 : std::max(0.f, m_held[side] - dt / LETS_GO);
		if (m_held[side] <= 0)
			continue;
		const v3f pole = (knee - hip) - (to / to.getLength()) * (knee - hip).dotProduct(to / to.getLength());
		const ArmTo solved = armTo(to / S, pole.getLengthSQ() > 1e-6f ? pole : v3f(0, 0, 1), THIGH, SHIN);
		// (armTo's axes are an arm's: its x is across the other way for a knee bending forward)
		const core::matrix4 thigh_now = axes(solved.x * -1.f, solved.y, solved.z * -1.f);
		b.thigh[side] = blend(b.thigh[side], { b.thigh[side].at, against(l2, thigh_now).getRotationDegrees() }, m_held[side]);
		b.shin[side] = blend(b.shin[side], { b.shin[side].at, v3f(-solved.elbow * core::RADTODEG, 0, 0) }, m_held[side]);
	}

	Bones out = asked;
	out.lower = blend(asked.lower, b.lower, m_share);
	out.hips = blend(asked.hips, b.hips, m_share);
	out.chest = blend(asked.chest, b.chest, m_share);
	out.head = blend(asked.head, b.head, m_share);
	for (int side : { 0, 1 }) {
		out.thigh[side] = blend(asked.thigh[side], b.thigh[side], m_share);
		out.shin[side] = blend(asked.shin[side], b.shin[side], m_share);
		out.upper[side] = blend(asked.upper[side], b.upper[side], m_share);
		out.fore[side] = blend(asked.fore[side], b.fore[side], m_share);
		out.hand[side] = blend(asked.hand[side], b.hand[side], m_share);
	}
	return out;
}

namespace {

// A clip's joint at a frame, on a smooth curve through its frames (held at its ends).
v3f sampleClip(const Clip &c, float frame, int joint)
{
	const float f = std::clamp(frame, 0.f, (float)(c.frames - 1));
	const int i = std::min((int)f, c.frames - 2);
	const float t = f - i;
	const auto at = [&](int k) { return c.at[std::clamp(k, 0, c.frames - 1)][joint]; };
	v3f out;
	for (int k = 0; k < 3; k++)
		(&out.X)[k] = through(at(i - 1)[k], at(i)[k], at(i + 1)[k], at(i + 2)[k], t);
	return out;
}

} // namespace

void Moves::jump()
{
	constexpr float LEAD_IN = 0.05f; // s of it before it leaves the ground: the push that's already happening
	m_clip = &JUMP;
	m_part = Part::LEAD;
	m_been_up = true; // (it's already off the ground)
	m_frame = m_clip->off - LEAD_IN * m_clip->fps;
	m_air = 0;
	m_until = m_clip->frames - 1;
}

void Moves::strike(Strike which)
{
	// one being thrown: this one's next (only the one: a flurry of clicks is a flurry, not a queue of them)
	if (m_part == Part::STRIKE) {
		m_queued = which;
		return;
	}
	start(which);
}

void Moves::start(Strike which)
{
	const int k = which == Strike::KICK;
	m_side = m_next[k];
	m_next[k] = 1 - m_side;
	static const Clip *const CLIPS[2][2] = { { &PUNCH_RIGHT, &PUNCH_LEFT }, { &KICK_RIGHT, &KICK_LEFT } };
	m_clip = CLIPS[k][m_side];
	m_strike = which;
	m_part = Part::STRIKE;
	m_frame = 0;
	m_until = m_clip->frames - 1;
	m_queued.reset();
}

void Moves::flyingKick()
{
	constexpr float LEAD_IN = 0.05f; // s
	m_clip = &FLYING_KICK;
	m_strike = Strike::KICK;
	m_side = 0; // (off its right foot, and its right kicks)
	m_part = Part::LEAD;
	m_been_up = false;
	m_frame = m_clip->off - LEAD_IN * m_clip->fps;
	m_air = 0;
	m_until = m_clip->frames - 1;
	m_queued.reset();
}

bool Moves::stop()
{
	// at once: something else has the body (a climb, a wall), and Posing eases it from how it was
	const bool was = m_share > 0;
	m_part = Part::NONE;
	m_share = 0;
	m_clip = nullptr;
	return was;
}

Bones Moves::step(float dt, const Bones &asked, bool airborne, float up, float above, bool going)
{
	m_followed = false;
	constexpr float COMES_IN = 14, LETS_GO = 8; // /s
	constexpr float GRAVITY = 9.81f, FADES = 0.15f; // nodes/s²; s before the end it's let go over
	if (!m_clip)
		return asked;
	const Clip &c = *m_clip;
	const float was = m_frame;
	switch (m_part) {
	case Part::LEAD:
		// (thrown from the ground, it waits at the push for the mover to have it up)
		m_frame = std::min(m_frame + dt * c.fps, (float)c.off);
		m_been_up = m_been_up || airborne;
		if (!airborne && m_been_up)
			m_part = Part::LANDING;
		else if (airborne && m_frame >= c.off)
			m_part = Part::FLIGHT;
		break;
	case Part::FLIGHT:
		if (!airborne) {
			m_part = Part::LANDING;
			m_frame = c.back;
			break;
		}
		{
			// as far through its flight as it is through its time in the air, by when it'll be down
			m_air += dt;
			const float left = (up + std::sqrt(std::max(0.f, up * up + 2 * GRAVITY * above))) / GRAVITY;
			const float through = m_air / std::max(m_air + left, 1e-3f);
			m_frame = c.off + std::min(through, 0.98f) * (c.back - c.off);
		}
		break;
	case Part::LANDING:
		m_frame += dt * c.fps;
		if (m_frame >= m_until)
			m_part = Part::NONE;
		break;
	case Part::STRIKE: {
		// the next, if there's one, from this far back from this one's hit: a fist's coming back as the other goes
		constexpr float FOLLOWS = 0.4f; // of the way from its hit to its end
		m_frame += dt * c.fps;
		if (m_queued && m_frame >= c.hit + FOLLOWS * (c.frames - 1 - c.hit)) {
			const Strike next = *m_queued;
			m_queued.reset();
			start(next);
			m_followed = true;
		} else if (m_frame >= m_until) {
			m_part = Part::NONE;
		}
		break;
	}
	case Part::NONE:
		break;
	}
	// a strike, flying or stood, lands as its clip goes past where it did (where: from what's drawn, below)
	if (m_clip == &c && c.hit >= 0 && was < c.hit && m_frame >= c.hit)
		m_landing = true;
	const bool letting_go = m_part == Part::NONE || (m_part == Part::LANDING && m_frame > m_until - FADES * c.fps);
	m_share += ((letting_go ? 0.f : 1.f) - m_share) * std::min(1.f, (letting_go ? LETS_GO : COMES_IN) * dt);
	if (m_share < 0.005f) {
		m_share = 0;
		if (m_part == Part::NONE)
			m_clip = nullptr;
		return asked;
	}
	const auto at = [&](int joint) { return sampleClip(c, m_frame, joint); };
	Bones b = pointed(at, asked, core::matrix4());
	// Its hips stood straight over its feet, as they leave the ground and come back to it; before and after, down
	// as far as the recording's go, crouching into it and taking the landing. In the air they're where the mover
	// has them: the jump's own rise is its, not the recording's again. A strike's, from as they stood to throw it
	const auto hips = [&](float f) { return sampleClip(c, f, HIPS).Y; };
	const float from = c.hit >= 0 ? hips(0) : m_frame < c.off ? hips(c.off) : m_frame > c.back ? hips(c.back) : hips(m_frame);
	b.lower.at.Y += HIP_HEIGHT * S + (hips(m_frame) - from) * LEG;
	Bones out = blend(asked, b, m_share);
	// a punch going about: its legs, and the hips they turn under it, as they're going, from whenever it goes
	constexpr float LEGS_FOLLOW = 8; // /s
	const float legs = m_strike == Strike::PUNCH && m_part == Part::STRIKE && going ? 0.f : 1.f;
	m_legs += (legs - m_legs) * std::min(1.f, LEGS_FOLLOW * dt);
	if (m_part == Part::STRIKE || m_legs < 1) {
		out.lower = blend(asked.lower, out.lower, m_legs);
		out.hips = blend(asked.hips, out.hips, m_legs);
		for (int side : { 0, 1 }) {
			out.thigh[side] = blend(asked.thigh[side], out.thigh[side], m_legs);
			out.shin[side] = blend(asked.shin[side], out.shin[side], m_legs);
		}
	}
	if (m_landing) {
		m_landing = false;
		const core::matrix4 lower = out.lower.matrix();
		v3f end;
		if (m_strike == Strike::PUNCH) {
			end = v3f(0, -FOREARM * S, 0);
			(lower * out.hips.matrix() * out.chest.matrix() * out.upper[m_side].matrix() * out.fore[m_side].matrix()).transformVect(end);
		} else {
			end = v3f(0, -SHIN * S, 0);
			(lower * out.thigh[m_side].matrix() * out.shin[m_side].matrix()).transformVect(end);
		}
		m_hit = Hit{ m_strike, m_side, end };
	}
	return out;
}

} // namespace physics
