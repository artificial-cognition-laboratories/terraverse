// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/memory.h"
#include "src/client/app/shell/host/gpu.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/base/exceptions.h"
#include "library/trace/owned.h"
#include "library/trace/trace.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

#if defined(__linux__)
	#include <dirent.h>
	#include <fstream>
	#include <malloc.h>
	#include <sstream>
	#include <unistd.h>
	#include <unordered_map>
#endif

namespace shell {

namespace {

constexpr auto EVERY = std::chrono::milliseconds(100);
// Of the cheap samples, every this many reads the processes and the machine too.
constexpr u64 DEAR_EVERY = 10;
// The page's: five minutes at ten a second. The report's: an hour at one.
constexpr size_t KEEP_FAST = 3000;
constexpr size_t KEEP_SLOW = 3600;

// What one sample takes: the last, to carry the dear figures between reads.
struct Taking {
	u64 seq;
	const Memory::Sample *last; // none: the first
	bool dear;                  // read the processes, the machine and the card this time
	Gpu *gpu;                   // none: the card isn't read
};
Memory::Sample take(const Taking &taking);

} // namespace

struct Memory::Impl
{
	Opts opts;
	mutable std::mutex lock;
	std::condition_variable wake;
	std::deque<Sample> samples; // ten a second, five minutes
	std::deque<Sample> hour;    // one a second (the dear ones), an hour
	bool stopping = false;
	std::thread thread; // last: it starts once the rest is there

	explicit Impl(Opts o) : opts(std::move(o)), thread([this] { run(); }) {}

	~Impl()
	{
		{
			std::lock_guard<std::mutex> hold(lock);
			stopping = true;
		}
		wake.notify_all();
		thread.join();
	}

