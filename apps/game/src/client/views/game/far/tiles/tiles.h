// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/land/mesh.h"
#include "src/client/views/game/far/tiles/tiling.h"
#include "src/client/views/game/far/tiles/budget.h"
#include "src/client/views/game/far/tiles/stats.h"
#include "src/client/views/game/far/land/terrain.h"
#include "irr_ptr.h"
#include "library/world/planet/atlas.h"
#include <CMeshBuffer.h>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

namespace terragen { class Step; }

namespace video { class IVideoDriver; }

namespace far {

class Palette;
class Workers;
struct Packed;

/*
	What tiles are chosen for: a point of view on the planet. Anything that
	looks at the planet makes one: the player's camera (far::View), the
	Planetarium's. In blocks.
*/
struct Eye
{
	Tiling::Vec at;   // the eye's point over the sea, from the planet's centre
	float altitude;  // above the sea: counts toward how far a tile is seen
	float cut;       // the near world's range (blocks; 0 none): tiles are finest from the eye out to it
	float range;     // farther than this along the ground, nothing is drawn
	float split;     // a tile splits while nearer than this many of its spans past the cut
	bool keep_fine = false; // what isn't built yet shows a coarser tile behind the rest (behind()), not instead of it
	// passing through: zooming fast, what's chosen now is gone in a moment.
	// Only a coarse preview of it's built meanwhile (tiles/preview.h; what's
	// held still draws); the rest once it's slowed (the Planetarium's zoom:
	// planetarium/zooming.h)
	bool hurried = false;
	// How much wider than what the eye would draw there a stand-in may be
	// (Eye::keep_fine); 0: any, a root's if nothing finer's built. Near the
	// ground, two levels: any ancestor stood in, a root's quarter kilometres
	// wide stood over a hole near the eye at its own coarse heights, and
	// seen edge-on, nothing in front of the sky, it stood across it, coming
	// and going as tiles were chosen again. A hole with nothing near enough
	// built stays open a moment, till its own tile is. An eye that never sees
	// across the ground (the Planetarium's, looking straight down) takes any.
	float stand_in = 4;
	// coming down (the Planetarium zooming in): the next level's built ahead
	// of tiles near splitting, after all that's needed now (Tiles: AHEAD)
	bool ahead = false;
	Tiling::Vec heading{ 0, 0, 0 }; // unit, or none: which way it's moving, on the planet. What's ahead is built first
	// What it can see, if it knows: the way it looks, the screen's right and
	// up (unit, square to each other), and how far its edges are off the way
	// it looks (the tangents of half its width and height, margin and all). A
	// tile out of that is neither drawn nor split: it's off screen (the
	// Planetarium's camera, looking down at a patch of the ground)
	Tiling::Vec look{ 0, 0, 0 }, right{ 0, 0, 0 }, upward{ 0, 0, 0 };
	float wide = 0, high = 0; // 0: sees all round
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
	// `driver` (none: a tile goes to the GPU when it's first drawn) takes each
	// tile as it's installed, within install's budget: drawn first, a body
	// coming into view sent all its tiles in one frame, 170 ms.
	// `budget`: a frame's time shared with the other Tiles drawn in it, its
	// owner starting each frame (none: one of its own, a frame an update).
	Tiles(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, unsigned threads,
			video::IVideoDriver *driver = nullptr, Budget *budget = nullptr);

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
		const std::vector<irr_ptr<TileBuffer>> &parts;
		// behind(): the quarter of it (a child's key) it stands in over; else none
		std::optional<TileKey> hole = std::nullopt;
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
	// where it is (blocks from the planet's centre). Its alpha is kept: it
	// marks a skirt (far/mesh.cpp), the land itself is opaque.
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
		std::vector<irr_ptr<TileBuffer>> parts;
		// the same land by the child it lies in, for standing in behind it
		// (behind()): made as it's taken, its own indices over `parts`'
		// vertices (one buffer, merged: quarter())
		std::array<std::vector<irr_ptr<TileBuffer>>, 4> quarters;
		struct Range
		{
			u32 v0 = 0, v1 = 0, i0 = 0, i1 = 0; // its vertices and indices in the merged buffer
		};
		std::array<Range, 4> ranges;
		bool merged = false; // parts is one buffer, the quarters' ranges in it (else they're its parts by quarter)
		std::vector<std::vector<video::SColor>> own; // each part's vertices' own colours (under a tint)
		u32 painted = 0; // the tint it's coloured by (Tiles::m_tints)
		bool ready = false;
		bool sketch = false; // ready, but only a stand-in till it's built (Generator::sketch)
		u32 used = 0; // the frame it was last wanted or drawn
		u32 whole = 0; // the frame it was last drawn as itself, not split
		size_t bytes = 0; // its mesh's (what evict() keeps the tiles under)
	};

