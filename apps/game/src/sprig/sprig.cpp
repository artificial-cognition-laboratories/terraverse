// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sprig.h"
#include "constants.h"
#include "exceptions.h"
#include "util/serialize.h"

namespace sprig {

const Sprig *Sprigs::get(v3s16 rel) const
{
	const auto it = m_sprigs.find(key(rel));
	return it == m_sprigs.end() ? nullptr : &it->second;
}

void Sprigs::serialize(std::ostream &os) const
{
	writeU16(os, u16(m_sprigs.size()));
	for (const auto &[k, s] : m_sprigs) {
		writeU16(os, k);
		writeU32(os, s.forest);
		writeU32(os, s.id);
		writeU8(os, s.pose);
	}
}

void Sprigs::deSerialize(std::istream &is)
{
	m_sprigs.clear();
	const u16 count = readU16(is);
	for (u16 i = 0; i < count; i++) {
		const u16 k = readU16(is);
		if (k >= MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE)
			throw SerializationError("Sprigs::deSerialize: node out of the block");
		Sprig s;
		s.forest = readU32(is);
		s.id = readU32(is);
		s.pose = readU8(is);
		m_sprigs[k] = s;
	}
}

u16 Sprigs::key(v3s16 rel)
{
	return u16((rel.Z * MAP_BLOCKSIZE + rel.Y) * MAP_BLOCKSIZE + rel.X);
}

v3s16 Sprigs::unkey(u16 k)
{
	return v3s16(k % MAP_BLOCKSIZE, (k / MAP_BLOCKSIZE) % MAP_BLOCKSIZE, k / (MAP_BLOCKSIZE * MAP_BLOCKSIZE));
}

} // namespace sprig
