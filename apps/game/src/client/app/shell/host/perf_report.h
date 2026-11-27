// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace shell {

/*
	The Perf tab's report: everything the recorder keeps, as a folder under
	`dir` an agent reads with grep, and a person opens in Perfetto:
	summary.md (frame times, the worst frames and what was in them, each
	thread's zones by cost, counters), frames.jsonl (one frame a line),
	spans.jsonl (one zone a line) and trace.json (Chrome's trace format).
	Its path. Throws when it can't be written.
*/
namespace perf_report {

std::string write(const std::string &dir);

} // namespace perf_report

} // namespace shell
