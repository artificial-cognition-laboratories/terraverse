// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// What streams.cpp and streams_grow.cpp share: the tuning, the branches and
// how they're kept. Not a door: only Streams includes it.
#pragma once

#include "library/world/terragen/src/rivers/streams.h"
#include <algorithm>
#include <array>
#include <mutex>
#include <unordered_map>

namespace terragen {
namespace streams {

// The finest level's lattice: under it a hillside's too short to be cut by
// its own stream (FINEST_M metres, and never under FINEST blocks); finer
// than that, the amplifier's gullies
constexpr int64_t FINEST = 40, FINEST_M = 200;
constexpr int MOST_LEVELS = 12;
// A valley's depth at a level: RELIEF · k_s · cell · (L / half a cell)^FALL,
// as stream power at its balance cuts (k_s the grid's channel steepness)
constexpr fix RELIEF = ratio(5, 10), FALL = ratio(35, 100);
// Its sides: never steeper than a hillside stands, at least VALLEY of its
// lattice wide each side, at most the lattice (what a column looks through)
constexpr fix CRITICAL = ratio(55, 100), VALLEY = ratio(1, 8);
// The grid's surface is a cell's mean: its trunk's floor a cell's valley
// depth under it (Streams::floor), its ridges up to UP of that over it
constexpr fix UP = ONE;
// What a channel drains (cells of average rain) for it to carry water
constexpr fix WET = ratio(15, 10000);
// A channel in its valley's floor: its water FREEBOARD of its width under the
// floor (at least a half block, at most two), its bed DEEP of its width under
// that (at least a block), and banks sloping from the water's edge up to the
// floor over BANKS of its width (at least two blocks)
constexpr fix FREEBOARD = ratio(1, 12), DEEP = ratio(1, 6), BANKS = ratio(1, 2);
inline int64_t bankOf(int64_t width) { return std::max<int64_t>(mulShift(width, BANKS, FRAC), 2 * ONE); }
// A node's edge to what it drains to, in pieces
constexpr int EDGE_PIECES = 8;
// A node's edge leaves along, and arrives along, TANGENT of its length in the
// way the water goes (so edges run on through nodes without a kink)
constexpr fix TANGENT = ratio(1, 2);
// It joins a coarser channel this much of its distance downstream of the
// nearest point (a tributary meets its trunk at about 60°, not square)
constexpr fix ADVANCE = ratio(6, 10);
// A grid run's line, in pieces; a drawn river's, at least this many a meander loop
constexpr int RUN_PIECES = 12, DRAWN_PIECES = 8;
// A drawn river free of the hills: its valley's floor this many of its widths
// either side of it (its meanders' floodplain)
constexpr fix FLOODPLAIN = ratio(15, 10);
// how far a run that isn't a drawn river swings across its line, of its length
constexpr fix BEND = ratio(18, 100);
constexpr int SUB = 8; // fractional bits of a position in blocks
// Kept: per level, shards, each emptied when it holds this many
constexpr size_t SHARDS = 64, SHARD_MOST = 1 << 13;
// The index of which branches pass a cube is made a block of BLOCK³ cubes at once
constexpr int64_t BLOCK = 4;
// A far tile carves only levels at least this many of its cells across
constexpr int64_t FAR_TILES = 4;

inline uint64_t mix(uint64_t v)
{
	v ^= v >> 30;
	v *= 0xbf58476d1ce4e5b9ull;
	v ^= v >> 27;
	v *= 0x94d049bb133111ebull;
	v ^= v >> 31;
	return v;
}
inline int64_t floorDiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
inline uint64_t keyOf(int64_t i, int64_t j, int64_t l)
{
	return ((uint64_t)(i + (1 << 20)) & 0x1fffff) | ((uint64_t)(j + (1 << 20)) & 0x1fffff) << 21 | ((uint64_t)(l + (1 << 20)) & 0x1fffff) << 42;
}

using V = Streams::V;

struct Near
{
	int64_t d = INT64_MAX, bed = 0, width = 0; // blocks, fixed
	V q{ 0, 0, 0 };
	const Streams::Branch *b = nullptr;
	size_t seg = 0; // q's piece of b, and how far along it
	fix t = 0;
};

} // namespace streams

struct Streams::Branch
{
	std::vector<V> pts;
	std::vector<int64_t> bed, width; // blocks, fixed; width 0: a dry valley
	int level = 0;
	bool drawn = false; // a grid river ground() draws itself
	V lo{ 0, 0, 0 }, hi{ 0, 0, 0 }; // its box (bounds())
	int64_t widest = 0;             // blocks, fixed
	int64_t lowest = 0;             // its lowest bed, blocks, fixed
	void bounds()
	{
		lowest = *std::min_element(bed.begin(), bed.end());
		lo = hi = pts.front();
		for (const V &p : pts) {
			lo = { std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z) };
			hi = { std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z) };
		}
		widest = width.empty() ? 0 : *std::max_element(width.begin(), width.end());
	}
	// whether any of it could be within `reach` (blocks, fixed) of p
	bool within(const V &p, int64_t reach) const
	{
		const int64_t r = (reach >> (FRAC - streams::SUB)) + 1;
		return p.x >= lo.x - r && p.x <= hi.x + r && p.y >= lo.y - r && p.y <= hi.y + r && p.z >= lo.z - r && p.z <= hi.z + r;
	}
};

