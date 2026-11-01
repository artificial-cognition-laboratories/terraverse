// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/atlas.h"
#include "terragen/sphere.h"
#include <array>
#include <atomic>
#include <deque>
#include <string>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace planet {

/*
	The atlas (terragen::Atlas, planet.paper.md) in the engine's map: where
	each of its cells is stored, as a node.

	The rule the layout keeps, as the cube's net does: a plain step from a
	stored node (p + dir) is the true neighbour or the seam, never some
	other cell. Every region's grid (a segment, a cap, the core) is one box
	of the map, walled by a map block of seam: x its i (east), y its k (up:
	the layer, so the sea is where the engine expects it), z its j (north).
	A grid's trimmed cells are seam too.

	Tiles (planning/sprint-39-tiles.md): a grid is stored in tiles of TILE ×
	TILE cells (i, j; on the grid's own multiples of TILE, so a tile is the
	same whatever the planet's extent). The map is a window onto them: SLOTS
	× SLOTS slots, a map block of seam between, each holding a tile or
	none (the window: which tile is where, shared by every copy of the
	atlas; the server moves tiles in and out as players go, clients
	follow). A tile sits in its slot as it sits in its square of the grid,
	so a map block is the same 16 cells wherever the tile is. Crossing from
	tile to tile is crossing a seam, with no turn. Down, a tile keeps the
	crust: CRUST layers under the sea; below, seam (the planet's floor). The
	core is stored only where the crust reaches it.

	Bodies (planning/sprint-40-bodies.md): the atlas holds every body of the
	world that can be stood on, each its own shape (a terragen::Atlas), and
	the window holds tiles of any of them. A tile, a cell and a spot name
	their body; nothing crosses from one body to another (a step, a look,
	a chart stays on its body).

	A node's spot is where it is on its body whatever slot it's in: its
	region's grid at a fixed origin in a plane of 32-bit positions. What's
	made at a node (its rock, its caves) and where a block is saved are
	found from its spot, never from where the window put it.

	Crossing between regions is the atlas's (terragen::Atlas::neighbours):
	Map::next and Map::see go through it, never p + dir.
*/
class Atlas
{
public:
	// A cell of one of the bodies (its index in bodies()).
	struct Cell : terragen::Atlas::Cell
	{
		int body;
		Cell(int body, const terragen::Atlas::Cell &at) : terragen::Atlas::Cell(at), body(body) {}
		bool operator==(const Cell &o) const { return body == o.body && terragen::Atlas::Cell::operator==(o); }
	};

	static constexpr int32_t ORIGIN = -30912; // the lowest node the map generator makes
	static constexpr int32_t SPAN = 61808;    // nodes it makes across, from there
	static constexpr int32_t GAP = 16;        // seam between tiles: a map block
	static constexpr int32_t TILE = 4096;     // cells a tile side (i, j)
	static constexpr int32_t SLOT = TILE + GAP;
	static constexpr int32_t SLOTS = SPAN / SLOT; // slots a side of the map
	static constexpr int32_t CRUST = 30000;   // layers kept under the sea

	// A node of the map (the engine's v3s16).
	struct Node
	{
		int32_t x, y, z;
	};

	struct Rect
	{
		int32_t x, z, width, depth;
	};

	// A tile of a region's grid, as stored: its box (x, z; y is the layer,
	// k0 to k1 inclusive) and the grid cell at its corner.
	struct Tile
	{
		int body;
		terragen::Atlas::Region kind;
		int index;
		Rect rect;
		int32_t k0, k1;
		int64_t i0, j0;
	};

	// A tile of a body: its region (regionNumber), and which of the grid's
	// TILE squares it is.
	struct TileId
	{
		int body;
		int region;
		int32_t ti, tj;
		// every part named: a tile left without its body isn't one
		TileId(int body, int region, int32_t ti, int32_t tj) : body(body), region(region), ti(ti), tj(tj) {}
		bool operator==(const TileId &o) const { return body == o.body && region == o.region && ti == o.ti && tj == o.tj; }
	};
	// A node's place on its body whatever slot holds it (see above).
	struct Spot
	{
		int body;
		int32_t x, y, z;
		Spot(int body, int32_t x, int32_t y, int32_t z) : body(body), x(x), y(y), z(z) {}
	};
	static constexpr int32_t STRIDE = 1 << 22; // spots between regions' origins (x)

