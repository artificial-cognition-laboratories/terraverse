// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/matter/fill/fill.h"
#include "src/shared/base/constants.h"
#include "src/shared/base/exceptions.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/util/data/serialize.h"
#include <map>
#include <mutex>
#include <vector>

namespace fill {

static std::mutex s_names_lock;
static std::vector<std::string> s_names{ "" }; // 0 is no liquid
static std::map<std::string, u16> s_ids;

u16 liquid(const std::string &n)
{
	std::lock_guard<std::mutex> lock(s_names_lock);
	if (n.empty())
		throw BaseException("fill::liquid: a liquid has a name");
	auto it = s_ids.find(n);
	if (it != s_ids.end())
		return it->second;
	if (s_names.size() >= 0xFFFF)
		throw BaseException("fill::liquid: too many liquids");
	s_names.push_back(n);
	return s_ids[n] = u16(s_names.size() - 1);
}

const std::string &name(u16 l)
{
	std::lock_guard<std::mutex> lock(s_names_lock);
	if (l == 0 || l >= s_names.size())
		throw BaseException("fill::name: no liquid " + std::to_string(l));
	return s_names[l];
}

u32 Room::total() const
{
	u32 n = 0;
	for (u16 l : layers)
		n += l;
	return n;
}

Room Room::of(const shape::Shape *s)
{
	Room r;
	for (int y = 0; y < 16; y++) {
		u16 n = 0;
		for (int z = 0; z < 16; z++)
			for (int x = 0; x < 16; x++)
				n += !s || !s->has(v3s16(x, y, z)) ? 1 : 0;
		r.layers[y] = s ? n : 256;
	}
	return r;
}

float Room::surface(float share) const
{
	const float total = (float)this->total();
	if (total <= 0)
		return 0;
	float left = std::max(0.0f, std::min(1.0f, share)) * total;
	for (int y = 0; y < 16; y++) {
		if (layers[y] == 0)
			continue;
		if (left <= layers[y])
			return (y + left / layers[y]) / 16.0f;
		left -= layers[y];
	}
	// full: its top is the top of its highest open layer
	for (int y = 15; y >= 0; y--)
		if (layers[y])
			return (y + 1) / 16.0f;
	return 0;
}

const Fill *Fills::get(v3s16 rel) const
{
	const auto it = m_fills.find(key(rel));
	return it == m_fills.end() ? nullptr : &it->second;
}

void Fills::set(v3s16 rel, const Fill &f)
{
	if (f.amount == 0 || f.liquid == 0)
		m_fills.erase(key(rel));
	else
		m_fills[key(rel)] = f;
}

// By name: a palette of the liquids in it, then each fill
void Fills::serialize(std::ostream &os) const
{
	std::vector<u16> palette;
	std::map<u16, u16> index;
	for (const auto &[k, f] : m_fills)
		if (!index.count(f.liquid)) {
			index[f.liquid] = u16(palette.size());
			palette.push_back(f.liquid);
		}
	writeU16(os, u16(palette.size()));
	for (u16 l : palette)
		os << serializeString16(name(l));
	writeU16(os, u16(m_fills.size()));
	for (const auto &[k, f] : m_fills) {
		writeU16(os, k);
		writeU16(os, index.at(f.liquid));
		writeU8(os, f.amount);
		writeU8(os, f.r);
		writeU8(os, f.g);
		writeU8(os, f.b);
	}
}

void Fills::deSerialize(std::istream &is)
{
	m_fills.clear();
	const u16 names = readU16(is);
	std::vector<u16> palette;
	for (u16 i = 0; i < names; i++)
		palette.push_back(liquid(deSerializeString16(is)));
	const u16 count = readU16(is);
	for (u16 i = 0; i < count; i++) {
		const u16 k = readU16(is);
		if (k >= MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE)
			throw SerializationError("Fills::deSerialize: node out of the block");
		const u16 p = readU16(is);
		if (p >= palette.size())
			throw SerializationError("Fills::deSerialize: no such liquid");
		Fill f;
		f.liquid = palette[p];
		f.amount = readU8(is);
		f.r = readU8(is);
		f.g = readU8(is);
		f.b = readU8(is);
		m_fills[k] = f;
	}
}

u16 Fills::key(v3s16 rel)
{
	return u16((rel.Z * MAP_BLOCKSIZE + rel.Y) * MAP_BLOCKSIZE + rel.X);
}

v3s16 Fills::unkey(u16 k)
{
	return v3s16(k % MAP_BLOCKSIZE, (k / MAP_BLOCKSIZE) % MAP_BLOCKSIZE, k / (MAP_BLOCKSIZE * MAP_BLOCKSIZE));
}

} // namespace fill