	// A tile's middle (Tiling::middle) and half its diagonal, worked out once:
	// each costs a trip round the atlas (terragen::Atlas::at), and choosing
	// asked for them several times a tile, each frame (sprint 114)
	struct Place
	{
		Tiling::Vec middle;
		double half;
	};
	const Place &place(const Key &key) const;
	mutable std::map<Key, Place> m_places;

	// how far a tile is from the eye along the ground (blocks, less half its size)
	float distance(const TileKey &key, const Eye &eye) const;
	// whether any of a tile is within what the eye sees (Eye::look)
	bool seen(const TileKey &key, const Eye &eye) const;
	bool choose(const Key &key, const Eye &eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want);
	// Whether a tile `span` wide may stand in for `hole` (Eye::keep_fine): no
	// coarser than Eye::stand_in times what the eye would draw there
	bool fits(s32 span, const Key &hole, const Eye &eye) const;
	bool install(); // whether it took any
	void take(const TileKey &key, Packed &packed, bool sketch); // one built: its buffers, painted, on the GPU
	// Whether the eye's as it was when the tiles were last chosen (choosing
	// again would choose the same): to within a centimetre, looking the same way
	bool still(const Eye &eye) const;
	void paint(const Key &key, Entry &e) const; // its colours, tinted or its own
	void upload(TileBuffer &buffer) const;       // to the GPU, if there's a driver
	void repaint(); // tiles still in another tint's colours, within the budget
	bool evict(); // whether it let any go
	void choose(const Eye &eye); // the tiles to draw and to build, for the eye
	void quarter(Entry &e, const std::array<aabb3f, 4> &boxes); // a merged tile's quarters, over its own vertices

	const planet::Atlas m_atlas; // kept: the workers build from them
	const Horizon m_horizon;
	const Tiling m_tiling;
	std::unique_ptr<Workers> m_workers;
	std::map<Key, Entry> m_tiles;
	std::vector<Key> m_draw;
	std::vector<std::pair<Key, s32>> m_behind; // a tile, and the child of it it stands in for
	u32 m_frame = 0; // choosings so far (not frames: standing still, nothing's chosen, and nothing ages)
	std::optional<Eye> m_chosen_for; // the eye the tiles were last chosen for
	bool m_changed = true; // a tile taken or let go since: choose again
	size_t m_bytes = 0; // every held tile's (Entry::bytes)
	size_t m_pending = 0;
	size_t m_holes = 0; // tiles chosen that aren't there, with nothing allowed to stand in (Eye::stand_in)
	std::vector<std::pair<float, Key>> m_ahead; // children of tiles near splitting, asked for after the rest (AHEAD)
	size_t m_unready = 0; // stand-ins chosen with nothing yet to draw them with (quarter())
	std::unique_ptr<terragen::Step> m_coarse; // the roots as they're built, till the coarse planet's all there (terragen/progress.h)
	u32 m_settling = 0; // frames changed but still, since last chosen (update)
	video::IVideoDriver *m_driver; // none: tiles go to the GPU as they're drawn
	std::unique_ptr<Budget> m_own; // when none's shared
	Budget &m_budget;
	std::shared_ptr<const Tint> m_tint;
	u32 m_tints = 0; // tints set so far: a tile painted by this one is up to date
	u32 m_swept = 0; // the tint every held tile's been painted by, once repaint() finds none left
	TileStats m_stats; // what's done with the tiles, told to the trace
};

} // namespace far
