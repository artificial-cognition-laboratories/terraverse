// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tuning.h"
#include "physics/reach.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace physics {

namespace {

v3s16 nodeAt(float x, float y, float z)
{
	return v3s16(std::floor(x + 0.5f), std::floor(y + 0.5f), std::floor(z + 0.5f));
}

} // namespace

std::optional<Ledge> findLedge(const Solid &solid, v3f foot, float yaw, float reach)
{
	constexpr float ROUND[] = { 0, 0.26f, -0.26f, 0.52f, -0.52f, 0.79f, -0.79f };
	for (const float off : ROUND) {
		const float a = yaw + off;
		const v3f way(-std::sin(a), 0, std::cos(a));
		v3s16 last = nodeAt(foot.X, foot.Y + 0.1f, foot.Z);
		for (float s = 0.2f; s <= reach; s += 0.1f) {
			const v3s16 cell = nodeAt(foot.X + way.X * s, foot.Y + 0.1f, foot.Z + way.Z * s);
			if (cell.X == last.X && cell.Z == last.Z)
				continue;
			// the first column it meets with something in reach: the lowest top in it
			const s16 low = std::floor(foot.Y + 0.5f), high = std::floor(foot.Y + MOST_REACH);
			for (s16 y = low; y <= high; y++) {
				const v3s16 wall(cell.X, y, cell.Z);
				const float top = y + 0.5f;
				if (!solid(wall) || solid(wall + v3s16(0, 1, 0)) || top - foot.Y < 0.3f || top - foot.Y > MOST_REACH)
					continue;
				// hands must get there: nothing on the near side at that height. A corner
				// crossed on the diagonal has no one face, and is left to a straighter look
				if (cell.X != last.X && cell.Z != last.Z)
					break;
				if (solid(v3s16(last.X, y + 1, last.Z)) || solid(v3s16(last.X, y, last.Z)))
					break;
				Ledge l;
				l.normal = v3f(last.X - cell.X, 0, last.Z - cell.Z);
				const v3f at = foot + way * s;
				const float face_x = cell.X + l.normal.X * 0.5f, face_z = cell.Z + l.normal.Z * 0.5f;
				l.lip = v3f(l.normal.X != 0 ? face_x : std::clamp(at.X, cell.X - 0.45f, cell.X + 0.45f), top,
						l.normal.Z != 0 ? face_z : std::clamp(at.Z, cell.Z - 0.45f, cell.Z + 0.45f));
				const v3s16 in(-(s16)l.normal.X, 0, -(s16)l.normal.Z);
				l.thin = !solid(wall + in) && !solid(wall + in + v3s16(0, 1, 0));
				l.room = !solid(wall + v3s16(0, 2, 0));
				l.far = top;
				if (l.thin)
					for (s16 d = y - 1; d >= y - 3; d--)
						if (solid(v3s16(wall.X + in.X, d, wall.Z + in.Z))) {
							l.far = d + 0.5f;
							break;
						}
				return l;
			}
			// something taller than it can reach, in this direction: not through it
			if (solid(cell))
				break;
			last = cell;
		}
	}
	return std::nullopt;
}

bool ledgeGoesOn(const Solid &solid, v3f lip, v3f normal)
{
	const v3f inside = lip - normal * 0.25f;
	const v3s16 wall = nodeAt(inside.X, lip.Y - 0.25f, inside.Z);
	const v3f outside = lip + normal * 0.25f;
	return solid(wall) && !solid(wall + v3s16(0, 1, 0)) &&
			!solid(nodeAt(outside.X, lip.Y - 0.25f, outside.Z)) && !solid(nodeAt(outside.X, lip.Y - 1.25f, outside.Z));
}

