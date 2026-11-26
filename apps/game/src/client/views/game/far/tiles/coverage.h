// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace far {

/*
	Which map blocks the near world draws: the far view's boundary. Each map
	block column (x, z in blocks) holds the blocks of it the near world has
	(a mesh, or air, which needs none), each on its own: a block between two
	it has may still be waiting for its mesh, and the far land shows there
	till it isn't. The far view draws everything else (far/mask.h). Made by ClientMap as it picks what to
	draw, with the same range test, so the two always agree on a block: the
	edge is the blocks' own square one, never a circle cut through them.

	A block in range that isn't meshed yet isn't the near world's: the far
	land shows there until it is.
*/
class Coverage
{
public:
	void clear() { m_columns.clear(); }
	void add(v3s16 block);
	// The draw list is made again: what's here is the near world's now.
	void done() { m_generation++; }

	const std::unordered_map<v2s16, std::vector<s16>> &columns() const { return m_columns; }
	// Changes each time it's made again.
	uint64_t generation() const { return m_generation; }

private:
	std::unordered_map<v2s16, std::vector<s16>> m_columns; // each column's blocks (y)
	uint64_t m_generation = 0;
};

} // namespace far
