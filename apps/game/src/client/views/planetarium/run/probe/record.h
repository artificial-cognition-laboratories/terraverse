// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace planetarium::probe {

// One step of a probe, as it went
struct StepRecord
{
	std::string name;
	double seconds = 0;              // the whole step: its move and its wait
	double moving = 0;               // its move
	std::optional<double> settled;   // seconds from the move's end till the land was all there (none: it never was)
	std::vector<double> work;        // each frame's work (ms), the wait for the next frame left out
	// each thread's time in each zone (ms; nested ones in their parents' too),
	// whole: a tile built over three frames counts all its time
	struct Zone
	{
		double ms = 0, max = 0;
		u64 count = 0;
	};
	struct Thread
	{
		unsigned threads = 0; // how many threads of that name there were
		std::map<std::string, Zone> zones;
	};
	std::map<std::string, Thread> threads; // by kind: every "Far tiles" worker together
	// each counter over the step: its frames' values summed, and the largest
	struct Counter
	{
		double sum = 0, max = 0;
	};
	std::map<std::string, Counter> counters;
	bool lost = false; // a thread's ring went round before it was read: its zones are short
	size_t tiles = 0, draws = 0, vertices = 0; // what the land costs to draw at its end
};

/*
	Folds what the trace keeps (library/trace) into a probe's steps: the
	frames' times and counters, and every thread's zones whole. Read once a
	frame, after trace::frame(), so nothing's lost to the rings going round.
*/
class Recorder
{
public:
	Recorder();

	// a step begins now
	void begin(const std::string &name);
	// its move's over, now
	void moved();
	// the land's all there, now (the first frame it is)
	void settled();
	// the frame just marked (trace::frame()), its work in ms
	void frame(double work);
	// the step's over: what's drawn now
	void end(size_t tiles, size_t draws, size_t vertices);
	// what's happened since the last step's end is no step's (work between them, not timed)
	void skip();

	const std::vector<StepRecord> &steps() const { return m_steps; }

private:
	std::vector<StepRecord> m_steps;
	u64 m_frame = 0;   // the last frame read (trace::framesSince)
	u64 m_cursor = 0;  // zones ended up to here are read (trace::now)
	u64 m_began = 0, m_moved = 0; // the step's, trace::now
	void spans(StepRecord &step, u64 to); // the zones ended since the cursor, up to `to`
};

} // namespace planetarium::probe