namespace {

enum class Height { LOW, HIGH, TOO_HIGH }; // LOW: within a vault of the feet. HIGH: a climb. TOO_HIGH: only to hang from

// The moves, first that fits wins. A move is only possible moving as it needs: a Kong takes
// going straight at the wall, fast; coming in on the slant it's the legs swung past a hand.
struct MoveRow
{
	Move move;
	Height height;
	bool thin, room;     // needs a far side to land on; needs a top to stand on
	float least, most;   // nodes/s
	float off;           // radians off square, at most
	float off_from = 0;  // and at least
};
constexpr float DEGREE = 3.14159265f / 180;
const MoveRow MOVES[] = {
	{ { "kong", Climb::VAULT, true, false, true }, Height::LOW, true, false, 4.5f, 1e3f, 15 * DEGREE },
	{ { "speed vault", Climb::VAULT, true, false }, Height::LOW, true, false, 2.5f, 1e3f, 25 * DEGREE },
	{ { "side vault", Climb::VAULT, true, true }, Height::LOW, true, false, 2.5f, 1e3f, 60 * DEGREE, 25 * DEGREE },
	{ { "onto it", Climb::VAULT, false, false }, Height::LOW, false, true, 0, 1e3f, 90 * DEGREE },
	{ { "over it", Climb::VAULT, true, false }, Height::LOW, true, false, 0, 1e3f, 90 * DEGREE }, // a roof over the top
	{ { "mantle", Climb::MANTLE, false, false }, Height::HIGH, false, true, 0, 1e3f, 90 * DEGREE },
	{ { "hang", Climb::HANG, false, false }, Height::HIGH, false, false, 0, 1e3f, 90 * DEGREE },
	{ { "hang", Climb::HANG, false, false }, Height::TOO_HIGH, false, false, 0, 1e3f, 90 * DEGREE },
};

} // namespace

Ledge overCorner(const Solid &solid, Ledge ledge, v3f velocity)
{
	constexpr float NEAR_END = 0.32f, LEAST = 20 * DEGREE, MOST = 70 * DEGREE;
	const v3f across(velocity.X, 0, velocity.Z);
	const float pace = across.getLength();
	if (pace < 1 || !ledge.room)
		return ledge;
	const v3f way = across / pace, in = ledge.normal * -1.f;
	const float off = std::acos(std::clamp(way.dotProduct(in), -1.f, 1.f));
	if (off < LEAST || off > MOST)
		return ledge;
	const v3f along(-ledge.normal.Z, 0, ledge.normal.X);
	const v3f inside = ledge.lip + in * 0.25f;
	const v3s16 wall = nodeAt(inside.X, ledge.lip.Y - 0.25f, inside.Z);
	const float from_middle = (inside - v3f(wall.X, inside.Y, wall.Z)).dotProduct(along);
	for (const float end : { 1.f, -1.f }) {
		const v3f out = along * end;
		const v3s16 next(wall.X + out.X, wall.Y, wall.Z + out.Z);
		// the end of the edge, and it's heading in across it from outside
		if (from_middle * end > 0.5f - NEAR_END && !solid(next) && way.dotProduct(out) < 0) {
			ledge.in = way;
			ledge.thin = false;
			return ledge;
		}
	}
	return ledge;
}

std::optional<Move> moveFor(const Ledge &ledge, float base, float foot_y, v3f velocity)
{
	const float reach = ledge.lip.Y - foot_y;
	if (reach < 0.3f || reach > MOST_REACH)
		return std::nullopt;
	const float high = ledge.lip.Y - std::min(base, foot_y);
	// on the way down with the edge up at its chest it isn't going over anything: it lands on the wall and pulls.
	// Coming down hard, its arms have enough to do just stopping it.
	const bool coming_down = velocity.Y < -1 && reach > COMING_DOWN;
	const Height height = (high <= VAULT_TO || (high <= MANTLE_TO && reach <= STEPS_OVER && (ledge.thin || ledge.room))) && !coming_down ? Height::LOW :
			high <= MANTLE_TO && velocity.Y > Haul::HARD_CATCH ? Height::HIGH : Height::TOO_HIGH;
	const v3f across(velocity.X, 0, velocity.Z);
	const float pace = across.getLength();
	const float off = pace > 0.1f ? std::acos(std::clamp(across.dotProduct(ledge.normal * -1.f) / pace, -1.f, 1.f)) : 0;
	for (const MoveRow &row : MOVES)
		if (row.height == height && (!row.thin || ledge.thin) && (!row.room || ledge.room) &&
				pace >= row.least && pace <= row.most && off <= row.off && off >= row.off_from)
			return row.move;
	return std::nullopt;
}

