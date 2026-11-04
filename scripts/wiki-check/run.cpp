// Runs every wiki scene (from specs.ts, on stdin) on the world's own matter
// (sandbox::Stage) for a simulated few minutes; reports any that can't be made or
// stepped, and any reaction's scene that never makes its products.
// run <matter dir> <palette.json>
#include "matter/matter.h"
#include "sandbox/sandbox.h"
#include "sandbox/stage.h"
#include <json/json.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>

using namespace sandbox;

int main(int argc, char **argv)
{
	matter::Matter m = matter::Matter::load(argv[1]);
	Json::Value palette, all;
	std::ifstream(argv[2]) >> palette;
	const std::vector<StageBlock> blocks = Stage::read(palette["scene"]);
	std::cin >> all;
	int bad = 0;
	for (const Json::Value &d : all) {
		const Json::Value &s = d["spec"];
		auto t0 = std::chrono::steady_clock::now();
		try {
			std::vector<std::pair<std::string, double>> air;
			for (const std::string &id : s["air"].getMemberNames())
				air.push_back({ id, s["air"][id].asDouble() });
			Stage stage(Stage::Opts{ m, blocks, s["size"][0].asInt(), s["size"][1].asInt(), s["size"][2].asInt(),
					s["ambient"].asDouble(), s["pressure"].asDouble(), 9.81, air });
			stage.setUp(s, s["ambient"].asDouble());
			std::vector<double> before;
			for (const Json::Value &id : d["expect"])
				before.push_back(stage.moles(id.asString()));
			stage.step(300);
			std::string missing;
			for (Json::ArrayIndex k = 0; k < d["expect"].size(); k++)
				// (a gas leaves by the open top: it may well be gone by the end)
				if (stage.moles(d["expect"][k].asString()) <= before[k] + 1e-9 &&
						m.substance(d["expect"][k].asString()).state(s["ambient"].asDouble(), s["pressure"].asDouble()) != matter::State::Gas)
					missing += (missing.empty() ? "" : ", ") + d["expect"][k].asString();
			// a scene's own say: how full a block ended, and whether any of it got there
			const std::vector<Stage::Cell> ended = stage.view();
			const auto fullness = [&](const Json::Value &at) {
				const Stage::Cell &c = ended.at(((size_t)at[1].asInt() * s["size"][2].asInt() + at[2].asInt()) * s["size"][0].asInt() + at[0].asInt());
				const bool leveled = c.block >= 0 && (blocks[c.block].spec.leveled || blocks[c.block].spec.state == matter::State::Liquid);
				return c.fill.substance >= 0 ? c.fill.share : leveled ? c.level : c.block >= 0 && blocks[c.block].spec.matter != "air" ? 1.0 : 0.0;
			};
			for (const Json::Value &w : d["wet"]) {
				const double f = fullness(w["at"]);
				if (f < w.get("min", 0).asDouble() || f > w.get("max", 2).asDouble())
					missing += (missing.empty() ? "" : ", ") + std::string("fullness ") + std::to_string(f) + " at [" + w["at"][0].asString() + "," + w["at"][1].asString() + "," + w["at"][2].asString() + "]";
			}
			double hottest = 0, active = 0, coldest = 1e9;
			std::string end;
			for (const Stage::Cell &c : stage.view()) {
				hottest = std::max(hottest, c.temperature);
				coldest = std::min(coldest, c.temperature);
				if (c.block >= 0 && blocks[c.block].spec.matter != "air" && end.find(blocks[c.block].spec.node) == std::string::npos)
					end += " " + blocks[c.block].spec.node;
				active = std::max(active, std::fabs(c.activity));
			}
			// how hot it got must be what it says (a fire that needs its air, and doesn't catch without)
			if (d.isMember("hottest") && (hottest < d["hottest"].get("min", 0).asDouble() || hottest > d["hottest"].get("max", 1e9).asDouble()))
				missing += (missing.empty() ? "" : ", ") + std::string("a different fire");
			if (d.isMember("active") && active > d["active"]["max"].asDouble())
				missing += (missing.empty() ? "" : ", ") + std::string("something still reacting");
			if (d.isMember("left")) { // how much of a substance is left
				const double there = stage.moles(d["left"]["id"].asString());
				if (there < d["left"].get("min", 0).asDouble() || there > d["left"].get("max", 1e12).asDouble())
					missing += (missing.empty() ? "" : ", ") + std::string("left ") + std::to_string(there) + " of " + d["left"]["id"].asString();
			}
			double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			if (!missing.empty()) {
				bad++;
				std::printf("FAIL %-34s wanted %s (hottest %.0f K, %.0f ms)\n", d["name"].asCString(), missing.c_str(), hottest, ms);
				continue;
			}
			std::printf("ok   %-34s %6.0f ms for 5 simulated min · hottest %5.0f K · coldest %.0f · most activity %.0f W · ends:%s\n", d["name"].asCString(), ms, hottest, coldest, active, end.c_str());
		} catch (const std::exception &e) {
			bad++;
			std::printf("FAIL %-34s %s\n", d["name"].asCString(), e.what());
		}
	}
	std::printf("wiki check: %d/%d scenes run\n", (int)all.size() - bad, (int)all.size());
	return bad ? 1 : 0;
}