	// An empty window: no tile in the map until placed. Throws for no
	// bodies, or a body whose layers don't fit the map.
	explicit Atlas(const std::vector<terragen::Atlas::Params> &bodies);
	// One body.
	explicit Atlas(const terragen::Atlas::Params &body) : Atlas(std::vector{ body }) {}
	// One body, every tile in the window at once, slot by slot; throws if
	// they don't fit.
	static Atlas resident(const terragen::Atlas::Params &body);
	// Every tile of a body, in the order resident() places them.
	std::vector<TileId> allTiles(int body) const;

	// The world's home: the body it's played on (the system's playable
	// one), always the first.
	static constexpr int HOME = 0;
	// A body's shape (throws for no such body), and how many there are.
	const terragen::Atlas &body(int body) const;
	int bodies() const { return (int)m_bodies.size(); }
	// The body a node's tile is of; none where no tile is.
	std::optional<int> bodyAt(Node node) const;
	// The tiles in the window now.
	std::vector<Tile> tiles() const;

	/*
		The window. Which tile each slot holds (none: empty), and changing it:
		a tile in a slot (none empties it). Every copy of the atlas sees the
		change at once; what was found about blocks is found again. Throws
		for a slot out of range, a tile that isn't one, or a tile already in
		another slot.
	*/
	std::vector<std::optional<TileId>> window() const;
	void put(int slot, std::optional<TileId> tile);
	// How many times the window has changed.
	uint64_t generation() const;
	// The window as text (what a world records), and back.
	std::string windowCode() const;
	void setWindow(const std::string &code);
	// The slot a tile is in, if it's in the window.
	std::optional<int> slotOf(const TileId &tile) const;
	// The slot a node is in (whether or not it holds a tile); none off the slots.
	static std::optional<int> slotAt(Node node);

	// The tile a cell is in (the core's when it's stored; throws for a cell
	// below the crust, or of a core that isn't stored).
	TileId tileOf(const Cell &cell) const;
	// The tiles a ball round a point of a body reaches into (`radius`,
	// blocks): those of every cell sampled within it, a few blocks apart
	// along the ground.
	std::vector<TileId> tilesNear(int body, const terragen::Atlas::Vec &point, double radius) const;
	// The region's number: segments 0.., then the north cap, the south, the core.
	int regionNumber(int body, terragen::Atlas::Region kind, int index) const;
	// And back: a region number's kind and index (throws for no region).
	std::pair<terragen::Atlas::Region, int> regionOfNumber(int body, int number) const;

	// A node's spot (none where no tile is), and a cell's.
	std::optional<Spot> spot(Node node) const;
	Spot spotOf(const Cell &cell) const;

	// The cell a node stores, or none (seam, or off the atlas).
	std::optional<Cell> cell(Node node) const;
	// The node a place is charted from: its own, if it stores a cell; past
	// the atlas's top (something flown above the sky, sprint 59), the top
	// cell of its column, whose chart carries on up. None for the seam
	// between tiles and for trimmed cells: no place.
	std::optional<Node> charted(Node node) const;
	// The node a cell is stored at. Throws for a cell that isn't stored
	// (trimmed, or above the atlas).
	Node node(const Cell &cell) const;

	/*
		Crossing. Each cell's grid carries on past its region (its chart:
		terragen::Atlas::at), so "one step east of here", "two over and one
		up", and "where this mover now is" mean something across any seam.
		Positions are in storage axes (x, y, z; a node's centre is its integer
		position), and `turn` is in quarter turns, east toward north: a
		direction (dx, dz) of the near grid is turned(dx, dz, turn) in the far one (for node
		rotation and light); `angle` is the same turn, exact, in radians (for
		velocities: rotate (x, z) by it, x toward z).
	*/
	struct Step
	{
		Node node;
		int turn;
	};
	// A direction along the ground turned by `turn` quarter turns, east
	// toward north: (dx, dz) of one grid in the next one's.
	template <typename T>
	static std::array<T, 2> turned(T dx, T dz, int turn)
	{
		for (int i = 0; i < ((turn % 4) + 4) % 4; i++) {
			const T t = dx;
			dx = -dz;
			dz = t;
		}
		return { dx, dz };
	}
	// The node across a seam one step along a storage axis (0 x, 1 y, 2 z):
	// the cell straight through the face's middle (of perhaps several across
	// a T-junction). None when the plain step is honest (the same grid, or
	// seam with nothing beyond: the top of the atlas), or `from` stores no cell.
	std::optional<Step> step(Node from, int axis, int sign) const;

