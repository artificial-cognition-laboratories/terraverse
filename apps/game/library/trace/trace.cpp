// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/trace/trace.h"
#include "library/trace/report.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <typeindex>
#include <cstdlib>
#ifndef _MSC_VER
#include <cxxabi.h>
#endif

namespace trace {

// Per thread: a few seconds of a busy thread's zones. A span is 24 bytes.
constexpr size_t SPANS = 1 << 15;
// Frames kept: twenty seconds at 60 fps.
constexpr size_t FRAMES = 1200;

static const auto START = std::chrono::steady_clock::now();

u64 now()
{
	return (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now() - START).count();
}

namespace {

struct Names
{
	std::mutex lock;
	std::deque<std::string> names; // a deque: references stay good as it grows
	std::unordered_map<std::string, u32> ids;
};
Names &names()
{
	static Names n;
	return n;
}

// One thread's zones. Written only by its thread; read, under the lock, by the debugger.
struct Ring
{
	std::mutex lock;
	std::string thread;
	std::vector<Span> spans = std::vector<Span>(SPANS);
	size_t next = 0;  // where the next span goes
	size_t count = 0; // spans kept, up to SPANS
	u16 depth = 0;    // zones open now: only its thread touches it
	std::atomic<bool> ended{ false }; // its thread's gone: let go once its spans are older than any frame kept

	u64 newest() const { return count ? spans[(next + SPANS - 1) % SPANS].end : 0; }

	void push(const Span &span)
	{
		std::lock_guard<std::mutex> guard(lock);
		spans[next] = span;
		next = (next + 1) % SPANS;
		count = std::min(count + 1, SPANS);
	}

