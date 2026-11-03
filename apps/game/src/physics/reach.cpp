// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "physics/reach.h"

#include <algorithm>
#include <cmath>

namespace physics {

namespace {

v3s16 nodeAt(float x, float y, float z)
{
	return v3s16(std::floor(x + 0.5f), std::floor(y + 0.5f), std::floor(z + 0.5f));
}

} // namespace

std::optional<Ledge> findLedge(const Solid &solid, v3f foot, float yaw)
{
	constexpr float ROUND[] = { 0, 0.26f, -0.26f, 0.52f, -0.52f, 0.79f, -0.79f };
	for (const float off : ROUND) {
		const float a = yaw + off;
		const v3f way(-std::sin(a), 0, std::cos(a));
		v3s16 last = nodeAt(foot.X, foot.Y + 0.1f, foot.Z);
		for (float s = 0.2f; s <= 1.2f; s += 0.1f) {
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

std::optional<Climb> climbFor(const Ledge &ledge, float base, float foot_y)
{
	const float reach = ledge.lip.Y - foot_y;
	if (reach < 0.3f || reach > MOST_REACH)
		return std::nullopt;
	const float high = ledge.lip.Y - std::min(base, foot_y);
	if (high <= VAULT_TO)
		return ledge.thin || ledge.room ? std::optional(Climb::VAULT) : std::nullopt;
	if (high <= MANTLE_TO && ledge.room)
		return Climb::MANTLE;
	return Climb::HANG; // a roof over it can still be hung from
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

ClimbPath climbPath(Climb kind, const Ledge &ledge, v3f foot, float radius, float pace)
{
	const v3f in = ledge.normal * -1.f;
	const v3f on(ledge.lip.X + in.X * 0.5f, ledge.lip.Y + 0.02f, ledge.lip.Z + in.Z * 0.5f);
	switch (kind) {
	case Climb::VAULT:
	{
		// over and down onto the far side, just past it, or onto the top and on
		v3f to = on;
		if (ledge.thin) {
			to = on + in * (0.5f + radius + 0.05f);
			to.Y = ledge.far + 0.02f;
		}
		const float across = v3f(to.X - foot.X, 0, to.Z - foot.Z).getLength();
		return { foot, to, ledge.lip.Y + 0.05f, std::max(across / std::max(pace, 2.f), 0.2f), true };
	}
	case Climb::MANTLE:
		return { foot, on, ledge.lip.Y + 0.05f, 0.55f };
	case Climb::HANG: {
		const v3f hung(ledge.lip.X + ledge.normal.X * (radius + 0.05f), ledge.lip.Y - HANG_DROP,
				ledge.lip.Z + ledge.normal.Z * (radius + 0.05f));
		return { foot, hung, hung.Y, 0.15f };
	}
	}
	return { foot, foot, foot.Y, 0.01f };
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
		if (!solid(n) || !solid(n + v3s16(0, 1, 0)))
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

v3f wallKick(v3f velocity, v3f normal, float up)
{
	constexpr float KEPT = 0.85f, OUT = 4.5f;
	const v3f across(velocity.X, 0, velocity.Z);
	const v3f along = across - normal * across.dotProduct(normal);
	return along * KEPT + normal * OUT + v3f(0, up, 0);
}

} // namespace physics
