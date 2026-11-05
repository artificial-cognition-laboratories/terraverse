// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes_bloated.h"
#include "sandbox/stage.h"
#include <SColor.h>
#include <vector>

namespace shell {

/*
	A scene's gas and flames: small cubes that rise and fade. As with FineWater,
	the matter is the truth and this is the picture of it: each block keeps as
	many puffs as its gas calls for, a flame burns only where a reaction with a
	flame runs, and nothing goes back.
*/
class Plume
{
public:
	struct Puff
	{
		v3f at; // nodes from the stage's corner
		float age, life, start, wander, opacity;
		u32 tint; // 0xrrggbb
		bool burning;
		bool flame() const { return burning; }
		float size() const;
		video::SColor colour() const;
	};
	// The stage as it stands; each substance's colour, and which is water (its gas is white steam).
	void sync(int32_t x, int32_t y, int32_t z, const std::vector<sandbox::Stage::Cell> &cells, const std::vector<sandbox::StageBlock> &blocks,
			const std::vector<video::SColor> &substances, const std::vector<bool> &water);
	void step(float dt);
	const std::vector<Puff> &puffs() const { return m_puffs; } // oldest first within gas, then flames

private:
	float random();
	int cellAt(v3f at) const; // −1: outside
	int m_x = 0, m_y = 0, m_z = 0;
	struct Source
	{
		v3f corner;
		v3f lo, hi;            // where in its block it comes off
		float want;            // gas: puffs wanted; flame: a second
		float rise;
		u32 tint;
		float opacity;
	};
	std::vector<Source> m_gas, m_flames;
	std::vector<float> m_rise;   // by cell: how fast its gas climbs
	std::vector<bool> m_closed;  // by cell: solid
	std::vector<int> m_counts;   // by cell
	std::vector<float> m_wanted; // by cell
	std::vector<Puff> m_puffs;
	u32 m_seed = 1;
};

} // namespace shell
