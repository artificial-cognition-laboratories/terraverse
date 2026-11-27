// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/perf_report.h"
#include "library/trace/marks.h"
#include "library/trace/trace.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/base/version.h"
#include "src/shared/platform/system/filesys.h"
#include <json/json.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace shell {

namespace {

// A frame slower than this many times the median is a hitch, and listed.
constexpr double HITCH = 2.0;
constexpr size_t WORST = 20;
// What a frame names of its main thread: its costliest zones.
constexpr size_t PARTS = 6;

struct Stats {
	size_t n = 0;
	double total = 0, p50 = 0, p95 = 0, max = 0;
};

Stats statsOf(std::vector<double> values);
std::string ms(double v);
void summarize(std::ostream &out, const std::vector<trace::Frame> &frames, const std::vector<trace::Track> &tracks);
// A frame's main-thread zones, costliest first, at most `most`.
std::vector<std::pair<std::string, double>> mainParts(const trace::Frame &frame, size_t most);
void chrome(const std::string &path, const std::vector<trace::Track> &tracks);
std::ofstream create(const std::string &path);

} // namespace

namespace perf_report {

std::string write(const std::string &dir)
{
	const std::string path = dir + DIR_DELIM "perf-" + std::to_string(std::time(nullptr));
	if (!fs::CreateAllDirs(path))
		throw BaseException("debugger: could not create " + path);
	const std::vector<trace::Frame> frames = trace::framesSince(0);
	const std::vector<trace::Track> tracks = trace::between(0, trace::now());

	{
		std::ofstream out = create(path + DIR_DELIM "summary.md");
		summarize(out, frames, tracks);
	}
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	{
		std::ofstream out = create(path + DIR_DELIM "frames.jsonl");
		for (const trace::Frame &f : frames) {
			Json::Value v;
			v["frame"] = (Json::UInt64)f.id;
			v["t"] = std::round(f.start / 1e4) / 1e2; // ms, two places
			v["ms"] = std::round((f.end - f.start) / 1e4) / 1e2;
			Json::Value &main = v["main"] = Json::Value(Json::objectValue);
			for (const auto &[name, took] : mainParts(f, PARTS))
				main[name] = std::round(took * 100) / 100;
			Json::Value &counters = v["counters"] = Json::Value(Json::objectValue);
			for (const auto &[id, value] : f.counters)
				counters[trace::nameOf(id)] = value;
			out << Json::writeString(writer, v) << "\n";
		}
	}
	{
		std::ofstream out = create(path + DIR_DELIM "spans.jsonl");
		for (const trace::Track &track : tracks)
			for (const trace::Span &span : track.spans) {
				Json::Value v;
				v["thread"] = track.thread;
				v["zone"] = trace::nameOf(span.name);
				v["depth"] = span.depth;
				v["t"] = std::round(span.start / 1e4) / 1e2;
				v["ms"] = std::round((span.end - span.start) / 1e3) / 1e3;
				out << Json::writeString(writer, v) << "\n";
			}
	}
	chrome(path + DIR_DELIM "trace.json", tracks);
	return path;
}

} // namespace perf_report

