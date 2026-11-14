// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <vector>

#include <list>
#include <map>
#include <ostream>
#include <set>
#include <unordered_map>
#include <memory>

#include "irrlichttypes_bloated.h"
#include "mapblock.h" // for forEachNodeInArea
#include "mapnode.h"
#include "shape/shape.h"
#include "ground/ground.h"
#include "constants.h"
#include "voxel.h"
#include "modifiedstate.h"
#include "util/numeric.h" // for forEachNodeInArea

class MapSector;
namespace planet { class Atlas; } // Terra Firma
class NodeMetadata;
class NodeTimer;
class IGameDef;

/*
	MapEditEvent
*/

enum MapEditEventType {
	// Node added (changed from air or something else to something)
	MEET_ADDNODE,
	// Node removed (changed to air)
	MEET_REMOVENODE,
	// Node swapped (changed without metadata change)
	MEET_SWAPNODE,
	// Node metadata changed
	MEET_BLOCK_NODE_METADATA_CHANGED,
	// Anything else (modified_blocks are set unsent)
	MEET_OTHER,
	// Terra Firma: a node's cut shape changed (Map::setShape)
	MEET_SHAPE,
	MEET_FILL, // Terra Firma: a node's liquid (sprint 81)
};

struct MapEditEvent
{
	MapEditEventType type = MEET_OTHER;
	v3s16 p;
	MapNode n = CONTENT_AIR;
	std::vector<v3s16> modified_blocks; // Represents a set
	bool is_private_change = false;
	// Setting low_priority to true allows the server
	// to send this change to clients with some delay.
	bool low_priority = false;

	MapEditEvent() = default;

	// Sets the event's position and marks the block as modified.
	void setPositionModified(v3s16 pos)
	{
		assert(modified_blocks.empty()); // only meant for initialization (once)
		p = pos;
		modified_blocks.push_back(getNodeBlockPos(pos));
	}

	void setModifiedBlocks(const std::map<v3s16, MapBlock *>& blocks)
	{
		assert(modified_blocks.empty()); // only meant for initialization (once)
		modified_blocks.reserve(blocks.size());
		for (const auto &block : blocks)
			modified_blocks.push_back(block.first);
	}

	VoxelArea getArea() const
	{
		switch(type){
		case MEET_ADDNODE:
		case MEET_REMOVENODE:
		case MEET_SWAPNODE:
		case MEET_BLOCK_NODE_METADATA_CHANGED:
		case MEET_SHAPE:
		case MEET_FILL:
			return VoxelArea(p);
		case MEET_OTHER:
		{
			VoxelArea a;
			for (v3s16 p : modified_blocks) {
				v3s16 np1 = p*MAP_BLOCKSIZE;
				v3s16 np2 = np1 + v3s16(MAP_BLOCKSIZE-1);
				a.addPoint(np1);
				a.addPoint(np2);
			}
			return a;
		}
		}
		return VoxelArea();
	}
};

class MapEventReceiver
{
public:
	// event shall be deleted by caller after the call.
	virtual void onMapEditEvent(const MapEditEvent &event) = 0;
};

class Map /*: public NodeContainer*/
{
public:

	Map(IGameDef *gamedef);
	virtual ~Map();
	DISABLE_CLASS_COPY(Map);

	void addEventReceiver(MapEventReceiver *event_receiver);
	void removeEventReceiver(MapEventReceiver *event_receiver);
	// event shall be deleted by caller after the call.
	void dispatchEvent(const MapEditEvent &event);

	// On failure returns NULL
	MapSector * getSectorNoGenerateNoLock(v2s16 p2d);
	// Same as the above (there exists no lock anymore)
	MapSector * getSectorNoGenerate(v2s16 p2d);

	/*
		This is overloaded by ClientMap and ServerMap to allow
		their differing fetch methods.
	*/
	virtual MapSector * emergeSector(v2s16 p){ return NULL; }

	// Returns InvalidPositionException if not found
	MapBlock * getBlockNoCreate(v3s16 p);
	// Returns NULL if not found
	MapBlock * getBlockNoCreateNoEx(v3s16 p);

	/* Server overrides */
	virtual MapBlock * emergeBlock(v3s16 p, bool create_blank=true)
	{ return getBlockNoCreateNoEx(p); }

	inline const NodeDefManager * getNodeDefManager() { return m_nodedef; }

	bool isValidPosition(v3s16 p);

	// throws InvalidPositionException if not found
	void setNode(v3s16 p, MapNode n);

