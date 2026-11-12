// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013, 2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "mesh_generator_thread.h"
#include "settings.h"
#include "profiler.h"
#include "client.h"
#include "mapblock.h"
#include "mapblock_mesh.h"
#include "map.h"
#include "util/directiontables.h"
#include "porting.h"
#include "exceptions.h"
#include "nodedef.h"
#include "planet/atlas.h"
#include <optional>

// Terra Firma: what a cut cell shows across its cut face (below)
static MapNode across(Map &map, const std::vector<planet::Atlas::Node> &nodes, std::vector<v3s16> &read);

/*
	QueuedMeshUpdate
*/


QueuedMeshUpdate::~QueuedMeshUpdate()
{
	delete data;
}

void QueuedMeshUpdate::retrieveBlocks(Map *map, u16 cell_size)
{
	const size_t total = (cell_size+2)*(cell_size+2)*(cell_size+2);
	if (map_blocks.empty())
		map_blocks.resize(total);
	else
		assert(map_blocks.size() == total); // must not change
	size_t i = 0;
	v3s16 pos;
	// order is not important, but it must be consistent
	// note the extra margin!
	for (pos.X = p.X - 1; pos.X <= p.X + cell_size; pos.X++)
	for (pos.Z = p.Z - 1; pos.Z <= p.Z + cell_size; pos.Z++)
	for (pos.Y = p.Y - 1; pos.Y <= p.Y + cell_size; pos.Y++) {
		if (!map_blocks[i]) {
			MapBlock *block = map->getBlockNoCreateNoEx(pos);
			if (block) {
				block->refGrab();
				map_blocks[i] = block;
			}
		}
		i++;
	}
	shapes.clear();
	for (MapBlock *block : map_blocks)
		if (block)
			block->m_shapes.each([&](v3s16 rel, const shape::Shape &s) {
				shapes.emplace_back(block->getPosRelative() + rel, s);
			});
	// a plant's drawn by the mesh its foot's in (it reaches past it: MOST_GROUND_REACH)
	sprigs.clear();
	for (MapBlock *block : map_blocks)
		if (block) {
			const v3s16 b = block->getPos();
			if (b.X >= p.X && b.Y >= p.Y && b.Z >= p.Z && b.X < p.X + cell_size && b.Y < p.Y + cell_size && b.Z < p.Z + cell_size)
				block->m_sprigs.each([&](v3s16 rel, const sprig::Sprig &s) {
					sprigs.emplace_back(block->getPosRelative() + rel, s);
				});
		}

	// Terra Firma: the border past an atlas's seams, read now (this is the
	// main thread; meshes are made on others)
	halo.clear();
	read.clear();
	atlas = map->getAtlas();
	if (atlas) {
		// every node the mesh reads that isn't a cell (seam: over
		// a seam, or the gap between shells, which can lie inside a block)
		// is what lies across (Atlas::rim). Grids don't line up over a seam:
		// a face is hidden only by what covers it whole. Across, every node
		// it overlaps the same: that node; mixed (stone over part, air over
		// the rest): air, lit as the air across is, so the face is drawn
		const NodeDefManager *ndef = map->getNodeDefManager();
		for (const planet::Atlas::RimNode &r : atlas->rim({ p.X, p.Y, p.Z })) {
			const MapNode first = map->getNodeTurned(v3s16(r.across[0].x, r.across[0].y, r.across[0].z), r.turn);
			MapNode open(CONTENT_AIR);
			bool mixed = false;
			for (const planet::Atlas::Node &a : r.across) {
				const v3s16 at(a.x, a.y, a.z);
				const v3s16 b = getNodeBlockPos(at);
				if (std::find(read.begin(), read.end(), b) == read.end())
					read.push_back(b);
				const MapNode there = map->getNode(at);
				mixed |= there.getContent() != first.getContent();
				if (ndef->get(there).light_propagates) // day and night banks, each the brightest
					open.param1 = std::max(open.param1 & 0x0f, there.param1 & 0x0f) |
							std::max(open.param1 & 0xf0, there.param1 & 0xf0);
			}
			halo.emplace_back(v3s16(r.at.x, r.at.y, r.at.z), mixed ? open : first);
		}
		cuts.clear();
		for (const planet::Atlas::CutCell &c : atlas->cutsIn({ p.X, p.Y, p.Z })) {
			std::vector<MapNode> faces;
			for (const std::vector<planet::Atlas::Node> &nodes : c.across)
				faces.push_back(across(*map, nodes, read));
			cuts.emplace_back(v3s16(c.node.x, c.node.y, c.node.z), std::move(faces));
		}
	}
}

