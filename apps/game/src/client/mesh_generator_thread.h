// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013, 2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <atomic>

#include "mapnode.h"
#include "shape/shape.h"
#include "sprig/sprig.h"
#include <ctime>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "irrlichttypes_bloated.h"
#include "threading/mutex_auto_lock.h"
#include "util/thread.h"
#include <vector>
#include <memory>
#include <condition_variable>
#include <deque>
#include <thread>

class Map;
namespace planet { class Atlas; }
class MapBlock;
class MapBlockMesh;
struct MeshMakeData;
struct MeshUpdateResult;
class Client;

struct QueuedMeshUpdate
{
	v3s16 p = v3s16(-1337, -1337, -1337);
	std::vector<v3s16> ack_list;
	int crack_level = -1;
	v3s16 crack_pos;
	MeshMakeData *data = nullptr; // This is generated in MeshUpdateQueue::pop()
	std::vector<MapBlock*> map_blocks;
	// Terra Firma: on a planet, the nodes round the mesh that are past its
	// face's edge, as seen from it (Map::getNodeSeen): written over what the
	// net has there (the seam) before meshing, so the next face's blocks
	// meet this one's.
	std::vector<std::pair<v3s16, MapNode>> halo;
	const planet::Atlas *atlas = nullptr; // Terra Firma: the map's, if it's one
	// Terra Firma, an atlas: the blocks the halo read over a seam (present
	// or not): when one arrives, this mesh is made again
	std::vector<v3s16> read;
	// Terra Firma, an atlas: what each cut cell of the mesh (planet::Atlas::cut)
	// shows across each of its cut faces: the node there, or, where a face
	// lies over several kinds, air lit as the air across is (so it's drawn)
	std::vector<std::pair<v3s16, std::vector<MapNode>>> cuts;
	// Terra Firma: the cut nodes of the blocks read (absolute), copied here
	// on the main thread, which changes them
	std::vector<std::pair<v3s16, shape::Shape>> shapes;
	std::vector<std::pair<v3s16, sprig::Sprig>> sprigs; // Terra Firma: of the cell's own blocks
	u16 epoch = U16_MAX;
	bool urgent = false;

	QueuedMeshUpdate() = default;
	~QueuedMeshUpdate();

	/**
	 * Get blocks needed for this mesh update from the map.
	 * Blocks that were already loaded are skipped.
	 * @param map Map
	 * @param cell_size mesh grid cell size
	 */
	void retrieveBlocks(Map *map, u16 cell_size);
	// The mesh's nodes, from what retrieveBlocks took (any thread).
	void fill(MeshMakeData *data) const;
	/**
	 * Drop block references.
	 * @note not done by destructor, since this is only safe on main thread
	 */
	void dropBlocks();
	/**
	 * Check if the blocks that would comprise the mesh are all air, so the
	 * update can be skipped entirely.
	 * @param cell_size mesh grid cell size
	 * @return (true = all air)
	 */
	bool checkSkip(u16 cell_size);
};

/*
	A thread-safe queue of mesh update tasks and a cache of MapBlock data
*/
class MeshUpdateQueue
{
public:
	MeshUpdateQueue(Client *client);

	~MeshUpdateQueue();

	/**
	 * Caches the block at p and its neighbors (if needed) and queues a mesh
	 * update for the block p.
	 * @param map Map
	 * @param p block position
	 * @param ack_to_server Should be acked to server when done?
	 * @param urgent High-priority?
	 * @param from_neighbor was this update only necessary due to a neighbor change?
	 */
	bool addBlock(Map *map, v3s16 p, bool ack_to_server, bool urgent, bool from_neighbor);

	/**
	 * Re-queues a block that was already finished.
	 */
	void reAddBlock(MeshUpdateResult &r);

	// Update internal epoch counter
	void setEpoch(u16 epoch) {
		MutexAutoLock lock(m_mutex);
		m_epoch = epoch;
	}

	// Returned pointer must be deleted
	// Returns NULL if queue is empty
	QueuedMeshUpdate *pop();

	// Marks a position as finished, unblocking the next update
	void done(v3s16 pos);

	size_t size()
	{
		MutexAutoLock lock(m_mutex);
		return m_queue.size();
	}

	/// @param finish if true, also clears updates that need to be acked to the server
	void clear(bool finish);

	// Terra Firma, an atlas: the meshes that read a block over a seam
	std::vector<v3s16> readersOf(v3s16 block);
	// Terra Firma, an atlas: the blocks whose geometry (Atlas::prepare) was
	// found since, queued now as they were asked (main thread, each frame)
	void takePrepared(Map *map);

private:
	/*
		Terra Firma, an atlas: what a block's mesh reads of the atlas, found
		on a thread of its own the first time (at a seam it takes a
		millisecond or two; the main thread would stutter near the poles).
		A block asked for before it's found waits here, then is queued.
	*/
	class Preparer
	{
	public:
		explicit Preparer(const planet::Atlas &atlas);
		~Preparer();
		void ask(v3s16 block);
		std::vector<v3s16> take(); // found since last asked
	private:
		void run();
		const planet::Atlas &m_atlas;
		std::mutex m_lock;
		std::condition_variable m_wake;
		std::deque<v3s16> m_asked;
		std::vector<v3s16> m_found;
		bool m_stop = false;
		std::thread m_thread; // last: started once the rest exists
	};
	struct Waiting
	{
		bool ack, urgent, neighbor;
	};
	std::unique_ptr<Preparer> m_preparer;
	std::unordered_map<v3s16, Waiting> m_waiting; // by block, main thread only

