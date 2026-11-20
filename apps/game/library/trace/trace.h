// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/core/irrlichttypes.h"
#include "library/trace/net.h"
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

/*
	The always-on recorder behind the debugger's Perf tab. Every thread keeps
	its last zones (a named span of time, nested) in a ring of its own, and the
	main thread marks where each frame ends, so the last several seconds can be
	looked at after the fact: the spike already happened when you go looking.

	A zone costs two clock reads and a push into its thread's ring, uncontended.
	Names are interned once, so a zone holds an id, not a string.
*/
namespace trace {

// Nanoseconds since the process started: the one clock every zone and frame uses.
u64 now();

// The id of a name, the same for every thread and call. Trailing " [us]"-style
// units (the old profiler's) are dropped: the debugger shows its own.
u32 intern(std::string_view name);
const std::string &nameOf(u32 id);
// A class's name as a zone's: "Draw3D" for a render step. Cached, so a hot loop can call it.
u32 internType(const std::type_info &type);

// How a counter's reports within one frame make its value for that frame.
enum class Fold : u8 { Sum, Mean, Max, Last };

// A value reported by any thread (blocks drawn, draw calls, packets): the
// debugger shows each counter per frame. A trailing unit, "[#]" or "[us]",
// is kept apart from the name (unitOf).
void count(std::string_view name, double value, Fold fold);
// A counter's unit, as its name gave it ("#", "us", "%"), or "" when none.
std::string unitOf(u32 id);

// The calling thread's name in the debugger (a thread unnamed is "Thread <n>").
void nameThread(std::string_view name);

// A span on the calling thread, from construction to destruction.
class Zone
{
public:
	explicit Zone(u32 name) noexcept;
	~Zone() noexcept;
	Zone(const Zone &) = delete;
	Zone &operator=(const Zone &) = delete;

private:
	u32 m_name;
	u64 m_start;
	u16 m_depth;
};

// The end of a frame (and the start of the next), on the main thread.
void frame();

struct Span
{
	u32 name;
	u16 depth;
	u64 start, end;
};

struct Frame
{
	u64 id;
	u64 start, end;
	// Each thread's time in each zone it closed during the frame, by name,
	// nested ones included (a zone's time is in its parent's too).
	struct Thread
	{
		std::string thread;
		std::vector<std::pair<u32, u64>> parts;
	};
	std::vector<Thread> threads;
	// Each counter reported during the frame, folded (Fold).
	std::vector<std::pair<u32, double>> counters;
	// The messages the client sent and received during the frame, by type.
	std::vector<net::Total> net;
};

// Frames ended after `id`, oldest first (as many as are kept).
std::vector<Frame> framesSince(u64 id);

struct Track
{
	std::string thread;
	// The oldest span it still has: before it, the ring has moved on.
	u64 since;
	std::vector<Span> spans;
};

// Every thread's zones that overlap [from, to], by thread, in the order the
// threads started; a thread with none there is still listed.
std::vector<Track> between(u64 from, u64 to);

} // namespace trace

#define TF_TRACE_CAT2(a, b) a##b
#define TF_TRACE_CAT(a, b) TF_TRACE_CAT2(a, b)
// A zone from here to the end of the scope: TF_ZONE("far.update");
#define TF_ZONE(name) \
	static const u32 TF_TRACE_CAT(tf_zone_name_, __LINE__) = ::trace::intern(name); \
	::trace::Zone TF_TRACE_CAT(tf_zone_, __LINE__)(TF_TRACE_CAT(tf_zone_name_, __LINE__))