// What a cut cell's cut face shows: the nodes just past it (Atlas::cutsIn).
// The same everywhere: that node; mixed: air, lit as the brightest air
// there, so the face is drawn. Air for a face with none (the other planes
// leave too little of it to sample): drawn if it's there at all.
static MapNode across(Map &map, const std::vector<planet::Atlas::Node> &nodes, std::vector<v3s16> &read)
{
	const NodeDefManager *ndef = map.getNodeDefManager();
	std::optional<MapNode> first;
	MapNode open(CONTENT_AIR);
	bool mixed = false;
	for (const planet::Atlas::Node &n : nodes) {
		const v3s16 there(n.x, n.y, n.z);
		const v3s16 block = getNodeBlockPos(there);
		if (std::find(read.begin(), read.end(), block) == read.end())
			read.push_back(block);
		const MapNode node = map.getNode(there);
		if (!first)
			first = node;
		mixed |= node.getContent() != first->getContent();
		if (ndef->get(node).light_propagates)
			open.param1 = std::max(open.param1 & 0x0f, node.param1 & 0x0f) | std::max(open.param1 & 0xf0, node.param1 & 0xf0);
	}
	if (!first)
		return MapNode(CONTENT_AIR);
	return mixed ? open : *first;
}

void QueuedMeshUpdate::fill(MeshMakeData *data) const
{
	data->fillBlockDataBegin(p);
	data->m_atlas = atlas;
	data->m_cuts.clear();
	for (const auto &[pos, faces] : cuts)
		data->m_cuts.emplace(pos, faces);
	data->m_shapes.clear();
	for (const auto &[pos, s] : shapes)
		data->m_shapes.emplace(pos, s);
	data->m_sprigs = sprigs;

	// NOTE: the "data race" mentioned by MapBlock::tryShrinkNodes() is right here
	for (auto *block : map_blocks) {
		if (block)
			block->copyTo(data->m_vmanip);
	}
	// Terra Firma: over the seam; what's inside the block is read, not drawn
	const s32 side = data->m_side_length;
	const v3s16 low = p * MAP_BLOCKSIZE;
	for (const auto &[pos, node] : halo) {
		data->m_vmanip.setNodeNoEmerge(pos, node);
		const v3s16 r = pos - low;
		if (r.X < 0 || r.Y < 0 || r.Z < 0 || r.X >= side || r.Y >= side || r.Z >= side)
			continue;
		if (data->m_hidden.empty())
			data->m_hidden.assign((size_t)side * side * side, false);
		data->m_hidden[(r.Z * side + r.Y) * side + r.X] = true;
	}
}

bool QueuedMeshUpdate::checkSkip(u16 cell_size)
{
	bool all_air = true;
	const v3s16 p_max = p + v3s16(cell_size);
	assert(!map_blocks.empty());
	for (auto *block : map_blocks) {
		// ignore extra margin
		if (block && block->getPos() >= p && block->getPos() < p_max) {
			all_air &= block->isAir();
		}
	}
	return all_air;
}

void QueuedMeshUpdate::dropBlocks()
{
	for (auto *block : map_blocks) {
		if (block)
			block->refDrop();
	}
	map_blocks.clear();
}

namespace {
	struct DroppingDeleter {
		void operator() (QueuedMeshUpdate *q) {
			if (q)
				q->dropBlocks();
			delete q;
		}
	};

	// Simple helper to avoid messing up the refcounting
	using UnqueuedMeshUpdate = std::unique_ptr<QueuedMeshUpdate, DroppingDeleter>;
}

