// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "fracture.h"

#include "constants.h"
#include "log.h"
#include "gamedef.h"
#include "motion.h"
#include "nodedef.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace workshop {

namespace {

constexpr int PER_NODE = 4;                    // cells a node, each way
constexpr int STEPS = shape::STEPS / PER_NODE; // sixteenths a cell, each way
constexpr float SIXTEENTH = 1.f / shape::STEPS;
constexpr float CELL = 1.f / PER_NODE;
constexpr u64 FULL = ~u64(0);
// A cell's reach past its box to find what it's standing on (nodes): a resting
// body sinks about this far in.
constexpr float TOUCH = 0.03f;
// A blast is over far quicker than a landing (s): its near side goes before the far side knows.
constexpr float BLAST_TIME = 0.02f;
// Of a blow's energy, what goes into cracks: most goes in heat, noise, bending and bounce.
constexpr float CRACK_SHARE = 0.05f;
// After a blow it may have bounced a little clear already.
constexpr float TOUCH_BLOW = 0.15f;

// A sixteenth's bit in its cell.
constexpr int bit(int x, int y, int z) { return x + STEPS * z + STEPS * STEPS * y; }

struct Key
{
	s32 x, y, z;
	bool operator==(const Key &o) const { return x == o.x && y == o.y && z == o.z; }
};
struct KeyHash
{
	size_t operator()(const Key &k) const
	{
		return (size_t)k.x * 73856093u ^ (size_t)k.y * 19349663u ^ (size_t)k.z * 83492791u;
	}
};
struct Raw
{
	u64 mask = 0;
	const Material *material = nullptr;
	unsigned index = 0;
};

} // namespace

