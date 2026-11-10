// Standalone terragen: where making a world spends its time (terragen/progress.h),
// and what a spawn's worth of columns costs after. `bun perf:gen` runs it cold and
// warm at a few sizes (scripts/perf-gen.sh):
//   scripts/terragen-tool.sh /tmp/perf scripts/terragen-check/perf.cpp
//   [SIZE=628320] [KEEP=<dir>] [JSON=<file>] /tmp/perf <code>
// KEEP: grids and river levels kept there, as the game keeps them (none: all
// made afresh). JSON: a line appended per run, for comparing runs.
#include "terragen/world.h"
#include "terragen/grid.h"
#include "terragen/progress.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace terragen;

static std::string quoted(const std::string &s)
{
	std::string out = "\"";
	for (char c : s)
		out += c == '"' || c == '\\' ? std::string("\\") + c : std::string(1, c);
	return out + "\"";
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "usage: [SIZE=<blocks round>] [KEEP=<dir>] [JSON=<file>] perf <code>\n");
		return 2;
	}
	WorldCode code = WorldCode::decode(argv[1]);
	if (const char *size = std::getenv("SIZE")) {
		code.size = std::stoi(size);
		code.keepGrid(); // (the size is in what a grid's kept by)
		code.text += "@" + std::string(size);
	}
	if (const char *keep = std::getenv("KEEP"))
		keepGridsIn(keep, "perf", "perf"); // (its own folder: no other build's grids to tell apart)

	std::shared_ptr<const World> world = worldFor(code);
	const Making made = making(code.text);

	// a spawn's worth of columns (128², one thread), at the map's middle
	constexpr int N = 128;
	const auto started = std::chrono::steady_clock::now();
	const double R = World::radiusOf(code) / 65536.0;
	for (int y = 0; y < N; y++)
		for (int x = 0; x < N; x++) {
			const double la = (y - N / 2) / R, lo = (x - N / 2) / R;
			const Place at((int32_t)std::llround(std::cos(la) * std::cos(lo) * PLACE_ONE), (int32_t)std::llround(std::sin(la) * PLACE_ONE),
					(int32_t)std::llround(std::cos(la) * std::sin(lo) * PLACE_ONE));
			world->ground(at, world->sunlight(at, std::nullopt));
		}
	const double columns_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() / (N * N);

	std::printf("%s %s\n", code.text.c_str(), breakdown(made).c_str());
	std::printf("columns: %.1f µs each\n", columns_us);
	// (warm: something was read from what was kept)
	bool warm = false;
	for (const StepRecord &s : made.steps)
		warm = warm || s.kept;
	if (const char *json = std::getenv("JSON")) {
		FILE *f = std::fopen(json, "a");
		if (!f) {
			std::fprintf(stderr, "perf: can't append to %s\n", json);
			return 1;
		}
		std::string line = "{\"code\":" + quoted(code.text) + ",\"size\":" + std::to_string(code.size) +
				",\"warm\":" + (warm ? "true" : "false") + ",\"seconds\":" + std::to_string(made.seconds) +
				",\"column_us\":" + std::to_string(columns_us) + ",\"steps\":[";
		for (size_t k = 0; k < made.steps.size(); k++) {
			const StepRecord &s = made.steps[k];
			line += (k ? "," : "") + std::string("{\"name\":") + quoted(s.name) + ",\"seconds\":" + std::to_string(s.seconds) +
					",\"kept\":" + (s.kept ? "true" : "false") + "}";
		}
		std::fprintf(f, "%s]}\n", line.c_str());
		std::fclose(f);
	}
	return 0;
}
