// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sandbox/stage.h"
#include <map>
#include <vector>

namespace shell {

/*
	A scene's powder as sixteenth columns that avalanche: each floor's heap holds
	what the matter has on that floor, but what lands goes on the heap's top and
	slides to its angle of repose, column by column, with no notion of blocks.
	The matter's own heap moves by the block; this is the picture of it, and
	nothing goes back. What's on its way down isn't part of a heap: falling()
	says where it is, for the scene's grains.
*/
class FinePowder
{
public:
	struct Column
	{
		int x, z;          // sixteenths from the stage's corner
		int32_t base, top; // sixteenths over its floor
		int substance;
		double temperature;
	};
	struct Falling
	{
		sandbox::Pos at;
		int substance;
	};
	// A block that's poured powder: a levelled solid, however full it is.
	static bool isPowder(const sandbox::StageBlock &b) { return b.spec.leveled && b.spec.state == matter::State::Solid; }
	void sync(int32_t x, int32_t y, int32_t z, const std::vector<sandbox::Stage::Cell> &cells, const std::vector<sandbox::StageBlock> &blocks);
	// Whether the columns changed.
	bool step(float dt, float pace);
	const std::vector<Column> &columns() const { return m_columns; }
	const std::vector<Falling> &falling() const { return m_falling; }

private:
	struct Layer // a floor's heap
	{
		std::vector<float> height;       // by column: sixteenths over its floor
		std::vector<float> want, has;    // by node: sixteenths of a whole block
		std::vector<bool> open;          // by node: it has this floor, and room over it
		int substance = -1;
		double temperature = 0;
	};
	void land(Layer &layer, int nx, int nz, float sixteenths);
	void lift(Layer &layer, int nx, int nz, float sixteenths);
	bool slide(Layer &layer) const;
	int32_t m_x = 0, m_y = 0, m_z = 0;
	std::vector<bool> m_closed; // by cell: something a powder stands on or leans against
	std::vector<bool> m_wet;    // by node: liquid has lain here, so what's solid here froze where it lay: no heap
	std::map<int32_t, Layer> m_layers; // by floor
	std::vector<Column> m_columns;
	std::vector<Falling> m_falling;
};

} // namespace shell