	struct Seen
	{
		Node node;
		int turn;
		double x, y, z; // where that node's centre is in `from`'s grid (storage axes)
	};
	// What's at `p` in `from`'s grid carried on: the node there, and where
	// its centre is in that grid (a T-junction's columns don't line up with
	// p). None when p is itself: in `from`'s own grid, or `from` stores no
	// cell. Past the top of the atlas, a seam node.
	std::optional<Seen> see(Node from, Node p) const;
	// The same at any point of `from`'s grid: always an answer when `from`
	// stores a cell (its own nodes included).
	std::optional<Seen> seeAt(Node from, double x, double y, double z) const;

	/*
		Map blocks (16 nodes a side; positions in blocks). What a client is
		sent and what it rebuilds are both found here, so the two agree.
	*/
	static constexpr int32_t BLOCK = 16;
	struct Block
	{
		int32_t x, y, z;
		bool operator==(const Block &o) const { return x == o.x && y == o.y && z == o.z; }
	};
	static Block blockOf(Node n)
	{
		const auto f = [](int32_t v) { return (int32_t)std::floor(v / (double)BLOCK); };
		return { f(n.x), f(n.y), f(n.z) };
	}
	// Where a map block is kept: its tile's spot, in blocks (a block is the
	// same 16 cells a side wherever its tile is). None for a block of no
	// tile: the seam between slots, an empty slot, off the slots.
	std::optional<Spot> blockSpot(Block block) const;
	// The blocks a block-sized box of `from`'s grid carried on, centred at
	// (x, y, z), shows: those its middle and its corners fall in (a box over
	// a seam lies across the other grid's blocks), and those across seams
	// from them that reach into it (cut cells between its corners). Seam
	// shows nothing. Kept, by the grid looked from (the same from any of its
	// cells) and the box.
	std::vector<Block> blocksAt(Node from, double x, double y, double z) const;
	// Whether a block and the nodes a step round it are all cells of one
	// grid, clear of its territory's edge (every step from it is plain).
	// Kept.
	bool interior(Block block) const;
	// How a block is drawn: one of its cells (a block holds one grid's cells
	// at most; its chart carries on over the whole block) and its middle on
	// the planet. A mesh's vertices go onto the planet by the chart, less
	// the middle (so they stay small), and the block is drawn moved there.
	// None for a block of no cells. Kept.
	struct BlockChart
	{
		Node cell;
		terragen::Atlas::Vec centre;
	};
	std::optional<BlockChart> blockChart(Block block) const;
	// Storage position (x, y, z) of a block on the planet, from its middle.
	terragen::Atlas::Vec onPlanet(const BlockChart &chart, double x, double y, double z) const
	{
		const terragen::Atlas::Vec p = *point(chart.cell, x, y, z);
		return { p.x - chart.centre.x, p.y - chart.centre.y, p.z - chart.centre.z };
	}

	// What a block's mesh reads past its own grid: each node of the block and
	// its rim that is seam (over a seam, or trimmed) beside a cell of the
	// block, and the cells across it covers (the one straight across first,
	// at `turn`; then the rest it overlaps). Kept.
	struct RimNode
	{
		Node at;
		int turn;
		std::vector<Node> across;
	};
	const std::vector<RimNode> &rim(Block block) const;

	// The blocks across a seam from a block's cells (what blocksAt
	// follows). Kept: the planet never changes.
	const std::vector<Block> &blocksAcross(Block block) const;

	struct Carried
	{
		double x, y, z; // storage position, in the new grid
		double angle;   // radians: rotate a velocity's (x, z) by this, x toward z
	};
	// A mover's node: the one a centimetre above its position (the engine's
	// positions are feet, and feet on a floor are on a node's face; across a
	// seam the node below can be another grid's). Every node taken from a
	// mover's position is this one: collision's, carry's.
	static constexpr double UP = 0.01;
	static Node moverNode(double x, double y, double z)
	{
		return { (int32_t)std::lround(x), (int32_t)std::floor(y + UP + 0.5), (int32_t)std::lround(z) };
	}

