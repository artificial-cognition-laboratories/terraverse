// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/tiles/preview.h"
#include <map>
#include <tuple>

namespace far::preview {

std::vector<std::pair<float, TileKey>> of(const std::vector<std::pair<float, TileKey>> &want, const Tiling &tiling,
		const std::function<bool(const TileKey &)> &ready)
{
	std::vector<std::pair<float, TileKey>> out;
	std::map<std::tuple<s32, s32, s32, s32>, size_t> at; // an ancestor's place in out
	for (const auto &[priority, key] : want) {
		if (priority < 0) {
			out.emplace_back(priority, key); // a root: wanted first, as it is
			continue;
		}
		TileKey up = key;
		for (int i = 0; i < LEVELS && up.level < tiling.top(key.face); i++)
			up = { up.level + 1, up.x >> 1, up.z >> 1, up.face };
		if (ready(up))
			continue;
		const auto id = std::make_tuple(up.level, up.x, up.z, up.face);
		auto it = at.find(id);
		if (it == at.end())
			at.emplace(id, out.size()), out.emplace_back(priority, up);
		else if (priority < out[it->second].first)
			out[it->second].first = priority;
	}
	return out;
}

} // namespace far::preview
