// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

// The Memory tab's report: a folder an agent reads with grep. summary.md says
// what grew, what peaked and what no owner claims; samples.jsonl has every
// figure of every sample, one fact a line.

#include "src/client/app/shell/host/memory.h"
#include "library/trace/marks.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/base/version.h"
#include "src/shared/platform/system/filesys.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace shell {

namespace {

// One figure through the samples: (t ms, bytes) points.
using Series = std::vector<std::pair<double, double>>;

// The figures a report follows, by name ("process: engine", "owner: Far view: tiles").
std::map<std::string, Series> seriesOf(const std::vector<Memory::Sample> &samples);
// Writes the summary's markdown.
void summarize(std::ostream &out, const std::vector<Memory::Sample> &samples, const std::map<std::string, Series> &series);
// One sample's facts, a JSON object a line.
void lines(std::ostream &out, const Memory::Sample &sample);
std::string mb(double bytes);
// An owner's system: its name up to the colon ("Far view: tiles #3" → "Far view").
std::string systemOf(const std::string &owner);
// A figure's value at `t` ms: the last sample at or before it (the first, before any).
double at(const Series &s, double t);
// What each figure kept after leaving a world, against just before entering it.
void kept(std::ostream &out, const std::map<std::string, Series> &series);
// Least squares, bytes per minute, over the points from `since` (ms).
double slope(const Series &s, double since);

} // namespace

namespace memory {

std::string write(const std::string &dir, const std::vector<Memory::Sample> &samples)
{
	if (samples.empty())
		throw Refused("No memory samples yet: the first comes a second after the game starts.");
	const std::string path = dir + DIR_DELIM "memory-" + std::to_string(std::time(nullptr));
	if (!fs::CreateAllDirs(path))
		throw BaseException("memory: could not create " + path);
	const std::map<std::string, Series> series = seriesOf(samples);
	{
		std::ofstream out(path + DIR_DELIM "summary.md");
		summarize(out, samples, series);
		if (!out)
			throw BaseException("memory: could not write " + path + "/summary.md");
	}
	std::ofstream out(path + DIR_DELIM "samples.jsonl");
	for (const Memory::Sample &s : samples)
		lines(out, s);
	if (!out)
		throw BaseException("memory: could not write " + path + "/samples.jsonl");
	return path;
}

} // namespace memory

