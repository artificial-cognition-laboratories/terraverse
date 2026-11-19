// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "report.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace trace::report {

namespace {

constexpr size_t WORST = 12;       // frames kept whole, the slowest
constexpr double SETTLED_S = 30;   // what's after this is play, not start-up
constexpr double BUDGET_MS = 1000.0 / 60;

struct ZoneTotal
{
	u64 sum = 0, max = 0; // ns
	u32 frames = 0;       // frames it ran in
	u64 worst_frame = 0;
};

struct CounterTotal
{
	double max = 0, sum = 0;
	u32 frames = 0;
	u64 worst_frame = 0;
};

struct Session
{
	std::mutex lock;
	u64 first = 0, last = 0; // ns
	std::vector<float> ms;   // every frame's
	std::map<std::string, std::unordered_map<u32, ZoneTotal>> zones; // by thread kind
	std::unordered_map<u32, CounterTotal> counters;
	std::vector<Frame> worst, worst_settled; // slowest first
	size_t threads = 0, threads_peak = 0;
};
Session &session()
{
	static Session s;
	return s;
}

// "MeshUpdate 2" and "MeshUpdate" are one kind: a pool's workers together.
std::string kindOf(const std::string &thread)
{
	size_t end = thread.size();
	while (end > 0 && thread[end - 1] >= '0' && thread[end - 1] <= '9')
		end--;
	if (end == thread.size() || end == 0 || thread[end - 1] != ' ')
		return thread;
	return thread.substr(0, end - 1);
}

double ms(u64 ns)
{
	return ns / 1e6;
}

void keepWorst(std::vector<Frame> &worst, const Frame &f)
{
	const auto slower = [](const Frame &a, const Frame &b) { return a.end - a.start > b.end - b.start; };
	if (worst.size() == WORST && !slower(f, worst.back()))
		return;
	worst.insert(std::upper_bound(worst.begin(), worst.end(), f, slower), f);
	if (worst.size() > WORST)
		worst.pop_back();
}

double percentile(std::vector<float> v, double p)
{
	if (v.empty())
		return 0;
	const size_t i = std::min(v.size() - 1, (size_t)(p * (v.size() - 1) + 0.5));
	std::nth_element(v.begin(), v.begin() + i, v.end());
	return v[i];
}

std::string fmt(const char *f, double a, double b = 0, double c = 0, double d = 0)
{
	char buf[256];
	std::snprintf(buf, sizeof buf, f, a, b, c, d);
	return buf;
}

// A frame's busiest zones on each thread, over `floor` ms, as lines.
void describe(std::ostringstream &out, const Frame &f, u64 first, double floor)
{
	out << "### Frame " << f.id << fmt(": %.1f ms, at %.1f s\n\n", ms(f.end - f.start), (f.start - first) / 1e9);
	for (const Frame::Thread &t : f.threads) {
		std::vector<std::pair<u32, u64>> parts = t.parts;
		std::sort(parts.begin(), parts.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
		std::string line;
		for (const auto &[name, took] : parts) {
			if (ms(took) < floor)
				break;
			line += (line.empty() ? "" : ", ") + nameOf(name) + fmt(" %.1f", ms(took));
		}
		if (!line.empty())
			out << "- **" << t.thread << "**: " << line << "\n";
	}
	std::string counts;
	for (const auto &[id, value] : f.counters)
		if (value != 0)
			counts += (counts.empty() ? "" : ", ") + nameOf(id) + fmt(" %g", value);
	if (!counts.empty())
		out << "- counters: " << counts << "\n";
	out << "\n";
}

} // namespace

void add(const Frame &f)
{
	Session &s = session();
	std::lock_guard<std::mutex> guard(s.lock);
	if (s.ms.empty())
		s.first = f.start;
	s.last = f.end;
	s.ms.push_back((float)ms(f.end - f.start));
	for (const Frame::Thread &t : f.threads) {
		auto &zones = s.zones[kindOf(t.thread)];
		for (const auto &[name, took] : t.parts) {
			ZoneTotal &z = zones[name];
			z.sum += took;
			z.frames++;
			if (took > z.max) {
				z.max = took;
				z.worst_frame = f.id;
			}
		}
	}
	for (const auto &[id, value] : f.counters) {
		CounterTotal &c = s.counters[id];
		c.sum += value;
		c.frames++;
		if (c.frames == 1 || value > c.max) {
			c.max = value;
			c.worst_frame = f.id;
		}
	}
	s.threads = f.threads.size();
	s.threads_peak = std::max(s.threads_peak, s.threads);
	keepWorst(s.worst, f);
	if ((f.start - s.first) / 1e9 > SETTLED_S)
		keepWorst(s.worst_settled, f);
}

std::string markdown()
{
	Session &s = session();
	std::lock_guard<std::mutex> guard(s.lock);
	std::ostringstream out;
	const double seconds = (s.last - s.first) / 1e9;
	const std::time_t now = std::time(nullptr);
	char when[64];
	std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	out << "# Performance report\n\n" << when << ", " << s.ms.size() << " frames over "
			<< fmt("%.0f s", seconds) << ". Times in ms; a zone's time includes the zones inside it."
			<< " Wait is idle (the frame cap, vsync, a server's rest).\n\n";

	size_t over[3] = {};
	for (float m : s.ms)
		for (int i = 0; i < 3; i++)
			over[i] += m > BUDGET_MS * (1 << i);
	out << "## Frames\n\n| p50 | p90 | p99 | worst | > 16.7 | > 33 | > 67 |\n|---|---|---|---|---|---|---|\n"
		<< fmt("| %.1f | %.1f | %.1f ", percentile(s.ms, 0.5), percentile(s.ms, 0.9), percentile(s.ms, 0.99))
		<< fmt("| %.1f | %.0f ", s.ms.empty() ? 0 : *std::max_element(s.ms.begin(), s.ms.end()), (double)over[0])
		<< fmt("| %.0f | %.0f |\n\n", (double)over[1], (double)over[2]);
	out << "Threads: " << s.threads << " now, " << s.threads_peak << " at most.\n\n";

	out << "## Where the time goes\n\nEach thread kind's zones by total time: per frame on average,"
		<< " the most in one frame, and in how many frames it ran.\n\n";
	for (const auto &[kind, zones] : s.zones) {
		std::vector<std::pair<u32, ZoneTotal>> list(zones.begin(), zones.end());
		std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.second.sum > b.second.sum; });
		if (list.empty() || ms(list.front().second.sum) < 1)
			continue;
		out << "### " << kind << "\n\n| zone | ms/frame | worst | frames | worst in |\n|---|---|---|---|---|\n";
		for (size_t i = 0; i < list.size() && i < 20; i++) {
			const ZoneTotal &z = list[i].second;
			if (ms(z.max) < 0.05)
				continue;
			out << "| " << nameOf(list[i].first) << fmt(" | %.3f | %.2f | %.0f ", ms(z.sum) / std::max<size_t>(1, s.ms.size()),
					ms(z.max), (double)z.frames) << "| " << z.worst_frame << " |\n";
		}
		out << "\n";
	}

	out << "## Worst frames after start-up (" << fmt("%.0f", SETTLED_S) << " s on)\n\nZones over 0.5 ms.\n\n";
	for (const Frame &f : s.worst_settled)
		describe(out, f, s.first, 0.5);
	out << "## Worst frames from the start\n\n";
	for (size_t i = 0; i < s.worst.size() && i < 4; i++)
		describe(out, s.worst[i], s.first, 1.0);

	out << "## Counters\n\n| counter | mean | peak | peak in |\n|---|---|---|---|\n";
	std::vector<std::pair<u32, CounterTotal>> counters(s.counters.begin(), s.counters.end());
	std::sort(counters.begin(), counters.end(), [](const auto &a, const auto &b) { return nameOf(a.first) < nameOf(b.first); });
	for (const auto &[id, c] : counters) {
		const std::string unit = unitOf(id);
		out << "| " << nameOf(id) << (unit.empty() ? "" : " [" + unit + "]")
			<< fmt(" | %g | %g ", c.sum / std::max<u32>(1, c.frames), c.max) << "| " << c.worst_frame << " |\n";
	}
	return out.str();
}

