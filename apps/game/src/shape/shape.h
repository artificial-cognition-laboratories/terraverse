// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_aabb3d.h"
#include "irr_v3d.h"
#include <bitset>
#include <iosfwd>
#include <optional>
#include <unordered_map>
#include <vector>

/*
	Shape: which sixteenths of a node its material fills (sprint 44). A
	node is the world's unit; a shape is how much of it is there, cut to a
	sixteenth each way. A node with no shape is whole.
*/
namespace shape {

constexpr int STEPS = 16; // a node's sixteenths each way
constexpr int CELLS = STEPS * STEPS * STEPS;

class Shape
{
public:
	static Shape whole();

	bool has(v3s16 c) const { return m_bits[index(c)]; }
	void set(v3s16 c, bool filled) { m_bits[index(c)] = filled; }
	bool empty() const { return m_bits.none(); }
	size_t count() const { return m_bits.count(); }
	bool isWhole() const { return m_bits.all(); }
	// At least half of it is there.
	bool half() const { return m_bits.count() * 2 >= CELLS; }
	bool operator==(const Shape &o) const { return m_bits == o.m_bits; }

	// What it fills: boxes in node coordinates (from its centre, BS), each
	// a run along x, merged along z where runs match.
	void boxes(std::vector<aabb3f> &out) const;

	void serialize(std::ostream &os) const;
	static Shape deSerialize(std::istream &is);

	// A sixteenth's coordinates in range (0..STEPS-1 each way).
	static bool inside(v3s16 c)
	{
		return c.X >= 0 && c.Y >= 0 && c.Z >= 0 && c.X < STEPS && c.Y < STEPS && c.Z < STEPS;
	}

private:
	static size_t index(v3s16 c) { return (size_t(c.Y) * STEPS + c.Z) * STEPS + c.X; }

	std::bitset<CELLS> m_bits;
};

/*
	What's left of `now` (none: whole) as a thing melts from outside in, or
	freezes back, at `cells` sixteenths. `exposed`: which faces it meets the open
	by, a bit each (+x −x +y −y +z −z). What's nearest them goes first, and most
	at the corners, where more than one is near. Goes on from `now`, as those
	faces change. At least one's left, in one piece: a bit cut off goes with what's
	melting, so it can be a little under `cells`.
*/
Shape melted(const Shape *now, int cells, unsigned exposed, bool melting);

/*
	A map block's cut nodes, by position in the block. Whoever changes a
	node's content drops its shape (Map::setNode, MapBlock::copyFrom); a
	whole shape is only kept when asked (keep: ground frozen as it stood,
	which smoothing would otherwise reshape).
*/
class Shapes
{
public:
	const Shape *get(v3s16 rel) const;
	// Keeps `s` for the node at rel; a whole one drops it.
	void set(v3s16 rel, const Shape &s);
	// Keeps `s` as it is, whole too (Map::keepShape)
	void keep(v3s16 rel, const Shape &s) { m_shapes[key(rel)] = s; }
	void erase(v3s16 rel) { m_shapes.erase(key(rel)); }
	bool empty() const { return m_shapes.empty(); }

	// Every cut node: (position in the block, shape).
	template <class F>
	void each(const F &f) const
	{
		for (const auto &[k, s] : m_shapes)
			f(unkey(k), s);
	}

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

private:
	static u16 key(v3s16 rel);
	static v3s16 unkey(u16 k);

	std::unordered_map<u16, Shape> m_shapes;
};

// One sixteenth of the world: a node, and the sixteenth in it.
struct Sixteenth
{
	v3s16 node;
	v3s16 cell;
};

/*
	The sixteenth a ray hit at `point` (world coordinates, BS) on a face
	with outward normal `normal`: the one just inside, counted from `node`
	(the node whose box was hit) and in whichever node it lies (smoothed
	ground rises into the cell above its own).
*/
Sixteenth hit(v3s16 node, v3f point, v3s16 normal);
// The sixteenth one step across `dir` (a unit step) from s: into the next
// node past its side.
Sixteenth beside(Sixteenth s, v3s16 dir);
// Its box, in its node's coordinates (from the centre, BS).
aabb3f box(v3s16 cell);

} // namespace shape
