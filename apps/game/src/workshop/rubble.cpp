// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "rubble.h"

#include "map.h"
#include "nodedef.h"
#include <chrono>

namespace workshop {

namespace {

constexpr int STEPS = shape::STEPS;
constexpr int MOST_FALL = 16 * STEPS; // past this it's over a drop too deep to be worth finding

s32 down(s32 v) { return v >= 0 ? v / STEPS : -((-v + STEPS - 1) / STEPS); }

shape::Shape nothing()
{
	shape::Shape s = shape::Shape::whole();
	for (s16 y = 0; y < STEPS; y++)
		for (s16 z = 0; z < STEPS; z++)
			for (s16 x = 0; x < STEPS; x++)
				s.set(v3s16(x, y, z), false);
	return s;
}

} // namespace

Laying::Laying(Map &map, const NodeDefManager *ndef, std::function<void(v3s16, MapNode)> place) :
	m_map(map), m_ndef(ndef), m_place(std::move(place))
{
}

Laying::Node &Laying::node(v3s16 p)
{
	const auto found = m_nodes.find(p);
	if (found != m_nodes.end())
		return found->second;
	const MapNode n = m_map.getNode(p);
	Node out{ n.getContent(), nothing(), false };
	if (n.getContent() == CONTENT_IGNORE) {
		out.solid = true;
	} else if (n.getContent() != CONTENT_AIR) {
		const ContentFeatures &f = m_ndef->get(n);
		if (f.liquid_type != LIQUID_NONE || itemgroup_get(f.groups, "liquid")) {
			out.solid = true;
		} else if (!f.walkable) {
			out.content = CONTENT_AIR; // a plant's buried
		} else {
			const shape::Shape *s = m_map.getShape(p);
			out.shape = s ? *s : shape::Shape::whole();
		}
	}
	return m_nodes.emplace(p, out).first->second;
}

bool Laying::drop(v3s32 at, content_t material)
{
	const s32 nx = down(at.X), nz = down(at.Z);
	const v3s16 in(at.X - nx * STEPS, 0, at.Z - nz * STEPS);
	const auto filled = [&](s32 y) {
		const s32 ny = down(y);
		const Node &n = node(v3s16(nx, ny, nz));
		return n.solid || n.shape.has(v3s16(in.X, y - ny * STEPS, in.Z));
	};
	const std::pair<s32, s32> column(at.X, at.Z);
	const auto known = m_tops.find(column);
	s32 y = at.Y;
	if (known != m_tops.end() && known->second <= at.Y) {
		y = known->second;
	} else {
		// inside something: up out of it; then down onto whatever's below, an empty node at a time
		for (int i = 0; filled(y); i++, y++)
			if (i > MOST_FALL)
				return false;
		for (int i = 0; !filled(y - 1); i++) {
			if (i > MOST_FALL)
				return false;
			const s32 below = down(y - 1);
			const Node &n = node(v3s16(nx, below, nz));
			y = !n.solid && n.shape.empty() ? below * STEPS : y - 1;
		}
	}
	m_tops[column] = y + 1;
	const s32 ny = down(y);
	Node &n = node(v3s16(nx, ny, nz));
	if (n.content == CONTENT_AIR)
		n.content = material;
	n.shape.set(v3s16(in.X, y - ny * STEPS, in.Z), true);
	n.changed = true;
	return true;
}

bool Laying::finish(float ms)
{
	const auto began = std::chrono::steady_clock::now();
	for (auto it = m_nodes.begin(); it != m_nodes.end(); it = m_nodes.erase(it)) {
		if (std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - began).count() > ms)
			return false;
		const v3s16 p = it->first;
		const Node &n = it->second;
		if (!n.changed)
			continue;
		const MapNode was = m_map.getNode(p);
		if (was.getContent() == CONTENT_AIR || !m_ndef->get(was).walkable)
			m_place(p, MapNode(n.content));
		m_map.setShape(p, n.shape);
	}
	m_tops.clear();
	return true;
}

} // namespace workshop
