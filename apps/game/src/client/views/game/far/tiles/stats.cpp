// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/tiles/stats.h"
#include "library/trace/trace.h"

namespace far {

void TileStats::taken(const TileKey &key, bool sketch, size_t bytes)
{
	trace::count(sketch ? "Far view: sketches taken [#]" : "Far view: tiles taken [#]", 1, trace::Fold::Sum);
	trace::count("Far view: bytes taken [#]", (double)bytes, trace::Fold::Sum);
	if (sketch)
		return;
	if (!m_ever.insert({ key.level, key.x, key.z, key.face }).second)
		trace::count("Far view: tiles taken again [#]", 1, trace::Fold::Sum);
}

void TileStats::evicted(size_t count)
{
	trace::count("Far view: tiles evicted [#]", (double)count, trace::Fold::Sum);
}

void TileStats::now(size_t held, size_t bytes, size_t pending, size_t drawn, size_t holes, size_t unready)
{
	trace::count("Far view: tiles held [#]", (double)held, trace::Fold::Sum); // (summed: every Tiles reports once an update)
	trace::count("Far view: MB held [#]", bytes / 1048576.0, trace::Fold::Sum);
	trace::count("Far view: tiles pending [#]", (double)pending, trace::Fold::Sum);
	trace::count("Far view: tiles chosen [#]", (double)drawn, trace::Fold::Sum);
	trace::count("Far view: holes [#]", (double)holes, trace::Fold::Sum);
	trace::count("Far view: stand-ins not ready [#]", (double)unready, trace::Fold::Sum);
}

} // namespace far