	// Returns a CONTENT_IGNORE node if not found
	// If is_valid_position is not NULL then this will be set to true if the
	// position is valid, otherwise false
	MapNode getNode(v3s16 p, bool *is_valid_position = NULL);

	/*
		These handle lighting but not faces.
	*/
	virtual void addNodeAndUpdate(v3s16 p, MapNode n,
			std::map<v3s16, MapBlock*> &modified_blocks,
			bool remove_metadata = true);
	void removeNodeAndUpdate(v3s16 p,
			std::map<v3s16, MapBlock*> &modified_blocks);

	/*
		Wrappers for the latter ones.
		These emit events.
		Return true if succeeded, false if not.
	*/
	bool addNodeWithEvent(v3s16 p, MapNode n, bool remove_metadata = true);
	bool removeNodeWithEvent(v3s16 p);

	// Call these before and after saving of many blocks
	virtual void beginSave() {}
	virtual void endSave() {}

	virtual void save(ModifiedState save_level) { FATAL_ERROR("FIXME"); }

	/*
		Return true unless the map definitely cannot save blocks.
	*/
	virtual bool maySaveBlocks() { return true; }

	// Server implements these.
	// Client leaves them as no-op.
	virtual bool saveBlock(MapBlock *block) { return false; }
	virtual bool deleteBlock(v3s16 blockpos) { return false; }

	/*
		Updates usage timers and unloads unused blocks and sectors.
		Saves modified blocks before unloading if possible.
	*/
	void timerUpdate(float dtime, float unload_timeout, s32 max_loaded_blocks,
			std::vector<v3s16> *unloaded_blocks=NULL);

	/*
		Unloads all blocks with a zero refCount().
		Saves modified blocks before unloading if possible.
	*/
	void unloadUnreferencedBlocks(std::vector<v3s16> *unloaded_blocks=NULL);
	// Terra Firma: take every block in a box (block positions, inclusive) out
	// of memory, saving those modified when `save`. False when some are in
	// use: those stay (the rest are out).
	bool unloadArea(v3s16 low, v3s16 high, bool save);

	// Deletes sectors and their blocks from memory
	// Takes cache into account
	// If deleted sector is in sector cache, clears cache
	void deleteSectors(const std::vector<v2s16> &list);

	// For debug printing. Prints "Map: ", "ServerMap: " or "ClientMap: "
	virtual void PrintInfo(std::ostream &out);

	/*
		Node metadata
		These are basically coordinate wrappers to MapBlock
	*/

	std::vector<v3s16> findNodesWithMetadata(v3s16 p1, v3s16 p2);
	NodeMetadata *getNodeMetadata(v3s16 p);

	/**
	 * Sets metadata for a node.
	 * This method sets the metadata for a given node.
	 * On success, it returns @c true and the object pointed to
	 * by @p meta is then managed by the system and should
	 * not be deleted by the caller.
	 *
	 * In case of failure, the method returns @c false and the
	 * caller is still responsible for deleting the object!
	 *
	 * @param p node coordinates
	 * @param meta pointer to @c NodeMetadata object
	 * @return @c true on success, false on failure
	 */
	bool setNodeMetadata(v3s16 p, NodeMetadata *meta);
	void removeNodeMetadata(v3s16 p);

	/*
		Terra Firma: node shapes (sprint 44)
	*/

	// The node at p's cut shape, if it's cut (nothing if not, or unloaded).
	const shape::Shape *getShape(v3s16 p);
	// Cuts the node at p to `s`: a whole shape is none. Not empty: that's
	// the node removed (removeNodeAndUpdate). False if its block isn't here.
	bool setShape(v3s16 p, const shape::Shape &s);
	// Keeps `s` at p exactly, whole too: cut ground that's whole stays as it
	// is, never smoothed by its neighbours (sprint 85: a dug hole's walls).
	bool keepShape(v3s16 p, const shape::Shape &s);
	// Terra Firma: the liquid in a node's open space (sprint 81: fill::Fill),
	// nullptr: none. Setting it tells the clients (MEET_FILL), not the mesh;
	// false where its block isn't loaded.
	const fill::Fill *getFill(v3s16 p);
	bool setFill(v3s16 p, const fill::Fill &f);
	/*
		The boxes the node at p is stood on and pointed at, if it isn't
		its definition's: a cut node's sixteenths, smoothed ground's
		columns (node coordinates). False if it's its definition's.
	*/
	bool getShapedBoxes(v3s16 p, std::vector<aabb3f> &out);
	// The shape smoothed ground at p takes, if it's smoothed (not cut).
	std::optional<ground::Top> groundTop(v3s16 p);
	// ... as it was while the node at `at` was `was` (just before it changed)
	std::optional<ground::Top> groundTopAs(v3s16 p, v3s16 at, MapNode was);
	// Terra Firma (sprint 81): the sixteenths of the node at p that are
	// solid, where it's partly there: cut, smoothed ground (its own cell),
	// or the air a smoothed block below rises into. nullopt: whole (it's
	// all there or all open, as its node is). Where liquid can lie.
	std::optional<shape::Shape> solidShape(v3s16 p);
	/*
		Smoothed ground at p becomes cut nodes of the shape it had: its own
		cell, and what rises into the cell above (if that's open). Before
		it's cut the first time. Nothing if it isn't smoothed.
	*/
	void settleGround(v3s16 p);
	// What the node n at p is pointed at by: its shaped boxes, else its
	// definition's selection boxes.
	void getPointedBoxes(v3s16 p, MapNode n, std::vector<aabb3f> &out);

