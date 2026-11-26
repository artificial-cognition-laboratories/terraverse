// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/fluid/fluid.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// The layer's state as bytes, to keep with the world.
namespace fluid {

static const char MAGIC[8] = { 'T', 'F', 'W', 'X', 0, 0, 0, 1 };

template <typename T>
static void put(std::string &out, const T &v)
{
	out.append(reinterpret_cast<const char *>(&v), sizeof v);
}

template <typename T>
static T take(const std::string &in, size_t &at)
{
	if (at + sizeof(T) > in.size())
		throw FluidError("weather state: cut short");
	T v;
	std::memcpy(&v, in.data() + at, sizeof v);
	at += sizeof v;
	return v;
}

std::string Layer::save() const
{
	std::string out(MAGIC, sizeof MAGIC);
	put(out, m_opts.width);
	put(out, m_opts.height);
	put(out, m_opts.seed);
	put(out, m_time);
	put(out, m_steps);
	for (const auto *f : { &m_f.t, &m_f.q, &m_f.ts, &m_f.soil, &m_f.ash, &m_f.recent, &m_f.normal })
		for (double v : *f)
			put(out, v);
	put(out, (uint32_t)m_storms.size());
	for (const Storm &s : m_storms) { // field by field: the struct's padding isn't state
		put(out, (uint8_t)s.kind);
		for (double v : { s.lon, s.lat, s.radius, s.depth, s.age })
			put(out, v);
	}
	return out;
}

void Layer::load(const std::string &in)
{
	if (in.size() < sizeof MAGIC || std::memcmp(in.data(), MAGIC, sizeof MAGIC) != 0)
		throw FluidError("weather state: not a weather file (or an older version)");
	size_t at = sizeof MAGIC;
	if (take<int32_t>(in, at) != m_opts.width || take<int32_t>(in, at) != m_opts.height
			|| take<uint64_t>(in, at) != m_opts.seed)
		throw FluidError("weather state: saved for a different world");
	double time = take<double>(in, at);
	uint64_t steps = take<uint64_t>(in, at);
	Fields f = m_f;
	for (auto *field : { &f.t, &f.q, &f.ts, &f.soil, &f.ash, &f.recent, &f.normal })
		for (double &v : *field)
			v = take<double>(in, at);
	uint32_t count = take<uint32_t>(in, at);
	std::vector<Storm> storms;
	for (uint32_t k = 0; k < count; k++) {
		uint8_t kind = take<uint8_t>(in, at);
		if (kind > (uint8_t)StormKind::Tropical)
			throw FluidError("weather state: an unknown kind of storm");
		Storm s{ (StormKind)kind, 0, 0, 0, 0, 0 };
		for (double *v : { &s.lon, &s.lat, &s.radius, &s.depth, &s.age })
			*v = take<double>(in, at);
		storms.push_back(s);
	}
	if (at != in.size())
		throw FluidError("weather state: trailing bytes");
	m_f = std::move(f);
	m_storms = std::move(storms);
	m_time = time;
	m_steps = steps;
}

} // namespace fluid