v3f ClimbPath::at(float t) const
{
	const float p = std::clamp(t / time, 0.f, 1.f);
	if (arc) {
		constexpr float UP = 0.4f, DOWN = 0.7f;
		float y = top;
		if (p < UP) {
			const float u = p / UP;
			y = from.Y + (top - from.Y) * u * (2 - u);
		} else if (p > DOWN) {
			const float u = (p - DOWN) / (1 - DOWN);
			y = top + (to.Y - top) * u * u;
		}
		return v3f(from.X + (to.X - from.X) * p, y, from.Z + (to.Z - from.Z) * p);
	}
	constexpr float RISE = 0.6f;
	if (p < RISE) {
		const float u = p / RISE;
		const float y = from.Y + (top - from.Y) * u * (2 - u); // quick, then easing in
		return v3f(from.X + (to.X - from.X) * 0.2f * u, y, from.Z + (to.Z - from.Z) * 0.2f * u);
	}
	const float u = (p - RISE) / (1 - RISE);
	const v3f over(from.X + (to.X - from.X) * 0.2f, top, from.Z + (to.Z - from.Z) * 0.2f);
	return over + (to - over) * u;
}

ClimbPath climbPath(const Move &move, const Ledge &ledge, v3f foot, float radius, float pace)
{
	const v3f in = ledge.inward();
	const v3f on(ledge.lip.X + in.X * 0.5f, ledge.lip.Y + 0.02f, ledge.lip.Z + in.Z * 0.5f);
	if (move.climb != Climb::VAULT)
		throw std::logic_error("reach: only a vault follows a path; the rest are a Haul");
	// over and down onto the far side, just past it, or onto the top and on
	v3f to = on;
	if (move.over) {
		to = on + in * (0.5f + radius + 0.05f);
		to.Y = ledge.far + 0.02f;
	}
	const float across = v3f(to.X - foot.X, 0, to.Z - foot.Z).getLength();
	// (the longest over both hands; onto it, the body's lifted as well as carried)
	const float least = move.dive ? 0.5f : move.over ? 0.38f : 0.45f;
	return { foot, to, ledge.lip.Y + 0.05f, std::max(across / std::clamp(pace, 2.f, VAULT_PACE), least), true };
}

Haul::Haul(const Ledge &ledge, v3f foot, v3f velocity, float radius, bool pull, float floor) :
	m_ledge(ledge), m_at(foot), m_velocity(velocity), m_floor(floor), m_pull(pull)
{
	const v3f in = ledge.inward();
	const v3f sideways(-in.Z, 0, in.X);
	m_velocity -= sideways * velocity.dotProduct(sideways); // (along the edge: the hands stopped that; the swing shows it)
	m_hung = v3f(ledge.lip.X - in.X * (radius + 0.05f), ledge.lip.Y - HANG_DROP, ledge.lip.Z - in.Z * (radius + 0.05f));
	m_on = v3f(ledge.lip.X + in.X * 0.5f, ledge.lip.Y + 0.02f, ledge.lip.Z + in.Z * 0.5f);
	// what it can keep going on the far side: its run, as far as it wasn't spent stopping a fall
	const float clean = std::clamp(1 + velocity.Y / 3, 0.f, 1.f);
	m_pace = v3f(velocity.X, 0, velocity.Z).getLength() * 0.75f * clean;
}

Haul Haul::pullUp(const Ledge &ledge, v3f foot, float radius)
{
	Haul h(ledge, foot, v3f(0), radius, true, foot.Y);
	h.m_phase = Phase::PULL;
	return h;
}

std::optional<v3f> Haul::stepOn() const
{
	if (m_phase != Phase::LEG_UP && m_phase != Phase::DRIVE)
		return std::nullopt;
	return m_ledge.lip + m_ledge.inward() * 0.2f;
}

v3f Haul::exit() const { return m_ledge.inward() * m_pace; }

bool Haul::handsOn() const
{
	constexpr float ON = 0.25f;
	if (!m_felt)
		return true;
	const v3f in = m_ledge.inward(), sideways(-in.Z, 0, in.X);
	for (const int side : { 0, 1 }) {
		v3f off = m_felt->wrist[side] - m_ledge.lip;
		off -= sideways * off.dotProduct(sideways); // (spread along the edge is where they should be)
		if (off.getLength() > ON)
			return false;
	}
	return true;
}

