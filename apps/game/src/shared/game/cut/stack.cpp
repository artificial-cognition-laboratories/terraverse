// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/game/cut/stack.h"

#include "src/shared/game/cut/record.h"

namespace stack {

namespace {
constexpr int LAYERS = 64; // the most a stack's looked up through for room
s32 floorBlock(s32 v);
s32 roundUp(s32 v);
bool sameSize(const peel::Box &a, const peel::Box &b);
bool overlapsFlat(const peel::Box &a, const peel::Box &b);
} // namespace

peel::Box next(v3s32 base, v3s32 dims, const std::vector<peel::Box> &taken)
{
	const s32 wide = roundUp(dims.X), deep = roundUp(dims.Z);
	peel::Box box{};
	for (int layer = 0; layer < LAYERS; layer++)
		for (s32 z = 0; z + dims.Z <= deep; z += dims.Z)
			for (s32 x = 0; x + dims.X <= wide; x += dims.X) {
				const v3s32 at = base + v3s32(x, layer * dims.Y, z);
				box = peel::Box{ at, at + dims - v3s32(1, 1, 1) };
				bool free = true;
				for (const peel::Box &t : taken)
					free = free && !record::meets(t, box);
				if (free)
					return box;
			}
	return box; // full that high: the top slot, which the server will refuse
}

v3s32 base(const peel::Box &member, const std::vector<peel::Box> &same)
{
	// down the run it rests on: one its size under it each time
	peel::Box bottom = member;
	for (bool found = true; found;) {
		found = false;
		for (const peel::Box &s : same)
			if (sameSize(s, bottom) && s.max.Y == bottom.min.Y - 1 && overlapsFlat(s, bottom)) {
				bottom = s;
				found = true;
				break;
			}
	}
	return v3s32(floorBlock(bottom.min.X), bottom.min.Y, floorBlock(bottom.min.Z));
}

std::optional<peel::Box> above(const peel::Box &member, const std::vector<peel::Box> &same)
{
	std::optional<peel::Box> out;
	for (const peel::Box &s : same)
		if (sameSize(s, member) && s.min.Y == member.max.Y + 1 && overlapsFlat(s, member))
			out = s;
	return out;
}

namespace {
s32 floorBlock(s32 v)
{
	return (v >= 0 ? v / 16 : -((-v + 15) / 16)) * 16;
}

s32 roundUp(s32 v)
{
	return (v + 15) / 16 * 16;
}

bool sameSize(const peel::Box &a, const peel::Box &b)
{
	return a.max - a.min == b.max - b.min;
}

bool overlapsFlat(const peel::Box &a, const peel::Box &b)
{
	return a.min.X <= b.max.X && b.min.X <= a.max.X && a.min.Z <= b.max.Z && b.min.Z <= a.max.Z;
}
} // namespace

} // namespace stack
