// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/base/parallel.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace terragen {

namespace {

// Steepness toward a neighbour is drop ÷ distance; compared as drop · SPAN ÷
// distance (fixed, unit sphere) so a one-unit drop never rounds away.
const int64_t SPAN = (int64_t)1 << 24;

// Union-find over basins, by size, with path halving.
struct Sets
{
	std::vector<int32_t> parent, size;
	explicit Sets(size_t n) : parent(n), size(n, 1)
	{
		for (size_t i = 0; i < n; i++)
			parent[i] = (int32_t)i;
	}
	int32_t find(int32_t a)
	{
		while (parent[a] != a)
			a = parent[a] = parent[parent[a]];
		return a;
	}
	bool join(int32_t a, int32_t b)
	{
		a = find(a);
		b = find(b);
		if (a == b)
			return false;
		if (size[a] < size[b])
			std::swap(a, b);
		parent[b] = a;
		size[a] += size[b];
		return true;
	}
};

} // namespace

std::vector<int32_t> receivers(const std::vector<fix> &surface)
{
	const Sphere &sphere = gridSphere();
	std::vector<int32_t> out(surface.size(), -1);
	parallelFor((int32_t)surface.size(), [&](int32_t i) {
		if (surface[i] < 0)
			return;
		int64_t steepest = 0;
		const fix *gap = sphere.distancesBegin(i);
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, gap++) {
			// (water runs down to the sea's surface, not its floor: how deep
			// the sea is off a coast never picks which way the coast drains)
			const int64_t steep = (int64_t)(surface[i] - std::max<fix>(surface[*p], 0)) * SPAN / *gap;
			if (steep > steepest) {
				steepest = steep;
				out[i] = *p;
			}
		}
	});
	return out;
}

std::vector<int32_t> upstreamOrder(const std::vector<int32_t> &receiver)
{
	size_t n = receiver.size();
	std::vector<int32_t> start(n + 1, 0), donors(n);
	for (int32_t r : receiver)
		if (r >= 0)
			start[r + 1]++;
	for (size_t i = 0; i < n; i++)
		start[i + 1] += start[i];
	std::vector<int32_t> fillpos(start.begin(), start.end() - 1);
	for (int32_t i = 0; i < (int32_t)n; i++)
		if (receiver[i] >= 0)
			donors[fillpos[receiver[i]]++] = i;

	std::vector<int32_t> order;
	order.reserve(n);
	for (int32_t i = 0; i < (int32_t)n; i++)
		if (receiver[i] < 0)
			order.push_back(i);
	for (size_t head = 0; head < order.size(); head++) {
		int32_t c = order[head];
		for (int32_t d = start[c]; d < start[c + 1]; d++)
			order.push_back(donors[d]);
	}
	return order;
}

std::vector<int64_t> accumulate(const std::vector<int32_t> &receiver, const std::vector<int32_t> &order,
		const std::vector<fix> &rain)
{
	const Sphere &sphere = gridSphere();
	std::vector<int64_t> area(receiver.size());
	for (size_t i = 0; i < area.size(); i++)
		area[i] = mul(rain[i], sphere.area((int32_t)i));
	for (size_t k = order.size(); k-- > 0;) {
		int32_t c = order[k];
		if (receiver[c] >= 0)
			area[receiver[c]] += area[c];
	}
	return area;
}

