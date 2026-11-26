// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/types.h"
#include <unordered_map>
#include <vector>

namespace liquids {

class Grid;

/*
	Runs the grid's disturbance (sprint 122): how far each column's water is
	off its level, as shallow water by virtual pipes (shallow.h), carried by
	the current and dying back; only its awake tiles. What moves through the
	water pushes it aside.
*/
class Flow
{
public:
	struct Opts
	{
		Grid &grid;
	};
	explicit Flow(Opts o);

	// What moves through water drives the columns it's in at its own speed
	// (a bow wave ahead, a trough behind); falling in throws it outward
	void push(const std::vector<Mover> &movers, float dtime);
	// A step: flows found from the surfaces either side, then moved; a tile
	// still for long enough sleeps (sooner the further from the camera)
	void step(float dt, v3f camera);

	// The step's length (a wave crosses under half a column in one), and
	// how many a frame at most (past it the water runs slow rather than the frame)
	float dt() const;
	int mostSteps() const;

private:
	void pushOne(const Mover &m, v3f v, float dtime);
	void pipes(int t, float dt, float L);
	void move(int t, float dt, float L);
	void carry(const std::vector<int> &awake, float dt);
	void settle(int t, float dt, v3f camera);

	Grid &m_grid;
	std::unordered_map<u32, v3f> m_movers; // where each was last frame
	std::vector<std::pair<int, float>> m_carried; // a step's carried disturbance, by column (kept: not made each step)
};

} // namespace liquids