	/*
		Node Timers
		These are basically coordinate wrappers to MapBlock
	*/

	NodeTimer getNodeTimer(v3s16 p);
	void setNodeTimer(const NodeTimer &t);
	void removeNodeTimer(v3s16 p);

	/*
		Utilities
	*/

	// Iterates through all nodes in the area in an unspecified order.
	// The given callback takes the position as its first argument and the node
	// as its second. If it returns false, forEachNodeInArea returns early.
	template<typename F>
	void forEachNodeInArea(v3s16 minp, v3s16 maxp, F func)
	{
		v3s16 bpmin = getNodeBlockPos(minp);
		v3s16 bpmax = getNodeBlockPos(maxp);
		for (s16 bz = bpmin.Z; bz <= bpmax.Z; bz++)
		for (s16 bx = bpmin.X; bx <= bpmax.X; bx++)
		for (s16 by = bpmin.Y; by <= bpmax.Y; by++) {
			// y is iterated innermost to make use of the sector cache.
			v3s16 bp(bx, by, bz);
			MapBlock *block = getBlockNoCreateNoEx(bp);
			v3s16 basep = bp * MAP_BLOCKSIZE;
			s16 minx_block = rangelim(minp.X - basep.X, 0, MAP_BLOCKSIZE - 1);
			s16 miny_block = rangelim(minp.Y - basep.Y, 0, MAP_BLOCKSIZE - 1);
			s16 minz_block = rangelim(minp.Z - basep.Z, 0, MAP_BLOCKSIZE - 1);
			s16 maxx_block = rangelim(maxp.X - basep.X, 0, MAP_BLOCKSIZE - 1);
			s16 maxy_block = rangelim(maxp.Y - basep.Y, 0, MAP_BLOCKSIZE - 1);
			s16 maxz_block = rangelim(maxp.Z - basep.Z, 0, MAP_BLOCKSIZE - 1);
			for (s16 z_block = minz_block; z_block <= maxz_block; z_block++)
			for (s16 y_block = miny_block; y_block <= maxy_block; y_block++)
			for (s16 x_block = minx_block; x_block <= maxx_block; x_block++) {
				v3s16 p = basep + v3s16(x_block, y_block, z_block);
				MapNode n = block ?
						block->getNodeNoCheck(x_block, y_block, z_block) :
						MapNode(CONTENT_IGNORE);
				if (!func(p, n))
					return;
			}
		}
	}

	bool isBlockOccluded(MapBlock *block, v3s16 cam_pos_nodes)
	{
		return isBlockOccluded(block->getPosRelative(), cam_pos_nodes);
	}
	bool isBlockOccluded(v3s16 pos_relative, v3s16 cam_pos_nodes, bool dense = false);

	// Terra Firma: the planet this map stores, if it is one
	// (src/planet/atlas.h): its regions are the map's. Set once, before
	// anything reads the map.
	void setAtlas(const planet::Atlas *atlas) { m_atlas = atlas; }
	const planet::Atlas *getAtlas() const { return m_atlas; }

	/*
		Terra Firma: the node next to `p` along `dir` (one axis, ±1). On a
		flat world, p + dir. On a planet, a step across a seam lands in the
		next grid (planet::Atlas::step), and `turn` is the quarter turns the
		grid took (east toward north). Whatever asks "what's next to this
		node" asks this, so it holds across seams.
	*/
	struct Next
	{
		v3s16 pos;
		int turn;
		// Terra Firma, an atlas: where pos's centre is in the grid looked
		// from, less p (nodes). Across a T-junction columns don't line up.
		v3f offset{};
	};
	Next next(v3s16 p, v3s16 dir) const;

