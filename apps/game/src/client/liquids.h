// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_aabb3d.h"
#include "irr_ptr.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <CMeshBuffer.h>
#include <ISceneNode.h>
#include <SMaterial.h>
#include <array>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ClientMap;
class IShaderSource;
class IShaderUniformSetterRC;
class NodeDefManager;
namespace video { class ITexture; class IVideoDriver; }

/*
	The liquid layer (sprint 81): liquid lying in the world's open space,
	drawn on its own, never by remeshing map blocks. Each node's fill
	(fill::Fill, kept by its map block) is drawn as a surface over the
	node's open columns at the height that much liquid stands at, and sides
	where it stands above what's beside it, in its mixture's colour. The
	sea's full blocks are drawn here too, so it meets the shore's fills.

	Water near the camera (sprint 82) is a grid of columns, each with the
	ground under it and a depth of water, run as shallow water by virtual
	pipes (waves, sloshing, wakes) and drawn on the GPU from a texture of
	its heights, each top on a sixteenth. Two grids: a fine one a sixteenth
	wide round the camera, and a coarse one a quarter wide round that, so
	the water's alive further out for the same work. Only tiles something
	stirred are run. The server's amounts are the truth: a pond's water is
	held to what the server has in it and moves freely within it; the sea is
	a reservoir held at its level. Poured liquid falls as sixteenth-cube
	drops that land in it. Only a picture: nothing goes back to the server.
*/
namespace liquids {

// Liquid poured, from a spout to where it lands (TOCLIENT_STREAM), in nodes
struct Stream
{
	v3f from, to;
	u8 r = 0, g = 0, b = 0;
	u16 cups = 0; // each a sixteenth-cube drop
};

// Something that pushes water aside: an object, the player; in nodes, its
// feet's middle and how far it reaches
struct Mover
{
	u32 id;
	v3f feet;
	float radius, height;
};

class Layer : public scene::ISceneNode
{
public:
	Layer(scene::ISceneManager *smgr, const NodeDefManager *ndef, IShaderSource *shaders);
	~Layer() override;

	// A frame: the blocks whose fills changed, those whose nodes did (or
	// that arrived), what was poured and what moves, the map, where the
	// scene's origin is (the camera offset, in nodes), where the camera is
	// (nodes), the sea's level (the highest node of it; none in a world without
	// one) and the time since the last frame.
	void update(const std::unordered_set<v3s16> &changed, const std::unordered_set<v3s16> &ground,
			const std::vector<Stream> &streams, const std::vector<Mover> &movers, ClientMap &map, v3s16 offset,
			v3f camera, std::optional<s16> sea, u32 daynight, float dtime, bool hurrying);

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

	// A grid's tile: 16×16 of its columns (a node fine, four a side coarse)
	struct Tile
	{
		bool known = false;   // looked at once: its water's been set from the server's
		bool scanned = false; // its ground and water looked at since they last changed
		bool stale = false;   // its ground changed while it was dry: looked at when water comes
		bool awake = false;
		bool drawn = false;   // its texels are as its water is
		bool sea = false;     // some of it is the sea's: held at its level, not to an amount
		float quiet = 0;      // s still
		float target = 0;     // nodes³ of water over its ground, as the server has it (not the sea's)
		float above = 0;      // ... of it over the sea's level (beside the sea, only that's held: what's under is the sea's)
		float credit = 0;     // nodes³ landed as drops the server hasn't sent back yet
		float volume = 0;     // nodes³ it holds now
		int pool = 0;         // the water it's joined to, held to the server as one
		bool coloured = false;
		u8 r = 0, g = 0, b = 0;
		bool wet = false;     // any top shown
		bool level = false;   // every top shown, at one height: drawn as one quad where it can be
		float mean = 0;       // its tops' mean, sixteenths over the grid's base
		float shade = 1;      // its light
		float low = 0, high = 0; // its lowest side's foot and highest top, sixteenths over the base (to cull it)
	};
	// A grid of columns round the camera
	struct Grid
	{
		int res;    // columns a node a side
		int nodes;  // nodes a side
		int follow; // nodes the camera goes from its middle before it follows
		int scans;  // tiles looked at a frame
		int cols;   // columns a side
		int tiles;  // tiles a side
		bool placed = false;
		v3s16 origin; // its corner node (x, z) and the height it reaches round (y)
		std::vector<s32> floor, shown, sea; // per column, world sixteenths: ground, top drawn, the sea's level (NONE: none)
		std::vector<s32> want;              // per column, world sixteenths: where the server's water stands (NONE: none)
		std::vector<float> depth;           // per column, node heights
		std::vector<std::array<float, 4>> out; // per column: flow out of each side (+x −x +z −z), node²/s
		std::vector<std::optional<v3s16>> tops; // per node: the liquid it draws (not flat)
		std::vector<Tile> tile;
		std::vector<int> nearest; // tiles, nearest the middle first
		float owed = 0, pooled = 0;
		s32 sea_level = INT32_MIN;     // world sixteenths, as last seen: water under it beside the sea is the sea's
		std::vector<char> coast;       // per pool: it touches the sea
		video::ITexture *heights = nullptr;
		std::vector<float> texels;
		bool upload = false;
	};

private:
	// Flat: the sea and fills beyond the grids, each node gliding to its level
	struct Shown
	{
		float level, target; // of its open space
		u8 r, g, b;
		bool sea = false; // a full block of liquid (the sea), not a fill
		// the sea's, looked at when its ground changes (not each time it's drawn):
		bool top = false;                // its top seen
		s8 from[4] = { -1, -1, -1, -1 }; // its sides: the sixteenth they stand from (−1: none)
	};
	struct Block
	{
		std::unordered_map<u16, Shown> nodes; // by position in the block
		scene::SMeshBuffer *mesh = nullptr;
		bool gliding = false;
	};

