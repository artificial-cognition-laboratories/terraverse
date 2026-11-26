// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/server/world/run/pile/pile.h"
#include "src/shared/base/irr_v3d.h"
#include "src/shared/map/block/mapnode.h"
#include "library/geometry/shape/shape.h"
#include <functional>
#include <map>
#include <optional>
#include <vector>

class Map;
class NodeDefManager;
struct ContentFeatures;

namespace pile {

// A loose node's material, from its groups (pile_repose, pile_yield,
// pile_bearing, pile_give); none: it doesn't move (no pile_bearing)
std::optional<Material> materialOf(const ContentFeatures &f);

/*
	The world's moving ground (sprint 84), on the server. What presses on
	loose ground (wheels, feet) wakes a region round it: a box of nodes read
	into a field (each column's surface from the nodes' solid sixteenths:
	cut, smoothed, or whole), pressed and run there, and written back into
	the map as cut shapes a few times a second while it moves, so the
	clients, collision and digging see it as any cut ground. A region with
	nothing moving in it for a second is written and let go: asleep, the
	ground is just the map's.

	A region is one material (the ground pressed): others in it are its
	floor and walls.
*/
class Piles
{
public:
	// `changed`: told of each node it writes (the client remeshes it; the
	// server's map sends its own events). `predict`: a client's, ahead of
	// the server: it writes often, and only reshapes the ground's own
	// nodes, never adds or removes one (the server's shapes put a reshaped
	// node right; a node made only here never would be). `settled`: told of
	// the nodes a region moved when it's let go (the server sends their
	// shapes: a prediction that differed is put right, one that didn't
	// costs nothing).
	Piles(Map &map, const NodeDefManager &ndef, std::function<void(v3s16)> changed = {}, bool predict = false,
			std::function<void(const std::vector<v3s16> &)> settled = {});

	/*
		Something presses the ground: `who` (a wheel, a foot: a key of the
		caller's) at `at` (nodes, where it touches), a patch `radius` nodes
		across, at `kpa`. Counted once per sixteenth it travels: standing
		still presses once. Ignored over ground that doesn't move, or off it.
	*/
	void press(u64 who, v3f at, float radius, float kpa);
	// Forget a presser that's gone.
	void forget(u64 who) { m_last.erase(who); }

	/*
		The node at `at` was dug or built (it was `was`): the loose ground
		round it keeps the shape it had (it's smoothed by its neighbours, so
		a hole would have ramped it, a block raised it), frozen as cut, and
		is stirred: what can't stand there slides, what can holds its wall.
	*/
	void changed(v3s16 at, MapNode was);

	void step(float dtime);

	size_t awake() const { return m_regions.size(); }

	static constexpr int REACH = 4;      // nodes round a press a region spans (x, z)
	static constexpr int BELOW = 2, ABOVE = 2; // ... and down and up
	static constexpr float TICK = 0.05f; // s a step of sliding
	static constexpr float WRITE = 0.25f; // s between writes while it moves
	static constexpr float PREDICT = 0.1f; // ... a client's own (each one's meshes made again)
	static constexpr float SLEEP = 1.0f; // s still before it's let go

private:
	struct Region
	{
		v3s16 lo, hi; // nodes, inclusive
		content_t c;
		Field field;
		std::vector<MapNode> node; // per node column (x, z): the node a cell filled there becomes
		std::vector<int> shown;    // per column, its top as last written (or read): only what's changed since is written
		std::vector<char> moved;   // per node column (x, z): ever written
		float still = 0, unwritten = 0;
		bool dirty = false;
	};

	// The region for a press at node p of material c, made if none holds it
	Region *regionAt(v3s16 p, content_t c, const Material &m);
	std::optional<Region> read(v3s16 lo, v3s16 hi, content_t c, const Material &m);
	void write(Region &r);
	// which of a cell's sixteenths are solid, and what it's made of (air
	// rising from smoothed ground below is the ground's)
	struct Cell
	{
		shape::Shape solid;
		MapNode node;
	};
	std::optional<Cell> cell(v3s16 p);

	Map &m_map;
	const NodeDefManager &m_ndef;
	std::function<void(v3s16)> m_changed;
	bool m_predict;
	std::function<void(const std::vector<v3s16> &)> m_settled;
	std::vector<Region> m_regions;
	std::map<u64, v3f> m_last; // where each presser last pressed
	float m_behind = 0;
};

} // namespace pile
