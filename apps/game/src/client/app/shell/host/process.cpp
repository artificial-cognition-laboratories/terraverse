// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/process.h"
#include "library/trace/trace.h"
#include <chrono>
#include <unordered_map>

#if defined(_WIN32)
	#include <windows.h>
	#include <psapi.h>
#else
	#include <sys/resource.h>
	#include <unistd.h>
#endif
#if defined(__linux__)
	#include <dirent.h>
	#include <fstream>
	#include <sstream>
#endif

namespace shell {

using Clock = std::chrono::steady_clock;

struct Process::Impl
{
	Clock::time_point at = Clock::now();
	double cpu = 0; // seconds of CPU, all threads, at `at`
	std::unordered_map<long, double> threads; // tid → its CPU seconds then
};

Process::Process() : m(std::make_unique<Impl>())
{
}

Process::~Process() = default;

// CPU seconds the process has used, every thread together.
static double processCpu()
{
#if defined(_WIN32)
	FILETIME created, ended, kernel, user;
	GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user);
	auto secs = [](FILETIME t) { return (((u64)t.dwHighDateTime << 32) | t.dwLowDateTime) / 1e7; };
	return secs(kernel) + secs(user);
#else
	rusage use{};
	getrusage(RUSAGE_SELF, &use);
	return use.ru_utime.tv_sec + use.ru_utime.tv_usec / 1e6 + use.ru_stime.tv_sec + use.ru_stime.tv_usec / 1e6;
#endif
}

// Resident memory, bytes.
static u64 residentBytes()
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS counters{};
	GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
	return counters.WorkingSetSize;
#elif defined(__linux__)
	std::ifstream statm("/proc/self/statm");
	u64 pages = 0, resident = 0;
	statm >> pages >> resident;
	return resident * (u64)sysconf(_SC_PAGESIZE);
#else
	rusage use{};
	getrusage(RUSAGE_SELF, &use);
	return (u64)use.ru_maxrss; // the most it's been (bytes on macOS); no current figure
#endif
}

Json::Value Process::sample()
{
	const Clock::time_point now = Clock::now();
	const double wall = std::chrono::duration<double>(now - m->at).count();
	const double cpu = processCpu();
	Json::Value out;
#if defined(_WIN32)
	out["pid"] = (Json::UInt)GetCurrentProcessId();
#else
	out["pid"] = (Json::Int)getpid();
#endif
	out["cpu"] = wall > 0 ? (cpu - m->cpu) / wall * 100 : 0;
	out["rss"] = (Json::UInt64)residentBytes();
	Json::Value &threads = out["threads"] = Json::Value(Json::arrayValue);
#if defined(__linux__)
	// each thread's name (as it set it) and CPU, from /proc
	const double tick = (double)sysconf(_SC_CLK_TCK);
	std::unordered_map<long, double> seen;
	if (DIR *dir = opendir("/proc/self/task")) {
		while (dirent *entry = readdir(dir)) {
			if (entry->d_name[0] == '.')
				continue;
			const long tid = std::atol(entry->d_name);
			const std::string base = std::string("/proc/self/task/") + entry->d_name;
			std::string name;
			std::getline(std::ifstream(base + "/comm"), name);
			std::string stat;
			std::getline(std::ifstream(base + "/stat"), stat);
			// fields after the ")" that ends the name: utime and stime are the 12th and 13th
			const size_t close = stat.rfind(')');
			if (close == std::string::npos)
				continue;
			std::istringstream fields(stat.substr(close + 2));
			std::string field;
			double used = 0;
			for (int i = 1; fields >> field && i <= 13; i++) {
				if (i == 12 || i == 13)
					used += std::atof(field.c_str()) / tick;
			}
			seen[tid] = used;
			auto before = m->threads.find(tid);
			Json::Value t;
			t["name"] = name;
			t["cpu"] = wall > 0 && before != m->threads.end() ? (used - before->second) / wall * 100 : 0;
			threads.append(std::move(t));
		}
		closedir(dir);
	}
	m->threads = std::move(seen);
#endif
	m->at = now;
	m->cpu = cpu;
	// the charts read these from the frames, like any counter
	trace::count("Process: CPU [%]", out["cpu"].asDouble(), trace::Fold::Last);
	trace::count("Process: memory [MB]", out["rss"].asDouble() / 1e6, trace::Fold::Last);
	return out;
}

} // namespace shell
