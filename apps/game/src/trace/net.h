// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>

/*
	The always-on message recorder behind the debugger's Network tab: every
	message the client sends or receives, as a compact entry in a ring (its
	time, type, direction, channel, size, and the head of an incoming one's
	payload), totalled per frame for the live charts, and the connection's
	milestones (connecting, content ready, in the world) as events.
*/
namespace trace::net {

enum class Dir : u8 { In, Out };

// How much of an incoming message's payload is kept: enough to read where a
// block is or which object an update is for.
constexpr size_t HEAD = 16;

// A message, as the client sent or received it. `head` is its payload's first
// bytes (an incoming one's; zeros for outgoing, whose payload has been written past).
void message(Dir dir, u16 command, u8 channel, bool reliable, u32 bytes, std::string_view head = {});

// A moment on the connection: "Connecting", "Content ready", "In the world".
// "Connecting" also starts the join over: after it, the first of each kind
// of incoming message is an event too (`command` set), which is what the
// Network tab's Join waterfall is drawn from.
void event(std::string_view what);

struct Entry
{
	u64 time; // trace::now()
	u32 bytes;
	u16 command;
	Dir dir;
	u8 channel;
	bool reliable;
	std::array<u8, HEAD> head;
};

struct Event
{
	u64 seq;
	u64 time;
	std::string what; // empty for a first arrival
	int command = -1; // the message type that first arrived, or -1
};

// What a frame moved, per message type and direction.
struct Total
{
	u16 command;
	Dir dir;
	u32 count;
	u64 bytes;
	u32 largest;
};

// The totals since the last call, which starts the next frame's (the recorder's frame() calls it).
std::vector<Total> takeFrame();

// The messages kept in [from, to], oldest first.
std::vector<Entry> between(u64 from, u64 to);

// Events after `seq`, oldest first.
std::vector<Event> eventsSince(u64 seq);

} // namespace trace::net
