// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/pile/pile.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace pile {

Field::Field(int width, int depth, Material material) :
	m_w(width), m_d(depth), m_material(material),
	m_floor(size_t(width) * depth, 0), m_loose(size_t(width) * depth, 0),
	m_packed(size_t(width) * depth, 0), m_stirred(size_t(width) * depth, 0)
{
	if (width <= 0 || depth <= 0)
		throw std::invalid_argument("pile::Field: empty field");
	if (material.repose < 0 || material.yield < 0 || material.bearing <= 0 || material.give < 0)
		throw std::invalid_argument("pile::Field: a material needs repose, yield, give ≥ 0 and bearing > 0");
}

void Field::set(int x, int z, int floor, int loose)
{
	if (x < 0 || z < 0 || x >= m_w || z >= m_d)
		throw std::out_of_range("pile::Field::set: column " + std::to_string(x) + "," + std::to_string(z));
	if (loose < 0)
		throw std::invalid_argument("pile::Field::set: negative loose ground");
	m_floor[at(x, z)] = floor;
	m_loose[at(x, z)] = loose;
	m_packed[at(x, z)] = 0;
}

long long Field::total() const
{
	long long t = 0;
	for (int l : m_loose)
		t += l;
	return t;
}

bool Field::step()
{
	const int rest = m_material.repose;
	const int holds = rest + m_material.yield;
	bool moved = false;
	// Four phases (x pairs from even columns, from odd, then z): in each, a
	// column is in one pair at most, so no two slides meet and the order
	// they're visited never matters. Parity is the world's (the origin's),
	// so fields with other edges pair the same columns.
	auto pair = [&](int a, int b) {
		if (!m_stirred[a] && !m_stirred[b])
			return;
		const int ta = m_floor[a] + m_loose[a], tb = m_floor[b] + m_loose[b];
		const int d = std::abs(ta - tb);
		if (d <= holds)
			return;
		const int hi = ta > tb ? a : b, lo = ta > tb ? b : a;
		m_stirred[a] = m_stirred[b] = 1;
		// half the excess: never past level, so it can't ring; at least a sixteenth
		const int flux = std::min(m_loose[hi], std::max(1, (d - rest) / 2));
		if (flux <= 0)
			return;
		m_loose[hi] -= flux;
		m_loose[lo] += flux;
		moved = true;
	};
	for (int parity = 0; parity < 2; parity++)
		for (int z = 0; z < m_d; z++)
			for (int x = ((parity + m_ox) & 1); x + 1 < m_w; x += 2)
				pair(at(x, z), at(x + 1, z));
	for (int parity = 0; parity < 2; parity++)
		for (int z = ((parity + m_oz) & 1); z + 1 < m_d; z += 2)
			for (int x = 0; x < m_w; x++)
				pair(at(x, z), at(x, z + 1));
	return moved;
}

int Field::settle(int most)
{
	int n = 0;
	while (n < most && step())
		n++;
	return n;
}

long long Field::press(float cx, float cz, float radius, float kpa)
{
	if (radius <= 0 || kpa < 0)
		throw std::invalid_argument("pile::Field::press: a patch needs a radius and a pressure ≥ 0");
	const int x0 = std::max(0, int(std::floor(cx - radius - 2)));
	const int x1 = std::min(m_w - 1, int(std::ceil(cx + radius + 2)));
	const int z0 = std::max(0, int(std::floor(cz - radius - 2)));
	const int z1 = std::min(m_d - 1, int(std::ceil(cz + radius + 2)));
	const float r2 = radius * radius, ring2 = (radius + 1.5f) * (radius + 1.5f);
	const int sink = int(std::lround(kpa / m_material.bearing));
	const int give = m_material.give;
	long long out = 0;
	std::vector<int> ring;
	for (int z = z0; z <= z1; z++)
		for (int x = x0; x <= x1; x++) {
			const float dx = x + 0.5f - cx, dz = z + 0.5f - cz, d2 = dx * dx + dz * dz;
			const int i = at(x, z);
			if (d2 > r2) {
				if (d2 <= ring2) {
					ring.push_back(i);
					m_stirred[i] = 1;
				}
				continue;
			}
			m_stirred[i] = 1;
			// packed ground gives less: by what's left of its give, rounded up
			// so a press always reaches it
			const int left = std::max(0, give - m_packed[i]);
			const int take = std::min({ m_loose[i], left, (sink * left + give - 1) / std::max(1, give) });
			m_loose[i] -= take;
			m_packed[i] += take;
			out += take;
		}
	if (out == 0)
		return 0;
	if (ring.empty())
		throw std::logic_error("pile::Field::press: a patch with no ground beside it");
	// evenly to the ring, the remainder to its first columns (a fixed order)
	const long long n = static_cast<long long>(ring.size());
	const long long each = out / n, rem = out % n;
	for (long long k = 0; k < n; k++)
		m_loose[ring[k]] += int(each + (k < rem ? 1 : 0));
	return out;
}

} // namespace pile
