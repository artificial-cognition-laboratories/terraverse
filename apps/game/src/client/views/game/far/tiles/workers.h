// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/land/mesh.h"
#include "src/client/views/game/far/land/terrain.h"
#include "library/world/planet/atlas.h"
#include <array>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace far {

class Palette;
class Tiling;
struct Horizon;

/*
	A tile's mesh as it's installed: one buffer if it fits 16-bit indices,
	the quarters' ranges in it (four a tile, a fine view was thousands of
	draws a frame, and the driver looks over every buffer it holds each
	frame: sprint 114), else its parts as they are; and each buffer's own
	colours, kept for a tint. Packed by the worker that built it: done in
	install(), it was most of the far view's spikes on the frame's thread.
*/
struct Packed
{
	struct Range
	{
		u32 v0 = 0, v1 = 0, i0 = 0, i1 = 0;
	};
	// a part as it's kept: its vertices packed (land/resident.h)
	struct Part
	{
		std::vector<PackedVertex> vertices;
		std::vector<u16> indices;
		u8 quarter = 0;
	};
	bool merged = false;
	std::vector<Part> parts; // merged: one, its quarter unused
	std::array<Range, 4> ranges;   // merged: each quarter's in it
	std::vector<std::vector<video::SColor>> own; // each part's
	std::vector<aabb3f> boxes; // each part's bounds
	std::array<aabb3f, 4> quarterBoxes{ aabb3f(v3f()), aabb3f(v3f()), aabb3f(v3f()), aabb3f(v3f()) }; // merged: each quarter's bounds (empty quarters' unused)
};

/*
	The workers: build tiles and their meshes off the main thread, nearest
	first. Each update says which tiles are wanted (want()); keys being
	built or waiting to be taken aren't asked for twice. Let go of, they're
	stopped and joined: each finishes at most the tile it's on. Detached, one
	could still be running as the process exited, and crash it.
*/
class Workers
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
		Packed mesh;
		bool sketch = false; // a stand-in: the tile itself is still to build
	};

	Workers(const planet::Atlas &atlas, int body, const Horizon &horizon, const Palette &palette, const Tiling &tiling,
			unsigned threads);
	~Workers();

	void want(const std::vector<Job> &jobs);
	std::optional<Result> take();

private:
	std::shared_ptr<class Crew> m_crew;
	std::vector<std::thread> m_threads;
};

} // namespace far
