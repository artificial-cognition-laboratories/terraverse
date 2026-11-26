// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/trace/marks.h"
#include "library/trace/trace.h"
#include <deque>
#include <mutex>

namespace trace {

// A startup's worth, many times over: a server start is a few dozen.
constexpr size_t KEEP = 1024;

namespace {
struct Marks
{
	std::mutex lock;
	std::deque<Mark> kept;
	u64 seq = 0;
};
Marks &marks()
{
	static Marks m;
	return m;
}
} // namespace

void mark(std::string_view lane, std::string_view what, u64 start, u64 end)
{
	Marks &m = marks();
	std::lock_guard<std::mutex> guard(m.lock);
	m.kept.push_back({ ++m.seq, std::string(lane), std::string(what), start, end });
	if (m.kept.size() > KEEP)
		m.kept.pop_front();
}

std::vector<Mark> marksSince(u64 seq)
{
	Marks &m = marks();
	std::lock_guard<std::mutex> guard(m.lock);
	std::vector<Mark> out;
	for (const Mark &mark : m.kept) {
		if (mark.seq > seq)
			out.push_back(mark);
	}
	return out;
}

Marking::Marking(std::string_view lane, std::string what) :
	m_lane(lane), m_what(std::move(what)), m_start(now())
{
}

Marking::~Marking()
{
	mark(m_lane, m_what, m_start, now());
}

} // namespace trace
