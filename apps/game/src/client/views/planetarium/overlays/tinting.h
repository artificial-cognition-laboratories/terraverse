// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/tiles/tiles.h"
#include "src/client/views/planetarium/overlays/paint.h"
#include "library/world/planet/atlas.h"
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace terragen { class World; }

namespace planetarium {

// An overlay on the land (planning/sprint-83-planetarium.md): painted
// from the world model's fields as each tile is built (Painter), the
// land's relief kept as shading; on every body drawn from tiles, but
// only the tiled body's when it's masked by what's been explored. None:
// the land as it is.
struct Layer
{
	Overlay overlay;
	std::shared_ptr<const Painter::Opts::Image> image; // instead (Painter::Opts)
	std::optional<double> date; // 0..1 through the year; none: the year's mean
	std::optional<std::vector<u8>> explored; // Painter::Opts
	u32 grid = 0;
};

/*
	What colours each body's land under the layer shown: a tint for its
	tiles, from its world model (built on a thread of its own the first time,
	seconds). A body whose world isn't built yet is untinted till it is
	(ripe()).
*/
class Tinting
{
public:
	struct Opts
	{
		const planet::Atlas &atlas;
		const far::Horizon &horizon;
		int home; // the tiled body (its explored mask and the page's painting are its)
	};
	explicit Tinting(Opts opts);

	void layer(std::optional<Layer> layer) { m_layer = std::move(layer); }
	const std::optional<Layer> &layer() const { return m_layer; }
	// An atlas body's tint now (none: the land as it is, or its world's not built yet)
	std::shared_ptr<const far::Tiles::Tint> tintFor(int body);
	// Contour lines' spacing in blocks (0: none): on the elevation overlay
	float contour() const;
	// The bodies whose world was built since they were left untinted, once
	std::vector<int> ripe();

private:
	Opts m_opts;
	std::optional<Layer> m_layer;
	std::map<int, std::shared_future<std::shared_ptr<const terragen::World>>> m_worlds;
	std::set<int> m_untinted;
	std::shared_ptr<const terragen::World> worldOf(int body);
};

} // namespace planetarium
