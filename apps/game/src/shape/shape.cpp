// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "shape.h"

#include "constants.h"
#include "util/numeric.h"
#include "util/serialize.h"

#include <cmath>

namespace shape {

Shape Shape::whole()
{
	Shape s;
	s.m_bits.set();
	return s;
}

void Shape::boxes(std::vector<aabb3f> &out) const
{
	auto at = [](int v) { return (v / float(STEPS) - 0.5f) * BS; };
	for (int y = 0; y < STEPS; y++) {
		// the runs along x of this layer, each grown along z while the next
		// row has the same run
		std::bitset<STEPS * STEPS> taken;
		for (int z = 0; z < STEPS; z++)
			for (int x = 0; x < STEPS;) {
				if (!has({ s16(x), s16(y), s16(z) }) || taken[z * STEPS + x]) {
					x++;
					continue;
				}
				int end = x + 1;
				while (end < STEPS && has({ s16(end), s16(y), s16(z) }) && !taken[z * STEPS + end])
					end++;
				auto run = [&](int zz) {
					if (zz >= STEPS)
						return false;
					for (int i = x; i < end; i++)
						if (!has({ s16(i), s16(y), s16(zz) }) || taken[zz * STEPS + i])
							return false;
					return true;
				};
				int far = z + 1;
				while (run(far))
					far++;
				for (int zz = z; zz < far; zz++)
					for (int i = x; i < end; i++)
						taken[zz * STEPS + i] = true;
				out.emplace_back(at(x), at(y), at(z), at(end), at(y + 1), at(far));
				x = end;
			}
	}
}

void Shape::serialize(std::ostream &os) const
{
	char bytes[CELLS / 8] = {};
	for (size_t i = 0; i < CELLS; i++)
		if (m_bits[i])
			bytes[i / 8] |= char(1 << (i % 8));
	os.write(bytes, sizeof bytes);
}

Shape Shape::deSerialize(std::istream &is)
{
	char bytes[CELLS / 8];
	is.read(bytes, sizeof bytes);
	if (is.gcount() != sizeof bytes)
		throw SerializationError("Shape::deSerialize: truncated");
	Shape s;
	for (size_t i = 0; i < CELLS; i++)
		s.m_bits[i] = (bytes[i / 8] >> (i % 8)) & 1;
	return s;
}

const Shape *Shapes::get(v3s16 rel) const
{
	const auto it = m_shapes.find(key(rel));
	return it == m_shapes.end() ? nullptr : &it->second;
}

void Shapes::set(v3s16 rel, const Shape &s)
{
	if (s.isWhole())
		m_shapes.erase(key(rel));
	else
		m_shapes[key(rel)] = s;
}

void Shapes::serialize(std::ostream &os) const
{
	writeU16(os, u16(m_shapes.size()));
	for (const auto &[k, s] : m_shapes) {
		writeU16(os, k);
		s.serialize(os);
	}
}

void Shapes::deSerialize(std::istream &is)
{
	m_shapes.clear();
	const u16 count = readU16(is);
	for (u16 i = 0; i < count; i++) {
		const u16 k = readU16(is);
		if (k >= MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE)
			throw SerializationError("Shapes::deSerialize: node out of the block");
		m_shapes[k] = Shape::deSerialize(is);
	}
}

u16 Shapes::key(v3s16 rel)
{
	return u16((rel.Z * MAP_BLOCKSIZE + rel.Y) * MAP_BLOCKSIZE + rel.X);
}

v3s16 Shapes::unkey(u16 k)
{
	return v3s16(k % MAP_BLOCKSIZE, (k / MAP_BLOCKSIZE) % MAP_BLOCKSIZE, k / (MAP_BLOCKSIZE * MAP_BLOCKSIZE));
}

Sixteenth hit(v3s16 node, v3f point, v3s16 normal)
{
	// a sixteenth's width in from the face, so the face's own plane (where
	// the point lies) falls to the cell behind it
	const v3f in = (point - intToFloat(node, BS)) / BS + v3f(0.5f)
			- v3f(normal.X, normal.Y, normal.Z) * (0.5f / STEPS);
	const v3s16 c(s16(std::floor(in.X * STEPS)), s16(std::floor(in.Y * STEPS)), s16(std::floor(in.Z * STEPS)));
	return beside(Sixteenth{ node, c }, v3s16(0));
}

Sixteenth beside(Sixteenth s, v3s16 dir)
{
	Sixteenth out = s;
	out.cell += dir;
	for (int a = 0; a < 3; a++) {
		const int over = int(std::floor(out.cell[a] / float(STEPS)));
		out.cell[a] -= over * STEPS;
		out.node[a] += over;
	}
	return out;
}

aabb3f box(v3s16 cell)
{
	auto at = [](int v) { return (v / float(STEPS) - 0.5f) * BS; };
	return aabb3f(at(cell.X), at(cell.Y), at(cell.Z), at(cell.X + 1), at(cell.Y + 1), at(cell.Z + 1));
}

} // namespace shape
