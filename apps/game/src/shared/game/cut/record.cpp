// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/game/cut/record.h"

#include <sstream>
#include <stdexcept>

namespace record {

namespace {
s16 block(s32 sixteenth) { return (s16)(sixteenth >= 0 ? sixteenth / shape::STEPS : -((-sixteenth + shape::STEPS - 1) / shape::STEPS)); }
} // namespace

// one a line: the item, then the box's corners
std::vector<Member> read(const std::string &text)
{
	std::vector<Member> out;
	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line)) {
		if (line.empty())
			continue;
		std::istringstream in(line);
		Member m;
		peel::Box &b = m.box;
		if (!(in >> m.item >> b.min.X >> b.min.Y >> b.min.Z >> b.max.X >> b.max.Y >> b.max.Z))
			throw std::invalid_argument("placed members: \"" + line + "\" isn't an item and a box");
		out.push_back(m);
	}
	return out;
}

std::string write(const std::vector<Member> &members)
{
	std::ostringstream out;
	for (const Member &m : members) {
		const peel::Box &b = m.box;
		out << m.item << ' ' << b.min.X << ' ' << b.min.Y << ' ' << b.min.Z << ' '
				<< b.max.X << ' ' << b.max.Y << ' ' << b.max.Z << '\n';
	}
	return out.str();
}

std::optional<Member> at(const std::vector<Member> &members, v3s32 cell)
{
	for (const Member &m : members)
		if (meets(m.box, peel::Box{ cell, cell }))
			return m;
	return std::nullopt;
}

bool meets(const peel::Box &a, const peel::Box &b)
{
	return a.min.X <= b.max.X && b.min.X <= a.max.X && a.min.Y <= b.max.Y && b.min.Y <= a.max.Y &&
			a.min.Z <= b.max.Z && b.min.Z <= a.max.Z;
}

v3s32 world(v3s16 node, v3s16 cell)
{
	return v3s32(node.X, node.Y, node.Z) * shape::STEPS + v3s32(cell.X, cell.Y, cell.Z);
}

bool touches(const peel::Box &box, const std::function<bool(v3s32)> &solid)
{
	const v3s32 &lo = box.min, &hi = box.max;
	for (s32 y = lo.Y; y <= hi.Y; y++)
		for (s32 z = lo.Z; z <= hi.Z; z++)
			if (solid(v3s32(lo.X - 1, y, z)) || solid(v3s32(hi.X + 1, y, z)))
				return true;
	for (s32 x = lo.X; x <= hi.X; x++) {
		for (s32 z = lo.Z; z <= hi.Z; z++)
			if (solid(v3s32(x, lo.Y - 1, z)) || solid(v3s32(x, hi.Y + 1, z)))
				return true;
		for (s32 y = lo.Y; y <= hi.Y; y++)
			if (solid(v3s32(x, y, lo.Z - 1)) || solid(v3s32(x, y, hi.Z + 1)))
				return true;
	}
	return false;
}

std::vector<v3s16> blocks(const peel::Box &box)
{
	std::vector<v3s16> out;
	for (s16 y = block(box.min.Y); y <= block(box.max.Y); y++)
		for (s16 z = block(box.min.Z); z <= block(box.max.Z); z++)
			for (s16 x = block(box.min.X); x <= block(box.max.X); x++)
				out.emplace_back(x, y, z);
	return out;
}

} // namespace record