namespace {

void summarize(std::ostream &out, const std::vector<trace::Frame> &frames, const std::vector<trace::Track> &tracks)
{
	out << "# Performance report\n\n";
	out << "build: " << g_version_string << " (" << g_version_hash << ")\n";
	out << "written: " << std::time(nullptr) << " (unix)\n";
	if (frames.empty()) {
		out << "\nNo frames recorded.\n";
		return;
	}
	const double from = frames.front().start / 1e6, to = frames.back().end / 1e6;
	std::vector<double> times;
	for (const trace::Frame &f : frames)
		times.push_back((f.end - f.start) / 1e6);
	const Stats frame = statsOf(times);
	out << "covers: t=" << ms(from) << " to t=" << ms(to) << " since the engine started, " << frames.size() << " frames\n";
	out << "\nTimes are ms. Zones nest: a zone's time is in its parent's too. Lines starting HITCH, ZONE, THREAD, COUNTER grep alone.\n";

	out << "\n## Frames\n\n";
	out << "FRAMES p50 " << ms(frame.p50) << " · p95 " << ms(frame.p95) << " · max " << ms(frame.max) << " · fps "
		<< std::fixed << std::setprecision(1) << frames.size() / std::max((to - from) / 1e3, 1e-9) << "\n";

	out << "\n## Worst frames (over " << HITCH << "× the median), with the main thread's costliest zones\n\n";
	std::vector<const trace::Frame *> worst;
	for (const trace::Frame &f : frames)
		if ((f.end - f.start) / 1e6 > frame.p50 * HITCH)
			worst.push_back(&f);
	std::sort(worst.begin(), worst.end(), [](const auto *a, const auto *b) { return a->end - a->start > b->end - b->start; });
	if (worst.size() > WORST)
		worst.resize(WORST);
	for (const trace::Frame *f : worst) {
		out << "- HITCH frame " << f->id << " t=" << ms(f->start / 1e6) << " took " << ms((f->end - f->start) / 1e6) << ":";
		for (const auto &[name, took] : mainParts(*f, PARTS))
			out << " " << name << " " << ms(took) << ";";
		out << "\n";
	}
	if (worst.empty())
		out << "None.\n";

	out << "\n## Threads, and their zones by total time\n";
	for (const trace::Track &track : tracks) {
		std::map<std::string, std::vector<double>> zones;
		double busy = 0;
		for (const trace::Span &span : track.spans) {
			const double took = (span.end - span.start) / 1e6;
			zones[trace::nameOf(span.name)].push_back(took);
			if (span.depth == 0)
				busy += took;
		}
		const double kept = std::max((trace::now() - track.since) / 1e6, 1e-9);
		out << "\nTHREAD " << track.thread << ": busy " << std::fixed << std::setprecision(0) << busy / kept * 100
			<< "% of the " << ms(kept) << " it still has\n\n";
		std::vector<std::pair<std::string, Stats>> ranked;
		for (auto &[name, values] : zones)
			ranked.push_back({ name, statsOf(std::move(values)) });
		std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.second.total > b.second.total; });
		for (const auto &[name, s] : ranked)
			out << "- ZONE [" << track.thread << "] " << name << ": total " << ms(s.total) << " · n " << s.n << " · p50 "
				<< ms(s.p50) << " · p95 " << ms(s.p95) << " · max " << ms(s.max) << "\n";
	}

	out << "\n## Counters, over the frames that reported them\n\n";
	std::map<std::string, std::vector<double>> counters;
	for (const trace::Frame &f : frames)
		for (const auto &[id, value] : f.counters)
			counters[trace::nameOf(id)].push_back(value);
	for (auto &[name, values] : counters) {
		double sum = 0;
		for (double v : values)
			sum += v;
		const double mean = sum / values.size();
		const Stats s = statsOf(std::move(values));
		out << "- COUNTER " << name << ": mean " << mean << " · max " << s.max << "\n";
	}

	out << "\n## Marks\n\n";
	for (const trace::Mark &mark : trace::marksSince(0))
		out << "- MARK t=" << ms(mark.start / 1e6) << " [" << mark.lane << "] " << mark.what << " (" << ms((mark.end - mark.start) / 1e6) << ")\n";

	out << "\n## Files\n\nframes.jsonl: `{\"frame\",\"t\",\"ms\",\"main\":{zone: ms},\"counters\":{…}}` a line.\n"
		<< "spans.jsonl: `{\"thread\",\"zone\",\"depth\",\"t\",\"ms\"}` a line, every zone kept.\n"
		<< "trace.json: Chrome's trace format, for Perfetto (ui.perfetto.dev) or chrome://tracing.\n";
}

std::vector<std::pair<std::string, double>> mainParts(const trace::Frame &frame, size_t most)
{
	std::vector<std::pair<std::string, double>> out;
	for (const trace::Frame::Thread &t : frame.threads) {
		if (t.thread != "Main")
			continue;
		for (const auto &[id, took] : t.parts)
			out.push_back({ trace::nameOf(id), took / 1e6 });
	}
	std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
	if (out.size() > most)
		out.resize(most);
	return out;
}

Stats statsOf(std::vector<double> values)
{
	Stats s;
	s.n = values.size();
	if (values.empty())
		return s;
	std::sort(values.begin(), values.end());
	for (double v : values)
		s.total += v;
	s.p50 = values[values.size() / 2];
	s.p95 = values[std::min(values.size() - 1, values.size() * 95 / 100)];
	s.max = values.back();
	return s;
}

std::string ms(double v)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(v >= 100 ? 0 : v >= 1 ? 2 : 3) << v << " ms";
	return out.str();
}

void chrome(const std::string &path, const std::vector<trace::Track> &tracks)
{
	Json::Value events(Json::arrayValue);
	int tid = 0;
	for (const trace::Track &track : tracks) {
		tid++;
		Json::Value meta;
		meta["ph"] = "M";
		meta["name"] = "thread_name";
		meta["pid"] = 1;
		meta["tid"] = tid;
		meta["args"]["name"] = track.thread;
		events.append(std::move(meta));
		for (const trace::Span &span : track.spans) {
			Json::Value e;
			e["ph"] = "X";
			e["name"] = trace::nameOf(span.name);
			e["pid"] = 1;
			e["tid"] = tid;
			e["ts"] = span.start / 1e3; // µs
			e["dur"] = (span.end - span.start) / 1e3;
			events.append(std::move(e));
		}
	}
	Json::Value root;
	root["traceEvents"] = std::move(events);
	std::ofstream out = create(path);
	out << Json::FastWriter().write(root);
}

std::ofstream create(const std::string &path)
{
	std::ofstream out(path);
	if (!out)
		throw BaseException("debugger: could not write " + path);
	return out;
}

} // namespace

} // namespace shell
