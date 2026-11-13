// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/tiles.h"
#include "far/mesh.h"
#include "far/tilecache.h"
#include "porting.h"
#include "constants.h"
#include "exceptions.h"
#include "log.h"
#include "terragen/progress.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include "profiler.h"
#include "terragen/parallel.h"

namespace far {

// Time an update may spend putting tiles on the GPU (a frame at 60 fps is 16.7).
static constexpr double INSTALL_MS = 3;
// And recolouring tiles for a new tint.
static constexpr double PAINT_MS = 3;
// What the held tiles' meshes may take, all of them: past it the least
// recently wanted go. (Dropped after ten seconds unwanted, a spot panned away
// from and back to was built again, coarse meanwhile; sprint 114)
static constexpr size_t KEEP_BYTES = (size_t)256 << 20;
static constexpr s32 BACKDROP_LEVEL = 3; // cells of 8 blocks
static constexpr float STAY_WHOLE = 0.85f;
// How many levels finer a tile not built yet looks for finer ones to stand in for it.
static constexpr s32 FINER_DEPTH = 3;

/*
	What the workers share: copies of what they build from, the queue, the
	results. Held by each worker thread as well as by Workers, so a world
	dropped while its model is still being built isn't waited for: its
	workers finish what they're on, see the stop, and go, the last one
	freeing this.
*/
class Crew
{
public:
	// a tile to build, or only to sketch (Generator::sketch)
	struct Job
	{
		TileKey key;
		bool sketch = false;
	};
	struct Result
	{
		TileKey key;
		Mesh mesh;
		bool sketch = false; // a stand-in: the tile itself is still to build
	};

