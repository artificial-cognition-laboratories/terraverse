// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "basin.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace rivers {

static int64_t key(int32_t x, int32_t z) { return ((int64_t)x << 32) | (uint32_t)z; }

Basin Basin::find(int32_t x, int32_t z, int32_t level, const Top &top, size_t most)
{
	return find(x, z, level, top, most, nullptr);
}

Basin Basin::find(int32_t x, int32_t z, int32_t level, const Top &top, size_t most, const Water &water)
{
	Basin b;
	b.m_level = b.m_spill = level;
	struct Open
	{
		int32_t top, x, z;
		uint64_t order; // (the same tops reached in the order found: the same basin every time)
		bool operator>(const Open &o) const { return top != o.top ? top > o.top : order > o.order; }
	};
	std::priority_queue<Open, std::vector<Open>, std::greater<Open>> open;
	std::unordered_map<int64_t, int32_t> seen; // each column looked at, and its top
	uint64_t order = 0;
	open.push({ std::max(level, top(x, z)), x, z, order++ });
	seen.emplace(key(x, z), open.top().top);
	int32_t reached = level; // the flood's level: the highest top it's had to rise over
	static const int32_t SIDES[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	while (!open.empty()) {
		const Open c = open.top();
		open.pop();
		if (c.top < level || (water && c.top <= std::max(reached, level) && water(c.x, c.z, c.top))) {
			// lower than the pool, or water it joins: what reaches it runs away
			b.m_spill = reached;
			b.m_out_x = c.x, b.m_out_z = c.z;
			break;
		}
		reached = std::max(reached, c.top);
		b.m_columns.push_back({ c.x, c.z, c.top, reached });
		if (b.m_columns.size() >= most) {
			b.m_spill = reached;
			b.m_capped = true;
			break;
		}
		for (const auto &s : SIDES) {
			const int32_t nx = c.x + s[0], nz = c.z + s[1];
			if (seen.count(key(nx, nz)))
				continue;
			const int32_t t = top(nx, nz);
			seen.emplace(key(nx, nz), t);
			if (t == UNKNOWN)
				b.m_capped = true;
			else
				open.push({ t, nx, nz, order++ });
		}
	}
	// What's past the pass isn't its: lower than its crest (the far side
	// falling away) or reached only over higher ground. Ground just at its
	// crest's height it reaches is: what it covers once it rises over the
	// crest (the crest itself, a flat beyond, a river's next pool up). A pool
	// that spills at once has none.
	std::vector<Column> &cs = b.m_columns;
	const int32_t spill = b.m_spill;
	const bool dammed = b.m_spill > level;
	cs.erase(std::remove_if(cs.begin(), cs.end(), [&](const Column &c) {
		return c.enter > spill || (c.enter == spill && (!dammed || c.top != spill));
	}), cs.end());
	// its ways out: crest columns with the ground beside them falling away
	// under its crest (each looked at already, as the flood passed)
	std::unordered_set<int64_t> in;
	for (const Column &c : cs)
		in.insert(key(c.x, c.z));
	for (const Column &c : cs) {
		if (c.top != spill)
			continue;
		for (const auto &s : SIDES) {
			const int64_t k = key(c.x + s[0], c.z + s[1]);
			auto t = seen.find(k);
			if (!in.count(k) && t != seen.end() && t->second != UNKNOWN && t->second < spill) {
				b.m_rim.push_back({ c.x, c.z, c.top });
				break;
			}
		}
	}
	return b;
}

Basin Basin::spread(int32_t x, int32_t z, int32_t level, int32_t height, const Top &top, size_t most)
{
	Basin b;
	b.m_level = level;
	b.m_spill = height;
	std::vector<std::pair<int32_t, int32_t>> open{ { x, z } };
	std::unordered_set<int64_t> seen{ key(x, z) };
	static const int32_t SIDES[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	for (size_t i = 0; i < open.size(); i++) {
		const auto [cx, cz] = open[i];
		const int32_t t = i == 0 ? level : top(cx, cz);
		b.m_columns.push_back({ cx, cz, t, level });
		if (b.m_columns.size() >= most) {
			b.m_capped = true;
			break;
		}
		for (const auto &s : SIDES) {
			const int32_t nx = cx + s[0], nz = cz + s[1];
			if (!seen.insert(key(nx, nz)).second)
				continue;
			const int32_t nt = top(nx, nz);
			if (nt == UNKNOWN)
				b.m_capped = true;
			else if (nt >= level && nt < height)
				open.emplace_back(nx, nz);
		}
	}
	return b;
}

void Basin::rise(double level, const Top &top, size_t most)
{
	if (level <= m_spill)
		return;
	static const int32_t SIDES[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	std::unordered_set<int64_t> in;
	for (const Column &c : m_columns)
		in.insert(key(c.x, c.z));
	std::unordered_map<int64_t, int32_t> tops; // what's beside it, looked at
	auto topOf = [&](int32_t x, int32_t z) {
		auto it = tops.find(key(x, z));
		return it != tops.end() ? it->second : tops.emplace(key(x, z), top(x, z)).first->second;
	};
	for (size_t i = 0; i < m_columns.size() && m_columns.size() < most; i++) {
		const Column c = m_columns[i];
		for (const auto &s : SIDES) {
			const int32_t nx = c.x + s[0], nz = c.z + s[1];
			if (in.count(key(nx, nz)))
				continue;
			const int32_t t = topOf(nx, nz);
			if (t == UNKNOWN) {
				m_capped = true;
				continue;
			}
			if (t < m_spill || t >= level)
				continue; // falling away (its water runs off there), or over it
			in.insert(key(nx, nz));
			m_columns.push_back({ nx, nz, t, t });
		}
	}
	// its ways out: what it holds beside ground falling away under its crest
	m_rim.clear();
	for (const Column &c : m_columns) {
		if (c.top < m_spill)
			continue; // (its bed: under its crest, nothing beside it lower than its crest's outside it)
		for (const auto &s : SIDES) {
			const int64_t k = key(c.x + s[0], c.z + s[1]);
			if (in.count(k))
				continue;
			const int32_t t = topOf(c.x + s[0], c.z + s[1]);
			if (t != UNKNOWN && t < m_spill) {
				m_rim.push_back({ c.x, c.z, c.top });
				break;
			}
		}
	}
}

double Basin::outflow(double level) const
{
	double q = 0;
	for (const Rim &r : m_rim)
		if (level > r.top)
			q += WEIR * std::pow(level - r.top, 1.5);
	return q;
}

double Basin::volume(double level) const
{
	double v = 0;
	for (const Column &c : m_columns)
		if (level > c.enter)
			v += level - std::max(c.top, m_level);
	return v;
}

double Basin::area(double level) const
{
	double a = 0;
	for (const Column &c : m_columns)
		a += level >= c.enter; // (one reached just at this level is under its surface, holding nothing yet)
	return a;
}

double Basin::levelOf(double volume) const
{
	if (volume <= 0)
		return m_level;
	if (volume >= this->volume(m_spill))
		return m_spill;
	double lo = m_level, hi = m_spill;
	for (int i = 0; i < 40; i++) {
		const double mid = (lo + hi) / 2;
		(this->volume(mid) < volume ? lo : hi) = mid;
	}
	return (lo + hi) / 2;
}

} // namespace rivers
