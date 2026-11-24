// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/game/dig/earthwork.h"

#include <algorithm>
#include <cmath>

namespace earthwork {

namespace {
s32 down(s32 v, s32 step) { return v >= 0 ? v / step * step : -((-v + step - 1) / step) * step; }
} // namespace

Target place(v3s32 aim, s32 ground, s32 feet, f32 yaw, Size size, Kind kind, Mode mode)
{
	if (kind == Kind::Cell) {
		// the cell of the tool's grid the aimed sixteenth is in: snapped whatever the mode
		const s32 c = size.along;
		const v3s32 lo(down(aim.X, c), down(aim.Y, c), down(aim.Z, c));
		return { lo, lo + v3s32(c - 1, c - 1, c - 1), kind };
	}
	// the way they face on the ground (yaw 0 faces +z): along x if that's nearer
	const bool x = std::abs(std::sin(yaw)) > std::abs(std::cos(yaw));
	const s32 dx = x ? size.along : size.across, dz = x ? size.across : size.along;
	s32 cx = aim.X - dx / 2, cz = aim.Z - dz / 2;
	if (mode != Mode::Free) {
		cx = down(cx + GRID / 2, GRID);
		cz = down(cz + GRID / 2, GRID);
	}
	if (kind == Kind::Level) {
		const s32 level = mode == Mode::Snap ? feet : ground;
		return { v3s32(cx, level, cz), v3s32(cx + dx - 1, level + size.deep - 1, cz + dz - 1), kind };
	}
	return { v3s32(cx, ground - size.deep, cz), v3s32(cx + dx - 1, ground - 1, cz + dz - 1), kind };
}

std::vector<v3s32> bore(v3f eye, v3f dir, f32 radius, f32 reach, s32 cell)
{
	// the cells round the line, each with how far along it its middle is
	const v3f end = eye + dir * reach;
	const auto lo = [&](f32 a, f32 b) { return down((s32)std::floor(std::min(a, b) - radius), cell); };
	const auto hi = [&](f32 a, f32 b) { return (s32)std::ceil(std::max(a, b) + radius); };
	std::vector<std::pair<f32, v3s32>> along;
	for (s32 y = lo(eye.Y, end.Y); y <= hi(eye.Y, end.Y); y += cell)
		for (s32 z = lo(eye.Z, end.Z); z <= hi(eye.Z, end.Z); z += cell)
			for (s32 x = lo(eye.X, end.X); x <= hi(eye.X, end.X); x += cell) {
				const v3f mid = v3f(x, y, z) + v3f(cell, cell, cell) / 2.0f - eye;
				const f32 t = mid.dotProduct(dir);
				if (t <= 0 || t > reach || (mid - dir * t).getLengthSQ() > radius * radius)
					continue;
				along.push_back({ t, v3s32(x, y, z) });
			}
	std::sort(along.begin(), along.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::vector<v3s32> out;
	for (const auto &[t, c] : along)
		out.push_back(c);
	return out;
}

bool holds(const Target &t, s32 x, s32 z)
{
	if (x < t.min.X || x > t.max.X || z < t.min.Z || z > t.max.Z)
		return false;
	if (t.kind != Kind::Round)
		return true;
	// within the ellipse the box bounds, cell centres
	const f32 rx = (t.max.X - t.min.X + 1) / 2.0f, rz = (t.max.Z - t.min.Z + 1) / 2.0f;
	const f32 u = (x + 0.5f - t.min.X - rx) / rx, v = (z + 0.5f - t.min.Z - rz) / rz;
	return u * u + v * v <= 1;
}

bool fits(const Target &t, Size size)
{
	const s32 dx = t.max.X - t.min.X + 1, dy = t.max.Y - t.min.Y + 1, dz = t.max.Z - t.min.Z + 1;
	return dy == size.deep && ((dx == size.along && dz == size.across) || (dx == size.across && dz == size.along));
}

std::vector<Column> bite(const Target &t, s32 budget, const std::function<s32(s32, s32)> &ground)
{
	std::vector<Column> cols;
	s32 high = t.min.Y;
	for (s32 z = t.min.Z; z <= t.max.Z; z++)
		for (s32 x = t.min.X; x <= t.max.X; x++)
			if (holds(t, x, z)) {
				const s32 top = std::min(ground(x, z), t.max.Y + 1);
				if (top > t.min.Y) {
					cols.push_back({ x, z, t.min.Y, top });
					high = std::max(high, top);
				}
			}
	// the lowest level whose layers above fit the budget; the top layer whatever it costs
	s32 level = high - 1;
	for (s32 l = high - 2; l >= t.min.Y; l--) {
		s32 cells = 0;
		for (const Column &c : cols)
			cells += std::max(0, c.top - l);
		if (cells > budget)
			break;
		level = l;
	}
	std::vector<Column> out;
	for (const Column &c : cols)
		if (c.top > level)
			out.push_back({ c.x, c.z, std::max(level, c.bed), c.top });
	return out;
}

std::optional<s32> surface(s32 x, s32 z, s32 near, const std::function<bool(v3s32)> &solid)
{
	for (s32 y = near + ABOVE; y >= near - ABOVE; y--)
		if (solid(v3s32(x, y, z)))
			return y + 1;
	return std::nullopt;
}

} // namespace earthwork
