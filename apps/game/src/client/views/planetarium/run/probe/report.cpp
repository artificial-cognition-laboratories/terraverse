// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/run/probe/report.h"
#include "library/world/terragen/src/base/progress.h"
#include <json/json.h>
#include <algorithm>

namespace planetarium::probe {

namespace {
Json::Value step(const StepRecord &s);
Json::Value frames(std::vector<double> work); // the frames' work, summarised
Json::Value making(const std::string &code);
}

Json::Value report(const Header &header, const std::vector<StepRecord> &steps)
{
	Json::Value out;
	out["code"] = header.code;
	out["script"] = header.script;
	out["cell"] = header.cell;
	out["width"] = header.width;
	out["height"] = header.height;
	out["cores"] = header.cores;
	out["steps"] = Json::arrayValue;
	for (const StepRecord &s : steps)
		out["steps"].append(step(s));
	out["making"] = making(header.code);
	return out;
}

namespace {

Json::Value step(const StepRecord &s)
{
	Json::Value out;
	out["name"] = s.name;
	out["seconds"] = s.seconds;
	out["moving"] = s.moving;
	out["settled"] = s.settled ? Json::Value(*s.settled) : Json::Value(Json::nullValue);
	out["lost"] = s.lost;
	out["frames"] = frames(s.work);
	out["drawn"]["tiles"] = (Json::UInt64)s.tiles;
	out["drawn"]["draws"] = (Json::UInt64)s.draws;
	out["drawn"]["vertices"] = (Json::UInt64)s.vertices;
	for (const auto &[name, thread] : s.threads) {
		Json::Value &t = out["threads"][name];
		t["threads"] = thread.threads;
		for (const auto &[zone, z] : thread.zones) {
			t["zones"][zone]["ms"] = z.ms;
			t["zones"][zone]["max"] = z.max;
			t["zones"][zone]["count"] = (Json::UInt64)z.count;
		}
	}
	for (const auto &[name, c] : s.counters) {
		out["counters"][name]["sum"] = c.sum;
		out["counters"][name]["max"] = c.max;
	}
	return out;
}

Json::Value frames(std::vector<double> work)
{
	Json::Value out;
	out["count"] = (Json::UInt64)work.size();
	if (work.empty())
		return out;
	std::sort(work.begin(), work.end());
	const auto at = [&](double q) { return work[std::min(work.size() - 1, (size_t)(q * work.size()))]; };
	out["p50"] = at(0.5);
	out["p95"] = at(0.95);
	out["max"] = work.back();
	out["over16"] = (Json::UInt64)std::count_if(work.begin(), work.end(), [](double ms) { return ms > 16.7; });
	out["over50"] = (Json::UInt64)std::count_if(work.begin(), work.end(), [](double ms) { return ms > 50; });
	return out;
}

Json::Value making(const std::string &code)
{
	const terragen::Making m = terragen::making(code);
	Json::Value out;
	out["seconds"] = m.seconds;
	out["done"] = m.done;
	out["steps"] = Json::arrayValue;
	for (const terragen::StepRecord &s : m.steps) {
		Json::Value step;
		step["name"] = s.name;
		step["seconds"] = s.seconds;
		step["kept"] = s.kept;
		out["steps"].append(step);
	}
	return out;
}

} // namespace

} // namespace planetarium::probe