/*
	MeshUpdateQueue
*/

MeshUpdateQueue::MeshUpdateQueue(Client *client):
	m_client(client)
{
	assert(m_client);
	m_cache_smooth_lighting = g_settings->getBool("smooth_lighting");
	m_cache_enable_water_reflections = g_settings->getBool("enable_water_reflections");
	m_cache_enable_waving_water = g_settings->getBool("enable_waving_water");
	m_detail_near = g_settings->getFloat("ground_detail_near");
	m_detail_far = g_settings->getFloat("ground_detail_far");
}

u8 MeshUpdateQueue::detailAt(v3s16 p) const
{
	// from the camera to the middle of the mesh's area, nodes
	const MeshGrid grid = m_client->getMeshGrid();
	const float side = MAP_BLOCKSIZE * grid.cell_size;
	const v3f mid = intToFloat(grid.getMeshPos(p) * MAP_BLOCKSIZE, 1) + v3f(side / 2 - 0.5f);
	const float d = mid.getDistanceFrom(v3f(m_cam_x, m_cam_y, m_cam_z)) - side / 2;
	return d < m_detail_near ? 1 : d < m_detail_far ? 4 : 16;
}

MeshUpdateQueue::~MeshUpdateQueue()
{
	clear(true);
}

bool MeshUpdateQueue::addBlock(Map *map, v3s16 p, bool ack_block_to_server,
	bool urgent, bool from_neighbor)
{
	// If block that causes update does not exist, skip.
	if (!map->getBlockNoCreateNoEx(p))
		return false;

	const MeshGrid mesh_grid = m_client->getMeshGrid();

	// Terra Firma, an atlas: its geometry found first, off this thread
	if (const planet::Atlas *atlas = map->getAtlas()) {
		if (!atlas->prepared({ p.X, p.Y, p.Z })) {
			if (!m_preparer)
				m_preparer = std::make_unique<Preparer>(*atlas);
			auto [it, fresh] = m_waiting.try_emplace(p, Waiting{ ack_block_to_server, urgent, from_neighbor });
			if (fresh)
				m_preparer->ask(p);
			else {
				it->second.ack |= ack_block_to_server;
				it->second.urgent |= urgent;
				it->second.neighbor &= from_neighbor;
			}
			return true;
		}
	}

	// Mesh is placed at the corner block of a chunk
	// (where all coordinate are divisible by the chunk size)
	const v3s16 mesh_position = mesh_grid.getMeshPos(p);

	MutexAutoLock lock(m_mutex);

	/*
		Mark the block as urgent if requested
	*/
	if (urgent)
		m_urgents.insert(mesh_position);

	/*
		Find if block is already in queue.
		If it is, update the data and quit.
	*/
	for (QueuedMeshUpdate *q : m_queue) {
		if (q->p == mesh_position) {
			if (ack_block_to_server)
				q->ack_list.push_back(p);
			q->crack_level = m_client->getCrackLevel();
			q->crack_pos = m_client->getCrackPos();
			q->urgent |= urgent;
			q->retrieveBlocks(map, mesh_grid.cell_size);
			noteReads(*q);
			return true;
		}
	}

	/*
		Grab the relevant blocks first
	*/
	UnqueuedMeshUpdate q{new QueuedMeshUpdate()};
	q->p = mesh_position;
	if (ack_block_to_server)
		q->ack_list.push_back(p);
	q->crack_level = m_client->getCrackLevel();
	q->crack_pos = m_client->getCrackPos();
	q->urgent = urgent;
	q->retrieveBlocks(map, mesh_grid.cell_size);
	noteReads(*q);

	/*
		Air blocks won't suddenly become visible due to a neighbor update, so
		skip those.
		Note: this can be extended with more precise checks in the future
	*/
	if (from_neighbor && q->checkSkip(mesh_grid.cell_size)) {
		assert(!ack_block_to_server);
		m_urgents.erase(mesh_position);
		g_profiler->add("MeshUpdateQueue: updates skipped", 1);
		return true;
	}

	// Put into queue, pointer moved from `q`.
	m_queue.push_back(q.release());

	return true;
}

