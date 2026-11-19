// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "coverage.h"

namespace far {

void Coverage::add(v3s16 block)
{
	m_columns[v2s16(block.X, block.Z)].push_back(block.Y);
}

} // namespace far