	// Something that moved from storage position `was` to (x, y, z), both in
	// the grid it was in: if it has left that grid, where it is in the grid
	// it's in now. None while it's still in its grid. Past the top of the
	// atlas it's charted from the column under it (charted), as the field is.
	std::optional<Carried> carry(double wasX, double wasY, double wasZ, double x, double y, double z) const;

	// Where storage position (x, y, z) is on the planet, in `from`'s grid
	// carried on. None if `from` stores no cell.
	std::optional<terragen::Atlas::Vec> point(Node from, double x, double y, double z) const;

	/*
		Seeing a region from another: where storage position s of a block's
		grid lands in the eye's grid carried on, as an affine map y = a·s + t
		(a row-major). Exact at the block's centre, first order round it: a
		map block (16 nodes, at most one region's cells) drawn with it is off
		by about 16²/8R nodes at its corners. How the client draws every
		block: the eye's own nearby blocks come out the identity.
	*/
	struct Affine
	{
		std::array<double, 9> a;
		std::array<double, 3> t;
		std::array<double, 3> operator()(double x, double y, double z) const
		{
			return { a[0] * x + a[1] * y + a[2] * z + t[0], a[3] * x + a[4] * y + a[5] * z + t[1],
				a[6] * x + a[7] * y + a[8] * z + t[2] };
		}
	};
	// A grid at a point: where the point is on the planet, and the grid's
	// three storage axes there (its chart's Jacobian). A block's never
	// changes; the eye's does as it moves. None if `from` stores no cell.
	struct Frame
	{
		int body;
		std::array<double, 3> at; // storage position
		terragen::Atlas::Vec origin;
		std::array<terragen::Atlas::Vec, 3> axes;
	};
	std::optional<Frame> frame(Node from, const std::array<double, 3> &at) const;
	// The view of a block's grid (`block`, at its centre) from the eye's;
	// throws for a block of another body (nothing of it is in the eye's
	// space).
	static Affine view(const Frame &eye, const Frame &block);

	/*
		A cut cell's solid: the part of its node inside its territory, cut by
		a plane for each edge of the territory that crosses it (one along a
		seam; two or three at a corner, where a segment's meridians meet a
		cap's circle), in the cell's own storage axes from its node's centre.
		Each edge is flat to about 1e-4 over a cell, and the cells beside it in
		both grids are cut by it: they meet on one surface, no gap and no
		overlap. Drawing, collision and pointing all take this as the cell.
		None for a whole cell, and for a node that stores no cell. Kept.
	*/
	struct Cut
	{
		// d with normal·d + offset >= 0 is kept; |normal| 1
		struct Plane
		{
			std::array<double, 3> normal;
			double offset;

			double at(double x, double y, double z) const { return normal[0] * x + normal[1] * y + normal[2] * z + offset; }
			// The storage axis it most faces.
			int axis() const;
		};
		std::vector<Plane> planes;

		// How far inside a point (storage, from the node's centre) is: >= 0 kept.
		double at(double x, double y, double z) const;
		// Boxes standing for the solid where only boxes will do (collision):
		// the node in `n` × `n` columns along the first plane's axis, each
		// kept to where its middle is. Storage, from the node's centre.
		struct Box
		{
			std::array<double, 3> low, high;
		};
		std::vector<Box> boxes(int n) const;
	};
	std::optional<Cut> cut(Node node) const;
	// A block's cut cells (cut() for the block's every node, found once),
	// each with what its cut faces lie against: for each plane, the nodes
	// just past it at its middle and toward its corners (where those are on
	// the face; none for a face the other planes leave nothing of). Empty
	// for an interior block. Kept: the planet never changes.
	struct CutCell
	{
		Node node;
		Cut cut;
		std::vector<std::vector<Node>> across; // by plane
	};
	using Cuts = std::vector<CutCell>;
	const Cuts &cutsIn(Block block) const;
	// Whether what a block's mesh reads of the atlas (its rim, its cut cells,
	// its chart) is found yet; and finding it (slow at seams: off the main
	// thread).
	bool prepared(Block block) const;
	void prepare(Block block) const;

	// A node that is always seam (above the atlas): what lies past the top.
	Node seam() const;

