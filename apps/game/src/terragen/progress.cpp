// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "progress.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>

namespace terragen {

namespace {

using Clock = std::chrono::steady_clock;

// a step as it runs: its count advanced from any thread
struct Live
{
	StepRecord record;
	Clock::time_point start;
	std::atomic<int64_t> done{ 0 };
	bool past = false; // a making before this one's: not told
};

// A few codes' records, latest first (a world's making is asked after by
// code; older ones are let go)
constexpr size_t KEPT = 8;

} // namespace

struct Step::Entry
{
	std::string code;
	Clock::time_point start = Clock::now();
	bool done = false;
	std::deque<Live> steps; // (a deque: steps' addresses stay put as more begin)
};

namespace {

std::mutex g_lock;
std::deque<std::shared_ptr<Step::Entry>> g_entries;
std::function<void(const std::string &, const Making &)> g_report;

double since(Clock::time_point t)
{
	return std::chrono::duration<double>(Clock::now() - t).count();
}

// (under the lock)
std::shared_ptr<Step::Entry> entryFor(const std::string &code)
{
	for (const auto &e : g_entries)
		if (e->code == code)
			return e;
	g_entries.push_front(std::make_shared<Step::Entry>());
	g_entries.front()->code = code;
	if (g_entries.size() > KEPT)
		g_entries.pop_back();
	return g_entries.front();
}

} // namespace

Step::Step(const std::string &code, const char *name, int64_t of)
{
	std::lock_guard<std::mutex> hold(g_lock);
	m_entry = entryFor(code); // (held: an entry let go while a step runs still takes its end)
	m_index = m_entry->steps.size();
	Live &live = m_entry->steps.emplace_back();
	live.record.name = name;
	live.record.of = of;
	live.start = Clock::now();
	live.record.begun = clock();
}

Step::~Step()
{
	std::lock_guard<std::mutex> hold(g_lock);
	Live &live = m_entry->steps[m_index];
	live.record.seconds = since(live.start);
	live.record.done = live.record.of > 0 ? live.record.of : live.done.load();
	live.record.finished = true;
}

void Step::advance(int64_t by)
{
	// (no lock: the step's own counter)
	m_entry->steps[m_index].done.fetch_add(by, std::memory_order_relaxed);
}

void Step::kept()
{
	std::lock_guard<std::mutex> hold(g_lock);
	m_entry->steps[m_index].record.kept = true;
}

void beginMaking(const std::string &code)
{
	std::lock_guard<std::mutex> hold(g_lock);
	for (auto it = g_entries.begin(); it != g_entries.end(); ++it)
		if ((*it)->code == code) {
			// Steps for it may already be running (whoever's showing it starts
			// its tiles as the world's asked for): they're this making's, so
			// the entry's kept, and only what finished before is let go (a
			// step's place in it can't move under it)
			Step::Entry &e = **it;
			bool running = false;
			for (Live &live : e.steps) {
				if (live.record.finished)
					live.past = true;
				else if (!running || live.start < e.start) {
					e.start = live.start;
					running = true;
				}
			}
			if (running) {
				e.done = false;
				return;
			}
			g_entries.erase(it);
			break;
		}
	entryFor(code);
}

static Making snapshot(const Step::Entry &e)
{
	Making m;
	m.done = e.done;
	m.seconds = since(e.start);
	for (const Live &live : e.steps) {
		if (live.past)
			continue;
		StepRecord r = live.record;
		if (!r.finished) {
			r.seconds = since(live.start);
			r.done = live.done.load(std::memory_order_relaxed);
		}
		m.steps.push_back(r);
	}
	return m;
}

void endMaking(const std::string &code)
{
	Making m;
	std::function<void(const std::string &, const Making &)> report;
	{
		std::lock_guard<std::mutex> hold(g_lock);
		const std::shared_ptr<Step::Entry> e = entryFor(code);
		e->done = true;
		m = snapshot(*e);
		report = g_report;
	}
	if (report)
		report(code, m);
}

Making making(const std::string &code)
{
	std::lock_guard<std::mutex> hold(g_lock);
	for (const auto &e : g_entries)
		if (e->code == code)
			return snapshot(*e);
	return {};
}

std::string breakdown(const Making &m)
{
	char buf[96];
	std::snprintf(buf, sizeof buf, "made in %.1f s:", m.seconds);
	std::string out = buf;
	double counted = 0;
	for (size_t k = 0; k < m.steps.size(); k++) {
		const StepRecord &s = m.steps[k];
		counted += s.seconds;
		std::snprintf(buf, sizeof buf, "%s %s %.2f%s", k ? " ·" : "", s.name.c_str(), s.seconds, s.kept ? " (kept)" : "");
		out += buf;
	}
	// (what no step covers: the world's own setting up between them)
	std::snprintf(buf, sizeof buf, " · other %.2f", std::max(0.0, m.seconds - counted));
	return out + buf;
}

double clock()
{
	return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

void setMakingReport(std::function<void(const std::string &, const Making &)> report)
{
	std::lock_guard<std::mutex> hold(g_lock);
	g_report = std::move(report);
}

} // namespace terragen
