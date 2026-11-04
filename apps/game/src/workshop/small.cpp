// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "small.h"
#include "piece/piece.h"
#include <cmath>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

namespace workshop {

namespace {

constexpr int MOST_CELLS_LONG = piece::SIDE * shape::STEPS;
constexpr int BREAKS = 7;  // the flat faces a stone has broken along
constexpr f32 MOST_BEND = 0.08f; // of a stick's length, at its middle

// Its grid's corner the least of them, so a long thing fits.
piece::Machine ofCells(IGameDef *gamedef, content_t material, std::vector<v3s32> cells)
{
	if (cells.empty())
		throw std::invalid_argument("small thing: nothing of it");
	v3s32 least = cells[0];
	for (const v3s32 &c : cells)
		for (int a = 0; a < 3; a++)
			least[a] = std::min(least[a], c[a]);
	for (v3s32 &c : cells)
		c -= least;
	return smallOfCells(gamedef, material, cells);
}

} // namespace

piece::Machine smallOfCells(IGameDef *gamedef, content_t material, const std::vector<v3s32> &cells)
{
	std::map<v3s16, shape::Shape> nodes;
	for (const v3s32 &c : cells) {
		const v3s16 node(c.X / shape::STEPS, c.Y / shape::STEPS, c.Z / shape::STEPS);
		if (c.X < 0 || c.Y < 0 || c.Z < 0 || !piece::Piece::area().contains(node))
			throw std::invalid_argument("small thing: off its grid");
		nodes[node].set(v3s16(c.X % shape::STEPS, c.Y % shape::STEPS, c.Z % shape::STEPS), true);
	}
	piece::Object o(gamedef);
	for (const auto &[n, s] : nodes) {
		o.design().map().setNode(n, MapNode(material));
		o.design().map().setShape(n, s);
	}
	o.build();
	piece::Machine m(gamedef);
	m.add({ std::move(o), piece::Pose{ v3s32(0), 0 } });
	m.setScale(SMALL_SCALE);
	return m;
}

piece::Machine stone(IGameDef *gamedef, content_t material, v3s16 half, u32 seed)
{
	if (half.X < 1 || half.Y < 1 || half.Z < 1 || std::max({ half.X, half.Y, half.Z }) * 2 > MOST_CELLS_LONG)
		throw std::invalid_argument("stone: no such size");
	std::mt19937 random(seed);
	std::uniform_real_distribution<f32> unit(-1.f, 1.f), depth(0.55f, 0.95f);
	struct Break { v3f normal; f32 at; };
	std::vector<Break> breaks;
	for (int i = 0; i < BREAKS; i++) {
		v3f n(unit(random), unit(random), unit(random));
		if (n.getLengthSQ() < 1e-4f)
			n = v3f(0, 1, 0);
		breaks.push_back({ n.normalize(), depth(random) });
	}
	std::vector<v3s32> cells;
	for (s32 x = -half.X; x <= half.X; x++)
		for (s32 y = -half.Y; y <= half.Y; y++)
			for (s32 z = -half.Z; z <= half.Z; z++) {
				const v3f p((f32)x / half.X, (f32)y / half.Y, (f32)z / half.Z);
				bool in = p.getLengthSQ() <= 1;
				for (const Break &b : breaks)
					in = in && p.dotProduct(b.normal) <= b.at;
				if (in)
					cells.emplace_back(x, y, z);
			}
	return ofCells(gamedef, material, cells);
}

piece::Machine stick(IGameDef *gamedef, content_t material, s16 radius, s16 length, u32 seed)
{
	if (radius < 1 || length < 2 || length > MOST_CELLS_LONG - 2 * MOST_BEND * MOST_CELLS_LONG - 2 * radius - 2)
		throw std::invalid_argument("stick: no such size");
	std::mt19937 random(seed);
	std::uniform_real_distribution<f32> unit(-1.f, 1.f);
	const v3f bend = v3f(unit(random), 0, unit(random)) * (MOST_BEND * length);
	std::vector<v3s32> cells;
	for (s32 y = 0; y < length; y++) {
		const f32 along = (f32)y / (length - 1);
		// its middle bowed out, as a parabola
		const v3f centre = bend * (4 * along * (1 - along));
		const s32 cx = (s32)std::lround(centre.X), cz = (s32)std::lround(centre.Z);
		for (s32 x = -radius; x <= radius; x++)
			for (s32 z = -radius; z <= radius; z++)
				if (x * x + z * z <= radius * radius)
					cells.emplace_back(cx + x, y, cz + z);
	}
	return ofCells(gamedef, material, cells);
}

} // namespace workshop