// A level's node: a cube's hashed point, if it lies on land, and the land
// there as the coarser levels left it
struct Streams::Node
{
	bool real = false;
	V p{ 0, 0, 0 };
	int64_t height = 0; // blocks, fixed
};

// Where a node drains: a neighbour (down: its cube), a coarser channel, or nowhere
struct Streams::Choice
{
	bool real = false, down = false;
	int64_t a = 0, b = 0, c = 0;
};

struct Streams::Kept
{
	struct Shard
	{
		std::mutex lock;
		std::unordered_map<uint64_t, Node> nodes;
		std::unordered_map<uint64_t, Choice> choices;
		std::unordered_map<uint64_t, int64_t> areas;
		std::unordered_map<uint64_t, std::shared_ptr<const Branch>> heads;
		// a block's index: each of its cubes' branches passing through
		using Block = std::unordered_map<uint64_t, std::vector<std::shared_ptr<const Branch>>>;
		std::unordered_map<uint64_t, std::shared_ptr<const Block>> blocks;
	};
	std::array<std::array<Shard, streams::SHARDS>, streams::MOST_LEVELS + 1> levels;
	std::vector<std::shared_ptr<const Branch>> runs;
	std::unique_ptr<std::once_flag[]> ran;
	// each cell's runs near enough to cut the land round it
	std::vector<std::vector<const Branch *>> rings;
	std::unique_ptr<std::once_flag[]> ringed;
};

namespace streams {

// the nearest point of a branch to p (squares compared; one root)
inline Near nearest(const Streams::Branch &b, const V &p)
{
	Near n;
	int64_t best = INT64_MAX;
	size_t at = 0;
	fix at_t = 0;
	for (size_t i = 0; i + 1 < b.pts.size(); i++) {
		const V &a = b.pts[i], &c = b.pts[i + 1];
		const int64_t ex = c.x - a.x, ey = c.y - a.y, ez = c.z - a.z;
		const int64_t l2 = ex * ex + ey * ey + ez * ez;
		const int64_t along = (p.x - a.x) * ex + (p.y - a.y) * ey + (p.z - a.z) * ez;
		const fix t = l2 > 0 ? (fix)std::clamp<int64_t>((int64_t)(((__int128)along << FRAC) / l2), 0, ONE) : 0;
		const int64_t dx = p.x - (a.x + (ex * t >> FRAC)), dy = p.y - (a.y + (ey * t >> FRAC)), dz = p.z - (a.z + (ez * t >> FRAC));
		const int64_t d2 = dx * dx + dy * dy + dz * dz;
		if (d2 < best) {
			best = d2;
			at = i;
			at_t = t;
		}
	}
	if (best == INT64_MAX)
		return n;
	const V &a = b.pts[at], &c = b.pts[at + 1];
	n.q = { a.x + ((c.x - a.x) * at_t >> FRAC), a.y + ((c.y - a.y) * at_t >> FRAC), a.z + ((c.z - a.z) * at_t >> FRAC) };
	n.d = (int64_t)isqrt((uint64_t)best) << (FRAC - SUB);
	n.b = &b;
	n.seg = at;
	n.t = at_t;
	n.bed = b.bed[at] + mulShift(b.bed[at + 1] - b.bed[at], at_t, FRAC);
	n.width = b.width[at] + mulShift(b.width[at + 1] - b.width[at], at_t, FRAC);
	return n;
}

} // namespace streams
} // namespace terragen