	void run()
	{
		// opened here, not at construction: loading the driver's library is I/O
		std::unique_ptr<Gpu> gpu = Gpu::open();
		if (!gpu)
			infostream << "memory: the graphics card's memory isn't read here (no NVIDIA driver's NVML)" << std::endl;
		u64 seq = 0;
		std::unique_lock<std::mutex> hold(lock);
		while (!stopping) {
			++seq;
			const bool dear = (seq - 1) % DEAR_EVERY == 0;
			// a copy: the samples may move while this one's taken
			const std::optional<Sample> last = samples.empty() ? std::nullopt : std::optional<Sample>(samples.back());
			const Taking taking{ seq, last ? &*last : nullptr, dear, gpu.get() };
			hold.unlock();
			Sample sample = take(taking);
			hold.lock();
			if (taking.dear) {
				hour.push_back(sample);
				if (hour.size() > KEEP_SLOW)
					hour.pop_front();
			}
			samples.push_back(std::move(sample));
			if (samples.size() > KEEP_FAST)
				samples.pop_front();
			wake.wait_for(hold, EVERY, [this] { return stopping; });
		}
	}
};

Memory::Memory(Opts opts) : m(std::make_unique<Impl>(std::move(opts)))
{
}

Memory::~Memory() = default;

Memory::Since Memory::since(u64 seq) const
{
	Since out{ Json::Value(Json::arrayValue), seq };
	std::lock_guard<std::mutex> hold(m->lock);
	for (const Sample &s : m->samples) {
		if (s.seq <= seq)
			continue;
		out.samples.append(memory::json(s));
		out.newest = s.seq;
	}
	return out;
}

std::string Memory::report() const
{
	std::vector<Sample> copy;
	{
		std::lock_guard<std::mutex> hold(m->lock);
		copy.assign(m->hour.begin(), m->hour.end());
	}
	return memory::write(m->opts.dir, copy);
}

namespace memory {

Json::Value json(const Memory::Sample &s)
{
	Json::Value out;
	out["seq"] = (Json::UInt64)s.seq;
	out["t"] = s.t;
	Json::Value &processes = out["processes"] = Json::Value(Json::arrayValue);
	for (const Memory::Process &p : s.processes) {
		Json::Value j;
		j["pid"] = (Json::Int64)p.pid;
		j["role"] = p.role;
		j["rss"] = (Json::UInt64)p.rss;
		j["pss"] = (Json::UInt64)p.pss;
		j["private"] = (Json::UInt64)p.priv;
		j["swap"] = (Json::UInt64)p.swap;
		j["gpu"] = (Json::UInt64)p.gpu;
		processes.append(std::move(j));
	}
	out["available"] = (Json::UInt64)s.available;
	out["total"] = (Json::UInt64)s.total;
	out["swapUsed"] = (Json::UInt64)s.swap_used;
	out["swapTotal"] = (Json::UInt64)s.swap_total;
	out["heap"] = (Json::UInt64)s.heap;
	out["gpuTotal"] = (Json::UInt64)s.gpu_total;
	out["gpuFree"] = (Json::UInt64)s.gpu_free;
	out["gpuOthers"] = (Json::UInt64)s.gpu_others;
	Json::Value &owners = out["owners"] = Json::Value(Json::arrayValue);
	for (const Memory::Owner &o : s.owners) {
		Json::Value j;
		j["name"] = o.name;
		j["bytes"] = (Json::UInt64)o.bytes;
		j["count"] = (Json::UInt64)o.count;
		j["gpu"] = o.gpu;
		owners.append(std::move(j));
	}
	return out;
}

} // namespace memory

namespace {

#if defined(__linux__)

// What /proc says, read as the sampler needs it.
namespace proc {

// "Key:   123 kB" lines of a file, as bytes by key.
std::unordered_map<std::string, u64> kilobytes(const std::string &path)
{
	std::unordered_map<std::string, u64> out;
	std::ifstream file(path);
	std::string line;
	while (std::getline(file, line)) {
		const size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::istringstream rest(line.substr(colon + 1));
		u64 kb = 0;
		if (rest >> kb)
			out[line.substr(0, colon)] = kb * 1024;
	}
	return out;
}

// Each process's parent, by pid: the whole machine's, read once a sample.
std::unordered_map<long, long> parents()
{
	std::unordered_map<long, long> out;
	DIR *dir = opendir("/proc");
	if (!dir)
		return out;
	while (dirent *entry = readdir(dir)) {
		if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
			continue;
		std::string stat;
		std::getline(std::ifstream(std::string("/proc/") + entry->d_name + "/stat"), stat);
		// "pid (name) state ppid": the name may hold spaces, so after its last ")"
		const size_t close = stat.rfind(')');
		if (close == std::string::npos)
			continue;
		std::istringstream fields(stat.substr(close + 2));
		std::string state;
		long ppid = 0;
		if (fields >> state >> ppid)
			out[std::atol(entry->d_name)] = ppid;
	}
	closedir(dir);
	return out;
}

// What a process of the game's tree is, by its command line.
std::string role(long pid, long self)
{
	if (pid == self)
		return "engine";
	std::ifstream file("/proc/" + std::to_string(pid) + "/cmdline");
	std::string arg, first;
	while (std::getline(file, arg, '\0')) {
		if (first.empty())
			first = arg;
		// Chromium's own: --type=gpu-process, renderer, utility, zygote
		if (arg.rfind("--type=", 0) == 0)
			return "chromium " + arg.substr(7);
		if (arg == "runtime" && first.find("terra") != std::string::npos)
			return "terra runtime";
	}
	const size_t slash = first.rfind('/');
	return first.empty() ? "?" : first.substr(slash == std::string::npos ? 0 : slash + 1);
}

} // namespace proc

Memory::Sample take(const Taking &taking)
{
	Memory::Sample s{};
	s.seq = taking.seq;
	s.t = trace::now() / 1e6;
	const struct mallinfo2 heap = mallinfo2();
	s.heap = heap.uordblks + heap.hblkhd;
	for (const trace::Owned &o : trace::owners())
		s.owners.push_back({ o.name, o.bytes, o.count, o.where == trace::Where::Gpu });
	if (!taking.dear && taking.last) {
		s.processes = taking.last->processes;
		s.total = taking.last->total;
		s.available = taking.last->available;
		s.swap_total = taking.last->swap_total;
		s.swap_used = taking.last->swap_used;
		s.gpu_total = taking.last->gpu_total;
		s.gpu_free = taking.last->gpu_free;
		s.gpu_others = taking.last->gpu_others;
		return s;
	}

	// the engine and everything under it (Chromium's zygote has its renderers)
	const long self = getpid();
	const std::unordered_map<long, long> parents = proc::parents();
	std::vector<long> tree{ self };
	for (size_t i = 0; i < tree.size(); i++)
		for (const auto &[pid, ppid] : parents)
			if (ppid == tree[i])
				tree.push_back(pid);
	Gpu::Reading card{};
	if (taking.gpu) {
		try {
			card = taking.gpu->read();
		} catch (const BaseException &e) {
			// said once, in the log; the card's figures stay 0 (unknown) rather than end the game
			static bool said = false;
			if (!said)
				errorstream << "memory: " << e.what() << std::endl;
			said = true;
		}
		s.gpu_total = card.total;
		s.gpu_free = card.free;
		for (const auto &[pid, bytes] : card.processes)
			s.gpu_others += bytes; // less the tree's, below
	}
	for (long pid : tree) {
		auto rollup = proc::kilobytes("/proc/" + std::to_string(pid) + "/smaps_rollup");
		if (rollup.empty())
			continue; // gone since it was listed
		const auto onCard = card.processes.find(pid);
		const u64 gpu = onCard == card.processes.end() ? 0 : onCard->second;
		s.gpu_others -= gpu;
		s.processes.push_back({ pid, proc::role(pid, self), rollup["Rss"], rollup["Pss"],
				rollup["Private_Clean"] + rollup["Private_Dirty"], rollup["Swap"], gpu });
	}

	auto info = proc::kilobytes("/proc/meminfo");
	s.total = info["MemTotal"];
	s.available = info["MemAvailable"];
	s.swap_total = info["SwapTotal"];
	s.swap_used = info["SwapTotal"] - info["SwapFree"];
	return s;
}

#else

// TEMP(2026-11-27): only Linux is read so far; elsewhere a sample has the owners alone
Memory::Sample take(const Taking &taking)
{
	Memory::Sample s{};
	s.seq = taking.seq;
	s.t = trace::now() / 1e6;
	for (const trace::Owned &o : trace::owners())
		s.owners.push_back({ o.name, o.bytes, o.count, o.where == trace::Where::Gpu });
	return s;
}

#endif

} // namespace

} // namespace shell
