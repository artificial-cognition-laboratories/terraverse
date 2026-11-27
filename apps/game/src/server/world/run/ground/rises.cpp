// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/ground/rises.h"
#include "src/shared/base/constants.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/util/data/serialize.h"
#include <algorithm>
#include <stdexcept>

namespace ground {

namespace {
// where key k is, or would go
template <class V>
auto find(V &v, u16 k)
{
	return std::lower_bound(v.begin(), v.end(), k, [](const auto &e, u16 key) { return e.first < key; });
}
} // namespace

int Rises::get(v3s16 rel) const
{
	const auto it = find(m_rises, key(rel));
	return it != m_rises.end() && it->first == key(rel) ? it->second : 0;
}

void Rises::set(v3s16 rel, int rise)
{
	if (rise < -8 || rise > 7)
		throw std::invalid_argument("ground::Rises: a rise of " + std::to_string(rise) + " sixteenths is out of −8..7");
	const u16 k = key(rel);
	const auto it = find(m_rises, k);
	const bool here = it != m_rises.end() && it->first == k;
	if (rise == 0) {
		if (here)
			m_rises.erase(it);
	} else if (here) {
		it->second = s8(rise);
	} else {
		m_rises.insert(it, { k, s8(rise) });
	}
}

void Rises::serialize(std::ostream &os) const
{
	writeU16(os, u16(m_rises.size()));
	for (const auto &[k, r] : m_rises) {
		writeU16(os, k);
		writeS8(os, r);
	}
}

void Rises::deSerialize(std::istream &is)
{
	m_rises.clear();
	const u16 count = readU16(is);
	m_rises.reserve(count);
	for (u16 i = 0; i < count; i++) {
		const u16 k = readU16(is);
		const s8 r = readS8(is);
		if (k >= MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE)
			throw SerializationError("Rises::deSerialize: node out of the block");
		if (r < -8 || r > 7 || r == 0)
			throw SerializationError("Rises::deSerialize: a rise out of −8..7");
		if (!m_rises.empty() && m_rises.back().first >= k)
			throw SerializationError("Rises::deSerialize: nodes out of order");
		m_rises.emplace_back(k, r);
	}
}

u16 Rises::key(v3s16 rel)
{
	return u16((rel.Z * MAP_BLOCKSIZE + rel.Y) * MAP_BLOCKSIZE + rel.X);
}

v3s16 Rises::unkey(u16 k)
{
	return v3s16(k % MAP_BLOCKSIZE, (k / MAP_BLOCKSIZE) % MAP_BLOCKSIZE, k / (MAP_BLOCKSIZE * MAP_BLOCKSIZE));
}

} // namespace ground
