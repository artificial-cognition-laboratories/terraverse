// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/pump.h"
#include <include/cef_app.h>
#include <chrono>
#include <climits>

namespace shell {

namespace {
int64_t nowMs();
} // namespace

// Chromium may never ask until the first work is done, and it's cheap to do once a frame
// anyway: due from the start, and at the latest every 1/30 s (CEF's own sample's ceiling).
constexpr int64_t CEILING_MS = 33;

Pump::Pump() : m_due(0) {}

void Pump::schedule(int64_t delay_ms)
{
	const int64_t due = nowMs() + (delay_ms > CEILING_MS ? CEILING_MS : delay_ms);
	int64_t was = m_due.load();
	while (due < was && !m_due.compare_exchange_weak(was, due)) {}
}

void Pump::work()
{
	if (!EXTERNAL || m_working)
		return;
	if (nowMs() < m_due.load())
		return;
	m_due.store(nowMs() + CEILING_MS);
	m_working = true;
	CefDoMessageLoopWork();
	m_working = false;
}

namespace {

int64_t nowMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

} // namespace shell