Routing route(const std::vector<fix> &h, const Balance *balance)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = (int32_t)h.size();
	Routing out;
	out.receiver = receivers(h);

	// Basins: every cell takes its root's (the sea's cells, and the land's
	// pits). The sea is one basin, 0; with no sea, the lowest pit's is.
	int32_t lowest = 0;
	bool sea = false;
	for (int32_t i = 0; i < n; i++) {
		sea = sea || h[i] < 0;
		if (h[i] < h[lowest])
			lowest = i;
	}
	std::vector<int32_t> basin(n, -1), pit(1, sea ? -1 : lowest);
	{
		const std::vector<int32_t> order = upstreamOrder(out.receiver);
		for (int32_t c : order) {
			if (out.receiver[c] >= 0)
				basin[c] = basin[out.receiver[c]];
			else if (h[c] < 0 || c == pit[0])
				basin[c] = 0;
			else {
				basin[c] = (int32_t)pit.size();
				pit.push_back(c);
			}
		}
	}
	const int32_t basins = (int32_t)pit.size();

	// The passes: each edge between two basins, at the higher of its two
	// cells. Kruskal's tree over them, lowest first, joins every basin to the
	// sea by the lowest way out it has. A cell high on a divide is the pass
	// for every edge it has across it: of those, the one falling furthest on
	// its other side (then by the cells, so it's the same on every machine).
	struct Pass
	{
		fix level, low; // its higher cell's height, and its lower's
		int32_t a, b;
	};
	std::vector<Pass> passes;
	for (int32_t i = 0; i < n; i++)
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			if (*p > i && basin[*p] != basin[i])
				passes.push_back({ std::max(h[i], h[*p]), std::min(h[i], h[*p]), i, *p });
	std::sort(passes.begin(), passes.end(), [](const Pass &x, const Pass &y) {
		return std::tie(x.level, x.low, x.a, x.b) < std::tie(y.level, y.low, y.a, y.b);
	});
	std::vector<std::vector<int32_t>> tree(basins); // the passes kept, by basin
	{
		Sets sets(basins);
		std::vector<Pass> kept;
		for (const Pass &q : passes)
			if (sets.join(basin[q.a], basin[q.b])) {
				tree[basin[q.a]].push_back((int32_t)kept.size());
				tree[basin[q.b]].push_back((int32_t)kept.size());
				kept.push_back(q);
				if ((int32_t)kept.size() == basins - 1)
					break;
			}
		passes.swap(kept);
	}

	// Out from the sea along the tree: each basin spills into the one it was
	// reached from, over the pass that joins them
	std::vector<int32_t> parent(basins, -1), link(basins, -1), queue{ 0 };
	std::vector<uint8_t> reached(basins, 0);
	reached[0] = 1;
	for (size_t head = 0; head < queue.size(); head++) {
		const int32_t b = queue[head];
		for (int32_t k : tree[b]) {
			const Pass &q = passes[k];
			const int32_t child = basin[basin[q.a] == b ? q.b : q.a];
			if (reached[child])
				continue;
			reached[child] = 1;
			queue.push_back(child);
			parent[child] = b;
			link[child] = k;
		}
	}
	if ((int32_t)queue.size() != basins)
		throw std::logic_error("route: a basin the tree never reached");

	// With a water balance, from the tree's leaves in: what reaches each
	// basin (its own runoff and what spills from those upstream) fills its
	// lake from the pit up, a cell at a time, its open water giving back what
	// evaporates. If that matches what comes in before the lake reaches the
	// pass, the basin is closed: everything drains in, nothing out (the
	// Caspian's, the Great Basin's, Tibet's lakes), and the lake stands
	// where it balances. Otherwise what's left spills on.
	std::vector<uint8_t> closed(basins, 0);
	std::vector<fix> level(basins, INT32_MIN);
	if (balance) {
		std::vector<std::vector<int32_t>> cells(basins);
		std::vector<int64_t> inflow(basins, 0);
		for (int32_t i = 0; i < n; i++)
			if (basin[i] > 0) {
				cells[basin[i]].push_back(i);
				inflow[basin[i]] += mul(balance->runoff[i], sphere.area(i));
			}
		for (size_t k = queue.size(); k-- > 1;) {
			const int32_t b = queue[k];
			std::vector<int32_t> &by = cells[b];
			std::sort(by.begin(), by.end(), [&](int32_t x, int32_t y) { return std::tie(h[x], x) < std::tie(h[y], y); });
			const fix pass = passes[link[b]].level;
			int64_t gives = 0;
			for (int32_t c : by) {
				if (h[c] >= pass)
					break;
				gives += mul(balance->evaporation[c], sphere.area(c));
				if (gives >= inflow[b]) {
					closed[b] = 1;
					level[b] = h[c];
					break;
				}
			}
			if (!closed[b])
				inflow[parent[b]] += inflow[b] - gives;
		}
	}

	// Each basin that spills has its pit's path up to the pass reversed, so
	// the pit drains out over it; its lake stands at the pass, or at the lake
	// downstream if that's higher (one lake over both). A closed one's pit
	// stays where its water ends, under the lake downstream if that rises over its pass.
	for (size_t k = 1; k < queue.size(); k++) {
		const int32_t b = queue[k];
		const Pass &q = passes[link[b]];
		if (closed[b]) {
			if (level[parent[b]] > q.level)
				level[b] = std::max(level[b], level[parent[b]]);
			continue;
		}
		const bool from_a = basin[q.a] == parent[b];
		level[b] = std::max(q.level, level[parent[b]]);
		int32_t c = from_a ? q.b : q.a, to = from_a ? q.a : q.b;
		while (c >= 0) {
			const int32_t next = out.receiver[c];
			out.receiver[c] = to;
			to = c;
			c = next;
		}
	}

	out.order = upstreamOrder(out.receiver);
	out.surface.resize(n);
	for (int32_t i = 0; i < n; i++)
		out.surface[i] = std::max(h[i], level[basin[i]]);
	return out;
}

} // namespace terragen
