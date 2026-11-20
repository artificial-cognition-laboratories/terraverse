// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/*
	Where making a world spends its time, and how far it's got (sprint 110).
	Each slow piece of work is a Step, recorded against the world code being
	made: its name, how long it took, how much of it is done of how much, and
	whether it was read from what was kept rather than made. Steps run one
	after another; a step's count may be advanced from any thread.

	Whoever's waiting (the create screen, a server opening a world, the perf
	tool) reads making(code) as often as it likes. Nothing here changes what's
	made.
*/
namespace terragen {

struct StepRecord
{
	std::string name;
	double begun = 0;       // when, on clock() (seconds)
	double seconds = 0;     // so far, if it's still running
	int64_t done = 0, of = 0; // of 0: not counted (only running or finished)
	bool kept = false;      // read from what was kept, not made
	bool finished = false;
};

struct Making
{
	std::vector<StepRecord> steps; // in the order they ran
	double seconds = 0;            // since the world was asked for
	bool done = false;             // the world's made (or failed)
};

class Step
{
public:
	// `of`: how many of something it works through (0: not counted)
	Step(const std::string &code, const char *name, int64_t of = 0);
	~Step();
	Step(const Step &) = delete;
	Step &operator=(const Step &) = delete;

	void advance(int64_t by = 1);
	// it was read from what was kept: nothing made
	void kept();

	struct Entry; // a code's steps (progress.cpp)

private:
	std::shared_ptr<Entry> m_entry;
	size_t m_index;
};

// A world's making begins (worldFor): what was recorded of it before is cleared
void beginMaking(const std::string &code);
// and ends: the breakdown is told to the report, if one's set
void endMaking(const std::string &code);
// What's recorded of a code's making (nothing: never asked for, or long ago)
Making making(const std::string &code);
// "made in 31.2 s: plates 1.2 · erosion 6.8 · river levels 14.1 (kept) · …"
std::string breakdown(const Making &m);
// The clock steps are begun on (seconds, steady: only differences mean anything)
double clock();
// Where each world's breakdown is told (the engine's log); none by default
void setMakingReport(std::function<void(const std::string &code, const Making &)> report);

} // namespace terragen