namespace {

std::map<std::string, Series> seriesOf(const std::vector<Memory::Sample> &samples)
{
	std::map<std::string, Series> out;
	for (const Memory::Sample &s : samples) {
		// a role's processes together: a renderer replaced is the same figure
		std::map<std::string, double> roles;
		double tree = 0, swapped = 0;
		for (const Memory::Process &p : s.processes) {
			roles[p.role] += (double)p.priv;
			tree += (double)p.priv;
			swapped += (double)p.swap;
		}
		for (const auto &[role, bytes] : roles)
			out["process: " + role].push_back({ s.t, bytes });
		out["tree: private"].push_back({ s.t, tree });
		out["tree: swapped"].push_back({ s.t, swapped });
		double claimed = 0;
		std::map<std::string, double> systems;
		for (const Memory::Owner &o : s.owners) {
			out["owner: " + o.name].push_back({ s.t, (double)o.bytes });
			systems[systemOf(o.name)] += (double)o.bytes;
			if (!o.gpu)
				claimed += (double)o.bytes;
		}
		for (const auto &[system, bytes] : systems)
			out["system: " + system].push_back({ s.t, bytes });
		if (s.gpu_total) {
			double tree = 0;
			for (const Memory::Process &p : s.processes)
				if (p.gpu) {
					out["gpu: " + p.role].push_back({ s.t, (double)p.gpu });
					tree += (double)p.gpu;
				}
			out["gpu: the game"].push_back({ s.t, tree });
			out["gpu: other apps"].push_back({ s.t, (double)s.gpu_others });
			out["gpu: free"].push_back({ s.t, (double)s.gpu_free });
		}
		out["heap: in use"].push_back({ s.t, (double)s.heap });
		out["heap: unaccounted"].push_back({ s.t, (double)s.heap - claimed });
		out["machine: available"].push_back({ s.t, (double)s.available });
		out["machine: swap used"].push_back({ s.t, (double)s.swap_used });
	}
	return out;
}

void summarize(std::ostream &out, const std::vector<Memory::Sample> &samples, const std::map<std::string, Series> &series)
{
	const Memory::Sample &first = samples.front(), &last = samples.back();
	const double minutes = (last.t - first.t) / 60e3;
	out << "# Memory report\n\n";
	out << "build: " << g_version_string << " (" << g_version_hash << ")\n";
	out << "written: " << std::time(nullptr) << " (unix)\n";
	out << "covers: t=" << std::lround(first.t / 1e3) << "s to t=" << std::lround(last.t / 1e3)
		<< "s since the engine started (" << std::fixed << std::setprecision(1) << minutes << " min, "
		<< samples.size() << " samples, one a second)\n";
	out << "machine: " << mb((double)last.total) << " total, " << mb((double)last.available) << " available now, swap "
		<< mb((double)last.swap_used) << " of " << mb((double)last.swap_total) << "\n\n";
	out << "Figures are bytes, shown in MB. `process:` is private memory (what it alone holds), its role's processes summed;\n"
		<< "`heap: unaccounted` is malloc's in-use less what every owner in RAM says it holds. It overstates: NVIDIA's driver mallocs\n"
		<< "a copy of each buffer it's given and, for our immutable ones, never touches it (no real memory). `process:` is what's real.\n"
		<< "`system:` is every owner of a system\n"
		<< "together (the name before the colon). `gpu:` is the graphics card, as NVIDIA's driver says (absent on other cards).\n"
		<< "Slopes are least squares, MB per minute. Lines start GROWING, KEPT or MARK, to grep.\n\n";

	// what's growing: the leak's signature, standing still
	const double recent = last.t - 5 * 60e3;
	std::vector<std::pair<double, std::string>> growing;
	for (const auto &[name, s] : series)
		if (name.rfind("machine:", 0) != 0)
			growing.push_back({ slope(s, recent), name });
	std::sort(growing.begin(), growing.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
	out << "## Growing (last 5 min)\n\n";
	for (const auto &[rate, name] : growing) {
		if (rate * 5 < 1e6)
			break; // under a megabyte in five minutes: noise
		out << "- GROWING " << name << ": " << mb(rate) << "/min, now " << mb(series.at(name).back().second) << "\n";
	}
	out << "\n## Every figure\n\n";
	out << "| figure | now | start | peak | peak at | MB/min (all) | MB/min (5 min) |\n|---|---|---|---|---|---|---|\n";
	for (const auto &[name, s] : series) {
		const auto peak = std::max_element(s.begin(), s.end(), [](const auto &a, const auto &b) { return a.second < b.second; });
		out << "| " << name << " | " << mb(s.back().second) << " | " << mb(s.front().second) << " | " << mb(peak->second)
			<< " | t=" << std::lround(peak->first / 1e3) << "s | " << mb(slope(s, 0)) << " | " << mb(slope(s, recent)) << " |\n";
	}

	out << "\n## Processes now\n\n| pid | role | private | pss | rss | swap | gpu |\n|---|---|---|---|---|---|---|\n";
	for (const Memory::Process &p : last.processes)
		out << "| " << p.pid << " | " << p.role << " | " << mb((double)p.priv) << " | " << mb((double)p.pss) << " | "
			<< mb((double)p.rss) << " | " << mb((double)p.swap) << " | " << mb((double)p.gpu) << " |\n";

	out << "\n## Owners now\n\n| owner | where | bytes | count |\n|---|---|---|---|\n";
	for (const Memory::Owner &o : last.owners)
		out << "| " << o.name << " | " << (o.gpu ? "gpu" : "ram") << " | " << mb((double)o.bytes) << " | " << o.count << " |\n";

	kept(out, series);

	out << "\n## Marks (what happened, for lining up with the figures)\n\n";
	for (const trace::Mark &mark : trace::marksSince(0))
		out << "- MARK t=" << std::lround(mark.start / 1e9) << "s [" << mark.lane << "] " << mark.what << "\n";

	out << "\n## samples.jsonl\n\nOne fact a line, every sample: `{\"t\":ms,\"what\":\"process\",\"pid\",\"role\",\"private\",\"pss\",\"rss\",\"swap\"}`,\n"
		<< "`{\"t\",\"what\":\"owner\",\"name\",\"bytes\",\"count\"}`, `{\"t\",\"what\":\"heap\",\"bytes\"}`,\n"
		<< "`{\"t\",\"what\":\"machine\",\"available\",\"swapUsed\"}`. grep '\"owner\".*Far view' to follow one.\n";
}

void lines(std::ostream &out, const Memory::Sample &s)
{
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	const auto line = [&](Json::Value v) {
		v["t"] = std::round(s.t);
		out << Json::writeString(writer, v) << "\n";
	};
	for (const Memory::Process &p : s.processes) {
		Json::Value v;
		v["what"] = "process";
		v["pid"] = (Json::Int64)p.pid;
		v["role"] = p.role;
		v["private"] = (Json::UInt64)p.priv;
		v["pss"] = (Json::UInt64)p.pss;
		v["rss"] = (Json::UInt64)p.rss;
		v["swap"] = (Json::UInt64)p.swap;
		v["gpu"] = (Json::UInt64)p.gpu;
		line(v);
	}
	for (const Memory::Owner &o : s.owners) {
		Json::Value v;
		v["what"] = "owner";
		v["name"] = o.name;
		v["bytes"] = (Json::UInt64)o.bytes;
		v["count"] = (Json::UInt64)o.count;
		v["where"] = o.gpu ? "gpu" : "ram";
		line(v);
	}
	Json::Value heap;
	heap["what"] = "heap";
	heap["bytes"] = (Json::UInt64)s.heap;
	line(heap);
	Json::Value machine;
	machine["what"] = "machine";
	machine["available"] = (Json::UInt64)s.available;
	machine["swapUsed"] = (Json::UInt64)s.swap_used;
	line(machine);
	if (s.gpu_total) {
		Json::Value card;
		card["what"] = "gpu";
		card["total"] = (Json::UInt64)s.gpu_total;
		card["free"] = (Json::UInt64)s.gpu_free;
		card["others"] = (Json::UInt64)s.gpu_others;
		line(card);
	}
}

std::string systemOf(const std::string &owner)
{
	const size_t colon = owner.find(':');
	return colon == std::string::npos ? owner : owner.substr(0, colon);
}

double at(const Series &s, double t)
{
	double v = s.front().second;
	for (const auto &[when, value] : s) {
		if (when > t)
			break;
		v = value;
	}
	return v;
}

void kept(std::ostream &out, const std::map<std::string, Series> &series)
{
	// a world's visit: entering, then leaving; read ten seconds after, once it's let go
	constexpr double SETTLE_MS = 10e3;
	out << "\n## Kept after leaving a world (each figure ten seconds after leaving, against just before entering)\n\n";
	double entered = -1;
	bool any = false;
	for (const trace::Mark &mark : trace::marksSince(0)) {
		if (mark.lane != "world")
			continue;
		const double t = mark.start / 1e6;
		if (mark.what == "entered the world") {
			entered = t;
			continue;
		}
		if (entered < 0)
			continue;
		any = true;
		out << "Visit t=" << std::lround(entered / 1e3) << "s to t=" << std::lround(t / 1e3) << "s:\n";
		for (const auto &[name, s] : series) {
			if (name.rfind("system:", 0) != 0 && name.rfind("process:", 0) != 0 && name.rfind("heap:", 0) != 0)
				continue;
			if (s.back().first < t + SETTLE_MS)
				continue; // not ten seconds on yet
			const double before = at(s, entered - 1), after = at(s, t + SETTLE_MS);
			if (after - before >= 1e6)
				out << "- KEPT " << name << ": " << mb(after - before) << " more than before (" << mb(before) << " → " << mb(after) << ")\n";
		}
		entered = -1;
	}
	if (!any)
		out << "No world left yet.\n";
}

std::string mb(double bytes)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(std::fabs(bytes) >= 100e6 ? 0 : 1) << bytes / 1e6 << " MB";
	return out.str();
}

double slope(const Series &s, double since)
{
	double n = 0, st = 0, sv = 0, stt = 0, stv = 0;
	for (const auto &[t, v] : s) {
		if (t < since)
			continue;
		const double m = t / 60e3; // minutes
		n++;
		st += m;
		sv += v;
		stt += m * m;
		stv += m * v;
	}
	const double d = n * stt - st * st;
	return n < 2 || d == 0 ? 0 : (n * stv - st * sv) / d;
}

} // namespace

} // namespace shell
