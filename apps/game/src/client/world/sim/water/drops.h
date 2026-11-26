// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/types.h"
#include <random>
#include <vector>
#include <CMeshBuffer.h>

class ClientMap;
class NodeDefManager;
namespace video { class IVideoDriver; }

namespace liquids {

class Grid;

/*
	Liquid poured, as sixteenth-cube drops (sprint 82): each falls and lands
	in the grid's water, or on whatever's under it, where it's gone (the
	server's fill shows it).
*/
class Drops
{
public:
	struct Opts
	{
		const NodeDefManager &ndef;
		Grid &grid;
		bool grid_runs; // the grid's run here (not without GL 3): drops land in it
	};
	explicit Drops(Opts o);
	~Drops();
	Drops(const Drops &) = delete;
	Drops &operator=(const Drops &) = delete;

	// A drop a cup, thrown to land where the server pours it
	void pour(const std::vector<Stream> &streams);
	void fall(ClientMap &map, float dtime);
	// Their mesh, in nodes from the scene's origin
	void build(ClientMap &map, v3s16 offset, u32 daynight);
	void render(video::IVideoDriver *driver);

private:
	struct Drop
	{
		v3f pos, vel; // nodes
		float cups;
		float age;
		u8 r, g, b;
		bool splash; // thrown up by another: it throws no more
	};
	int land(Drop &d, std::vector<Drop> &thrown); // −1: not over the grid, 0: still falling, 1: landed
	bool landsOutside(ClientMap &map, const Drop &d) const;

	const NodeDefManager &m_ndef;
	Grid &m_grid;
	const bool m_grid_runs;
	std::vector<Drop> m_drops;
	scene::SMeshBuffer *m_mesh = nullptr; // owned (a mesh buffer counts its own references)
	std::mt19937 m_random{ 82 };
};

} // namespace liquids
