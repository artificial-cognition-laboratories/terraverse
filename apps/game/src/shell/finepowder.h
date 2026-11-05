// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sandbox/stage.h"
#include <map>
#include <vector>

namespace shell {

/*
	A scene's powder as sixteenth columns: what rests in each block's stack is
	eased towards, and the surface between block centres sloped, stopping at
	walls. As with FineWater, the matter is the truth and this is the picture of
	it: nothing goes back. What's on its way down isn't part of a surface:
	falling() says where it is, for the scene's grains.
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
	struct Span
	{
		float target = 0, shown = 0; // sixteenths over its base
		int substance = -1;
		double temperature = 0;
	};
	float surface(int nx, int nz, float level) const;
	bool closed(int nx, int ny, int nz) const { return m_closed[((size_t)ny * m_z + nz) * m_x + nx]; }
	int32_t m_x = 0, m_y = 0, m_z = 0;
	std::vector<bool> m_closed; // by cell: something a powder stands on or leans against
	std::vector<std::map<int32_t, Span>> m_spans; // by node, by base
	std::vector<Column> m_columns;
	std::vector<Falling> m_falling;
};

} // namespace shell