bool Haul::footOn() const
{
	constexpr float ON = 0.25f;
	if (!m_felt)
		return true;
	const v3f top = m_ledge.lip + m_ledge.inward() * 0.2f, in = m_ledge.inward(), sideways(-in.Z, 0, in.X);
	for (const int side : { 0, 1 }) {
		v3f off = m_felt->foot[side] - top;
		off -= sideways * off.dotProduct(sideways);
		if (off.getLength() <= ON)
			return true;
	}
	return false;
}

bool Haul::waiting(float dt)
{
	constexpr float WAITS = 0.6f;
	m_waited += dt;
	if (m_waited < WAITS)
		return true;
	m_gave_up = true;
	m_waited = 0;
	return false;
}

void Haul::step(float dt)
{
	// Its arms: stiff enough to stop a person, not so stiff it's a snap. Critically damped.
	constexpr float ARMS = 40, DAMPING = 12.6f, MOST = 40; // /s², /s, nodes/s²
	constexpr float PULL = 6, LET_GO = 0.55f;              // nodes/s²; nodes under the lip it's let go at
	constexpr float STOPPED = 0.4f;                        // nodes/s down: as good as stopped
	constexpr float SLOWS = 8, LEAST_UP = 0.5f, ACROSS = 6, DRIVE_LEAST = 3;
	const auto toward = [&](v3f to) {
		v3f a = (to - m_at) * ARMS - m_velocity * DAMPING;
		if (a.getLength() > MOST)
			a = a / a.getLength() * MOST;
		return a;
	};
	const v3f in = m_ledge.inward();
	m_time += dt;
	switch (m_phase) {
	case Phase::CATCH:
		if (m_pull) {
			// Its arms only stop it: going on up, nothing lowers it to a hang first. That lowering
			// never quite ended (critically damped: it nears, never passes), and held it there for good.
			v3f to = m_hung;
			to.Y = std::min(m_hung.Y, m_at.Y);
			m_velocity += toward(to) * dt;
			if (m_velocity.Y >= -STOPPED && (handsOn() || !waiting(dt))) {
				m_waited = 0;
				m_phase = Phase::PULL;
			}
		} else if (m_velocity += toward(m_hung) * dt; ((m_hung - m_at).getLength() < 0.03f && m_velocity.getLength() < 0.3f) || m_time > 1.5f) {
			m_at = m_hung;
			m_velocity = v3f(0);
			m_phase = Phase::DONE;
			return;
		}
		break;
	case Phase::PULL: {
		v3f a = toward(m_hung);
		a.Y = PULL;
		m_velocity += a * dt;
		if (m_at.Y >= m_ledge.lip.Y - LET_GO)
			m_phase = Phase::LEG_UP;
		break;
	}
	case Phase::LEG_UP: {
		// slowing to arrive on the top, never stopped by it
		const float left = m_on.Y - m_at.Y;
		const float slows = std::max(SLOWS, m_velocity.Y * m_velocity.Y / (2 * std::max(left, 0.02f)));
		m_velocity.Y = std::max(m_velocity.Y - slows * dt, LEAST_UP);
		// over the edge only as the feet clear it
		if (left <= 0.15f)
			m_velocity += in * (ACROSS * dt);
		if (left <= 0.02f) {
			m_at.Y = m_on.Y;
			m_velocity.Y = 0;
			if (footOn() || !waiting(dt)) {
				m_waited = 0;
				m_phase = Phase::DRIVE;
			} else {
				m_velocity = v3f(0); // stood up on its arms till the foot's there
			}
		}
		break;
	}
	case Phase::DRIVE: {
		const float speed = std::min(m_velocity.dotProduct(in) + ACROSS * dt, std::max(DRIVE_LEAST, m_pace));
		m_velocity = in * speed;
		const v3f left = m_on - m_at;
		if (left.dotProduct(in) <= speed * dt) {
			m_at = m_on;
			m_phase = Phase::DONE;
			return;
		}
		break;
	}
	case Phase::DONE:
		return;
	}
	m_at += m_velocity * dt;
	if (m_at.Y < m_floor) {
		m_at.Y = m_floor;
		m_velocity.Y = std::max(m_velocity.Y, 0.f);
	}
}

