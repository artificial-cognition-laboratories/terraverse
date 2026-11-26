// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/latency.h"
#include "library/trace/trace.h"

namespace shell {

// A move over a still part of the page paints nothing, so the next paint
// may answer something else (an animation, the state stream). Past this an
// input is taken as unanswered, not timed: a stall that long is Chromium's
// (its threads in the debugger's Flame tab) or the engine's verbs ("UI: inbox").
static constexpr u64 STALE_NS = 250'000'000;

// A paint answers the latest input before it began, not the first
// unanswered: moving within a row paints nothing, and the move into the next
// row is what its highlight answers (timed from the first, a slow sweep read
// as 150 ms of lag). Not simply the latest either: a late paint begins
// before the next move, and would go untimed.
void Latency::input(u64 at)
{
	m_inputs.push_back(at);
	while (!m_inputs.empty() && at - m_inputs.front() > STALE_NS)
		m_inputs.pop_front();
}

void Latency::painted(u64 first, u32 paints, double area)
{
	m_paints += paints;
	m_area += area;
	if (paints == 0)
		return;
	u64 answered = 0;
	while (!m_inputs.empty() && m_inputs.front() <= first) {
		answered = m_inputs.front();
		m_inputs.pop_front();
	}
	if (answered == 0 || first - answered > STALE_NS)
		return;
	trace::count("UI: input to paint", (first - answered) / 1000.0, trace::Fold::Max);
	m_answer = answered;
}

void Latency::shown(u64 at)
{
	if (m_answer == 0)
		return;
	trace::count("UI: input to screen", (at - m_answer) / 1000.0, trace::Fold::Max);
	m_answer = 0;
}

void Latency::report()
{
	trace::count("UI: page frames [#]", m_paints, trace::Fold::Sum);
	trace::count("UI: page painted [%]", m_area * 100, trace::Fold::Sum);
	m_paints = 0;
	m_area = 0;
}

} // namespace shell
