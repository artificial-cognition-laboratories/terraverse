// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grid.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

/*
	Built grids on disk. A grid is seconds of plates and rivers and a few MB
	of plain numbers, so a kept one is simply its fields in order. A file is
	moved into place whole, and named for its build and code; one that's
	there and doesn't read back is a fault, not a miss.
*/
namespace terragen {

namespace fs = std::filesystem;

namespace {

constexpr size_t GRID_SIZE = 1184;

std::mutex g_lock;
std::string g_dir, g_build;

struct Writer
{
	std::ostream &out;
	void raw(const void *p, size_t n) { out.write(static_cast<const char *>(p), (std::streamsize)n); }
};

struct Reader
{
	std::istream &in;
	void raw(void *p, size_t n)
	{
		in.read(static_cast<char *>(p), (std::streamsize)n);
		if (!in)
			throw std::runtime_error("terragen: a kept grid ends early");
	}
};

template <typename T>
void field(Writer &io, const T &v)
{
	static_assert(std::is_trivially_copyable_v<T>);
	io.raw(&v, sizeof v);
}
template <typename T>
void field(Reader &io, T &v)
{
	static_assert(std::is_trivially_copyable_v<T>);
	io.raw(&v, sizeof v);
}
template <typename T>
void field(Writer &io, const std::vector<T> &v)
{
	static_assert(std::is_trivially_copyable_v<T>);
	const uint64_t n = v.size();
	field(io, n);
	io.raw(v.data(), n * sizeof(T));
}
template <typename T>
void field(Reader &io, std::vector<T> &v)
{
	static_assert(std::is_trivially_copyable_v<T>);
	uint64_t n;
	field(io, n);
	v.resize(n);
	io.raw(v.data(), n * sizeof(T));
}

// Every field of a Grid: a new one that isn't listed comes back empty, and
// the size check below stops the build until it's looked at.
template <typename IO, typename G>
void fields(IO &io, G &g)
{
	static_assert(sizeof(Grid) == GRID_SIZE, "Grid changed: list the new field here, then set GRID_SIZE");
	field(io, g.sea_offset); field(io, g.base); field(io, g.eroded); field(io, g.worn); field(io, g.sediment);
	field(io, g.placer); field(io, g.to_deep); field(io, g.uplifted); field(io, g.hollows); field(io, g.filled); field(io, g.pan);
	field(io, g.receiver); field(io, g.drained); field(io, g.lake); field(io, g.lake_level);
	field(io, g.rain_season[0]); field(io, g.rain_season[1]); field(io, g.sea_heat); field(io, g.heat);
	field(io, g.heat_gain); field(io, g.ice); field(io, g.fertility); field(io, g.river_threshold);
	field(io, g.stream_threshold);
	auto &t = g.tectonics;
	field(io, t.plates); field(io, t.plate); field(io, t.boundary); field(io, t.distance); field(io, t.side);
	field(io, t.belt); field(io, t.relief); field(io, t.formed); field(io, t.age); field(io, t.fold);
	field(io, t.plateau); field(io, t.oldness); field(io, t.front); field(io, t.stable);
	field(io, g.history); field(io, g.province); field(io, g.volcanoes); field(io, g.rise); field(io, g.canyons);
	field(io, g.volcano_start); field(io, g.volcano_list); field(io, g.craters); field(io, g.crater_stamp);
	field(io, g.crater_young); field(io, g.crater_start); field(io, g.crater_list); field(io, g.crater_from);
	field(io, g.mare); field(io, g.mare_level); field(io, g.titanium); field(io, g.thin);
}

fs::path fileOf(const std::string &code)
{
	return fs::path(g_dir) / (g_build + "-" + std::to_string(std::hash<std::string>{}(code)) + ".grid");
}

} // namespace

void keepGridsIn(const std::string &dir, const std::string &build)
{
	std::lock_guard<std::mutex> hold(g_lock);
	g_dir = dir;
	g_build = build;
	fs::create_directories(dir);
	for (const fs::directory_entry &e : fs::directory_iterator(dir))
		if (e.path().filename().string().rfind(build + "-", 0) != 0)
			fs::remove(e.path()); // another build's (a dev tree makes one per compile)
}

std::shared_ptr<const Grid> loadGrid(const std::string &code)
{
	std::lock_guard<std::mutex> hold(g_lock);
	if (g_dir.empty())
		return nullptr;
	std::ifstream in(fileOf(code), std::ios::binary);
	if (!in)
		return nullptr;
	Reader io{ in };
	uint64_t n;
	field(io, n);
	std::string named(n, 0);
	io.raw(named.data(), n);
	if (named != code)
		return nullptr; // another code with this hash: it's rebuilt, and takes the file
	auto g = std::make_shared<Grid>();
	fields(io, *g);
	return g;
}

void saveGrid(const std::string &code, const Grid &grid)
{
	std::lock_guard<std::mutex> hold(g_lock);
	if (g_dir.empty())
		return;
	const fs::path file = fileOf(code);
	fs::path part = file;
	part += "." + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
	{
		std::ofstream out(part, std::ios::binary);
		Writer io{ out };
		const uint64_t n = code.size();
		field(io, n);
		io.raw(code.data(), n);
		fields(io, grid);
		if (!out)
			throw std::runtime_error("terragen: couldn't write a grid to " + part.string());
	}
	fs::rename(part, file);
}

} // namespace terragen
