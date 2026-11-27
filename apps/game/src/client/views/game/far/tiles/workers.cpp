// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/tiles/workers.h"
#include "src/client/views/game/far/tiles/tilecache.h"
#include "src/client/views/game/far/tiles/tiling.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/platform/system/profiler.h"
#include "src/shared/base/exceptions.h"
#include "library/trace/trace.h"
#include "library/world/terragen/src/base/progress.h"
#include "library/world/terragen/src/base/parallel.h"
#include "src/shared/base/constants.h"
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <string>

namespace far {

namespace {
Packed pack(Mesh mesh);
}

/*
	What the workers share: copies of what they build from, the queue, the
	results. Held by each worker thread as well as by Workers, so a world
	dropped while its model is still being built isn't waited for: its
	workers finish what they're on, see the stop, and go, the last one
	freeing this.
*/
// Built tiles waiting to be taken, most: a few frames' worth of installs.
constexpr size_t WAITING = 32;

class Crew
{
public:
	using Job = Workers::Job;
	using Result = Workers::Result;

	Crew(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, const Tiling &tiling) :
		m_atlas(atlas), m_body(body), m_horizon(horizon), m_palette(palette), m_tiling(tiling),
		m_cache(porting::path_cache, horizon, atlas, body)
	{}

	void stop()
	{
		{
			std::lock_guard lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_all();
	}

	// the tiles wanted, nearest first; replaces what was asked before
	void want(const std::vector<Job> &jobs)
	{
		{
			std::lock_guard lock(m_mutex);
			m_queue.clear();
			// what's kept on disk first, nearest first: it's read in a
			// fraction of a build, and behind the builds a spot seen before
			// stayed coarse as long as one coming into view
			for (int pass = 0; pass < 2; pass++)
				for (const Job &job : jobs)
					if (!m_busy.count(id(job.key)) && (m_kept.count(id(job.key)) > 0) == (pass == 0))
						m_queue.push_back(job);
		}
		m_wake.notify_all();
	}

	std::optional<Result> take()
	{
		std::lock_guard lock(m_mutex);
		if (!m_error.empty())
			throw BaseException("far view: " + m_error);
		if (m_results.empty())
			return std::nullopt;
		Result r = std::move(m_results.front());
		m_results.pop_front();
		m_busy.erase(id(r.key));
		m_wake.notify_one(); // room for one more
		return r;
	}

private:
	static std::tuple<s32, s32, s32, s32> id(const TileKey &k) { return { k.level, k.x, k.z, k.face }; }

public:
	void run()
	{
		try {
			list();
		} catch (const std::exception &e) {
			std::lock_guard failed(m_mutex);
			m_error = e.what();
			m_wake.notify_all();
			return;
		}
		std::unique_lock lock(m_mutex);
		while (true) {
			// built tiles wait for the main thread to take them, a few a frame: past
			// WAITING the workers wait too, or they piled up in RAM (800 MB at a peak)
			m_wake.wait(lock, [this] { return m_stop || !m_error.empty() || (!m_queue.empty() && m_results.size() < WAITING); });
			if (!m_error.empty())
				return; // take() reports it
			if (m_stop)
				return;
			const Job job = m_queue.front();
			m_queue.pop_front();
			m_busy.insert(id(job.key));
			lock.unlock();

			Result r{ job.key, {} };
			try {
				const ScopeProfiler timed(g_profiler, "Far view: tile built [ms]", SPT_AVG);
				// each stage its own zone and count, so a probe can say where
				// a tile's time goes (planetarium/run/probe)
				TF_ZONE("Far tile");
				// kept from before if it can be: the world model's only made
				// once a tile isn't
				std::optional<Tile> tile;
				{
					TF_ZONE("Far tile: cache read");
					tile = m_cache.load(job.key);
				}
				if (tile) {
					trace::count("Far tile: read [#]", 1, trace::Fold::Sum);
				} else if (job.sketch) {
					// never kept: only the tile itself is
					TF_ZONE("Far tile: sketch");
					tile = generator().sketch(job.key);
					r.sketch = true;
					trace::count("Far tile: sketched [#]", 1, trace::Fold::Sum);
				} else {
					{
						TF_ZONE("Far tile: build");
						tile = generator().build(job.key);
					}
					{
						TF_ZONE("Far tile: cache write");
						m_cache.save(*tile);
					}
					trace::count("Far tile: built [#]", 1, trace::Fold::Sum);
					std::lock_guard kept(m_mutex);
					m_kept.insert(id(job.key));
				}
				Mesh mesh;
				{
					TF_ZONE("Far tile: mesh");
					mesh = onPlanet(*tile);
				}
				TF_ZONE("Far tile: pack");
				r.mesh = pack(std::move(mesh));
			} catch (const std::exception &e) {
				lock.lock();
				m_error = e.what();
				m_wake.notify_all();
				return;
			}

			lock.lock();
			m_results.push_back(std::move(r));
		}
	}

private:
	// What's kept on disk, listed by the first worker (a directory of them:
	// tens of ms, on the main thread a hitch each time a planet was shown);
	// what was asked for meanwhile is put in kept-first order then, as want() does
	void list()
	{
		std::call_once(m_listed, [&] {
			TF_ZONE("Far view: kept tiles listed");
			const std::vector<TileKey> kept = m_cache.kept();
			std::lock_guard lock(m_mutex);
			for (const TileKey &key : kept)
				m_kept.insert(id(key));
			std::stable_partition(m_queue.begin(), m_queue.end(), [&](const Job &job) { return m_kept.count(id(job.key)) > 0; });
		});
	}

	// The tile's mesh put on the planet (Tiling::place); its height over
	// the sea (blocks) in the first texture coordinate, for the haze (and the
	// Planetarium's contours), and its place in the tile's grid in the second.
	Mesh onPlanet(const Tile &tile) const
	{
		Mesh m = far::mesh(tile, m_palette);
		for (Mesh::Part &part : m.parts)
			for (Vertex &v : part.vertices) {
				v.TCoords = v2f(v.Pos.Y / BS + 0.5f - (m_horizon.water_level + 1), 0);
				v.TCoords2 = v2f(v.Pos.X / BS, v.Pos.Z / BS); // (far/mesh.h: which column it's in)
			}
		m_tiling.place(m, tile.key);
		return m;
	}

	const planet::Atlas m_atlas; // copies: the crew can outlive the tiles
	const int m_body;
	const Horizon m_horizon;
	const Palette m_palette;
	const Tiling m_tiling;
	const TileCache m_cache;
	// the world model, made by the first worker a tile isn't kept for (the
	// others missing one wait for it); none made if every tile was kept
	std::once_flag m_listed;
	std::once_flag m_made;
	std::unique_ptr<Generator> m_generator;

	const Generator &generator()
	{
		std::call_once(m_made, [&] {
			TF_ZONE("Far tile: world model");
			try {
				m_generator = std::make_unique<Generator>(m_horizon, m_atlas, m_body);
			} catch (const std::exception &e) {
				throw BaseException(std::string("the world can't be built from the server's data: ") + e.what());
			}
		});
		return *m_generator;
	}

	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_stop = false;
	std::deque<Job> m_queue;
	std::set<std::tuple<s32, s32, s32, s32>> m_busy; // being built, or built and not yet taken
	std::set<std::tuple<s32, s32, s32, s32>> m_kept; // on disk (TileCache: some may be swept since)
	std::deque<Result> m_results;
	std::string m_error;
};

Workers::Workers(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, const Tiling &tiling,
		unsigned threads) :
	m_crew(std::make_shared<Crew>(atlas, body, horizon, palette, tiling))
{
	if (threads == 0)
		throw BaseException("far tiles: no threads to build with");
	for (unsigned i = 0; i < threads; i++)
		m_threads.emplace_back([crew = m_crew] {
			trace::nameThread("Far tiles");
			terragen::background();
			crew->run();
		});
}

Workers::~Workers()
{
	m_crew->stop();
	for (std::thread &t : m_threads)
		t.join();
}

void Workers::want(const std::vector<Job> &jobs) { m_crew->want(jobs); }
std::optional<Workers::Result> Workers::take() { return m_crew->take(); }

namespace {

Packed pack(Mesh mesh)
{
	Packed out;
	size_t all = 0;
	for (const Mesh::Part &part : mesh.parts)
		all += part.vertices.size();
	if (all <= 65536) {
		Mesh::Part one;
		one.vertices.reserve(all);
		for (u8 q = 0; q < 4; q++) {
			Packed::Range &range = out.ranges[q];
			range.v0 = (u32)one.vertices.size();
			range.i0 = (u32)one.indices.size();
			for (const Mesh::Part &part : mesh.parts) {
				if (part.quarter != q)
					continue;
				const u16 base = (u16)one.vertices.size();
				one.vertices.insert(one.vertices.end(), part.vertices.begin(), part.vertices.end());
				for (u16 i : part.indices)
					one.indices.push_back((u16)(base + i));
			}
			range.v1 = (u32)one.vertices.size();
			range.i1 = (u32)one.indices.size();
			if (range.v1 > range.v0) {
				aabb3f &box = out.quarterBoxes[q];
				box.reset(one.vertices[range.v0].Pos);
				for (u32 v = range.v0; v < range.v1; v++)
					box.addInternalPoint(one.vertices[v].Pos);
			}
		}
		out.merged = true;
		mesh.parts.clear();
		mesh.parts.push_back(std::move(one));
	}
	for (Mesh::Part &part : mesh.parts) {
		std::vector<video::SColor> own;
		own.reserve(part.vertices.size());
		aabb3f box(part.vertices.empty() ? v3f() : part.vertices.front().Pos);
		Packed::Part kept;
		kept.vertices.reserve(part.vertices.size());
		for (const Vertex &v : part.vertices) {
			own.push_back(v.Color);
			box.addInternalPoint(v.Pos);
			kept.vertices.push_back(packed(v));
		}
		kept.indices = std::move(part.indices);
		kept.quarter = part.quarter;
		out.own.push_back(std::move(own));
		out.boxes.push_back(box);
		out.parts.push_back(std::move(kept));
	}
	return out;
}

} // namespace

} // namespace far
