// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "knap.h"
#include "small.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <stdexcept>
#include <tuple>

namespace workshop {

namespace {

constexpr int ROUND = 3;              // cells: how far round a cell is looked at to judge it
constexpr float FLAKES_BELOW = 0.42f; // how full round it (a flat face is about half): nearer an edge than this, it flakes
constexpr float SHARP_BELOW = 0.26f;  // and an edge is this thin
constexpr float FINE_BELOW = 0.21f;   // on average, along a fine one
constexpr int ROUGH_LONG = 6, FINE_LONG = 12; // cells along it: about 5 and 10 cm
constexpr float MOST_SCOOP = 10, LEAST_SCOOP = 2; // cells across, half
constexpr float SCOOP_A_KILO = 6;
constexpr float SPLIT_A_KILO = 0.04f; // a chance a blow, by the striker's weight
constexpr size_t SPLITS_OVER = 200;   // cells: a little one just loses flakes
constexpr size_t LEAST_KEPT = 27;     // cells: smaller than this are chips, and gone

const v3s32 SIDES[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

// A part's cells on a box just round them.
class Grid
{
public:
	explicit Grid(const piece::Piece &p)
	{
		std::vector<v3s32> cells;
		p.each([&](v3s16 n, MapNode node, const shape::Shape *s) {
			material = node.getContent();
			for (s16 x = 0; x < shape::STEPS; x++)
				for (s16 y = 0; y < shape::STEPS; y++)
					for (s16 z = 0; z < shape::STEPS; z++)
						if (!s || s->has({ x, y, z }))
							cells.emplace_back(n.X * shape::STEPS + x, n.Y * shape::STEPS + y, n.Z * shape::STEPS + z);
		});
		if (cells.empty())
			return;
		m_lo = m_hi = cells[0];
		for (const v3s32 &c : cells)
			for (int a = 0; a < 3; a++) {
				m_lo[a] = std::min(m_lo[a], c[a]);
				m_hi[a] = std::max(m_hi[a], c[a]);
			}
		m_filled.assign(size_t(m_hi.X - m_lo.X + 1) * (m_hi.Y - m_lo.Y + 1) * (m_hi.Z - m_lo.Z + 1), 0);
		for (const v3s32 &c : cells)
			m_filled[index(c)] = 1;
	}
	bool has(v3s32 c) const { return inside(c) && m_filled[index(c)]; }
	void clear(v3s32 c)
	{
		if (inside(c))
			m_filled[index(c)] = 0;
	}
	template <class F>
	void each(const F &f) const
	{
		if (m_filled.empty())
			return;
		for (s32 x = m_lo.X; x <= m_hi.X; x++)
			for (s32 y = m_lo.Y; y <= m_hi.Y; y++)
				for (s32 z = m_lo.Z; z <= m_hi.Z; z++)
					if (m_filled[index({ x, y, z })])
						f(v3s32(x, y, z));
	}
	// How full the ball round a cell is.
	float fullness(v3s32 c) const
	{
		int filled = 0, all = 0;
		for (s32 x = -ROUND; x <= ROUND; x++)
			for (s32 y = -ROUND; y <= ROUND; y++)
				for (s32 z = -ROUND; z <= ROUND; z++)
					if (x * x + y * y + z * z <= ROUND * ROUND) {
						all++;
						filled += has(c + v3s32(x, y, z));
					}
		return (float)filled / all;
	}
	bool onSurface(v3s32 c) const
	{
		for (const v3s32 &d : SIDES)
			if (!has(c + d))
				return true;
		return false;
	}
	content_t material = CONTENT_AIR;

private:
	bool inside(v3s32 c) const
	{
		return !m_filled.empty() && c.X >= m_lo.X && c.Y >= m_lo.Y && c.Z >= m_lo.Z && c.X <= m_hi.X && c.Y <= m_hi.Y && c.Z <= m_hi.Z;
	}
	size_t index(v3s32 c) const
	{
		return (size_t(c.Y - m_lo.Y) * (m_hi.Z - m_lo.Z + 1) + (c.Z - m_lo.Z)) * (m_hi.X - m_lo.X + 1) + (c.X - m_lo.X);
	}
	v3s32 m_lo, m_hi;
	std::vector<u8> m_filled;
};

struct Run
{
	std::vector<v3s32> cells;
	float thin = 0; // average fullness
	int along = 0;  // cells, its longest way
};

// Sharp cells, joined up into runs: an edge has length; a lone corner doesn't.
std::vector<Run> runsOf(const Grid &g)
{
	std::map<std::tuple<s32, s32, s32>, float> sharp;
	g.each([&](v3s32 c) {
		if (!g.onSurface(c))
			return;
		const float f = g.fullness(c);
		if (f < SHARP_BELOW)
			sharp[{ c.X, c.Y, c.Z }] = f;
	});
	std::vector<Run> runs;
	while (!sharp.empty()) {
		Run run;
		std::vector<std::tuple<s32, s32, s32>> todo{ sharp.begin()->first };
		float total = sharp.begin()->second;
		sharp.erase(sharp.begin());
		v3s32 least, most;
		bool first = true;
		while (!todo.empty()) {
			const auto [x, y, z] = todo.back();
			todo.pop_back();
			const v3s32 c(x, y, z);
			run.cells.push_back(c);
			if (first) {
				least = most = c;
				first = false;
			}
			for (int a = 0; a < 3; a++) {
				least[a] = std::min(least[a], c[a]);
				most[a] = std::max(most[a], c[a]);
			}
			for (s32 dx = -1; dx <= 1; dx++)
				for (s32 dy = -1; dy <= 1; dy++)
					for (s32 dz = -1; dz <= 1; dz++) {
						const auto it = sharp.find({ x + dx, y + dy, z + dz });
						if (it == sharp.end())
							continue;
						total += it->second;
						todo.push_back(it->first);
						sharp.erase(it);
					}
		}
		run.thin = total / run.cells.size();
		run.along = std::max({ most.X - least.X, most.Y - least.Y, most.Z - least.Z }) + 1;
		runs.push_back(std::move(run));
	}
	return runs;
}

const piece::Piece &onePart(const piece::Machine &m)
{
	if (m.parts().size() != 1)
		throw std::invalid_argument("knapping: not a thing of one part");
	return m.parts()[0].object.built();
}

} // namespace

Struck strike(const piece::Machine &m, v3s32 cell, v3f in, float striker_kg, float clean, u32 seed, IGameDef *gamedef)
{
	Grid g(onePart(m));
	if (in.getLengthSQ() < 1e-6f || !(striker_kg > 0))
		throw std::invalid_argument("knapping: no blow");
	in.normalize();
	clean = std::clamp(clean, 0.f, 1.f);
	Struck out;
	// (it's aimed by the client, a cell off is nothing to refuse: a little further in)
	for (int i = 0; i < 4 && !g.has(cell); i++) {
		const v3f next = v3f(cell.X, cell.Y, cell.Z) + in * (i + 1);
		cell = v3s32(std::lround(next.X), std::lround(next.Y), std::lround(next.Z));
	}
	if (!g.has(cell) || g.fullness(cell) >= FLAKES_BELOW)
		return out; // a thud

	std::mt19937 random(seed);
	std::uniform_real_distribution<float> unit(0.f, 1.f);
	const float scoop = std::clamp(SCOOP_A_KILO * std::cbrt(striker_kg), LEAST_SCOOP, MOST_SCOOP);
	// a flake runs on along the blow and is thin across it, peeling the face it
	// runs under: why striking a rim, both ways, thins it to an edge
	const float runs = scoop * 2, thick = std::max(1.5f, scoop * 0.5f);
	std::vector<v3s32> cleared;
	size_t cells = 0;
	g.each([&](v3s32 c) {
		cells++;
		const v3f d((f32)(c.X - cell.X), (f32)(c.Y - cell.Y), (f32)(c.Z - cell.Z));
		const float along = d.dotProduct(in);
		const float across = (d - in * along).getLength();
		// what doesn't break clean breaks raggedly: its scoop's edge wanders
		const float rag = 1 + (1 - clean) * (unit(random) - 0.5f);
		const float a = along / (runs * rag), b = across / (thick * rag);
		if (along >= -1 && a * a + b * b <= 1)
			cleared.push_back(c);
	});
	for (const v3s32 &c : cleared)
		g.clear(c);
	out.removed = cleared.size();

	// too hard a blow, and it splits across, through where it was hit
	if (cells > SPLITS_OVER && unit(random) < SPLIT_A_KILO * striker_kg * (1.5f - clean)) {
		v3f across = in.crossProduct(v3f(unit(random) - 0.5f, unit(random) - 0.5f, unit(random) - 0.5f));
		if (across.getLengthSQ() > 1e-4f) {
			across.normalize();
			std::vector<v3s32> crack;
			g.each([&](v3s32 c) {
				if (std::fabs(v3f((f32)(c.X - cell.X), (f32)(c.Y - cell.Y), (f32)(c.Z - cell.Z)).dotProduct(across)) < 0.7f)
					crack.push_back(c);
			});
			for (const v3s32 &c : crack)
				g.clear(c);
		}
	}

	// what's still in one piece is a thing; what's small is chips
	std::vector<std::vector<v3s32>> pieces;
	std::vector<v3s32> all;
	g.each([&](v3s32 c) { all.push_back(c); });
	for (const v3s32 &start : all) {
		if (!g.has(start))
			continue;
		std::vector<v3s32> todo{ start }, piece;
		g.clear(start);
		while (!todo.empty()) {
			const v3s32 c = todo.back();
			todo.pop_back();
			piece.push_back(c);
			for (const v3s32 &d : SIDES)
				if (g.has(c + d)) {
					g.clear(c + d);
					todo.push_back(c + d);
				}
		}
		if (piece.size() >= LEAST_KEPT)
			pieces.push_back(std::move(piece));
	}
	std::sort(pieces.begin(), pieces.end(), [](const auto &a, const auto &b) { return a.size() > b.size(); });
	for (const std::vector<v3s32> &p : pieces)
		out.left.push_back(smallOfCells(gamedef, g.material, p));
	return out;
}

int edgeOf(const piece::Machine &m)
{
	int best = 0;
	for (const Run &r : runsOf(Grid(onePart(m)))) {
		if (r.along >= FINE_LONG && r.thin < FINE_BELOW)
			best = 2;
		else if (r.along >= ROUGH_LONG)
			best = std::max(best, 1);
	}
	return best;
}

std::vector<v3s32> edgeCells(const piece::Piece &built)
{
	std::vector<v3s32> out;
	for (const Run &r : runsOf(Grid(built)))
		if (r.along >= ROUGH_LONG)
			out.insert(out.end(), r.cells.begin(), r.cells.end());
	return out;
}

} // namespace workshop
