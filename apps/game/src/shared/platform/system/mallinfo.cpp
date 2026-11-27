// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

// glibc's mallinfo(), as Chromium reads it, made safe past 2 GB.
//
// Chromium's MemoryInfra thread measures the heap with mallinfo() now and
// then, and crashes the process on purpose (a CHECK: SIGILL in libcef) when a
// field is negative. mallinfo()'s fields are ints: past 2 GB of heap they
// wrap negative, so a game whose heap grew past 2 GB died "at random" (crash
// reports: thread 'MemoryInfra'). Defined here, in the executable, it's the
// one libcef binds to: mallinfo2()'s figures, held where they can't wrap
// (Chromium's check: arena + hblkhd, as ints, isn't negative; uordblks isn't).

#include <climits>
#include <cstddef> // (glibc's own header first: __GLIBC__ is said by it)

#if defined(__linux__) && defined(__GLIBC__)

#include <malloc.h>

namespace {

int held(size_t v)
{
	return v > (size_t)INT_MAX ? INT_MAX : (int)v;
}

} // namespace

extern "C" __attribute__((visibility("default"))) struct mallinfo mallinfo(void)
{
	const struct mallinfo2 m = mallinfo2();
	struct mallinfo out;
	// Chromium adds arena and hblkhd as ints and checks the sum isn't negative:
	// each held at half the int's most, so the two together fit
	const size_t half = (size_t)INT_MAX / 2;
	out.arena = held(m.arena < half ? m.arena : half);
	out.ordblks = held(m.ordblks);
	out.smblks = held(m.smblks);
	out.hblks = held(m.hblks);
	out.hblkhd = held(m.hblkhd < half ? m.hblkhd : half);
	out.usmblks = held(m.usmblks);
	out.fsmblks = held(m.fsmblks);
	out.uordblks = held(m.uordblks);
	out.fordblks = held(m.fordblks);
	out.keepcost = held(m.keepcost);
	return out;
}

#endif