	Crew(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, const Tiling &tiling) :
		m_atlas(atlas), m_body(body), m_horizon(horizon), m_palette(palette), m_tiling(tiling),
		m_cache(porting::path_cache, horizon, atlas, body)
	{
		for (const TileKey &key : m_cache.kept())
			m_kept.insert(id(key));
	}

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
		return r;
	}

private:
	static std::tuple<s32, s32, s32, s32> id(const TileKey &k) { return { k.level, k.x, k.z, k.face }; }

public:
	void run()
	{
		std::unique_lock lock(m_mutex);
		while (true) {
			m_wake.wait(lock, [this] { return m_stop || !m_error.empty() || !m_queue.empty(); });
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
				// kept from before if it can be: the world model's only made
				// once a tile isn't
				std::optional<Tile> tile = m_cache.load(job.key);
				if (!tile && job.sketch) {
					// never kept: only the tile itself is
					tile = generator().sketch(job.key);
					r.sketch = true;
				} else if (!tile) {
					tile = generator().build(job.key);
					m_cache.save(*tile);
					std::lock_guard kept(m_mutex);
					m_kept.insert(id(job.key));
				}
				r.mesh = onPlanet(*tile);
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
	// The tile's mesh put on the planet (Tiling::place); its height over
	// the sea (blocks) in the first texture coordinate, for the haze (and the
	// Planetarium's contours).
	Mesh onPlanet(const Tile &tile) const
	{
		Mesh m = far::mesh(tile, m_palette);
		for (Mesh::Part &part : m.parts)
			for (video::S3DVertex &v : part.vertices)
				v.TCoords = v2f(v.Pos.Y / BS + 0.5f - (m_horizon.water_level + 1), 0);
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
	std::once_flag m_made;
	std::unique_ptr<Generator> m_generator;

	const Generator &generator()
	{
		std::call_once(m_made, [&] {
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

/*
	The workers: build tiles and their meshes off the main thread, nearest
	first. Each update says which tiles are wanted (want()); keys being
	built or waiting to be taken aren't asked for twice. Let go of, they're
	stopped and not waited for (Crew).
*/
class Workers
{
public:
	using Job = Crew::Job;
	using Result = Crew::Result;

	Workers(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, const Tiling &tiling,
			unsigned threads) :
		m_crew(std::make_shared<Crew>(atlas, body, horizon, palette, tiling))
	{
		if (threads == 0)
			throw BaseException("far tiles: no threads to build with");
		for (unsigned i = 0; i < threads; i++)
			std::thread([crew = m_crew] { terragen::background(); crew->run(); }).detach();
	}

	~Workers() { m_crew->stop(); }

	void want(const std::vector<Job> &jobs) { m_crew->want(jobs); }
	std::optional<Result> take() { return m_crew->take(); }

private:
	std::shared_ptr<Crew> m_crew;
};

unsigned Tiles::threadsFor(float share)
{
	const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
	return std::max(1u, (unsigned)(cores * share));
}

Tiles::Tiles(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, unsigned threads) :
	m_atlas(atlas),
	m_horizon(horizon),
	m_tiling(atlas.body(body)),
	m_workers(std::make_unique<Workers>(m_atlas, body, m_horizon, palette, m_tiling, threads)),
	m_coarse(std::make_unique<terragen::Step>(horizon.code, "coarse planet", (int64_t)m_tiling.roots().size()))
{
}

Tiles::~Tiles() = default;

void Tiles::update(const Eye &eye)
{
	m_frame++;
	m_draw.clear();
	m_behind.clear();
	std::vector<std::pair<float, Key>> want;
	for (const TileKey &root : m_tiling.roots())
		choose(Key(root), eye, m_draw, want);

	// the roots, everywhere, before anything: a coarse planet always there to
	// stand in while the eye's own tiles are built (turning never opens holes).
	// Sketched first, all of them, so the whole planet shows in a fraction of
	// the time; then built properly
	std::set<Key> sketches;
	for (const TileKey &root : m_tiling.roots()) {
		Entry &e = m_tiles[Key(root)];
		e.used = m_frame;
		if (e.ready && !e.sketch)
			continue;
		const float first = e.ready ? -1.0f : -2.0f;
		if (!e.ready)
			sketches.insert(Key(root));
		auto again = std::find_if(want.begin(), want.end(), [&](const auto &w) { return !(w.second < Key(root)) && !(Key(root) < w.second); });
		if (again != want.end())
			again->first = first; // wanted already: first, now
		else
			want.emplace_back(first, Key(root));
	}
	m_pending = want.size();
	std::sort(want.begin(), want.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::vector<Workers::Job> jobs;
	jobs.reserve(want.size());
	for (const auto &w : want)
		jobs.push_back({ w.second.tile(), sketches.count(w.second) > 0 });
	m_workers->want(jobs);

	for (const auto &[key, child] : m_behind)
		quarter(m_tiles.at(key), child);
	install();
	if (m_coarse && based()) {
		m_coarse.reset();
		actionstream << "far: " << m_horizon.code << " coarse planet there; " << terragen::breakdown(terragen::making(m_horizon.code)) << std::endl;
	}
	repaint();
	if (m_frame % 60 == 0)
		evict();
}

float Tiles::basedShare() const
{
	const auto &roots = m_tiling.roots();
	size_t built = 0;
	for (const TileKey &root : roots) {
		auto it = m_tiles.find(Key(root));
		built += it != m_tiles.end() && it->second.ready;
	}
	return roots.empty() ? 1.0f : (float)built / roots.size();
}

bool Tiles::based() const
{
	for (const TileKey &root : m_tiling.roots()) {
		auto it = m_tiles.find(Key(root));
		if (it == m_tiles.end() || !it->second.ready)
			return false;
	}
	return true;
}

std::vector<Tiles::Drawn> Tiles::behind() const
{
	std::vector<Drawn> out;
	out.reserve(m_behind.size());
	for (const auto &[key, child] : m_behind)
		out.push_back({ key.tile(), place(key).middle, m_tiles.at(key).quarters[child] });
	return out;
}

std::vector<Tiles::Drawn> Tiles::drawn() const
{
	std::vector<Drawn> out;
	out.reserve(m_draw.size());
	for (const Key &key : m_draw)
		out.push_back({ key.tile(), place(key).middle, m_tiles.at(key).parts });
	return out;
}

float Tiles::distance(const TileKey &key, const Eye &eye) const
{
	const Place &at = place(Key(key));
	const Tiling::Vec &p = at.middle;
	const double r = m_tiling.radius(), sea = r + m_horizon.water_level + 1;
	const double d = std::hypot(p.x * sea / r - eye.at.x, p.y * sea / r - eye.at.y, p.z * sea / r - eye.at.z);
	return (float)std::max(d - at.half, 0.0);
}

bool Tiles::inside(const TileKey &key, const Eye &eye) const
{
	if (eye.cut <= 0)
		return false;
	// its farthest point from the eye, at the sea, and its land's highest a
	// tile's relief over that (a peak could be farther off than the sea under it)
	const Place &at = place(Key(key));
	const Tiling::Vec &p = at.middle;
	const double r = m_tiling.radius(), sea = r + m_horizon.water_level + 1;
	const double d = std::hypot(p.x * sea / r - eye.at.x, p.y * sea / r - eye.at.y, p.z * sea / r - eye.at.z) + at.half;
	const double far = std::hypot(d, (double)eye.altitude) + Tiles::RELIEF;
	return far < eye.cut;
}

const Tiles::Place &Tiles::place(const Key &key) const
{
	auto it = m_places.find(key);
	if (it != m_places.end())
		return it->second;
	// (a long journey's worth, then started again)
	if (m_places.size() > 1 << 18)
		m_places.clear();
	return m_places.emplace(key, Place{ m_tiling.middle(key.tile()), m_tiling.diagonal(key.tile()) / 2 }).first->second;
}

bool Tiles::seen(const TileKey &key, const Eye &eye) const
{
	if (eye.wide <= 0)
		return true;
	// The tile as a ball round its middle, half its diagonal wide, at the sea
	// and RELIEF over it (its land is between), against the four sides of
	// what the eye sees: a side through the eye, square to n, inside where
	// v · n ≤ 0 (v from the eye). A circle round the screen took in its
	// corners' worth all round it
	const double r = std::hypot(eye.at.x, eye.at.y, eye.at.z), up = (r + eye.altitude) / r;
	const Tiling::Vec &m = place(Key(key)).middle;
	const double mr = std::hypot(m.x, m.y, m.z), half = place(Key(key)).half;
	const auto dot = [](const Tiling::Vec &a, double x, double y, double z) { return a.x * x + a.y * y + a.z * z; };
	for (const double h : { 0.0, (double)RELIEF }) {
		const double k = (mr + h) / mr;
		const double vx = m.x * k - eye.at.x * up, vy = m.y * k - eye.at.y * up, vz = m.z * k - eye.at.z * up;
		const double ahead = dot(eye.look, vx, vy, vz), across = dot(eye.right, vx, vy, vz), above = dot(eye.upward, vx, vy, vz);
		// each side's (v · n) · |n|, n = (±side − look · tan); |n| = √(1 + tan²)
		const double nw = std::sqrt(1 + (double)eye.wide * eye.wide), nh = std::sqrt(1 + (double)eye.high * eye.high);
		if (std::abs(across) - ahead * eye.wide <= half * nw && std::abs(above) - ahead * eye.high <= half * nh)
			return true;
	}
	return false;
}

bool Tiles::choose(const Key &key, const Eye &eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want)
{
	if (!m_tiling.within(key.tile()))
		return true; // past its region's grid: nothing there
	float d = distance(key.tile(), eye);
	if (d > eye.range)
		return true; // beyond the eye's reach: nothing to draw
	if (!seen(key.tile(), eye))
		return true; // off screen: nothing to draw (drawn, a coarse planet's worth of it went to the GPU each frame)
	// all within the near world's range: its blocks are the land there, and this is
	// only what shows where they haven't arrived, so it needn't be fine
	const bool only_backdrop = inside(key.tile(), eye) && key.level <= BACKDROP_LEVEL;
	// high up, a tile's size on screen goes by how far it is through the air
	float seen = std::sqrt(d * d + eye.altitude * eye.altitude);
	// (once whole a tile stays whole a little longer: on the line, it flipped between two details)
	const auto held = m_tiles.find(key);
	const float stay = held != m_tiles.end() && held->second.whole == m_frame - 1 ? STAY_WHOLE : 1.0f;
	bool split = !only_backdrop && key.level > 0 && seen - eye.cut < (float)key.tile().span() * eye.split * stay;

	if (!split) {
		Entry &e = m_tiles[key];
		e.used = m_frame;
		if (e.ready) {
			e.whole = m_frame;
			draw.push_back(key);
			return true;
		}
		{
			const Tiling::Vec &m = place(key).middle;
			const double to[3] = { m.x - eye.at.x, m.y - eye.at.y, m.z - eye.at.z };
			const double len = std::max(1.0, std::hypot(to[0], to[1], to[2]));
			const double ahead = (to[0] * eye.heading.x + to[1] * eye.heading.y + to[2] * eye.heading.z) / len;
			want.emplace_back(d * (float)(1 - 0.6 * ahead), key);
		}
		// meanwhile, finer tiles from before, if all of them are there
		size_t mark = draw.size();
		// (standing in behind them, a coarser tile needn't have all of them:
		// what's there is kept, and it shows only through the rest)
		// (no deeper than FINER_DEPTH: kept looking past a missing one, with
		// keep_fine, a coarse planet not there yet was a search of every tile
		// under it to the finest, millions a frame; sprint 114)
		std::function<bool(const Key &, s32)> finer = [&](const Key &k, s32 depth) {
			if (k.level == 0 || depth == FINER_DEPTH)
				return false;
			bool all = true;
			for (s32 c = 0; c < 4 && (all || eye.keep_fine); c++) {
				const Key child = k.child(c);
				auto it = m_tiles.find(child);
				if (it != m_tiles.end() && it->second.ready) {
					it->second.used = m_frame;
					draw.push_back(child);
				} else if (!finer(child, depth + 1)) {
					all = false;
				}
			}
			return all;
		};
		if (finer(key, 0))
			return true;
		if (!eye.keep_fine)
			draw.resize(mark);
		return false;
	}

	size_t mark = draw.size();
	bool all = true, there[4];
	for (s32 c = 0; c < 4; c++)
		all &= there[c] = choose(key.child(c), eye, draw, want);
	if (all)
		return true;
	// a finer tile isn't there yet: this one stands in for all four
	auto it = m_tiles.find(key);
	if (it != m_tiles.end() && it->second.ready && eye.keep_fine) {
		// behind the finer ones that are there, so only the hole shows it: swapped
		// for them, a whole view flashed coarse for each new tile. Only its
		// quarters over the holes: the rest of it, at its own coarser heights,
		// showed past the finer land's edge, a block of it above or below the ground
		it->second.used = m_frame;
		for (s32 c = 0; c < 4; c++)
			if (!there[c])
				m_behind.emplace_back(key, c);
		return true;
	}
	if (it != m_tiles.end() && it->second.ready) {
		draw.resize(mark);
		draw.push_back(key);
		it->second.used = m_frame;
		return true;
	}
	return false;
}

void Tiles::install()
{
	const auto start = std::chrono::steady_clock::now();
	while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() < INSTALL_MS) {
		std::optional<Workers::Result> result = m_workers->take();
		if (!result)
			return;
		Entry &e = m_tiles[Key(result->key)];
		m_bytes -= e.bytes;
		e.bytes = 0;
		e.parts.clear();
		for (auto &q : e.quarters)
			q.clear();
		e.own.clear();
		e.merged = false;
		// one buffer for all of it if it fits 16-bit indices, quarter by
		// quarter: four a tile, a fine view was thousands of draws a frame,
		// and the driver looks over every buffer it holds each frame (sprint 114)
		size_t all = 0;
		for (const Mesh::Part &part : result->mesh.parts)
			all += part.vertices.size();
		if (all <= 65536) {
			auto buffer = make_irr<scene::SMeshBuffer>();
			auto &vertices = buffer->Vertices->Data;
			auto &indices = buffer->Indices->Data;
			vertices.reserve(all);
			for (u8 q = 0; q < 4; q++) {
				Entry::Range &range = e.ranges[q];
				range.v0 = (u32)vertices.size();
				range.i0 = (u32)indices.size();
				for (const Mesh::Part &part : result->mesh.parts) {
					if (part.quarter != q)
						continue;
					const u16 base = (u16)vertices.size();
					vertices.insert(vertices.end(), part.vertices.begin(), part.vertices.end());
					for (u16 i : part.indices)
						indices.push_back((u16)(base + i));
				}
				range.v1 = (u32)vertices.size();
				range.i1 = (u32)indices.size();
			}
			std::vector<video::SColor> own;
			own.reserve(vertices.size());
			for (const video::S3DVertex &v : vertices)
				own.push_back(v.Color);
			e.own.push_back(std::move(own));
			e.bytes += vertices.size() * sizeof(video::S3DVertex) + indices.size() * sizeof(u16);
			buffer->recalculateBoundingBox();
			buffer->setHardwareMappingHint(scene::EHM_STATIC);
			e.parts.push_back(std::move(buffer));
			e.merged = true;
		}
		for (Mesh::Part &part : result->mesh.parts) {
			if (e.merged)
				break;
			auto buffer = make_irr<scene::SMeshBuffer>();
			std::vector<video::SColor> own;
			own.reserve(part.vertices.size());
			for (const video::S3DVertex &v : part.vertices)
				own.push_back(v.Color);
			e.own.push_back(std::move(own));
			e.bytes += part.vertices.size() * sizeof(video::S3DVertex) + part.indices.size() * sizeof(u16);
			buffer->Vertices->Data = std::move(part.vertices);
			buffer->Indices->Data = std::move(part.indices);
			buffer->recalculateBoundingBox();
			buffer->setHardwareMappingHint(scene::EHM_STATIC);
			e.quarters[part.quarter].push_back(buffer);
			e.parts.push_back(std::move(buffer));
		}
		m_bytes += e.bytes;
		if (m_tint)
			paint(Key(result->key), e);
		e.painted = m_tints;
		if (m_coarse && !e.ready && result->key.level == m_tiling.roots().front().level)
			m_coarse->advance();
		e.ready = true;
		e.sketch = result->sketch;
	}
}

void Tiles::quarter(Entry &e, s32 child)
{
	if (!e.merged || !e.quarters[child].empty())
		return;
	const Entry::Range &r = e.ranges[child];
	if (r.v1 == r.v0)
		return; // nothing of it there
	const scene::SMeshBuffer &all = *e.parts.front();
	auto buffer = make_irr<scene::SMeshBuffer>();
	buffer->Vertices->Data.assign(all.Vertices->Data.begin() + r.v0, all.Vertices->Data.begin() + r.v1);
	for (u32 i = r.i0; i < r.i1; i++)
		buffer->Indices->Data.push_back((u16)(all.Indices->Data[i] - r.v0));
	buffer->recalculateBoundingBox();
	buffer->setHardwareMappingHint(scene::EHM_STATIC);
	e.quarters[child].push_back(std::move(buffer));
}

void Tiles::tint(std::shared_ptr<const Tint> tint)
{
	m_tint = std::move(tint);
	m_tints++; // every tile out of date: update() sweeps over them
}

void Tiles::repaint()
{
	if (m_swept == m_tints)
		return; // (looking over every tile and the clock each, each frame, cost the Planetarium's frames)
	const auto start = std::chrono::steady_clock::now();
	const auto spent = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(); };
	// the drawn first: what's on screen changes first
	for (const Key &key : m_draw) {
		if (spent() >= PAINT_MS)
			return;
		Entry &e = m_tiles.at(key);
		if (e.ready && e.painted != m_tints) {
			paint(key, e);
			e.painted = m_tints;
		}
	}
	for (auto &[key, e] : m_tiles) {
		if (e.ready && e.painted != m_tints) {
			if (spent() >= PAINT_MS)
				return;
			paint(key, e);
			e.painted = m_tints;
		}
	}
	m_swept = m_tints; // (a tile installed since is painted as it's installed)
}

size_t Tiles::unpainted() const
{
	size_t n = 0;
	for (const auto &[key, e] : m_tiles)
		n += e.ready && e.painted != m_tints;
	return n;
}

void Tiles::paint(const Key &key, Entry &e) const
{
	const Tiling::Vec mid = place(key).middle;
	for (size_t p = 0; p < e.parts.size(); p++) {
		auto &vertices = e.parts[p]->Vertices->Data;
		const std::vector<video::SColor> &own = e.own.at(p);
		for (size_t i = 0; i < vertices.size(); i++) {
			video::S3DVertex &v = vertices[i];
			if (!m_tint) {
				v.Color = own[i];
				continue;
			}
			const Tiling::Vec at{ mid.x + v.Pos.X / BS, mid.y + v.Pos.Y / BS, mid.z + v.Pos.Z / BS };
			video::SColor c = (*m_tint)(own[i], at);
			c.setAlpha(255); // the land is opaque
			v.Color = c;
		}
		e.parts[p]->setDirty(scene::EBT_VERTEX);
	}
	if (e.merged)
		for (auto &q : e.quarters)
			q.clear(); // (made again from the repainted buffer when they're wanted)
}

void Tiles::evict()
{
	if (m_bytes <= KEEP_BYTES)
		return;
	// the least recently wanted first, down to the budget; nothing wanted
	// this update (the roots are wanted every update: kept)
	std::vector<std::pair<u32, Key>> old;
	for (const auto &[key, e] : m_tiles)
		if (e.used != m_frame)
			old.emplace_back(e.used, key);
	std::sort(old.begin(), old.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	for (const auto &[used, key] : old) {
		if (m_bytes <= KEEP_BYTES)
			break;
		auto it = m_tiles.find(key);
		m_bytes -= it->second.bytes;
		m_tiles.erase(it);
	}
}

} // namespace far
