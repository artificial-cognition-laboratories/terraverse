// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Cutting's placing half (cutting.h): where a member held would go, which way,
// lined up with what's built, and asking the server for it. Cutting itself is cutting.cpp.

#include "src/client/world/cut/cutting.h"
#include "src/client/world/cut/cutting_detail.h"

#include "src/client/world/objects/localplayer.h"
#include "src/client/world/session/client.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/content/nodemetadata.h"
#include "src/shared/game/cut/snap.h"
#include "src/shared/game/cut/stack.h"
#include "src/shared/game/pointing/pointedthing.h"
#include "src/shared/map/map.h"
#include <algorithm>
#include <cmath>

namespace cut {

using namespace detail;

namespace {
constexpr s32 GAP = 24; // sixteenths: how near a member must be, each side, to place between them
constexpr f32 RANGE = 4.0f * BS;  // how far out a member's placed
constexpr int DROP = 8;           // blocks: how far down from there the ground's looked for
} // namespace

void Cutting::look(const core::line3d<f32> &ray, Mode mode)
{
	// no further than a member's placed
	m_ray = ray;
	if (ray.getLength() > RANGE)
		m_ray.end = ray.start + ray.getVector().normalize() * RANGE;
	m_mode = mode;
}

std::optional<Cutting::Ghost> Cutting::ghost(const PointedThing &pointed) const
{
	const std::optional<Laying> l = laying(pointed);
	if (!l)
		return std::nullopt;
	const v3s32 from = record::world(l->aim.node, v3s16(0, 0, 0));
	const auto edge = [&](v3s32 c) { return (v3f(c.X - from.X, c.Y - from.Y, c.Z - from.Z) / shape::STEPS - v3f(0.5f)) * BS; };
	return Ghost{ l->aim.node, aabb3f(edge(l->box.min), edge(l->box.max + v3s32(1, 1, 1))), l->fits };
}

bool Cutting::place(const PointedThing &pointed) const
{
	const std::optional<Laying> l = laying(pointed);
	if (!l)
		return false;
	Json::Value payload;
	payload["at"] = list(l->box.min);
	payload["axes"] = list(v3s32(l->axes[0], l->axes[1], l->axes[2]));
	send(m_opts.client, "cut.place", payload);
	return true;
}

bool Cutting::turn()
{
	if (!held().isObject())
		return false;
	std::swap(m_view[SIDE], m_view[FORWARD]);
	return true;
}

bool Cutting::tip()
{
	if (!held().isObject())
		return false;
	// end over end: its length stood up from whichever way it lies, or laid back down that way
	const int along = m_view[SIDE] == 0 ? SIDE : m_view[FORWARD] == 0 ? FORWARD : UP;
	if (along != UP)
		m_laid = along;
	std::swap(m_view[UP], m_view[m_laid]);
	return true;
}

std::optional<Cutting::Laying> Cutting::laying(const PointedThing &pointed) const
{
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	const Json::Value &m = held();
	if (!player || !m.isObject())
		return std::nullopt;
	// the way they face, to the nearest quarter, is forward; across it is the side
	const float yaw = player->getYaw() * core::DEGTORAD;
	const int forward = std::abs(std::sin(yaw)) > std::abs(std::cos(yaw)) ? 0 : 2;
	const peel::Size size{ (s16)m["along"].asInt(), (s16)m["across"].asInt(), (s16)m["in"].asInt() };
	if (m_mode == Mode::Stack) // flat, along the way it lies unless that's up, then the way they look
		return stacking(pointed, size, m_view[SIDE] == 0 ? 2 - forward : forward);
	const std::optional<Aim> a = placingAim(pointed);
	if (!a)
		return std::nullopt;
	const int world[3] = { 2 - forward, 1, forward }; // SIDE, UP, FORWARD
	std::array<int, 3> axes; // along, across, in
	for (int part = 0; part < 3; part++)
		for (int v = 0; v < 3; v++)
			if (m_view[v] == part)
				axes[part] = world[v];
	peel::Box box = peel::lay(a->node, a->face, a->cell, peel::dims(size, axes), m_mode == Mode::Free);
	const int normal = a->face.X ? 0 : a->face.Y ? 1 : 2;
	if (a->between) {
		// in line with the two it's between, along the way they look
		const s32 middle = (normal == 0 ? a->between->X : normal == 1 ? a->between->Y : a->between->Z) / 4;
		const s32 shift = middle - ((normal == 0 ? box.min.X + box.max.X : normal == 1 ? box.min.Y + box.max.Y : box.min.Z + box.max.Z) / 2);
		(normal == 0 ? box.min.X : normal == 1 ? box.min.Y : box.min.Z) += shift;
		(normal == 0 ? box.max.X : normal == 1 ? box.max.Y : box.max.Z) += shift;
	}
	if (m_mode != Mode::Free)
		box = snap::align(box, near(box), normal);
	if (a->between) // longer than the gap: on their faces, not through them
		box = snap::clear(box, near(box), a->face);

	return Laying{ *a, axes, box, fits(box) };
}

std::optional<Cutting::Laying> Cutting::stacking(const PointedThing &pointed, const peel::Size &size, int along) const
{
	const std::optional<Aim> a = aimed(pointed);
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	if (!a || !player || pointed.intersection_point.getDistanceFrom(m_ray.start) > RANGE)
		return std::nullopt;
	ItemStack selected, hand;
	const std::string item = player->getWieldedItem(&selected, &hand).name;
	const v3s32 aim = record::world(a->node, a->cell);
	const auto axesOf = [](int along) { return std::array<int, 3>{ along, 2 - along, 1 }; };
	std::array<int, 3> axes = axesOf(along);
	std::optional<record::Member> on = record::at(near(peel::Box{ aim, aim }), aim);
	if (on && on->item != item)
		on.reset();
	v3s32 base;
	if (on) {
		// onto that stack, lying as it does
		if (peel::dims(size, axesOf(0)) == on->box.max - on->box.min + v3s32(1, 1, 1))
			axes = axesOf(0);
		else if (peel::dims(size, axesOf(2)) == on->box.max - on->box.min + v3s32(1, 1, 1))
			axes = axesOf(2);
		else
			on.reset(); // stood up, or a size we don't know: not a stack
	}
	const v3s32 dims = peel::dims(size, axes);
	const auto same = [&](const peel::Box &round) {
		std::vector<peel::Box> out;
		for (const record::Member &n : near(round))
			if (n.item == item)
				out.push_back(n.box);
		return out;
	};
	if (on) {
		base = stack::base(on->box, same(on->box));
	} else {
		// a new one in the block off the face they aim at (on top of what's aimed, on the ground),
		// its footprint centred on that block along its length
		const v3s32 off = aim + v3s32(a->face.X, a->face.Y, a->face.Z);
		base = record::world(blockAt(off), v3s16(0, 0, 0));
		if (a->face.Y == 1)
			base.Y = off.Y;
		const s32 blocks = ((along == 0 ? dims.X : dims.Z) + 15) / 16;
		(along == 0 ? base.X : base.Z) -= (blocks - 1) / 2 * 16;
	}
	const peel::Box column{ base, base + v3s32((dims.X + 15) / 16 * 16, 4 * 16, (dims.Z + 15) / 16 * 16) };
	const peel::Box box = stack::next(base, dims, same(column));
	return Laying{ Aim{ blockAt(box.min), v3s16(0, 1, 0), v3s16(0, 0, 0) }, axes, box, fits(box) };
}

bool Cutting::fits(const peel::Box &box) const
{
	// open blocks, or timber with room
	Map &map = m_opts.client->getEnv().getMap();
	const NodeDefManager *ndef = m_opts.client->ndef();
	bool fits = true;
	for (const v3s16 &p : record::blocks(box)) {
		if (!fits)
			break;
		bool valid;
		const MapNode n = map.getNode(p, &valid);
		if (!valid || ndef->get(n).buildable_to)
			continue;
		const shape::Shape *s = map.getShape(p);
		fits = ndef->get(n).groups.count("timber") && s;
		const v3s32 base = record::world(p, v3s16(0, 0, 0));
		for (s32 y = std::max(box.min.Y, base.Y); fits && y <= std::min(box.max.Y, base.Y + 15); y++)
			for (s32 z = std::max(box.min.Z, base.Z); fits && z <= std::min(box.max.Z, base.Z + 15); z++)
				for (s32 x = std::max(box.min.X, base.X); fits && x <= std::min(box.max.X, base.X + 15); x++)
					fits = !s->has(v3s16(x - base.X, y - base.Y, z - base.Z));
	}
	// it must rest against something: the ground, a block, another member
	const auto solid = [&](v3s32 c) {
		const v3s16 p(c.X >> 4, c.Y >> 4, c.Z >> 4); // sixteenths to their block (floor)
		bool valid;
		const MapNode n = map.getNode(p, &valid);
		if (!valid || ndef->get(n).buildable_to)
			return false;
		const shape::Shape *s = map.getShape(p);
		return !s || s->has(v3s16(c.X & 15, c.Y & 15, c.Z & 15));
	};
	return fits && record::touches(box, solid);
}

std::optional<Cutting::Aim> Cutting::placingAim(const PointedThing &pointed) const
{
	const v3f dir = m_ray.getVector().normalize();
	const f32 hit = pointed.type == POINTEDTHING_NODE ? pointed.intersection_point.getDistanceFrom(m_ray.start) : 1e9f;
	// between members (two posts, say), nearer than what the line hits: there, in the air
	if (m_mode != Mode::Free)
		if (const std::optional<Between> b = between(std::min(hit, m_ray.getLength()))) {
			Aim air = airAt(m_ray.start + dir * b->t, dir);
			air.between = b->middle;
			return air;
		}
	if (hit <= RANGE)
		return aimed(pointed);
	// past reach: on the ground under where the line ends (as if on the top of the
	// sixteenth below it), or in the air there if there's none near under it
	const v3f end = m_ray.end / BS * shape::STEPS + v3f(shape::STEPS / 2.0f);
	v3s32 below(std::floor(end.X), std::floor(end.Y) - 1, std::floor(end.Z));
	Map &map = m_opts.client->getEnv().getMap();
	const NodeDefManager *ndef = m_opts.client->ndef();
	for (int down = 0; down <= DROP; down++) {
		const v3s16 p = blockAt(below) - v3s16(0, down, 0);
		bool valid;
		const MapNode n = map.getNode(p, &valid);
		if (!valid || ndef->get(n).buildable_to)
			continue;
		// its top: the highest sixteenth there in the column (a cut block's shape, else whole)
		const shape::Shape *s = map.getShape(p);
		const v3s32 base = record::world(p, v3s16(0, 0, 0));
		const v3s16 at(below.X - base.X, 0, below.Z - base.Z);
		s16 top = shape::STEPS - 1;
		while (s && top >= 0 && !s->has(v3s16(at.X, top, at.Z)))
			top--;
		if (top < 0)
			continue;
		below = base + v3s32(at.X, top, at.Z);
		break;
	}
	const v3s16 node = blockAt(below);
	const v3s32 in = below - record::world(node, v3s16(0, 0, 0));
	return Aim{ node, v3s16(0, 1, 0), v3s16(in.X, in.Y, in.Z) };
}

std::optional<Cutting::Between> Cutting::between(f32 before) const
{
	// the members round the line: every block it passes and those beside it
	const v3f dir = m_ray.getVector().normalize();
	std::vector<record::Member> members;
	for (f32 t = 0; t <= before; t += 0.5f * BS)
		for (const record::Member &m : near(peel::Box{ cellAt(m_ray.start + dir * t), cellAt(m_ray.start + dir * t) }))
			if (std::none_of(members.begin(), members.end(), [&](const record::Member &o) {
					return o.box.min == m.box.min && o.box.max == m.box.max; }))
				members.push_back(m);
	// the point along it with a member to each side of it, each within GAP: the nearest such
	for (f32 t = 0; t <= before; t += BS / 8) {
		const v3s32 c = cellAt(m_ray.start + dir * t);
		int sides = 0;
		v3s32 first;
		const record::Member *one = nullptr;
		for (const record::Member &m : members) {
			const v3s32 nearest(std::clamp(c.X, m.box.min.X, m.box.max.X), std::clamp(c.Y, m.box.min.Y, m.box.max.Y),
					std::clamp(c.Z, m.box.min.Z, m.box.max.Z));
			const v3s32 off = nearest - c;
			if (off == v3s32(0, 0, 0) || std::abs(off.X) + std::abs(off.Y) + std::abs(off.Z) > GAP)
				continue;
			// one each side: the second's off the other way from the first
			if (sides == 0) {
				first = off;
				sides = 1;
				one = &m;
			} else if (off.X * first.X + off.Y * first.Y + off.Z * first.Z < 0) {
				return Between{ t, one->box.min + one->box.max + m.box.min + m.box.max };
			}
		}
	}
	return std::nullopt;
}

Cutting::Aim Cutting::airAt(v3f at, v3f dir)
{
	// laid against an imagined face turned to them, across the way they look
	const int axis = std::abs(dir.X) >= std::abs(dir.Y) && std::abs(dir.X) >= std::abs(dir.Z) ? 0 :
			std::abs(dir.Y) >= std::abs(dir.Z) ? 1 : 2;
	v3s16 face(0, 0, 0);
	const f32 d = axis == 0 ? dir.X : axis == 1 ? dir.Y : dir.Z;
	(axis == 0 ? face.X : axis == 1 ? face.Y : face.Z) = d > 0 ? -1 : 1;
	const v3s32 behind = cellAt(at) - v3s32(face.X, face.Y, face.Z);
	const v3s16 node = blockAt(behind);
	const v3s32 in = behind - record::world(node, v3s16(0, 0, 0));
	return Aim{ node, face, v3s16(in.X, in.Y, in.Z) };
}

std::vector<record::Member> Cutting::near(const peel::Box &box) const
{
	Map &map = m_opts.client->getEnv().getMap();
	std::vector<record::Member> out;
	const v3s32 grow(GAP, GAP, GAP);
	for (const v3s16 &p : record::blocks(peel::Box{ box.min - grow, box.max + grow }))
		if (const NodeMetadata *meta = map.getNodeMetadata(p))
			for (const record::Member &m : record::read(meta->getString(record::KEY)))
				if (std::none_of(out.begin(), out.end(), [&](const record::Member &o) {
						return o.box.min == m.box.min && o.box.max == m.box.max; }))
					out.push_back(m);
	return out;
}

} // namespace cut
