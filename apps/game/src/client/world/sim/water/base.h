// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"

namespace liquids {

class Grid;

/*
	The water's base (sprint 122): where each column's water stands at rest,
	as the server has it. A tile's columns' targets come from its nodes' (a
	field through their corners, held::), and each column's level eases to
	its target: a river risen, a fill poured or a level found again as the
	neighbours come in is seen rising, never jumping, and the disturbance on
	it rides along untouched.
*/
class Base
{
public:
	struct Opts
	{
		Grid &grid;
	};
	explicit Base(Opts o);

	// Tiles marked to be found again: their columns' targets, kinds and
	// currents from their nodes'. A tile first seen is put at its targets.
	void retarget();
	// Levels eased toward their targets, `dtime` s on
	void ease(float dtime);
	// Tiles' targets found again, ever (the debugger's: how often levels move)
	u64 retargeted() const { return m_retargeted; }

private:
	void retargetTile(int t);

	Grid &m_grid;
	u64 m_retargeted = 0;
};

} // namespace liquids