std::optional<v3f> findWall(const Solid &solid, v3f foot, float radius)
{
	const float y = foot.Y + 1;
	const float reach = radius + 0.3f;
	std::optional<v3f> best;
	float nearest = reach + 1;
	for (const v3f &out : { v3f(1, 0, 0), v3f(-1, 0, 0), v3f(0, 0, 1), v3f(0, 0, -1) }) {
		const v3f at = foot - out * reach;
		const v3s16 n = nodeAt(at.X, y, at.Z);
		if (!solid(n))
			continue;
		// how far its face is
		const float gap = out.X != 0 ? (foot.X - (n.X + out.X * 0.5f)) * out.X : (foot.Z - (n.Z + out.Z * 0.5f)) * out.Z;
		if (gap < nearest) {
			nearest = gap;
			best = out;
		}
	}
	return best;
}

bool runsAlong(v3f velocity, v3f normal)
{
	constexpr float FAST = 3.5f;
	const v3f across(velocity.X, 0, velocity.Z);
	const float into = -across.dotProduct(normal);
	const float along = (across + normal * into).getLength();
	return along >= FAST && along >= std::fabs(into) * 0.8f;
}

v3f wallKick(v3f velocity, v3f normal, float up, v3f look)
{
	constexpr float KEPT = 0.85f;
	const float OUT = tuning().kick_out, STEERED = tuning().kick_steer;
	const v3f across(velocity.X, 0, velocity.Z);
	const v3f along = across - normal * across.dotProduct(normal);
	// looking out, along it or away: its legs drive it that way. Into the wall there's nothing to steer by
	const float out = look.dotProduct(normal);
	return along * KEPT + (look - normal * out) * STEERED + normal * (OUT + std::max(out, 0.f) * 1.5f) + v3f(0, up, 0);
}

WallPush::WallPush(v3f normal, v3f velocity, float up, v3f face, v3f look) : m_normal(normal)
{
	const v3f across(velocity.X, 0, velocity.Z);
	const float pace = across.getLength();
	const float into = -across.dotProduct(normal);
	m_along = across + normal * into;
	// even running along it, it leans in to push: faster, closer
	m_into = normal * -std::max(into, pace * 0.15f);
	m_fall = velocity.Y;
	// how square it comes at the wall; hardly moving, how square it looks at it
	m_square = pace > 1 ? std::clamp(into / pace, 0.f, 1.f) : std::clamp(-look.dotProduct(normal), 0.f, 1.f);
	if (look.getLengthSQ() == 0 && pace <= 1)
		m_square = 1;
	m_out = wallKick(velocity, normal, up, look);
	m_press = std::clamp(0.08f + 0.012f * pace, 0.08f, 0.16f); // (long enough for the hands to get there)
	m_foot_on = face + v3f(0, 0.6f, 0);
	m_hands_at = face + v3f(0, 1.25f, 0);
}

v3f WallPush::step(float dt)
{
	m_t += dt;
	if (m_t < m_press) {
		const float left = 1 - m_t / m_press;
		return m_along + m_into * left + v3f(0, m_fall * left, 0);
	}
	const float u = std::min(1.f, (m_t - m_press) / SPRING);
	return m_along + (m_out - m_along) * (u * u * (3 - 2 * u));
}

v3f WallPush::facing() const
{
	const float pace = m_along.getLength();
	v3f f = m_normal * -m_square;
	if (pace > 0.1f)
		f += m_along / pace * (1 - m_square);
	else
		f = m_normal * -1.f;
	f.normalize();
	return f;
}

int WallPush::nearSide() const
{
	if (m_square >= BOTH_FROM)
		return 2;
	const v3f f = facing();
	return v3f(f.Z, 0, -f.X).dotProduct(m_normal) < 0 ? 0 : 1; // its right, toward the wall
}

v3f wallFaceAt(v3f foot, v3f normal, float radius)
{
	const v3f at = foot - normal * (radius + 0.3f);
	v3f face = foot;
	if (normal.X != 0)
		face.X = std::floor(at.X + 0.5f) + normal.X * 0.5f;
	else
		face.Z = std::floor(at.Z + 0.5f) + normal.Z * 0.5f;
	return face;
}

} // namespace physics