std::optional<StructureOf> structureOf(const piece::Machine &m, const Materials &materials, IGameDef *gamedef)
{
	if (m.parts().size() != 1 || !m.links().empty())
		return std::nullopt;
	const piece::Machine::Part &part = m.parts().front();
	const core::matrix4 at = part.pose.matrix();

	std::unordered_map<Key, Raw, KeyHash> raw;
	std::vector<Key> order;
	bool judged = true, fails = false;
	part.object.built().each([&](v3s16 n, MapNode node, const shape::Shape *s) {
		const Material *mat = materials.get(node.getContent());
		if (!mat)
			throw std::runtime_error(gamedef->ndef()->get(node).name + " has no physics material");
		if (mat->role != Role::None)
			judged = false;
		fails |= mat->tension > 0 || mat->compression > 0;
		for (int cy = 0; cy < PER_NODE; cy++)
			for (int cz = 0; cz < PER_NODE; cz++)
				for (int cx = 0; cx < PER_NODE; cx++) {
					u64 mask = FULL;
					if (s) {
						mask = 0;
						for (int y = 0; y < STEPS; y++)
							for (int z = 0; z < STEPS; z++)
								for (int x = 0; x < STEPS; x++)
									if (s->has(v3s16(cx * STEPS + x, cy * STEPS + y, cz * STEPS + z)))
										mask |= u64(1) << bit(x, y, z);
					}
					if (!mask)
						continue;
					const Key key{ n.X * PER_NODE + cx, n.Y * PER_NODE + cy, n.Z * PER_NODE + cz };
					raw[key] = Raw{ mask, mat };
					order.push_back(key);
				}
	});
	if (!judged || !fails || order.empty() || order.size() > MOST_CELLS)
		return std::nullopt;

	StructureOf out;
	out.structure.cells.reserve(order.size());
	const float volume = SIXTEENTH * SIXTEENTH * SIXTEENTH;
	for (size_t i = 0; i < order.size(); i++) {
		Raw &r = raw[order[i]];
		r.index = (unsigned)i;
		const Key &k = order[i];
		// from the node's centre, as the physics' frame is
		const v3f least = v3f(k.x, k.y, k.z) * CELL - v3f(0.5f);
		aabb3f box(least, least + v3f(CELL));
		box.MinEdge *= BS;
		box.MaxEdge *= BS;
		at.transformBoxEx(box);
		box.MinEdge /= BS;
		box.MaxEdge /= BS;
		out.boxes.push_back(box);
		out.cells.push_back(v3s32(k.x, k.y, k.z));
		out.structure.cells.push_back({ box.getCenter(), (float)__builtin_popcountll(r.mask) * volume * r.material->density,
				r.material->tension, r.material->compression, r.material->toughness });
	}
	// Each face between neighbours: what's filled on both sides of it, and that
	// shape's second moments about its own middle.
	for (const Key &k : order) {
		const Raw &a = raw[k];
		for (int axis = 0; axis < 3; axis++) {
			Key next = k;
			(axis == 0 ? next.x : axis == 1 ? next.y : next.z)++;
			const auto found = raw.find(next);
			if (found == raw.end())
				continue;
			const Raw &b = found->second;
			// the two axes across it, in order
			const int p = axis == 0 ? 1 : 0, q = axis == 2 ? 1 : 2;
			int count = 0;
			double sum_p = 0, sum_q = 0, sum_pp = 0, sum_qq = 0;
			float lo_p = 1e9, hi_p = -1e9, lo_q = 1e9, hi_q = -1e9;
			for (int u = 0; u < STEPS; u++)
				for (int v = 0; v < STEPS; v++) {
					std::array<int, 3> in{}, there{};
					in[axis] = STEPS - 1;
					in[p] = there[p] = u;
					in[q] = there[q] = v;
					there[axis] = 0;
					if (!(a.mask >> bit(in[0], in[1], in[2]) & 1) || !(b.mask >> bit(there[0], there[1], there[2]) & 1))
						continue;
					count++;
					const float cu = (u + 0.5f) * SIXTEENTH, cv = (v + 0.5f) * SIXTEENTH;
					sum_p += cu;
					sum_q += cv;
					sum_pp += cu * cu;
					sum_qq += cv * cv;
					lo_p = std::min(lo_p, cu - SIXTEENTH / 2);
					hi_p = std::max(hi_p, cu + SIXTEENTH / 2);
					lo_q = std::min(lo_q, cv - SIXTEENTH / 2);
					hi_q = std::max(hi_q, cv + SIXTEENTH / 2);
				}
			if (!count)
				continue;
			const double da = SIXTEENTH * SIXTEENTH;
			const double mid_p = sum_p / count, mid_q = sum_q / count;
			// a square's own moment, plus each sixteenth's distance from the middle
			const double own = da * SIXTEENTH * SIXTEENTH / 12;
			const double along_p = da * (sum_pp - count * mid_p * mid_p) + count * own;
			const double along_q = da * (sum_qq - count * mid_q * mid_q) + count * own;
			physics::StructureBond bond;
			bond.a = a.index;
			bond.b = b.index;
			bond.axis = (unsigned char)axis;
			bond.length = CELL;
			bond.area = (float)(count * da);
			// bent about p, the fibres lie along q; and the other way
			bond.bend[0] = (float)along_q;
			bond.bend[1] = (float)along_p;
			bond.reach[0] = (float)std::max(hi_p - mid_p, mid_p - lo_p);
			bond.reach[1] = (float)std::max(hi_q - mid_q, mid_q - lo_q);
			out.structure.bonds.push_back(bond);
		}
	}
	return out;
}

