// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include "src/server/world/run/ground/ground.h"
#include "library/geometry/shape/shape.h"
#include <optional>

class Map;

// A column's solid shapes (Map::solidShape), walked down from a node a node at
// a time. A node's shape needs its own ground top and the one under it's, so
// walking down, each top (a dear thing: the columns round it searched) is
// found once rather than twice.
class ShapesDown
{
public:
	struct Opts
	{
		Map &map;
		v3s16 top; // the first node next() gives
	};
	explicit ShapesDown(Opts o);

	// The next node down's solid shape (none: whole, or open)
	std::optional<shape::Shape> next();

private:
	Map &m_map;
	v3s16 m_p;
	std::optional<std::optional<ground::Top>> m_here; // m_p's top, when found walking past
};
