// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/types.h"
#include <unordered_map>
#include <vector>

namespace liquids {

class Grid;

/*
	Runs the grid's water as shallow water by virtual pipes (shallow.h), only
	its awake tiles, and lets what moves through it push it aside.
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

	Grid &m_grid;
	std::unordered_map<u32, v3f> m_movers; // where each was last frame
};

} // namespace liquids
