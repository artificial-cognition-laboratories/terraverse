// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "shell/loading.h"
#include "terragen/code.h"
#include "terragen/progress.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace shell {

namespace {

// The stages, in order: which of terragen's steps each gathers, and how long
// it takes made afresh on a 100k world (`bun perf:gen`, sprint 110), the
// rivers' growing with the planet
struct Stage
{
	const char *id;
	const char *label;
	std::vector<const char *> steps;
	double seconds;
	bool by_size;
};

const Stage STAGES[] = {
	{ "layout", "Laying out the system", { "system laid out" }, 0.7, false },
	{ "plates", "Moving the plates", { "grid kept", "plates" }, 0.5, false },
	{ "climate", "Setting the climate", { "crust and climate" }, 0.35, false },
	{ "erosion", "Wearing the land", { "erosion", "lakes and rivers", "geology" }, 0.15, false },
	{ "rivers", "Tracing the rivers", { "rivers' runs", "channels", "river levels" }, 4.8, true },
	{ "planet", "Seeing it from orbit", { "coarse planet" }, 6.8, false },
};

constexpr double SIZE = 628320; // the world the times are for (blocks round)
constexpr double GUESSED_MOST = 0.95; // a running stage that can't count, by time alone, never shows done

} // namespace

Json::Value loadingOf(const std::string &code, double since)
{
	const terragen::Making made = terragen::making(code);
	// (a moon's code is its home's and "@" its own: timed by its home's size)
	const double size = terragen::WorldCode::decode(code.substr(0, code.find('@'))).size;
	struct Seen
	{
		bool any = false, running = false, all_kept = true;
		double seconds = 0;
		int64_t done = 0, of = 0;
	};
	constexpr size_t N = sizeof STAGES / sizeof STAGES[0];
	Seen seen[N];
	for (const terragen::StepRecord &r : made.steps) {
		if (r.finished && r.begun < since)
			continue; // an earlier showing's
		for (size_t k = 0; k < N; k++)
			if (std::any_of(STAGES[k].steps.begin(), STAGES[k].steps.end(), [&](const char *s) { return r.name == s; })) {
				Seen &s = seen[k];
				s.any = true;
				s.seconds += r.seconds;
				s.all_kept = s.all_kept && r.kept;
				if (!r.finished) {
					s.running = true;
					s.done = r.done;
					s.of = r.of;
				}
			}
	}
	// the last stage seen: any before it that never ran was kept
	int last = -1;
	for (size_t k = 0; k < N; k++)
		if (seen[k].any)
			last = (int)k;

	Json::Value stages(Json::arrayValue);
	double total = 0, behind = 0;
	for (size_t k = 0; k < N; k++) {
		const Stage &stage = STAGES[k];
		const Seen &s = seen[k];
		const double weight = stage.seconds * (stage.by_size ? size / SIZE : 1);
		total += weight;
		Json::Value out;
		out["id"] = stage.id;
		out["label"] = stage.label;
		out["seconds"] = std::round(s.seconds * 10) / 10;
		Json::Value share; // null: not counted
		if (s.running) {
			out["state"] = "running";
			const double counted = s.of > 0 ? std::clamp((double)s.done / s.of, 0.0, 1.0) : -1;
			if (counted >= 0)
				share = std::round(counted * 100) / 100;
			behind += weight * (counted >= 0 ? counted : std::min(GUESSED_MOST, s.seconds / weight));
		} else if (s.any) {
			out["state"] = s.all_kept ? "kept" : "done";
			behind += weight;
		} else if ((int)k < last) {
			out["state"] = "kept";
			behind += weight;
		} else {
			out["state"] = "waiting";
		}
		out["share"] = share;
		stages.append(out);
	}
	Json::Value loading;
	loading["share"] = std::round(behind / total * 100) / 100;
	loading["stages"] = stages;
	return loading;
}

} // namespace shell