void MeshUpdateQueue::takePrepared(Map *map)
{
	if (!m_preparer)
		return;
	for (const v3s16 &p : m_preparer->take()) {
		const auto it = m_waiting.find(p);
		if (it == m_waiting.end())
			continue; // cleared meanwhile
		const Waiting w = it->second;
		m_waiting.erase(it);
		addBlock(map, p, w.ack, w.urgent, w.neighbor);
	}
}

MeshUpdateQueue::Preparer::Preparer(const planet::Atlas &atlas) : m_atlas(atlas), m_thread([this] { run(); }) {}

MeshUpdateQueue::Preparer::~Preparer()
{
	{
		std::lock_guard<std::mutex> hold(m_lock);
		m_stop = true;
	}
	m_wake.notify_all();
	m_thread.join();
}

void MeshUpdateQueue::Preparer::ask(v3s16 block)
{
	{
		std::lock_guard<std::mutex> hold(m_lock);
		m_asked.push_back(block);
	}
	m_wake.notify_all();
}

std::vector<v3s16> MeshUpdateQueue::Preparer::take()
{
	std::lock_guard<std::mutex> hold(m_lock);
	return std::move(m_found);
}

void MeshUpdateQueue::Preparer::run()
{
	std::unique_lock<std::mutex> lock(m_lock);
	while (true) {
		m_wake.wait(lock, [this] { return m_stop || !m_asked.empty(); });
		if (m_stop)
			return;
		const v3s16 b = m_asked.front();
		m_asked.pop_front();
		lock.unlock();
		m_atlas.prepare({ b.X, b.Y, b.Z }); // throws on a broken atlas: the client stops, loudly
		lock.lock();
		m_found.push_back(b);
	}
}

void MeshUpdateQueue::noteReads(const QueuedMeshUpdate &q)
{
	for (const v3s16 &b : q.read)
		m_readers[b].insert(q.p);
}

std::vector<v3s16> MeshUpdateQueue::readersOf(v3s16 block)
{
	MutexAutoLock lock(m_mutex);
	const auto it = m_readers.find(block);
	return it == m_readers.end() ? std::vector<v3s16>() : std::vector<v3s16>(it->second.begin(), it->second.end());
}

void MeshUpdateQueue::reAddBlock(MeshUpdateResult &r)
{
	const MeshGrid mesh_grid = m_client->getMeshGrid();

	const v3s16 mesh_position = mesh_grid.getMeshPos(r.p);

	MutexAutoLock lock(m_mutex);

	if (r.urgent)
		m_urgents.insert(mesh_position);

	// Update if exists in queue
	for (QueuedMeshUpdate *q : m_queue) {
		if (q->p == mesh_position) {
			for (auto p : r.ack_list)
				q->ack_list.push_back(p);
			q->urgent |= r.urgent;
			// Do not retrieve blocks again, but make sure to drop our old ones
			r.dropBlocks();
			return;
		}
	}

	// Queue a new update
	UnqueuedMeshUpdate q{new QueuedMeshUpdate()};
	q->p = mesh_position;
	q->ack_list = std::move(r.ack_list);
	q->crack_level = m_client->getCrackLevel();
	q->crack_pos = m_client->getCrackPos();
	q->urgent = r.urgent;
	q->map_blocks = std::move(r.map_blocks);

	m_queue.push_back(q.release());
}

// Returned pointer must be deleted
// Returns NULL if queue is empty
QueuedMeshUpdate *MeshUpdateQueue::pop()
{
	QueuedMeshUpdate *result = nullptr;
	{
		MutexAutoLock lock(m_mutex);

		bool must_be_urgent = !m_urgents.empty();
		for (auto i = m_queue.begin(); i != m_queue.end(); ++i) {
			QueuedMeshUpdate *q = *i;
			if (must_be_urgent && m_urgents.count(q->p) == 0)
				continue;
			// Make sure no two threads are processing the same mapblock, as that causes racing conditions
			if (m_inflight_blocks.find(q->p) != m_inflight_blocks.end())
				continue;
			m_queue.erase(i);
			m_urgents.erase(q->p);
			m_inflight_blocks.insert(q->p);
			result = q;
			break;
		}

		// When it leaves the queue, it becomes too late to update it, so the
		// epoch counts at this point.
		if (result)
			result->epoch = m_epoch;
	}

	if (result)
		fillDataFromMapBlocks(result);

	return result;
}

