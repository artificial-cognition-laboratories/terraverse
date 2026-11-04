// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "mapnode.h"
#include "shape/shape.h"
#include <functional>
#include <map>

class Map;
class NodeDefManager;

namespace workshop {

/*
	Loose stuff laid on the world, a sixteenth at a time: each falls down its
	column to what's there and stacks, into the node's open space as cut shapes.
	Nothing's written till finish, so a heap's one edit a node, not thousands.
*/
class Laying
{
public:
	// `place`: a node put where there was none (the world's lights it; a Piece's just sets it).
	Laying(Map &map, const NodeDefManager *ndef, std::function<void(v3s16, MapNode)> place);

	// A sixteenth (the world's, counted from node 0's least) of `material`, let
	// fall from there. False: there was nowhere to put it.
	bool drop(v3s32 at, content_t material);
	// Whatever's laid written to the world, for at most `ms` this time: true when it's all done.
	bool finish(float ms = 1e9f);

private:
	struct Node
	{
		content_t content;  // CONTENT_AIR: nothing there yet
		shape::Shape shape;
		bool solid;         // nothing goes in it: liquid, or not loaded
		bool changed = false;
	};
	Node &node(v3s16 p);

	Map &m_map;
	const NodeDefManager *m_ndef;
	std::function<void(v3s16, MapNode)> m_place;
	struct Less
	{
		bool operator()(const v3s16 &a, const v3s16 &b) const
		{
			return a.X != b.X ? a.X < b.X : a.Y != b.Y ? a.Y < b.Y : a.Z < b.Z;
		}
	};
	std::map<v3s16, Node, Less> m_nodes;
	// where the next one in each column lands: a heap's thousands don't each search it
	std::map<std::pair<s32, s32>, s32> m_tops;
};

} // namespace workshop
