// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/tiling.h"
#include "far/terrain.h"
#include "irr_ptr.h"
#include "planet/atlas.h"
#include <CMeshBuffer.h>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace terragen { class Step; }

namespace far {

class Palette;
class Workers;

/*
	What tiles are chosen for: a point of view on the planet. Anything that
	looks at the planet makes one: the player's camera (far::View), the
	Planetarium's. In blocks.
*/
struct Eye
{
	Tiling::Vec at;   // the eye's point over the sea, from the planet's centre
	float altitude;  // above the sea: counts toward how far a tile is seen
	float cut;       // nearer than this from the eye, someone else draws (the near world); 0 for none
	float range;     // farther than this along the ground, nothing is drawn
	float split;     // a tile splits while nearer than this many of its spans past the cut
	bool keep_fine = false; // what isn't built yet shows a coarser tile behind the rest (behind()), not instead of it
	Tiling::Vec heading{ 0, 0, 0 }; // unit, or none: which way it's moving, on the planet. What's ahead is built first
};

/*
	A planet's land from afar: a quadtree of its regions' tiles (far/tiling.h) for
	an Eye, finer near it and coarser with distance, built on worker
	threads nearest first. A tile keeps drawing until what replaces it is
	ready, so refining never opens holes. Each tile's mesh is in true
	positions on the planet round its middle (BS units): whoever draws it
	places the middle.
*/
class Tiles
{
public:
	// `palette` outlives the tiles: the workers read it. `threads`: how many
	// build them (the far view shares the machine with the game; the
	// Planetarium alone has most of it).
	// One body of the atlas's.
	Tiles(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, unsigned threads);

	// Blocks: how far a tile's land can stand off the sea (its peaks, its
	// trees): whether any of it is past the near world's range, and how far
	// past the eye's horizon a peak still shows.
	static constexpr double RELIEF = 256;

	// The threads a view may build with: a share of the machine's (0..1), at least one.
	static unsigned threadsFor(float share);
	~Tiles();

	const Tiling &tiling() const { return m_tiling; }

	// Once a frame: the tiles to draw for the eye (falling back to what's
	// ready), the rest asked of the workers, and a few finished ones taken
	// onto the GPU. Throws if building one failed.
	void update(const Eye &eye);

	struct Drawn
	{
		TileKey key;
		Tiling::Vec middle; // where its mesh is from, on the planet
		const std::vector<irr_ptr<scene::SMeshBuffer>> &parts;
	};
	// What update() chose, ready to draw.
	std::vector<Drawn> drawn() const;
	// How much of based() is there: the share of the roots built (0..1).
	float basedShare() const;
	// With Eye::keep_fine: coarser tiles filling holes, to draw behind everything else:
	// each only the quarter over its child that isn't there (all of it stood
	// above or below the finer land round the hole: its own heights, coarser).
	std::vector<Drawn> behind() const;
	// How many tiles update() wanted that aren't built yet: 0 once the eye's
	// view is complete.
	size_t pending() const { return m_pending; }
	// Whether every root is built: the coarse planet is all there.
	bool based() const;

	// A layer over the land: the colour a vertex is drawn, from its own and
	// where it is (blocks from the planet's centre). Its alpha is ignored:
	// the land is opaque.
	using Tint = std::function<video::SColor(video::SColor own, const Tiling::Vec &at)>;
	// The land recoloured by it (none: their own colours), a few tiles each
	// update (the drawn first) so no frame stalls; each as it's built.
	void tint(std::shared_ptr<const Tint> tint);
	// How many held tiles aren't in the tint's colours yet: 0 once it's swept over them.
	size_t unpainted() const;

private:
	struct Key
	{
		s32 level = 0, x = 0, z = 0, face = 0;
		Key() = default;
		Key(const TileKey &k) : level(k.level), x(k.x), z(k.z), face(k.face) {}
		Key(s32 level, s32 x, s32 z, s32 face) : level(level), x(x), z(z), face(face) {}
		bool operator<(const Key &o) const
		{
			return std::tie(level, face, x, z) < std::tie(o.level, o.face, o.x, o.z);
		}
		TileKey tile() const { return { level, x, z, face }; }
		Key child(s32 c) const { return { level - 1, x * 2 + (c & 1), z * 2 + (c >> 1), face }; }
	};
	struct Entry
	{
		std::vector<irr_ptr<scene::SMeshBuffer>> parts;
		std::array<std::vector<irr_ptr<scene::SMeshBuffer>>, 4> quarters; // the same parts, by the child they lie in
		std::vector<std::vector<video::SColor>> own; // each part's vertices' own colours (under a tint)
		u32 painted = 0; // the tint it's coloured by (Tiles::m_tints)
		bool ready = false;
		u32 used = 0; // the frame it was last wanted or drawn
		u32 whole = 0; // the frame it was last drawn as itself, not split
	};

	// how far a tile is from the eye along the ground (blocks, less half its size)
	float distance(const TileKey &key, const Eye &eye) const;
	// whether all of a tile is within the eye's cut (the near world's range)
	bool inside(const TileKey &key, const Eye &eye) const;
	bool choose(const Key &key, const Eye &eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want);
	void install();
	void paint(const Key &key, Entry &e) const; // its colours, tinted or its own
	void repaint(); // tiles still in another tint's colours, within the budget
	void evict();

	const planet::Atlas m_atlas; // kept: the workers build from them
	const Horizon m_horizon;
	const Tiling m_tiling;
	std::unique_ptr<Workers> m_workers;
	std::map<Key, Entry> m_tiles;
	std::vector<Key> m_draw;
	std::vector<std::pair<Key, s32>> m_behind; // a tile, and the child of it it stands in for
	u32 m_frame = 0;
	size_t m_pending = 0;
	std::unique_ptr<terragen::Step> m_coarse; // the roots as they're built, till the coarse planet's all there (terragen/progress.h)
	std::shared_ptr<const Tint> m_tint;
	u32 m_tints = 0; // tints set so far: a tile painted by this one is up to date
};

} // namespace far