std::vector<Cut> cut(const piece::Machine &m, const StructureOf &s, const physics::StructureResult &r, IGameDef *gamedef)
{
	const piece::Machine::Part &part = m.parts().front();
	std::unordered_map<Key, unsigned, KeyHash> group;
	for (size_t i = 0; i < s.cells.size(); i++)
		group[Key{ s.cells[i].X, s.cells[i].Y, s.cells[i].Z }] = r.group[i];
	std::vector<piece::Object> objects;
	for (unsigned g = 0; g < r.groups; g++)
		objects.emplace_back(gamedef);
	part.object.built().each([&](v3s16 n, MapNode node, const shape::Shape *whole) {
		std::unordered_map<unsigned, shape::Shape> shares;
		for (int y = 0; y < shape::STEPS; y++)
			for (int z = 0; z < shape::STEPS; z++)
				for (int x = 0; x < shape::STEPS; x++) {
					if (whole && !whole->has(v3s16(x, y, z)))
						continue;
					const Key key{ n.X * PER_NODE + x / STEPS, n.Y * PER_NODE + y / STEPS, n.Z * PER_NODE + z / STEPS };
					auto [share, fresh] = shares.try_emplace(group.at(key));
					if (fresh)
						for (int a = 0; a < shape::CELLS; a++)
							share->second.set(v3s16(a % 16, a / 256, a / 16 % 16), false);
					share->second.set(v3s16(x, y, z), true);
				}
		for (const auto &[g, share] : shares) {
			objects[g].design().map().setNode(n, node);
			objects[g].design().map().setShape(n, share);
		}
	});
	const core::matrix4 at = part.pose.matrix();
	std::vector<Cut> out;
	for (piece::Object &o : objects) {
		v3s16 least(piece::SIDE, piece::SIDE, piece::SIDE);
		o.design().each([&](v3s16 n, MapNode, const shape::Shape *) {
			least.X = std::min(least.X, n.X);
			least.Y = std::min(least.Y, n.Y);
			least.Z = std::min(least.Z, n.Z);
		});
		o.design().shift(-least);
		o.build();
		piece::Machine piece(gamedef);
		piece.setFront(m.front());
		piece.add({ std::move(o), part.pose });
		v3f by = v3f::from(least) * BS;
		by = at.rotateAndScaleVect(by);
		out.push_back({ std::move(piece), by / BS });
	}
	return out;
}

std::optional<StructureQuestion> asked(const piece::Machine &m, const Materials &materials, IGameDef *gamedef,
		const physics::World &world, physics::World::Id id, const std::optional<physics::World::Blow> &blow)
{
	std::optional<StructureOf> s = structureOf(m, materials, gamedef);
	if (!s)
		return std::nullopt;
	const physics::Quat q = world.pose(id).rotation;
	const physics::Quat back{ -q.x, -q.y, -q.z, q.w };
	const v3f gravity = turned(back, world.gravity(id));
	// the blow pushed it one way: its mass goes on the other, and what hit it is on that side
	const v3f lost = blow ? turned(back, blow->velocity) : v3f(0);
	const v3f spun = blow ? turned(back, blow->spin) : v3f(0);
	const v3f acceleration = gravity - lost / BLOW_TIME;
	if (acceleration.getLengthSQ() == 0 && spun.getLengthSQ() == 0)
		return std::nullopt; // nothing's pulling on it
	const v3f toward = lost.getLengthSQ() > 0.01f ? -v3f(lost).normalize()
			: gravity.getLengthSQ() > 0 ? v3f(gravity).normalize() : v3f(0);
	// each cell reached a little way that way: what's there is what holds it
	const float far = blow ? TOUCH_BLOW : TOUCH;
	std::vector<aabb3f> reach = s->boxes;
	for (aabb3f &b : reach) {
		const v3f by = toward * far;
		b.MinEdge += v3f(std::min(by.X, 0.f), std::min(by.Y, 0.f), std::min(by.Z, 0.f));
		b.MaxEdge += v3f(std::max(by.X, 0.f), std::max(by.Y, 0.f), std::max(by.Z, 0.f));
	}
	std::vector<bool> resting = world.touching(id, reach);
	// already bounced clear: it was hit on the side it was going
	if (blow && std::find(resting.begin(), resting.end(), true) == resting.end()) {
		float most = -1e9f;
		for (const aabb3f &b : s->boxes)
			most = std::max(most, b.getCenter().dotProduct(toward));
		for (size_t i = 0; i < resting.size(); i++)
			resting[i] = s->boxes[i].getCenter().dotProduct(toward) > most - CELL;
	}
	physics::StructureLoad load(acceleration, -spun / BLOW_TIME);
	if (blow) {
		float mass = 0;
		for (const physics::StructureCell &c : s->structure.cells)
			mass += c.mass;
		// (a turn's counted at about a node out)
		load.energy = std::max(1.f, CRACK_SHARE * 0.5f * mass * (lost.getLengthSQ() + spun.getLengthSQ()));
	}
	return StructureQuestion{ std::move(*s), std::move(resting), load };
}