	/*
		Terra Firma: what's at `p` as seen from `from`'s grid. On a planet,
		past a seam `from`'s grid carries on into the next one
		(planet::Atlas::see): `pos` is the node the map keeps there and `turn`
		the grid's turn between. Elsewhere (a flat world, inside a grid, from
		seam) it's p itself. Whatever looks at an area round something
		(collision, meshes, rays) looks through this.
	*/
	Next see(v3s16 from, v3s16 p) const;
	// Terra Firma: every node `p`'s cell of `from`'s grid overlaps (an atlas
	// across a seam: one to a few, each at its own offset); see() alone
	// elsewhere. What collision gathers, so no sliver of a cell is missed.
	std::vector<Next> seeAll(v3s16 from, v3s16 p) const;

	// Terra Firma: how far apart two positions are (BS units): on a planet,
	// straight through it, each where it is on the planet (infinite for one
	// past the atlas's top); straight across the map if not.
	f32 distance(v3f a, v3f b) const;
	// The node at `p` seen from `from`'s grid, turned into that grid.
	MapNode getNodeSeen(v3s16 from, v3s16 p, bool *is_valid_position = nullptr);
	// Terra Firma: the node at p, from a grid `turn` quarter turns from its own
	MapNode getNodeTurned(v3s16 p, int turn);

protected:
	IGameDef *m_gamedef;
	const planet::Atlas *m_atlas = nullptr;   // Terra Firma

	std::set<MapEventReceiver*> m_event_receivers;

	std::unordered_map<v2s16, MapSector*> m_sectors;

	// Be sure to set this to NULL when the cached sector is deleted
	MapSector *m_sector_cache = nullptr;
	v2s16 m_sector_cache_p;

	// Delayed deletion of old metadata objects
	std::vector<std::unique_ptr<NodeMetadata>> m_metadata_trash;

	// This stores the properties of the nodes on the map.
	const NodeDefManager *m_nodedef;

	// Can be implemented by child class
	virtual void reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks) {}

	bool determineAdditionalOcclusionCheck(v3s16 pos_camera,
		const core::aabbox3d<s16> &block_bounds, v3s16 &to_check);
	bool isOccluded(v3s16 pos_camera, v3s16 pos_target,
		float end_offset, u32 needed_count, bool dense);
};

class MMVManip : public VoxelManipulator
{
public:
	MMVManip(Map *map);
	~MMVManip() override;
	DISABLE_CLASS_COPY(MMVManip)

	/*
		Loads specified area from map and *adds* it to the area already
		contained in the VManip.
	*/
	void initialEmerge(v3s16 blockpos_min, v3s16 blockpos_max,
		bool load_if_inexistent = true);

	/**
		Uses the flags array to determine which blocks the VManip covers,
		and for which of them we have any data.
		@warning requires VManip area to be block-aligned
		@return map of blockpos -> any data?
	*/
	std::map<v3s16, bool> getCoveredBlocks() const;

	/**
		Writes data in VManip back to the map. Blocks without any data in the VManip
		are skipped.
		@note VOXELFLAG_NO_DATA is checked per-block, not per-node. So you need
		to ensure that the relevant parts of m_data are initialized.
		@param modified_blocks output array of touched blocks (optional)
		@param overwrite_generated if false, blocks marked as generate in the map are not changed
	*/
	void blitBackAll(std::map<v3s16, MapBlock*> * modified_blocks,
		bool overwrite_generated = true) const;
	// Terra Firma: ... and the blocks from `always_min` to `always_max` (a
	// chunk's own, its border's generated neighbours left as they are)
	void blitBackAll(std::map<v3s16, MapBlock*> *modified_blocks,
		v3s16 always_min, v3s16 always_max) const;

	/*
		Creates a copy of this VManip including contents, the copy will not be
		associated with a Map.
	*/
	MMVManip *clone() const;

	// Reassociates a copied VManip to a map
	void reparent(Map *map);

	// Is it impossible to call initialEmerge / blitBackAll?
	inline bool isOrphan() const { return !m_map; }

	std::list<MMVManip **>::iterator addTrackedRef(MMVManip **ref_ref);

	void removeTrackedRef(std::list<MMVManip **>::iterator it);

	bool m_is_dirty = false;

protected:
	MMVManip() {};

	// may be null
	Map *m_map = nullptr;

private:
	// references to this that need to be cleared on destruction
	std::list<MMVManip **> m_tracked_refs;
};
