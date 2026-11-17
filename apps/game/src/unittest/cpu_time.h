// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdlib>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h> // (force-included already, with its words given back: windows_names.h)
#else
#include <ctime>
#endif

/*
	The CPU time this thread has used, in ms: what a test's time budget
	measures. Wall time also counted what else the machine was running (other
	builds and agents share it), so a budget failed for the load, not for the
	code. Only for code that does its work on the calling thread.
*/
inline double threadCpuMs()
{
#ifdef _WIN32
	FILETIME created, exited, kernel, user;
	if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
		throw std::runtime_error("GetThreadTimes failed");
	const auto ticks = [](const FILETIME &t) { return ((unsigned long long)t.dwHighDateTime << 32) | t.dwLowDateTime; };
	return (ticks(kernel) + ticks(user)) / 1e4; // 100 ns ticks
#else
	timespec t;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) != 0)
		throw std::runtime_error("clock_gettime(CLOCK_THREAD_CPUTIME_ID) failed");
	return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
#endif
}

/*
	Whether a test holds its time budgets: only with TF_BUDGETS set (bun
	check:budgets, one module at a time on a quiet machine). Even in CPU time a
	step costs twice as much when every core is busy (caches, shared cores), so
	the parallel unit run reports the times and asserts only what's counted.
*/
inline bool budgetsHeld()
{
	return std::getenv("TF_BUDGETS") != nullptr;
}