	// Newest first, while `keep` says go on.
	template <typename F>
	void backwards(F &&keep) const
	{
		for (size_t i = 0; i < count; i++) {
			if (!keep(spans[(next + SPANS - 1 - i) % SPANS]))
				return;
		}
	}
};

struct Rings
{
	std::mutex lock;
	// kept a while when a thread ends (its history still counts), then let go:
	// a ring is most of a megabyte, and threads that came and went piled up
	std::vector<std::unique_ptr<Ring>> all;
};
Rings &rings()
{
	static Rings r;
	return r;
}

thread_local Ring *t_ring = nullptr;

// Marks its thread's ring ended as the thread goes.
struct Owner
{
	~Owner()
	{
		if (t_ring)
			t_ring->ended = true;
	}
};
thread_local Owner t_owner;

Ring &ring()
{
	if (t_ring)
		return *t_ring;
	Rings &r = rings();
	std::lock_guard<std::mutex> guard(r.lock);
	r.all.push_back(std::make_unique<Ring>());
	t_ring = r.all.back().get();
	(void)&t_owner; // made with the ring, so it marks it as the thread goes
	t_ring->thread = "Thread " + std::to_string(r.all.size());
	return *t_ring;
}

struct Counter
{
	double value = 0;
	u32 reports = 0;
	Fold fold = Fold::Sum;
};
struct Counters
{
	std::mutex lock;
	std::unordered_map<u32, Counter> frame; // this frame's, so far
	std::unordered_map<u32, std::string> units;
};
Counters &counters()
{
	static Counters c;
	return c;
}

struct Frames
{
	std::mutex lock;
	std::deque<Frame> kept;
	u64 next = 1;
	u64 last_end = 0;
};
Frames &frames()
{
	static Frames f;
	return f;
}

} // namespace

u32 intern(std::string_view name)
{
	// The old profiler names its units: "Client step [us]", sometimes twice
	// over ("[us] [us]", a timed scope's own unit added). Ours are the debugger's.
	while (!name.empty() && name.back() == ']') {
		const size_t open = name.rfind(" [");
		if (open == std::string_view::npos)
			break;
		name = name.substr(0, open);
	}
	// A name already met by this thread is found without the lock: dynamic
	// names (the old profiler's) are interned on every call.
	thread_local std::unordered_map<std::string, u32> cache;
	std::string key(name);
	if (auto it = cache.find(key); it != cache.end())
		return it->second;

	Names &n = names();
	std::lock_guard<std::mutex> guard(n.lock);
	auto [it, fresh] = n.ids.try_emplace(key, (u32)n.names.size());
	if (fresh)
		n.names.push_back(key);
	cache.emplace(std::move(key), it->second);
	return it->second;
}

const std::string &nameOf(u32 id)
{
	Names &n = names();
	std::lock_guard<std::mutex> guard(n.lock);
	return n.names.at(id);
}

u32 internType(const std::type_info &type)
{
	thread_local std::unordered_map<std::type_index, u32> cache;
	if (auto it = cache.find(type); it != cache.end())
		return it->second;
#ifdef _MSC_VER
	// MSVC's ABI names types readably already: "class shell::Menu"
	std::string name = type.name();
	for (const char *kind : { "class ", "struct " })
		if (name.rfind(kind, 0) == 0)
			name = name.substr(std::char_traits<char>::length(kind));
#else
	int status = 0;
	char *plain = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
	std::string name = status == 0 && plain ? plain : type.name();
	std::free(plain);
#endif
	// a class's own name reads as a zone; its namespaces don't
	if (const size_t colon = name.rfind("::"); colon != std::string::npos)
		name = name.substr(colon + 2);
	const u32 id = intern(name);
	cache.emplace(type, id);
	return id;
}

void count(std::string_view name, double value, Fold fold)
{
	std::string_view unit;
	if (!name.empty() && name.back() == ']') {
		if (const size_t open = name.rfind(" ["); open != std::string_view::npos)
			unit = name.substr(open + 2, name.size() - open - 3);
	}
	const u32 id = intern(name); // drops the unit
	Counters &c = counters();
	std::lock_guard<std::mutex> guard(c.lock);
	if (!unit.empty())
		c.units.try_emplace(id, unit);
	Counter &counter = c.frame[id];
	counter.fold = fold;
	counter.reports++;
	switch (fold) {
	case Fold::Sum:
	case Fold::Mean: counter.value += value; break;
	case Fold::Max: counter.value = counter.reports == 1 ? value : std::max(counter.value, value); break;
	case Fold::Last: counter.value = value; break;
	}
}

std::string unitOf(u32 id)
{
	Counters &c = counters();
	std::lock_guard<std::mutex> guard(c.lock);
	auto it = c.units.find(id);
	return it == c.units.end() ? "" : it->second;
}

void nameThread(std::string_view name)
{
	Ring &self = ring();
	// Threads of a kind share a name (every mesh worker is "MeshUpdate"): the
	// second and on are numbered, so each keeps a rail of its own.
	std::string unique(name);
	{
		Rings &r = rings();
		std::lock_guard<std::mutex> guard(r.lock);
		for (int n = 2; std::any_of(r.all.begin(), r.all.end(), [&](const auto &other) {
				return other.get() != &self && other->thread == unique; }); n++)
			unique = std::string(name) + " " + std::to_string(n);
	}
	std::lock_guard<std::mutex> guard(self.lock);
	self.thread = std::move(unique);
}

Zone::Zone(u32 name) noexcept : m_name(name), m_start(now()), m_depth(ring().depth++)
{
}

Zone::~Zone() noexcept
{
	Ring &r = ring();
	r.depth--;
	r.push({ m_name, m_depth, m_start, now() });
}

void frame()
{
	const u64 end = now();
	Ring &r = ring();
	if (r.thread.rfind("Thread ", 0) == 0)
		nameThread("Main"); // whoever marks frames is the main thread

	Frames &f = frames();
	std::lock_guard<std::mutex> guard(f.lock);
	Frame done{ f.next++, f.last_end, end, {} };
	f.last_end = end;

	// every thread's time in each zone it closed during the frame
	std::vector<Ring *> all;
	{
		Rings &rs = rings();
		std::lock_guard<std::mutex> rsguard(rs.lock);
		// an ended thread's ring goes once nothing kept could show it
		const u64 oldest = f.kept.empty() ? done.start : f.kept.front().start;
		rs.all.erase(std::remove_if(rs.all.begin(), rs.all.end(), [&](const std::unique_ptr<Ring> &ring) {
			if (!ring->ended)
				return false;
			std::lock_guard<std::mutex> rguard(ring->lock);
			return ring->newest() < oldest;
		}), rs.all.end());
		for (const auto &ring : rs.all)
			all.push_back(ring.get());
	}
	for (Ring *ring : all) {
		std::lock_guard<std::mutex> rguard(ring->lock);
		Frame::Thread thread{ ring->thread, {} };
		ring->backwards([&](const Span &span) {
			if (span.end <= done.start)
				return false;
			auto part = std::find_if(thread.parts.begin(), thread.parts.end(),
					[&](const auto &p) { return p.first == span.name; });
			const u64 took = span.end - std::max(span.start, done.start);
			if (part == thread.parts.end())
				thread.parts.emplace_back(span.name, took);
			else
				part->second += took;
			return true;
		});
		if (!thread.parts.empty())
			done.threads.push_back(std::move(thread));
	}
	{
		Counters &c = counters();
		std::lock_guard<std::mutex> cguard(c.lock);
		for (const auto &[id, counter] : c.frame)
			done.counters.emplace_back(id, counter.fold == Fold::Mean ? counter.value / counter.reports : counter.value);
		c.frame.clear();
	}
	done.net = net::takeFrame();
	report::add(done);
	f.kept.push_back(std::move(done));
	if (f.kept.size() > FRAMES)
		f.kept.pop_front();
}

std::vector<Frame> framesSince(u64 id)
{
	Frames &f = frames();
	std::lock_guard<std::mutex> guard(f.lock);
	std::vector<Frame> out;
	for (const Frame &frame : f.kept) {
		if (frame.id > id)
			out.push_back(frame);
	}
	return out;
}

std::vector<Track> between(u64 from, u64 to)
{
	Rings &r = rings();
	std::lock_guard<std::mutex> guard(r.lock);
	std::vector<Track> out;
	for (const auto &ring : r.all) {
		std::lock_guard<std::mutex> rguard(ring->lock);
		// a ring not yet full has dropped nothing: it has everything since the start
		Track track{ ring->thread, ring->count < SPANS ? 0 : ring->spans[ring->next].start, {} };
		ring->backwards([&](const Span &span) {
			if (span.end >= from && span.start <= to)
				track.spans.push_back(span);
			// spans land in the order they end; one that ended long before
			// `from` means everything older did too, give or take a long zone
			return span.end + 10'000'000'000ULL >= from;
		});
		out.push_back(std::move(track)); // idle ones too: their rails stay put
	}
	return out;
}

} // namespace trace
