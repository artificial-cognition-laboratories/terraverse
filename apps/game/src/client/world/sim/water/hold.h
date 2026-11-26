// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vector>

namespace liquids {

class Grid;

/*
	Holds the grid's water to the server's. The sea is a reservoir: its
	columns close on its level. A river's or lake's held water (sprint 115)
	closes on its own level, and runs toward its nearest step down, carrying
	what stands over it. Other water is joined into pools, each closed on the
	server's amount for it, and free to move within it.

	TEMP (2026-11-22, sprint 122): rivers as staircases of held levels. Goes
	whole when the grid runs a disturbance over the water's base (phase 5).
*/
class Hold
{
public:
	struct Opts
	{
		Grid &grid;
	};
	explicit Hold(Opts o);

	// A frame: the pools and currents found again now and then, and each
	// pool closed on the server's amount
	void frame(float dtime);
	// A step of the currents: what stands over or under a river's level is
	// carried downstream; the level itself is held, so still water stays as it is
	void carry(float dt);

private:
	void pools();
	void currents();
	void hold(float dtime);
	void holdSea(int t, float rate, float river_rate);
	void add(int t, float by);
	void drain(int t, float keep, bool coast);
	float under(int c) const;

	Grid &m_grid;
	float m_pooled = 0;       // s till the pools are found again
	std::vector<char> m_coast; // per pool: it touches the sea
};

} // namespace liquids