void MeshUpdateQueue::done(v3s16 pos)
{
	MutexAutoLock lock(m_mutex);
	m_inflight_blocks.erase(pos);
}

void MeshUpdateQueue::clear(bool finish)
{
	MutexAutoLock lock(m_mutex);
	if (finish)
		m_waiting.clear(); // Terra Firma: nothing is acked after this
	decltype(m_queue) new_queue;
	for (auto *it : m_queue) {
		// If we're in an active game session clearing updates that the
		// server expects us to ack will cause problems.
		if (it->ack_list.empty() || finish) {
			m_urgents.erase(it->p);
			m_inflight_blocks.erase(it->p);
			it->dropBlocks();
			delete it;
		} else {
			new_queue.push_back(it);
		}
	}
	m_queue = std::move(new_queue);
}

void MeshUpdateQueue::fillDataFromMapBlocks(QueuedMeshUpdate *q)
{
	auto mesh_grid = m_client->getMeshGrid();
	MeshMakeData *data = new MeshMakeData(m_client->ndef(),
			MAP_BLOCKSIZE * mesh_grid.cell_size, mesh_grid);
	q->data = data;

	q->fill(data);

	data->setCrack(q->crack_level, q->crack_pos);

	float ao_gamma = m_client->getCommittedStaticLighting().ao_gamma;
	data->m_ao_gamma_inv = ao_gamma == 0.0f ? 0.0f : (1.0f / ao_gamma);

	data->m_generate_minimap = !!m_client->getMinimap();
	data->m_smooth_lighting = m_cache_smooth_lighting;
	data->m_detail = detailAt(q->p);
	data->m_enable_water_reflections = m_cache_enable_water_reflections;
	data->m_enable_waving_water = m_cache_enable_waving_water;
}

/*
	MeshUpdateResult
*/

void MeshUpdateResult::dropBlocks()
{
	for (auto *block : map_blocks) {
		if (block)
			block->refDrop();
	}
	map_blocks.clear();
}

/*
	MeshUpdateWorkerThread
*/

MeshUpdateWorkerThread::MeshUpdateWorkerThread(Client *client, MeshUpdateQueue *queue_in, MeshUpdateManager *manager) :
		UpdateThread("Mesh"), m_client(client), m_queue_in(queue_in), m_manager(manager)
{
	m_generation_interval = g_settings->getU16("mesh_generation_interval");
	m_generation_interval = rangelim(m_generation_interval, 0, 25);
}

void MeshUpdateWorkerThread::doUpdate()
{
	QueuedMeshUpdate *q;
	while ((q = m_queue_in->pop())) {
		ScopeProfiler sp(g_profiler, "Client: Mesh making (sum)");

		// This generates the mesh:
		MapBlockMesh *mesh_new = new MapBlockMesh(m_client, q->data);

		MeshUpdateResult r;
		r.p = q->p;
		r.mesh = std::unique_ptr<MapBlockMesh>(mesh_new);
		r.solid_sides = get_solid_sides(q->data);
		r.ack_list = std::move(q->ack_list);
		r.map_blocks = std::move(q->map_blocks);
		r.epoch = q->epoch;
		r.urgent = q->urgent;
		r.detail = q->data->m_detail;

		m_manager->putResult(std::move(r));
		m_queue_in->done(q->p);
		delete q;
		sp.stop();

		porting::TriggerMemoryTrim();

		// do this after we're done so the interval is enforced without
		// adding extra latency.
		if (m_generation_interval)
			sleep_ms(m_generation_interval);
	}
}

/*
	MeshUpdateManager
*/

