// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/tiles.h"
#include "far/mesh.h"
#include "constants.h"
#include "exceptions.h"
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

namespace far {

// Time an update may spend putting tiles on the GPU (a frame at 60 fps is 16.7).
static constexpr double INSTALL_MS = 3;
// And recolouring tiles for a new tint.
static constexpr double PAINT_MS = 3;
// A tile not wanted for this many updates is dropped.
static constexpr u32 KEEP_FRAMES = 600;
static constexpr s32 BACKDROP_LEVEL = 3; // cells of 8 blocks
static constexpr float STAY_WHOLE = 0.85f;

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
	struct Result
	{
		TileKey key;
		Mesh mesh;
	};

	Crew(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, const Tiling &tiling) :
		m_atlas(atlas), m_body(body), m_horizon(horizon), m_palette(palette), m_tiling(tiling)
	{
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
	void want(const std::vector<std::pair<float, TileKey>> &keys)
	{
		{
			std::lock_guard lock(m_mutex);
			m_queue.clear();
			for (const auto &k : keys)
				if (!m_busy.count(id(k.second)))
					m_queue.push_back(k.second);
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
		{
			// the first worker in builds the generator (the world model: a
			// moment), outside the lock: the main thread asks and takes meanwhile
			std::unique_lock lock(m_mutex);
			if (!m_generator && !m_building && m_error.empty()) {
				m_building = true;
				lock.unlock();
				std::unique_ptr<Generator> made;
				std::string error;
				try {
					made = std::make_unique<Generator>(m_horizon, m_atlas, m_body);
				} catch (const std::exception &e) {
					error = std::string("the world can't be built from the server's data: ") + e.what();
				}
				lock.lock();
				m_generator = std::move(made);
				m_error = error;
				m_building = false;
			}
		}
		m_wake.notify_all();
		std::unique_lock lock(m_mutex);
		while (true) {
			m_wake.wait(lock, [this] { return m_stop || !m_error.empty() || (!m_queue.empty() && m_generator); });
			if (!m_error.empty())
				return; // take() reports it
			if (m_stop)
				return;
			TileKey key = m_queue.front();
			m_queue.pop_front();
			m_busy.insert(id(key));
			lock.unlock();

			Result r{ key, {} };
			try {
				const ScopeProfiler timed(g_profiler, "Far view: tile built [ms]", SPT_AVG);
				r.mesh = onPlanet(m_generator->build(key));
			} catch (const std::exception &e) {
				lock.lock();
				m_error = e.what();
				return;
			}

			lock.lock();
			m_results.push_back(std::move(r));
		}
	}

private:
	// The tile's mesh put on the planet (Tiling::place); its height over
	// the sea (blocks) in the first texture coordinate, for the haze.
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
	std::unique_ptr<Generator> m_generator;
	bool m_building = false; // a worker is making m_generator

	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_stop = false;
	std::deque<TileKey> m_queue;
	std::set<std::tuple<s32, s32, s32, s32>> m_busy; // being built, or built and not yet taken
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
	using Result = Crew::Result;

	Workers(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, const Tiling &tiling,
			unsigned threads) :
		m_crew(std::make_shared<Crew>(atlas, body, horizon, palette, tiling))
	{
		if (threads == 0)
			throw BaseException("far tiles: no threads to build with");
		for (unsigned i = 0; i < threads; i++)
			std::thread([crew = m_crew] { crew->run(); }).detach();
	}

	~Workers() { m_crew->stop(); }

	void want(const std::vector<std::pair<float, TileKey>> &keys) { m_crew->want(keys); }
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
	m_workers(std::make_unique<Workers>(m_atlas, body, m_horizon, palette, m_tiling, threads))
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
	// stand in while the eye's own tiles are built (turning never opens holes)
	for (const TileKey &root : m_tiling.roots()) {
		Entry &e = m_tiles[Key(root)];
		e.used = m_frame;
		if (e.ready)
			continue;
		auto again = std::find_if(want.begin(), want.end(), [&](const auto &w) { return !(w.second < Key(root)) && !(Key(root) < w.second); });
		if (again != want.end())
			again->first = -1.0f; // wanted already: first, now
		else
			want.emplace_back(-1.0f, Key(root));
	}
	m_pending = want.size();
	std::sort(want.begin(), want.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::vector<std::pair<float, TileKey>> keys;
	keys.reserve(want.size());
	for (const auto &w : want)
		keys.emplace_back(w.first, w.second.tile());
	m_workers->want(keys);

	install();
	repaint();
	if (m_frame % 60 == 0)
		evict();
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
	for (const Key &key : m_behind)
		out.push_back({ key.tile(), m_tiling.middle(key.tile()), m_tiles.at(key).parts });
	return out;
}

std::vector<Tiles::Drawn> Tiles::drawn() const
{
	std::vector<Drawn> out;
	out.reserve(m_draw.size());
	for (const Key &key : m_draw)
		out.push_back({ key.tile(), m_tiling.middle(key.tile()), m_tiles.at(key).parts });
	return out;
}

float Tiles::distance(const TileKey &key, const Eye &eye) const
{
	const Tiling::Vec p = m_tiling.middle(key);
	const double r = m_tiling.radius(), sea = r + m_horizon.water_level + 1;
	const double d = std::hypot(p.x * sea / r - eye.at.x, p.y * sea / r - eye.at.y, p.z * sea / r - eye.at.z);
	return (float)std::max(d - m_tiling.diagonal(key) / 2, 0.0);
}

bool Tiles::inside(const TileKey &key, const Eye &eye) const
{
	if (eye.cut <= 0)
		return false;
	// its farthest point from the eye, at the sea, and its land's highest a
	// tile's relief over that (a peak could be farther off than the sea under it)
	const Tiling::Vec p = m_tiling.middle(key);
	const double r = m_tiling.radius(), sea = r + m_horizon.water_level + 1;
	const double d = std::hypot(p.x * sea / r - eye.at.x, p.y * sea / r - eye.at.y, p.z * sea / r - eye.at.z) +
			m_tiling.diagonal(key) / 2;
	const double far = std::hypot(d, (double)eye.altitude) + Tiles::RELIEF;
	return far < eye.cut;
}

bool Tiles::choose(const Key &key, const Eye &eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want)
{
	if (!m_tiling.within(key.tile()))
		return true; // past its region's grid: nothing there
	float d = distance(key.tile(), eye);
	if (d > eye.range)
		return true; // beyond the eye's reach: nothing to draw
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
			const Tiling::Vec m = m_tiling.middle(key.tile());
			const double to[3] = { m.x - eye.at.x, m.y - eye.at.y, m.z - eye.at.z };
			const double len = std::max(1.0, std::hypot(to[0], to[1], to[2]));
			const double ahead = (to[0] * eye.heading.x + to[1] * eye.heading.y + to[2] * eye.heading.z) / len;
			want.emplace_back(d * (float)(1 - 0.6 * ahead), key);
		}
		// meanwhile, finer tiles from before, if all of them are there
		size_t mark = draw.size();
		std::function<bool(const Key &)> finer = [&](const Key &k) {
			if (k.level == 0)
				return false;
			for (s32 c = 0; c < 4; c++) {
				const Key child = k.child(c);
				auto it = m_tiles.find(child);
				if (it != m_tiles.end() && it->second.ready) {
					it->second.used = m_frame;
					draw.push_back(child);
				} else if (!finer(child)) {
					return false;
				}
			}
			return true;
		};
		if (finer(key))
			return true;
		draw.resize(mark);
		return false;
	}

	size_t mark = draw.size();
	bool all = true;
	for (s32 c = 0; c < 4; c++)
		all &= choose(key.child(c), eye, draw, want);
	if (all)
		return true;
	// a finer tile isn't there yet: this one stands in for all four
	auto it = m_tiles.find(key);
	if (it != m_tiles.end() && it->second.ready && eye.keep_fine) {
		// behind the finer ones that are there, so only the hole shows it: swapped
		// for them, a whole view flashed coarse for each new tile
		it->second.used = m_frame;
		m_behind.push_back(key);
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
		e.parts.clear();
		e.own.clear();
		for (Mesh::Part &part : result->mesh.parts) {
			auto buffer = make_irr<scene::SMeshBuffer>();
			std::vector<video::SColor> own;
			own.reserve(part.vertices.size());
			for (const video::S3DVertex &v : part.vertices)
				own.push_back(v.Color);
			e.own.push_back(std::move(own));
			buffer->Vertices->Data = std::move(part.vertices);
			buffer->Indices->Data = std::move(part.indices);
			buffer->recalculateBoundingBox();
			buffer->setHardwareMappingHint(scene::EHM_STATIC);
			e.parts.push_back(std::move(buffer));
		}
		if (m_tint)
			paint(Key(result->key), e);
		e.painted = m_tints;
		e.ready = true;
	}
}

void Tiles::tint(std::shared_ptr<const Tint> tint)
{
	m_tint = std::move(tint);
	m_tints++; // every tile out of date: update() sweeps over them
}

void Tiles::repaint()
{
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
		if (spent() >= PAINT_MS)
			return;
		if (e.ready && e.painted != m_tints) {
			paint(key, e);
			e.painted = m_tints;
		}
	}
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
	const Tiling::Vec mid = m_tiling.middle(key.tile());
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
}

void Tiles::evict()
{
	for (auto it = m_tiles.begin(); it != m_tiles.end();) {
		if (m_frame - it->second.used > KEEP_FRAMES) // (the roots are used every update: kept)
			it = m_tiles.erase(it);
		else
			++it;
	}
}

} // namespace far
