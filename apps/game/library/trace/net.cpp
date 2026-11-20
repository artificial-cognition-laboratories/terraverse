// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/trace/net.h"
#include "library/trace/trace.h"
#include <algorithm>
#include <deque>
#include <mutex>
#include <unordered_set>

namespace trace::net {

// Messages kept: tens of seconds of a busy connection, at 40 bytes each.
constexpr size_t KEEP = 1 << 16;
constexpr size_t EVENTS = 256;

namespace {

struct Log
{
	std::mutex lock;
	std::vector<Entry> ring = std::vector<Entry>(KEEP);
	size_t next = 0;
	size_t count = 0;
	std::vector<Total> frame; // this frame's, so far
	std::deque<Event> events;
	u64 seq = 0;
	std::unordered_set<u16> arrived; // incoming kinds met since "Connecting"
};
Log &log()
{
	static Log l;
	return l;
}

// Under the lock.
void remember(Log &l, Event e)
{
	e.seq = ++l.seq;
	l.events.push_back(std::move(e));
	if (l.events.size() > EVENTS)
		l.events.pop_front();
}

} // namespace

void message(Dir dir, u16 command, u8 channel, bool reliable, u32 bytes, std::string_view head)
{
	Entry entry{ now(), bytes, command, dir, channel, reliable, {} };
	std::copy_n(head.begin(), std::min(head.size(), HEAD), entry.head.begin());

	Log &l = log();
	std::lock_guard<std::mutex> guard(l.lock);
	l.ring[l.next] = entry;
	l.next = (l.next + 1) % KEEP;
	l.count = std::min(l.count + 1, KEEP);

	auto total = std::find_if(l.frame.begin(), l.frame.end(),
			[&](const Total &t) { return t.command == command && t.dir == dir; });
	if (total == l.frame.end())
		l.frame.push_back({ command, dir, 1, bytes, bytes });
	else {
		total->count++;
		total->bytes += bytes;
		total->largest = std::max(total->largest, bytes);
	}
	if (dir == Dir::In && l.arrived.insert(command).second)
		remember(l, { 0, entry.time, "", command });
}

void event(std::string_view what)
{
	Log &l = log();
	std::lock_guard<std::mutex> guard(l.lock);
	if (what == "Connecting")
		l.arrived.clear();
	remember(l, { 0, now(), std::string(what), -1 });
}

std::vector<Total> takeFrame()
{
	Log &l = log();
	std::lock_guard<std::mutex> guard(l.lock);
	std::vector<Total> out;
	out.swap(l.frame);
	return out;
}

std::vector<Entry> between(u64 from, u64 to)
{
	Log &l = log();
	std::lock_guard<std::mutex> guard(l.lock);
	std::vector<Entry> out;
	for (size_t i = 0; i < l.count; i++) {
		const Entry &e = l.ring[(l.next + KEEP - l.count + i) % KEEP];
		if (e.time >= from && e.time <= to)
			out.push_back(e);
	}
	return out;
}

std::vector<Event> eventsSince(u64 seq)
{
	Log &l = log();
	std::lock_guard<std::mutex> guard(l.lock);
	std::vector<Event> out;
	for (const Event &e : l.events) {
		if (e.seq > seq)
			out.push_back(e);
	}
	return out;
}

} // namespace trace::net
