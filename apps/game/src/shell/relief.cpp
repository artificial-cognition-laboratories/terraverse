// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "relief.h"
#include "exceptions.h"
#include <algorithm>
#include <cmath>

namespace shell {

static constexpr float ROUND = 1.2f, ENDS = 2.5f; // s a round takes, s it stays at the end before it starts again

static std::vector<int> ints(const Json::Value &v)
{
	std::vector<int> out;
	out.reserve(v.size());
	for (const Json::Value &i : v)
		out.push_back(i.asInt());
	return out;
}

Relief::Relief(const Json::Value &baked) : m_side(baked["side"].asInt())
{
	if (m_side <= 0 || !baked["land"].isArray() || baked["land"].empty())
		throw BaseException("wiki: no river in the palette (bun palette)");
	m_columns = ints(baked["columns"]);
	for (const Json::Value &r : baked["land"])
		m_land.push_back(ints(r));
	for (const Json::Value &r : baked["sediment"])
		m_sediment.push_back(ints(r));
	for (const Json::Value &r : baked["stream"])
		m_stream.push_back(ints(r));
	m_core = ints(baked["core"]);
	m_placer = ints(baked["placer"]);
	m_lake = ints(baked["lake"]);
	m_river = ints(baked["river"]);
	for (const std::vector<int> &r : m_land)
		for (const int h : r)
			m_highest = std::max(m_highest, h);
}

void Relief::update(float dt)
{
	if (!running)
		return;
	if (at >= rounds() - 1) {
		m_hold += dt * (float)speed;
		if (m_hold > ENDS) {
			m_hold = 0;
			at = 0;
			m_version++;
		}
		return;
	}
	at = std::min<double>(rounds() - 1, at + dt * speed / ROUND);
	m_version++;
}

static u32 mix(u32 a, u32 b, float k)
{
	const auto c = [&](int s) { return (u32)(((a >> s) & 255) * (1 - k) + ((b >> s) & 255) * k) << s; };
	return c(16) | c(8) | c(0);
}

Scene::Ground Relief::ground() const
{
	constexpr float NODES = 4, TALLEST = 1.6f; // the patch's side and its highest ground, in the scene's nodes
	const int r0 = std::clamp((int)at, 0, rounds() - 1), r1 = std::min(r0 + 1, rounds() - 1);
	const float k = (float)at - r0;
	Scene::Ground out;
	out.corner = v3f(-NODES / 2, 0, -NODES / 2);
	out.width = out.depth = m_side;
	out.cell = NODES / m_side;
	out.rise = TALLEST / m_highest;
	out.top.resize((size_t)m_side * m_side);
	out.colours.resize(out.top.size());
	out.version = m_version;
	out.floor = 0;
	const std::vector<int> &first = m_land[0];
	for (size_t i = 0; i < m_columns.size(); i++) {
		const int c = m_columns[i];
		const float land = m_land[r0][c] + (m_land[r1][c] - m_land[r0][c]) * k;
		const float laid = m_sediment[r0][c] + (m_sediment[r1][c] - m_sediment[r0][c]) * k;
		const bool sea = land < 0, stream = m_stream[r1][c];
		out.top[i] = sea ? 0 : (int)std::max(1.f, land);
		u32 colour = land > m_highest * 0.6f ? 0x7c7c78 : land > m_highest * 0.3f ? 0x77785f : 0x5f7a45; // bare heights to green lowland
		if (lens == "land") {
			colour = mix(colour, 0xc9b27c, std::min(1.f, laid / 12));
		} else {
			colour = mix(colour, 0x2a2e33, 0.75f);
			if (lens == "worn") {
				const float gone = (float)first[c] - land;
				if (gone > 0)
					colour = mix(colour, 0xe0603d, std::min(1.f, gone / 40));
			} else if (lens == "sediment") {
				colour = mix(colour, 0xf0d890, std::min(1.f, laid / 12));
			} else if (lens == "heavy") {
				if (m_placer[c] > 0)
					colour = mix(colour, 0xffc832, std::min(1.f, m_placer[c] / 25.f) * std::min(1.f, laid / 4));
				if (m_core[c])
					colour = mix(colour, 0xb06ac0, 0.5f);
			}
		}
		if (stream && !sea)
			colour = 0x3f76e4;
		if (sea)
			colour = lens == "sediment" ? mix(0x1d3f7a, 0xf0d890, std::min(1.f, laid / 12)) : 0x1d3f7a;
		if (m_lake[c] && r1 == rounds() - 1 && k > 0.9f)
			colour = 0x4f8fe0;
		out.colours[i] = colour;
	}
	return out;
}

v3f Relief::focus() const
{
	constexpr float NODES = 4;
	float sx = 0, sz = 0;
	float weight = 0;
	const int last = rounds() - 1;
	for (size_t i = 0; i < m_columns.size(); i++) {
		const int c = m_columns[i];
		const float w = lens == "sediment" ? (float)m_sediment[last][c] : lens == "heavy" ? (float)m_placer[c] :
				lens == "worn" ? std::max(0.f, (float)(m_land[0][c] - m_land[last][c])) : 0;
		sx += w * (float)(i % m_side);
		sz += w * (float)(i / m_side);
		weight += w;
	}
	if (weight <= 0)
		return v3f(0);
	return v3f((sx / weight + 0.5f) / m_side * NODES - NODES / 2, 0, (sz / weight + 0.5f) / m_side * NODES - NODES / 2);
}

Json::Value Relief::state() const
{
	Json::Value out;
	out["round"] = at;
	out["rounds"] = rounds() - 1;
	return out;
}

double Relief::totalSediment(int round) const
{
	double total = 0;
	for (const int s : m_sediment[round])
		total += s;
	return total;
}

int Relief::lowestOnRiver(int round) const
{
	int low = 1 << 30;
	for (size_t c = 0; c < m_river.size(); c++)
		if (m_river[c] && m_land[round][c] >= 0)
			low = std::min(low, m_land[round][c]);
	return low;
}

int Relief::richest() const
{
	return m_placer.empty() ? 0 : *std::max_element(m_placer.begin(), m_placer.end());
}

} // namespace shell
