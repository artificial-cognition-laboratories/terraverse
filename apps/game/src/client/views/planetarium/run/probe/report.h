// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/planetarium/run/probe/record.h"
#include <string>
#include <vector>

namespace Json { class Value; }

namespace planetarium::probe {

// What a probe ran on, beside its steps
struct Header
{
	std::string code, script;
	float cell;              // pixels a tile's cell may span (OrbitCamera::cell)
	unsigned width, height;  // the picture's
	unsigned cores;          // the process's (terragen::cores: fewer when pinned)
};

// The run as JSON: the header, each step (its frames' times summarised,
// every thread's zones, the counters), and where making the world spent its
// time (terragen::making). `bun perf:planetarium` reads it.
Json::Value report(const Header &header, const std::vector<StepRecord> &steps);

} // namespace planetarium::probe
