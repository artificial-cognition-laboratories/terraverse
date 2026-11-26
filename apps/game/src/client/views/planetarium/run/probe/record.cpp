// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/run/probe/record.h"
#include "library/trace/trace.h"
#include "src/shared/base/exceptions.h"
#include <algorithm>

namespace planetarium::probe {

namespace {
double seconds(u64 from, u64 to); // between two trace::now()s
std::string kind(const std::string &thread); // "Far tiles 3" → "Far tiles" (trace::nameThread numbers the second on)
}

Recorder::Recorder() : m_cursor(trace::now())
{
	// frames from here on only
	const std::vector<trace::Frame> before = trace::framesSince(0);
	m_frame = before.empty() ? 0 : before.back().id;
}

void Recorder::begin(const std::string &name)
{
	m_began = m_moved = trace::now();
	m_steps.push_back({ name });
}

void Recorder::moved()
{
	m_moved = trace::now();
	m_steps.back().moving = seconds(m_began, m_moved);
}

void Recorder::settled()
{
	m_steps.back().settled = seconds(m_moved, trace::now());
}

void Recorder::frame(double work)
{
	if (m_steps.empty())
		throw BaseException("planetarium probe: a frame before any step");
	StepRecord &step = m_steps.back();
	step.work.push_back(work);
	for (const trace::Frame &f : trace::framesSince(m_frame)) {
		m_frame = f.id;
		for (const auto &[id, value] : f.counters) {
			StepRecord::Counter &c = step.counters[trace::nameOf(id)];
			c.sum += value;
			c.max = std::max(c.max, value);
		}
	}
	spans(step, trace::now());
}

void Recorder::end(size_t tiles, size_t draws, size_t vertices)
{
	StepRecord &step = m_steps.back();
	spans(step, trace::now());
	step.seconds = seconds(m_began, trace::now());
	step.tiles = tiles;
	step.draws = draws;
	step.vertices = vertices;
}

void Recorder::skip()
{
	const std::vector<trace::Frame> since = trace::framesSince(m_frame);
	if (!since.empty())
		m_frame = since.back().id;
	m_cursor = trace::now();
}

void Recorder::spans(StepRecord &step, u64 to)
{
	// by thread name: the tile workers are many threads of one name
	std::map<std::string, unsigned> named;
	for (const trace::Track &track : trace::between(m_cursor, to)) {
		const std::string name = kind(track.thread);
		named[name]++;
		if (track.since > m_cursor)
			step.lost = true;
		for (const trace::Span &span : track.spans) {
			if (span.end <= m_cursor || span.end > to)
				continue; // read last time, or next
			StepRecord::Zone &z = step.threads[name].zones[trace::nameOf(span.name)];
			const double ms = (span.end - span.start) / 1e6;
			z.ms += ms;
			z.max = std::max(z.max, ms);
			z.count++;
		}
	}
	for (const auto &[name, n] : named) {
		StepRecord::Thread &t = step.threads[name];
		t.threads = std::max(t.threads, n);
	}
	m_cursor = to;
}

namespace {

double seconds(u64 from, u64 to)
{
	return (to - from) / 1e9;
}

std::string kind(const std::string &thread)
{
	const size_t space = thread.find_last_of(' ');
	if (space == std::string::npos || space + 1 == thread.size())
		return thread;
	for (size_t i = space + 1; i < thread.size(); i++)
		if (thread[i] < '0' || thread[i] > '9')
			return thread;
	return thread.substr(0, space);
}

} // namespace

} // namespace planetarium::probe
