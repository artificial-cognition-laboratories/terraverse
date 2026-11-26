// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/tiles/budget.h"

namespace far {

namespace {

// ms a frame for each kind of work (a frame at 60 fps is 16.7), by Budget::Work
constexpr double LIMIT_MS[] = {
	1.5, // Install: at 3, installs ran to 10 ms a frame
	2,   // Paint
};
static_assert(sizeof(LIMIT_MS) / sizeof(LIMIT_MS[0]) == (size_t)Budget::Work::Count);

constexpr double LEARN = 0.2; // how much the last one moves what one usually costs

} // namespace

void Budget::frame()
{
	for (Kind &k : m_kinds)
		k.spent = 0;
}

bool Budget::room(Work work) const
{
	const Kind &k = m_kinds[(size_t)work];
	return k.spent == 0 || k.spent + k.usual <= LIMIT_MS[(size_t)work];
}

void Budget::spent(Work work, double ms)
{
	Kind &k = m_kinds[(size_t)work];
	k.spent += ms;
	k.usual = k.usual == 0 ? ms : k.usual + (ms - k.usual) * LEARN;
}

} // namespace far
