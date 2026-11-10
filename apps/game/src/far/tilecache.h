// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/terrain.h"
#include <cstdint>
#include <optional>
#include <string>

/*
	Far tiles kept on disk between runs (sprint 110): a planet opened before
	shows at once, its tiles read rather than built, and the world model
	isn't needed till a tile is missing. Kept by what decides a tile: the
	sources it's built from (FAR_SOURCES: any change to generation builds
	them afresh), the horizon (the code, the sea, the game's block ids) and
	the body's place on the atlas. Thread-safe: the far view's workers share one.

	Bounded (CAP_BYTES, every world's together): as tiles are written, now and
	then the least recently used go, read counting as used, so a world opened
	often keeps its planet and a long journey's fine tiles make way.
*/
namespace far {

// The hash of the sources a far tile is built from (generated: src/CMakeLists.txt)
extern const char FAR_SOURCES[];

class TileCache
{
public:
	static constexpr uintmax_t CAP_BYTES = (uintmax_t)512 << 20;
	static constexpr uintmax_t SWEEP_EVERY = (uintmax_t)32 << 20; // bytes written between sweeps

	// Under `root` (the engine's cache folder); other sources' tiles there are
	// removed. `cap`: what every world's together are swept down under
	TileCache(const std::string &root, const Horizon &horizon, const planet::Atlas &atlas, int body, uintmax_t cap = CAP_BYTES);

	std::optional<Tile> load(const TileKey &key) const;
	void save(const Tile &tile) const;

	// Down to under the cap if it's over, least recently used first (save()
	// calls it; public for the tests)
	void sweep() const;

private:
	std::string path(const TileKey &key) const;

	std::string m_base; // every world's
	std::string m_dir;  // this one's
	uintmax_t m_cap;
};

} // namespace far
