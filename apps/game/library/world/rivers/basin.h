// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

/*
	A basin behind an obstruction (planning/sprint-115-rivers.md, phase 5):
	from a pool of water, how much it holds at each level and where it spills.
	A priority flood over the columns round it, lowest first: each column
	is reached at the least level that gets water to it (its own top, or the
	lowest pass on the way), and the first column whose top is under the
	pool's own level is a way out: the flood's level then is the spill. So a
	dam spills over its crest, or round it by a lower saddle; a pool with a
	lower river beside it (a river's next step down) spills at once.

	Pure: columns in blocks, their tops from a function (the map's where
	it's loaded or built on, terragen's elsewhere).
*/
namespace rivers {

class Basin
{
public:
	// What stands at a column: the first open block over its ground or its
	// water (water's surface where there is any)
	// (UNKNOWN where it can't be told, a column not loaded: the flood goes
	// no further that way, and the basin is `capped`: it may spill there)
	using Top = std::function<int32_t(int32_t x, int32_t z)>;
	static constexpr int32_t UNKNOWN = INT32_MAX;

	// The pool at (x, z), its water standing up to `level` (blocks under it
	// full); `most` columns looked at before giving up (then it spills at the
	// level reached: `capped`, as one that met columns it couldn't tell).
	static Basin find(int32_t x, int32_t z, int32_t level, const Top &top, size_t most);
	// ... a dry hollow's (a stream filling it): its way out is also where its
	// water meets other water (`water`: a river's, a lake's, the sea's at a
	// column) at its level, not only ground lower than its floor
	using Water = std::function<bool(int32_t x, int32_t z, int32_t top)>;
	static Basin find(int32_t x, int32_t z, int32_t level, const Top &top, size_t most, const Water &water);
	// A pool's water risen to `height` over its banks (a flood): the dry
	// columns it reaches, from its own (at `level`) through ones whose tops
	// are at its level or over it and under `height`. Lower ground beside
	// isn't its: another pool's, or where the flood runs off. Its spill is `height`.
	static Basin spread(int32_t x, int32_t z, int32_t level, int32_t height, const Top &top, size_t most);

	struct Column
	{
		int32_t x, z;
		int32_t top;   // its ground's or water's top as it was
		int32_t enter; // the least level that reaches it
	};

	int32_t level() const { return m_level; } // as it stood
	int32_t spill() const { return m_spill; } // the level it pours out at
	bool dammed() const { return m_spill > m_level; }
	bool capped() const { return m_capped; }
	// the column it pours out by (outside the basin), when it isn't capped
	int32_t outX() const { return m_out_x; }
	int32_t outZ() const { return m_out_z; }
	// every column it covers up to its spill, in the order the water reaches them
	const std::vector<Column> &columns() const { return m_columns; }
	// its ways out: the columns of its crest (a dam's top, a saddle, a notch)
	// with the ground beside them falling away: it pours out over these
	struct Rim
	{
		int32_t x, z, top;
	};
	const std::vector<Rim> &rim() const { return m_rim; }
	// m³/s it pours out at `level` over its ways out, each a weir a block
	// wide (Q = C · (level − its crest)^1.5): a lake settles where this is what
	// comes in, so a narrow notch holds it higher than a wide one, and a
	// spillway cut under a dam's crest takes its water before the crest
	double outflow(double level) const;
	static constexpr double WEIR = 1.7; // C, m^½/s: a broad-crested weir's

	// Risen over its spill to `level`: it covers what that reaches from it
	// across ground at its crest or over it (never down the far side, under
	// its crest: there its water runs off), and its ways out are what it then
	// holds beside ground falling away under its crest. `most` columns at most.
	void rise(double level, const Top &top, size_t most);

	// m³ (a block is a metre) more than as it stood, at `level` (to its spill)
	double volume(double level) const;
	double area(double level) const; // m², its surface at `level` (what it rises over from there)
	// the level holding `volume` m³ more than as it stood, up to its spill
	double levelOf(double volume) const;

private:
	int32_t m_level = 0, m_spill = 0, m_out_x = 0, m_out_z = 0;
	bool m_capped = false;
	std::vector<Column> m_columns;
	std::vector<Rim> m_rim;
};

} // namespace rivers