	// Where a node of a segment or a cap is on the planet: the direction of
	// its column (its cell at the surface layer) from the centre, as the
	// world model takes it. What the generator builds the column from. None
	// in seam and the core.
	std::optional<terragen::Place> place(Node node) const;
	// The point place() is the direction of: its column's cell centre at the
	// surface layer, blocks from the planet's centre.
	std::optional<terragen::Atlas::Vec> surface(Node node) const;
	// The same for a cell of a segment or a cap (throws for the core's).
	terragen::Place place(const Cell &cell) const;
	// A point's place from the centre, as the world model takes it (to 1/1024
	// of a block first, so it's the same wherever it's asked).
	static terragen::Place direction(const terragen::Atlas::Vec &point);

private:
	struct Layout;
	struct Found;
	const Layout &layout() const;
	Found &kept() const; // what's found about blocks, for the window as it is
	const Tile *tileAt(Node node) const;  // the tile whose box it's in, or none
	int tileIndex(Node node) const;       // its slot, −2 none
	Tile tileIn(int slot, const TileId &id) const; // a tile of the grid, placed in a slot

	// A cell's chart: its grid continued, d (storage axes) from its centre.
	terragen::Atlas::Vec chart(const Cell &c, double dx, double dy, double dz) const;
	std::array<double, 3> unchart(const Cell &c, const terragen::Atlas::Vec &point) const;
	double turning(const Cell &from, const Cell &to) const; // radians, `to`'s east from `from`'s
	int quarters(const Cell &from, const Cell &to) const;
	int face(const Cell &c, int axis, int sign) const; // storage axis → terragen face

	struct Body
	{
		terragen::Atlas shape;
		bool coreStored; // the crust reaches the core
	};
	std::vector<Body> m_bodies;
	const terragen::Atlas &shapeOf(const Cell &c) const { return m_bodies[c.body].shape; }
	Cell on(const Cell &c, const terragen::Atlas::Cell &at) const { return Cell(c.body, at); } // a cell of c's body

	// Which nodes of a block and its rim (BLOCK + 2 a side) are cells.
	std::vector<bool> cellsAround(Block block) const;
	std::vector<Block> findAcross(Block block) const;
	std::vector<Block> findAt(Node from, double x, double y, double z) const;
	std::optional<Step> findStep(Node from, const Cell &c, int axis, int sign) const;
	std::optional<BlockChart> findChart(Block block) const;
	std::vector<RimNode> findRim(Block block) const;
	std::optional<Cut> findCut(const Cell &c) const;
	std::vector<std::vector<Node>> findAcross(Node n, const Cut &cut) const;
	// What's found about blocks, kept (shared by copies of the atlas, and
	// between threads) while the window doesn't change
	struct Found
	{
		std::mutex lock;
		std::unordered_map<uint64_t, std::vector<Block>> across;     // by block
		std::unordered_map<uint64_t, std::optional<BlockChart>> charts; // by block
		std::unordered_map<uint64_t, std::vector<RimNode>> rims;     // by block
		std::map<std::array<int32_t, 4>, std::optional<Step>> steps; // by node and direction, at seams
		std::map<std::array<int64_t, 4>, std::vector<Block>> at;     // by the grid looked from, and the box's middle
		std::map<std::array<int32_t, 3>, std::optional<Cut>> cuts;   // cells near an edge, by node
		std::unordered_map<uint64_t, std::shared_ptr<const Cuts>> blockCuts; // by block
		std::unordered_map<uint64_t, bool> interior;                 // by block
	};
	// The window as it is: each slot's tile, placed. A change makes a new
	// one; the old are kept a while (KEPT changes), as what they found may
	// still be in use on another thread.
	struct Layout
	{
		std::vector<std::optional<Tile>> slots; // SLOTS × SLOTS
		std::vector<std::optional<TileId>> ids;
		std::unordered_map<uint64_t, int> slotOf; // by tile
		uint64_t generation = 0;
		mutable Found found;
	};
	static constexpr size_t KEPT = 64;
	struct Shared
	{
		std::mutex changing;
		std::atomic<const Layout *> current{ nullptr };
		std::deque<std::unique_ptr<Layout>> layouts; // the newest last
	};
	std::shared_ptr<Shared> m_shared = std::make_shared<Shared>();
};

} // namespace planet
