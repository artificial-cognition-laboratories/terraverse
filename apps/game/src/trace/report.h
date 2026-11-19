// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "trace.h"
#include <string>

/*
	The session's performance, kept from its first frame to its last (the
	debugger's ring only holds the last twenty seconds): how long frames took,
	where each thread's time went, the worst frames whole, and the counters'
	peaks. Written as Markdown, to be read after a run, by a person or an agent:
	the spikes are there even if nobody was watching when they happened.
*/
namespace trace::report {

// Each frame as trace::frame() ends it (trace.cpp calls it).
void add(const Frame &frame);

// The report so far, as Markdown.
std::string markdown();

// Written to `path` (its folder made); false if it couldn't be.
bool write(const std::string &path);

// Once a frame: `dir`/last.md rewritten each minute, so a crash still leaves one.
void tick(const std::string &dir);
// The session's end: `dir`/last.md, and a copy named by when it ended (the
// newest 20 kept).
void finish(const std::string &dir);

} // namespace trace::report