std::optional<Blasted> blasted(const piece::Machine &m, const Materials &materials, IGameDef *gamedef,
		const physics::World &world, physics::World::Id id, v3f centre, float power)
{
	std::optional<StructureOf> s = structureOf(m, materials, gamedef);
	if (!s)
		return std::nullopt;
	const physics::Pose pose = world.pose(id);
	const physics::Quat q = pose.rotation, back{ -q.x, -q.y, -q.z, q.w };
	const v3f from = turned(back, centre - v3f::from(pose.position));
	std::unordered_map<Key, unsigned, KeyHash> at;
	for (size_t i = 0; i < s->cells.size(); i++)
		at[Key{ s->cells[i].X, s->cells[i].Y, s->cells[i].Z }] = (unsigned)i;
	const size_t n = s->cells.size();
	std::vector<v3f> each(n);
	v3f total(0);
	float mass = 0, energy = 0;
	for (size_t i = 0; i < n; i++) {
		const physics::StructureCell &c = s->structure.cells[i];
		mass += c.mass;
		const v3f away = c.centre - from;
		const float r2 = std::max(away.getLengthSQ(), CELL * CELL);
		const v3f dir = v3f(away).normalize();
		// only what's bare to the blast takes it: the next cell toward it shields this one
		const v3s32 cell = s->cells[i];
		const int axis = std::fabs(dir.X) > std::fabs(dir.Y) ? (std::fabs(dir.X) > std::fabs(dir.Z) ? 0 : 2) : (std::fabs(dir.Y) > std::fabs(dir.Z) ? 1 : 2);
		Key next{ cell.X, cell.Y, cell.Z };
		const float along = axis == 0 ? dir.X : axis == 1 ? dir.Y : dir.Z;
		(axis == 0 ? next.x : axis == 1 ? next.y : next.z) += along > 0 ? -1 : 1;
		if (at.count(next))
			continue;
		const v3f impulse = dir * (power / r2 * CELL * CELL);
		total += impulse;
		const float dv = impulse.getLength() / std::max(c.mass, 1e-3f);
		each[i] = impulse / std::max(c.mass, 1e-3f) / BLAST_TIME;
		energy += 0.5f * c.mass * dv * dv;
	}
	if (total.getLengthSQ() == 0)
		return std::nullopt;
	// the cell as a whole goes with the rest: what's left is what it's asked to take
	const v3f mean = total / std::max(mass, 1e-3f) / BLAST_TIME;
	for (v3f &a : each)
		a -= mean;
	const v3f gravity = turned(back, world.gravity(id));
	// Too quick for the floor to matter: the far side's own inertia is what the near side's
	// driven against, so it's the near side that goes.
	const v3f out = v3f(total).normalize();
	float most = -1e9f;
	for (const aabb3f &b : s->boxes)
		most = std::max(most, b.getCenter().dotProduct(out));
	std::vector<bool> resting(n);
	for (size_t i = 0; i < n; i++)
		resting[i] = s->boxes[i].getCenter().dotProduct(out) > most - CELL;
	physics::StructureLoad load(gravity);
	load.each = std::move(each);
	load.energy = std::max(1.f, CRACK_SHARE * energy);
	return Blasted{ StructureQuestion{ std::move(*s), std::move(resting), std::move(load) }, turned(q, total) };
}

} // namespace workshop