	Client *m_client;
	std::unordered_map<v3s16, std::unordered_set<v3s16>> m_readers; // Terra Firma: block → meshes that read it
	std::vector<QueuedMeshUpdate *> m_queue;
	std::unordered_set<v3s16> m_urgents;
	std::unordered_set<v3s16> m_inflight_blocks;
	u16 m_epoch = 0;
	std::mutex m_mutex;

	// TODO: Add callback to update these when g_settings changes, and update all meshes
	bool m_cache_smooth_lighting;
	bool m_cache_enable_water_reflections;
	bool m_cache_enable_waving_water;
	// Terra Firma (sprint 85): where the camera is (nodes), for each mesh's
	// detail, and the rings it changes at (ground_detail_near / _far)
	std::atomic<s16> m_cam_x{ 0 }, m_cam_y{ 0 }, m_cam_z{ 0 };
	float m_detail_near, m_detail_far;
public:
	void setCamera(v3s16 node) { m_cam_x = node.X; m_cam_y = node.Y; m_cam_z = node.Z; }
	// The detail a mesh at block p is made at now (MeshMakeData::m_detail)
	u8 detailAt(v3s16 p) const;
private:

	void fillDataFromMapBlocks(QueuedMeshUpdate *q);
	void noteReads(const QueuedMeshUpdate &q);
};

struct MeshUpdateResult
{
	v3s16 p = v3s16(-1338, -1338, -1338);
	std::unique_ptr<MapBlockMesh> mesh;
	u8 solid_sides;
	std::vector<v3s16> ack_list;
	std::vector<MapBlock*> map_blocks;
	u16 epoch = U16_MAX;
	bool urgent = false;
	u8 detail = 1; // Terra Firma: the detail it was made at (MeshMakeData::m_detail)

	MeshUpdateResult() = default;

	/**
	 * Drop block references.
	 * @note not done by destructor, since this is only safe on main thread
	 */
	void dropBlocks();
};

class MeshUpdateManager;

class MeshUpdateWorkerThread : public UpdateThread
{
public:
	MeshUpdateWorkerThread(Client *client, MeshUpdateQueue *queue_in, MeshUpdateManager *manager);

protected:
	virtual void doUpdate();

private:
	Client *m_client;
	MeshUpdateQueue *m_queue_in;
	MeshUpdateManager *m_manager;

	// TODO: Add callback to update these when g_settings changes
	int m_generation_interval;
};

class MeshUpdateManager
{
public:
	MeshUpdateManager(Client *client);

	// Caches the block at p and its neighbors (if needed) and queues a mesh
	// update for the block at p
	void updateBlock(Map *map, v3s16 p, bool ack_block_to_server, bool urgent,
			bool update_neighbors = false);

	void putResult(MeshUpdateResult &&r);

	/// @note caller needs to refDrop() the affected map_blocks
	bool getNextResult(MeshUpdateResult &r);

	/**
	 * Forces all in-progress meshes to be re-generated and adds all passed
	 * positions to the queue.
	 * This is used after parameters the meshgen depends on have changed.
	 * @param pp block positions
	 */
	void forceRegenerate(Map *map, const std::vector<v3s16> &pp);

	/// @param finish if true, also clears updates that need to be acked to the server
	void clearAllQueues(bool finish);

	// Terra Firma, an atlas: the meshes that read a block over a seam (made
	// again when it arrives)
	std::vector<v3s16> readersOf(v3s16 block) { return m_queue_in.readersOf(block); }
	// Terra Firma, an atlas: queues the blocks whose geometry is found (each frame)
	void takePrepared(Map *map);
	// Terra Firma (sprint 85): where the camera is, and the detail a block's
	// mesh is made at from there
	void setCamera(v3s16 node) { m_queue_in.setCamera(node); }
	u8 detailAt(v3s16 p) const { return m_queue_in.detailAt(p); }

	void start();
	void stop();
	void wait();

	bool isRunning();

private:
	typedef MutexedQueue<MeshUpdateResult> ResultQueue;

	void deferUpdate();

	// Epoch counter: used to identify outdated mesh results
	u16 m_epoch = 0;

	MeshUpdateQueue m_queue_in;
	ResultQueue m_queue_out;
	ResultQueue m_queue_out_urgent;

	std::vector<std::unique_ptr<MeshUpdateWorkerThread>> m_workers;
};
