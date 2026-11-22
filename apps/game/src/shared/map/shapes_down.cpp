// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/map/shapes_down.h"
#include "src/shared/map/map.h"
#include "src/shared/game/content/nodedef.h"

ShapesDown::ShapesDown(Opts o) :
	m_map(o.map), m_p(o.top)
{}

std::optional<shape::Shape> ShapesDown::next()
{
	const v3s16 p = m_p, under = p - v3s16(0, 1, 0);
	m_p = under;
	std::optional<std::optional<ground::Top>> here;
	here.swap(m_here);
	if (const shape::Shape *s = m_map.getShape(p))
		return *s;
	if (!here)
		here = m_map.groundTop(p);
	if (*here)
		return (*here)->shape(0);
	const MapNode n = m_map.getNode(p);
	if (n.getContent() == CONTENT_IGNORE || m_map.getNodeDefManager()->get(n).walkable)
		return std::nullopt;
	// open: unless smoothed ground below rises into it (kept: the next node's own)
	m_here = m_map.groundTop(under);
	if (*m_here) {
		shape::Shape high = (*m_here)->shape(1);
		if (!high.empty())
			return high;
	}
	return std::nullopt;
}
