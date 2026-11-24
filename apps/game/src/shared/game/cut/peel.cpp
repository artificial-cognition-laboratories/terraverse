// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/game/cut/peel.h"

#include <algorithm>

namespace peel {

namespace {
s32 &on(v3s32 &v, int axis) { return axis == 0 ? v.X : axis == 1 ? v.Y : v.Z; }
s32 of(v3s32 v, int axis) { return axis == 0 ? v.X : axis == 1 ? v.Y : v.Z; }
int axisOf(v3s16 face) { return face.X != 0 ? 0 : face.Y != 0 ? 1 : 2; }
v3s16 step(int axis) { return v3s16(axis == 0, axis == 1, axis == 2); }
bool has(const Log &log, v3s32 c);
bool whole(const Log &log, const Box &b);
// the box `size` at along `a`, across `c`, `depth` in from `face`
Box placed(const Log &log, v3s16 face, Size size, s32 a, s32 c, s32 depth);
} // namespace

int grain(u8 param2)
{
	// facedir's top points: 0 +y, 1 +z, 2 −z, 3 +x, 4 −x, 5 −y
	switch ((param2 >> 2) % 6) {
	case 1: case 2: return 2;
	case 3: case 4: return 0;
	default: return 1;
	}
}

Log read(v3s16 p, const std::function<std::optional<Block>(v3s16)> &block)
{
	const std::optional<Block> n = block(p);
	if (!n)
		return Log{ p, 1, { shape::Shape::whole() } };
	const int g = grain(n->param2);
	const v3s16 dir = step(g);
	const auto like = [&](v3s16 q) {
		const std::optional<Block> m = block(q);
		return m && m->content == n->content && grain(m->param2) == g;
	};
	// as many back as forward, p in the middle; what one side lacks the other may have
	int back = 0, ahead = 0;
	while (back + ahead + 1 < LONGEST) {
		const bool b = like(p - dir * (back + 1)), f = like(p + dir * (ahead + 1));
		if (!b && !f)
			break;
		if (b && (back <= ahead || !f))
			back++;
		else
			ahead++;
	}
	Log log{ p - dir * back, g, {} };
	for (int i = 0; i <= back + ahead; i++) {
		const shape::Shape *s = block(log.start + dir * i)->shape;
		log.shapes.push_back(s ? *s : shape::Shape::whole());
	}
	return log;
}

v3s32 cell(const Log &log, v3s16 p, v3s16 c)
{
	v3s32 out(c.X, c.Y, c.Z);
	const v3s16 from = p - log.start;
	on(out, log.grain) += shape::STEPS * (from.X + from.Y + from.Z);
	return out;
}

std::optional<Box> at(const Log &log, v3s16 face, Size size, v3s32 aim)
{
	const int in = axisOf(face), across = 3 - log.grain - in;
	const s32 length = shape::STEPS * (s32)log.shapes.size();
	if (in == log.grain || size.along < 1 || size.across < 1 || size.in < 1 || size.along > length ||
			size.across > shape::STEPS || size.in > shape::STEPS)
		return std::nullopt;
	// on the member's own grid where it fits one (planks side by side, no slivers between), else centred
	const auto lay = [](s32 aimed, s32 size, s32 room) {
		const s32 at = room % size == 0 ? aimed / size * size : aimed - (size - 1) / 2;
		return std::clamp<s32>(at, 0, room - size);
	};
	const s32 a = lay(of(aim, log.grain), size.along, length);
	const s32 c = lay(of(aim, across), size.across, shape::STEPS);
	for (s32 depth = 0; depth + size.in <= shape::STEPS; depth++) {
		const Box b = placed(log, face, size, a, c, depth);
		if (whole(log, b))
			return b;
	}
	return std::nullopt;
}

bool any(const Log &log, v3s16 face, Size size)
{
	const s32 length = shape::STEPS * (s32)log.shapes.size();
	if (axisOf(face) == log.grain || size.along > length)
		return false;
	for (s32 depth = 0; depth + size.in <= shape::STEPS; depth++)
		for (s32 c = 0; c + size.across <= shape::STEPS; c++)
			for (s32 a = 0; a + size.along <= length; a++)
				if (whole(log, placed(log, face, size, a, c, depth)))
					return true;
	return false;
}

std::vector<std::pair<int, std::pair<v3s16, v3s16>>> split(const Log &log, const Box &box)
{
	std::vector<std::pair<int, std::pair<v3s16, v3s16>>> out;
	for (int i = 0; i < (int)log.shapes.size(); i++) {
		const s32 lo = std::max(of(box.min, log.grain), i * shape::STEPS);
		const s32 hi = std::min(of(box.max, log.grain), i * shape::STEPS + shape::STEPS - 1);
		if (lo > hi)
			continue;
		v3s32 min = box.min, max = box.max;
		on(min, log.grain) = lo - i * shape::STEPS;
		on(max, log.grain) = hi - i * shape::STEPS;
		out.push_back({ i, { v3s16(min.X, min.Y, min.Z), v3s16(max.X, max.Y, max.Z) } });
	}
	return out;
}

Box lay(v3s16 node, v3s16 face, v3s16 cell, v3s32 dims, bool free)
{
	// the sixteenth just outside the face, where it's aimed
	const v3s32 aimed = v3s32(node.X, node.Y, node.Z) * shape::STEPS + v3s32(cell.X, cell.Y, cell.Z)
			+ v3s32(face.X, face.Y, face.Z);
	const int normal = axisOf(face);
	const bool outward = (face.X + face.Y + face.Z) > 0;
	const auto down = [](s32 v, s32 step) { return v >= 0 ? v / step * step : -((-v + step - 1) / step) * step; };
	const s32 QUARTER = shape::STEPS / 4;
	Box b;
	for (int i = 0; i < 3; i++) {
		const s32 a = of(aimed, i), d = of(dims, i);
		s32 lo;
		if (i == normal)
			lo = outward ? a : a - d + 1; // out from the face
		else if (free)
			lo = a - (d - 1) / 2;
		else if (shape::STEPS % d == 0)
			lo = down(a, d); // its own grid
		else
			lo = down(a - d / 2 + QUARTER / 2, QUARTER); // centred on the aim, on a quarter
		on(b.min, i) = lo;
		on(b.max, i) = lo + d - 1;
	}
	return b;
}

v3s32 dims(Size size, const std::array<int, 3> &axes)
{
	v3s32 out;
	on(out, axes[0]) = size.along;
	on(out, axes[1]) = size.across;
	on(out, axes[2]) = size.in;
	return out;
}

u8 facedir(int axis)
{
	return axis == 0 ? 3 << 2 : axis == 2 ? 1 << 2 : 0;
}

shape::Shape without(shape::Shape s, v3s16 min, v3s16 max)
{
	for (s16 y = min.Y; y <= max.Y; y++)
		for (s16 z = min.Z; z <= max.Z; z++)
			for (s16 x = min.X; x <= max.X; x++)
				s.set(v3s16(x, y, z), false);
	return s;
}

namespace {
bool has(const Log &log, v3s32 c)
{
	const s32 along = of(c, log.grain);
	const int i = along / shape::STEPS;
	if (along < 0 || i >= (int)log.shapes.size())
		return false;
	on(c, log.grain) = along - i * shape::STEPS;
	return log.shapes[i].has(v3s16(c.X, c.Y, c.Z));
}

bool whole(const Log &log, const Box &b)
{
	for (s32 y = b.min.Y; y <= b.max.Y; y++)
		for (s32 z = b.min.Z; z <= b.max.Z; z++)
			for (s32 x = b.min.X; x <= b.max.X; x++)
				if (!has(log, v3s32(x, y, z)))
					return false;
	return true;
}

Box placed(const Log &log, v3s16 face, Size size, s32 a, s32 c, s32 depth)
{
	const int in = axisOf(face), across = 3 - log.grain - in;
	const bool outward = (face.X + face.Y + face.Z) > 0;
	const s32 lo = outward ? shape::STEPS - size.in - depth : depth;
	Box b;
	on(b.min, in) = lo;
	on(b.max, in) = lo + size.in - 1;
	on(b.min, across) = c;
	on(b.max, across) = c + size.across - 1;
	on(b.min, log.grain) = a;
	on(b.max, log.grain) = a + size.along - 1;
	return b;
}
} // namespace

} // namespace peel