	struct Drop
	{
		v3f pos, vel; // nodes
		float cups;
		float age;
		u8 r, g, b;
		bool splash; // thrown up by another: it throws no more
	};

	void take(v3s16 blockpos, ClientMap &map, bool ground);
	void seaFaces(v3s16 p, Shown &s, ClientMap &map);
	void build(v3s16 blockpos, Block &b, ClientMap &map, u32 daynight);
	void buildDrops(ClientMap &map, u32 daynight);
	float shadeAt(ClientMap &map, v3s16 p, u32 daynight);

	// the grids
	bool owned(v3s16 p) const; // drawn by a grid, not flat
	bool covers(v3s16 p) const; // the fine grid draws there, ready (the coarse one doesn't then)
	void place(Grid &g);
	void scan(Grid &g, int tile, ClientMap &map);
	bool uneven(Grid &g, int tile);
	void wake(Grid &g, int tile);
	void pools(Grid &g);
	void hold(Grid &g, float dtime);
	void flow(Grid &g, float dt);
	void push(Grid &g, const Mover &m, v3f v, float dtime);
	void pour(const std::vector<Stream> &streams);
	void fall(ClientMap &map, float dtime);
	int land(Grid &g, Drop &d, std::vector<Drop> &thrown); // −1: not over it, 0: still falling, 1: landed
	bool settle(Grid &g, int tile);
	void texels(Grid &g, int tile, ClientMap &map, u32 daynight);
	void run(Grid &g, ClientMap &map, u32 daynight, float dtime, bool relight, const std::unordered_set<v3s16> &changed);
	void draw(Grid &g, video::IVideoDriver *driver);
	int columnAt(const Grid &g, float x, float z) const; // −1: outside
	int tileOf(const Grid &g, int column) const;

	const NodeDefManager *m_ndef;
	std::unordered_map<v3s16, Block> m_blocks;
	std::unordered_set<v3s16> m_relit;  // flat blocks to draw again by the day's light, a few a frame
	std::unordered_set<v3s16> m_reflat; // flat blocks to draw again: a grid took their liquid
	std::unordered_set<v3s16> m_unowned; // ... let go of it: drawn again at once (else a gap)

	bool m_gpu = false; // the grids are drawn on the GPU; without it (no GL 3), water's drawn flat only
	Grid m_fine, m_coarse;
	scene::SMeshBuffer *m_shape = nullptr; // a tile's columns: a top and four sides each
	scene::SMeshBuffer *m_whole = nullptr; // a level tile: one top
	video::SMaterial m_gpu_material;
	std::shared_ptr<struct WaterDraw> m_draw;
	irr_ptr<IShaderUniformSetterRC> m_uniforms;

	std::unordered_map<u32, v3f> m_movers; // where each was last frame
	std::vector<Drop> m_drops;
	scene::SMeshBuffer *m_drop_mesh = nullptr;
	std::mt19937 m_random{ 82 };
	v3s16 m_offset;
	v3f m_camera; // nodes
	// how far the map's blocks are drawn (nodes): past it the far view draws
	// the sea, so the flat sea stops where they do
	float m_range = 0;
	std::optional<s16> m_sea; // the sea's highest node: full water at or under it is the sea's, a reservoir
	video::SMaterial m_material;
	aabb3f m_box{ v3f(-1e7f), v3f(1e7f) }; // it is wherever liquid is: never culled as a whole
	float m_relight = 0; // s until the light's looked at again
	u32 m_daynight = 0;  // the day's light it was last drawn by
};

} // namespace liquids