MeshUpdateManager::MeshUpdateManager(Client *client):
	m_queue_in(client)
{
	int number_of_threads = rangelim(g_settings->getS32("mesh_generation_threads"), 0, 8);

	// Automatically use 25% of the system cores for mesh generation, max 3
	if (number_of_threads == 0)
		number_of_threads = std::min(3U, Thread::getNumberOfProcessors() / 4);

	// use at least one thread
	number_of_threads = std::max(1, number_of_threads);
	infostream << "MeshUpdateManager: using " << number_of_threads << " threads" << std::endl;

	for (int i = 0; i < number_of_threads; i++)
		m_workers.push_back(std::make_unique<MeshUpdateWorkerThread>(client, &m_queue_in, this));
}

void MeshUpdateManager::updateBlock(Map *map, v3s16 p, bool ack_block_to_server,
		bool urgent, bool update_neighbors)
{
	if (!m_queue_in.addBlock(map, p, ack_block_to_server, urgent, false)) {
		warningstream << "Update requested for non-existent block at "
				<< p << std::endl;
		return;
	}
	// Terra Firma: every neighbour, edges and corners too: the ground's shape
	// reads the columns round a block (ground::Ground), across them (was:
	// only with smooth lighting)
	if (update_neighbors)
		for (v3s16 dp : g_26dirs)
			m_queue_in.addBlock(map, p + dp, false, urgent, true);
	deferUpdate();
}

void MeshUpdateManager::takePrepared(Map *map)
{
	m_queue_in.takePrepared(map);
	deferUpdate();
}

void MeshUpdateManager::putResult(MeshUpdateResult &&result)
{
	if (result.urgent)
		m_queue_out_urgent.push_back(std::move(result));
	else
		m_queue_out.push_back(std::move(result));
}

bool MeshUpdateManager::getNextResult(MeshUpdateResult &r)
{
	if (!m_queue_out_urgent.empty())
		r = m_queue_out_urgent.pop_frontNoEx();
	else if (!m_queue_out.empty())
		r = m_queue_out.pop_frontNoEx();
	else
		return false;

	if (r.epoch != m_epoch) {
		// just kidding, we can't return this result. put it back into the queue.
		verbosestream << "MeshUpdateManager: have to regenerate " << r.p
			<< " due to epoch difference (" << r.epoch << ")" << std::endl;
		m_queue_in.reAddBlock(r);
		r = MeshUpdateResult(); // don't leave stale data to caller
		return getNextResult(r);
	}
	return true;
}

void MeshUpdateManager::forceRegenerate(Map *map, const std::vector<v3s16> &pp)
{
	// This invalidates all previously produced results (if still pending)
	m_queue_in.setEpoch(++m_epoch);

	verbosestream << "MeshUpdateManager: m_epoch=" << m_epoch << "; will queue "
		<< pp.size() << " blocks to be updated" << std::endl;

	for (v3s16 p : pp) {
		m_queue_in.addBlock(map, p, false, false, false);
	}
	deferUpdate();
}

void MeshUpdateManager::clearAllQueues(bool finish)
{
	m_queue_in.clear(finish);

	// Same problem as in MeshUpdateQueue::clear() here: we can't just blindly
	// throw away results that the server expects to receive an ack for.
	const auto &do_it = [finish] (ResultQueue &queue) {
		auto helper = queue.iterLocked();
		for (auto it = helper.begin(); it != helper.end(); ) {
			if (it->ack_list.empty() || finish) {
				it->dropBlocks();
				it = helper.erase(it);
			} else {
				++it;
			}
		}
	};
	do_it(m_queue_out_urgent);
	do_it(m_queue_out);
}

void MeshUpdateManager::deferUpdate()
{
	for (auto &thread : m_workers)
		thread->deferUpdate();
}

void MeshUpdateManager::start()
{
	for (auto &thread: m_workers)
		thread->start();
}

void MeshUpdateManager::stop()
{
	for (auto &thread: m_workers)
		thread->stop();
}

void MeshUpdateManager::wait()
{
	for (auto &thread: m_workers)
		thread->wait();
}

bool MeshUpdateManager::isRunning()
{
	for (auto &thread: m_workers)
		if (thread->isRunning())
			return true;
	return false;
}
