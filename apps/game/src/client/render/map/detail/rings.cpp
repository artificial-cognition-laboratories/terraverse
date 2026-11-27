// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/render/map/detail/rings.h"
#include <algorithm>

namespace detail {

Rings::Rings(Sight sight) : m_sight(sight)
{
}

void Rings::see(Sight sight)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sight = sight;
}

u8 Rings::make(v3s16 p, float d)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_made.find(p);
	const u8 detail = it == m_made.end() ? at(d) : held(d, it->second);
	m_made[p] = detail;
	return detail;
}

u8 Rings::wanted(v3s16 p, float d) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_made.find(p);
	return it == m_made.end() ? at(d) : held(d, it->second);
}

void Rings::forget(v3s16 p)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_made.erase(p);
}

u8 Rings::at(float d) const
{
	// a sixteenth's exponent is -4: 1 << (e + 4) sixteenths, between one and a whole node
	return u8(1 << std::clamp(m_sight.exponent(d) + 4, 0, 4));
}

u8 Rings::held(float d, u8 was) const
{
	// kept while it's the detail somewhere within the margin (details rise with distance)
	if (at(d * (1 - MARGIN)) <= was && was <= at(d * (1 + MARGIN)))
		return was;
	return at(d);
}

} // namespace detail
