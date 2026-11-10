// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/tilecache.h"
#include "far/horizon.h"
#include "exceptions.h"
#include "network/networkpacket.h"
#include "serialization.h"
#include <cstring>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <vector>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace far {

// A kept tile's file: this, then its cells, zstd. Changed with the layout
// below (the sources' hash changes with it anyway: this guards a stray file)
static const char MAGIC[4] = { 'T', 'F', 'T', '1' };

static uint64_t fnv(std::string_view bytes, uint64_t h = 1469598103934665603ull)
{
	for (unsigned char c : bytes)
		h = (h ^ c) * 1099511628211ull;
	return h;
}

// written since the last sweep, by every cache of the process (the first
// save sweeps: what earlier runs left)
static std::atomic<uintmax_t> g_written{ TileCache::SWEEP_EVERY };
static std::mutex g_sweeping;

static std::string hex(uint64_t v)
{
	char buf[17];
	std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v);
	return buf;
}

TileCache::TileCache(const std::string &root, const Horizon &horizon, const planet::Atlas &atlas, int body, uintmax_t cap) :
	m_cap(cap)
{
	const fs::path base = fs::path(root) / "far";
	const std::string sources = std::string(FAR_SOURCES).substr(0, 16);
	// what decides the tiles besides the sources: the horizon as it's sent,
	// and where the body lies on the atlas
	NetworkPacket pkt(0, 0, 0);
	write(pkt, horizon);
	pkt.seek(0); // (one offset for writing and reading: back to the start to read it all)
	const uint64_t world = fnv(atlas.body(body).code() + "/" + std::to_string(body), fnv(pkt.getRemainingNoCopy()));
	m_base = base.string();
	m_dir = (base / sources / hex(world)).string();
	fs::create_directories(m_dir);
	// (another build's tiles are another generation's: never read again)
	for (const fs::directory_entry &e : fs::directory_iterator(base))
		if (e.path().filename().string() != sources)
			fs::remove_all(e.path());
}

std::string TileCache::path(const TileKey &key) const
{
	return m_dir + "/" + std::to_string(key.level) + "_" + std::to_string(key.face) + "_" + std::to_string(key.x) + "_" +
			std::to_string(key.z) + ".tile";
}

// (the machine's own byte order: a cache, never moved between machines)
template <typename T>
static void put(std::string &out, T v)
{
	out.append((const char *)&v, sizeof v);
}

template <typename T>
static T get(const std::string &in, size_t &at)
{
	if (at + sizeof(T) > in.size())
		throw SerializationError("far tile cache: a kept tile ends early");
	T v;
	std::memcpy(&v, in.data() + at, sizeof v);
	at += sizeof v;
	return v;
}

static void putNode(std::string &out, const MapNode &n)
{
	put<u16>(out, n.param0);
	put<u8>(out, n.param1);
	put<u8>(out, n.param2);
}

static MapNode getNode(const std::string &in, size_t &at)
{
	const u16 c = get<u16>(in, at);
	const u8 p1 = get<u8>(in, at), p2 = get<u8>(in, at);
	return MapNode(c, p1, p2);
}

static void putRun(std::string &out, const Run &r)
{
	put<s32>(out, r.low);
	put<s32>(out, r.high);
	putNode(out, r.node);
}

static Run getRun(const std::string &in, size_t &at)
{
	Run r;
	r.low = get<s32>(in, at);
	r.high = get<s32>(in, at);
	r.node = getNode(in, at);
	return r;
}

std::optional<Tile> TileCache::load(const TileKey &key) const
{
	const std::string file_path = path(key);
	std::ifstream file(file_path, std::ios::binary);
	if (!file)
		return std::nullopt;
	// used now: the sweep lets the least recently used go (a failure's a
	// tile swept meanwhile, still read whole from the open file)
	std::error_code ignored;
	fs::last_write_time(file_path, fs::file_time_type::clock::now(), ignored);
	char magic[4];
	if (!file.read(magic, 4) || std::memcmp(magic, MAGIC, 4) != 0)
		return std::nullopt; // (another layout's: built again, and written over)
	std::ostringstream raw;
	decompressZstd(file, raw);
	const std::string in = raw.str();
	size_t at = 0;
	Tile t;
	t.key = key;
	t.cells.resize((size_t)(TILE + 2) * (TILE + 2));
	for (Cell &c : t.cells) {
		c.space = get<u8>(in, at) != 0;
		c.ground = get<s32>(in, at);
		c.top = getNode(in, at);
		c.top_end = get<s32>(in, at);
		c.filler_end = get<s32>(in, at);
		c.filler = getNode(in, at);
		c.rock = getNode(in, at);
		c.water = get<s32>(in, at);
		c.liquid = getNode(in, at);
		c.trunk = getRun(in, at);
		c.canopy = getRun(in, at);
	}
	if (at != in.size())
		throw SerializationError("far tile cache: a kept tile runs on past its cells");
	return t;
}

void TileCache::save(const Tile &tile) const
{
	std::string out;
	out.reserve(tile.cells.size() * 48);
	for (const Cell &c : tile.cells) {
		put<u8>(out, c.space);
		put<s32>(out, c.ground);
		putNode(out, c.top);
		put<s32>(out, c.top_end);
		put<s32>(out, c.filler_end);
		putNode(out, c.filler);
		putNode(out, c.rock);
		put<s32>(out, c.water);
		putNode(out, c.liquid);
		putRun(out, c.trunk);
		putRun(out, c.canopy);
	}
	// written whole under another name, then moved over: a reader never sees
	// half a tile, and two workers (or two processes) never share a file
	thread_local std::mt19937_64 random{ std::random_device{}() };
	const std::string to = path(tile.key), temporary = to + "." + hex(random()) + ".part";
	{
		std::ofstream file(temporary, std::ios::binary);
		file.write(MAGIC, 4);
		compressZstd(out, file);
		if (!file)
			throw BaseException("far tile cache: can't write " + temporary);
	}
	fs::rename(temporary, to);
	if ((g_written += fs::file_size(to)) >= SWEEP_EVERY)
		sweep();
}

void TileCache::sweep() const
{
	std::unique_lock<std::mutex> one(g_sweeping, std::try_to_lock);
	if (!one.owns_lock())
		return; // another worker's at it
	g_written = 0;
	struct Kept
	{
		fs::path path;
		fs::file_time_type used;
		uintmax_t bytes;
	};
	std::vector<Kept> all;
	uintmax_t total = 0;
	const auto stale = fs::file_time_type::clock::now() - std::chrono::hours(1);
	// (entries vanish as other processes sweep too: those errors are skipped)
	std::error_code e;
	for (fs::recursive_directory_iterator it(m_base, e), end; !e && it != end; it.increment(e)) {
		if (!it->is_regular_file(e))
			continue;
		const fs::path p = it->path();
		const fs::file_time_type used = it->last_write_time(e);
		if (e)
			continue;
		if (p.extension() == ".part") {
			if (used < stale)
				fs::remove(p, e); // a writer that died
			continue;
		}
		const uintmax_t bytes = it->file_size(e);
		if (e)
			continue;
		all.push_back({ p, used, bytes });
		total += bytes;
	}
	if (total <= m_cap)
		return;
	// to nine tenths, so it isn't over again the next tile
	std::sort(all.begin(), all.end(), [](const Kept &a, const Kept &b) { return a.used < b.used; });
	for (const Kept &k : all) {
		if (total <= m_cap / 10 * 9)
			break;
		if (fs::remove(k.path, e))
			total -= k.bytes;
	}
}

} // namespace far