bool write(const std::string &path)
{
	const std::string text = markdown();
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
	const std::string part = path + ".part";
	{
		std::ofstream f(part, std::ios::binary | std::ios::trunc);
		if (!f)
			return false;
		f << text;
		if (!f)
			return false;
	}
	std::filesystem::rename(part, path, ec);
	return !ec;
}

void tick(const std::string &dir)
{
	static u64 written = 0;
	const u64 at = now();
	if (at - written < 60'000'000'000ULL)
		return;
	written = at;
	write(dir + "/last.md");
}

void finish(const std::string &dir)
{
	if (session().ms.empty() || !write(dir + "/last.md"))
		return;
	const std::time_t t = std::time(nullptr);
	char name[64];
	std::strftime(name, sizeof name, "%Y-%m-%d-%H%M%S.md", std::localtime(&t));
	std::error_code ec;
	std::filesystem::copy_file(dir + "/last.md", dir + "/" + name, std::filesystem::copy_options::overwrite_existing, ec);
	std::vector<std::filesystem::path> old;
	for (const auto &e : std::filesystem::directory_iterator(dir, ec))
		if (e.path().filename() != "last.md" && e.path().extension() == ".md")
			old.push_back(e.path());
	std::sort(old.begin(), old.end());
	for (size_t i = 0; i + 20 < old.size(); i++)
		std::filesystem::remove(old[i], ec);
}

} // namespace trace::report
