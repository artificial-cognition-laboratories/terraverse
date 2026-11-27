// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "library/trace/owned.h"
#include "src/client/app/shell/host/gpu.h"
#include "src/client/app/shell/host/memory.h"
#include "src/client/app/shell/host/perf_report.h"
#include "src/shared/platform/system/filesys.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

class TestMemory : public TestBase {
public:
	TestMemory() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMemory"; }

	void runTests(IGameDef *gamedef);

	void testSampledAndReported();
	void testPerfReport();
};

static TestMemory g_test_instance;

void TestMemory::runTests(IGameDef *gamedef)
{
	TEST(testSampledAndReported);
	TEST(testPerfReport);
}

namespace {

std::string read(const std::string &path)
{
	std::ifstream in(path);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

} // namespace

void TestMemory::testSampledAndReported()
{
	const std::string dir = fs::TempPath() + DIR_DELIM "terraverse-memory-test";
	fs::RecursiveDelete(dir);
	trace::owned("Test: things", 3'000'000, 7);
	{
		shell::Memory memory({ dir });
		// the first sample at once, the next a second on
		std::this_thread::sleep_for(std::chrono::milliseconds(1300));
		const shell::Memory::Since got = memory.since(0);
		UASSERT(got.samples.size() >= 2);
		UASSERT(got.newest >= 2);
		const Json::Value &first = got.samples[0];
#ifdef __linux__
		UASSERT(first["processes"].size() >= 1);
		UASSERTEQ(std::string, first["processes"][0]["role"].asString(), "engine");
		UASSERT(first["processes"][0]["private"].asUInt64() > 0);
		UASSERT(first["heap"].asUInt64() > 0);
		UASSERT(first["total"].asUInt64() > 0);
#endif
		// the card, where NVIDIA's driver is: read on the first (dear) sample
		if (shell::Gpu::open()) {
			UASSERT(first["gpuTotal"].asUInt64() > 0);
			UASSERT(first["gpuFree"].asUInt64() <= first["gpuTotal"].asUInt64());
		}
		bool found = false;
		for (const Json::Value &o : first["owners"])
			found |= o["name"].asString() == "Test: things" && o["bytes"].asUInt64() == 3'000'000 && o["count"].asUInt64() == 7;
		UASSERT(found);
		// nothing newer than the newest
		UASSERT(memory.since(got.newest).samples.empty());

		const std::string path = memory.report();
		const std::string summary = read(path + DIR_DELIM "summary.md");
		UASSERT(summary.find("# Memory report") != std::string::npos);
		UASSERT(summary.find("owner: Test: things") != std::string::npos);
		UASSERT(summary.find("heap: unaccounted") != std::string::npos);
		const std::string lines = read(path + DIR_DELIM "samples.jsonl");
		UASSERT(lines.find("\"what\":\"owner\"") != std::string::npos);
		UASSERT(lines.find("\"name\":\"Test: things\"") != std::string::npos);
	}
	trace::disowned("Test: things");
	fs::RecursiveDelete(dir);
}

void TestMemory::testPerfReport()
{
	const std::string dir = fs::TempPath() + DIR_DELIM "terraverse-perf-test";
	fs::RecursiveDelete(dir);
	const std::string path = shell::perf_report::write(dir);
	UASSERT(read(path + DIR_DELIM "summary.md").find("# Performance report") != std::string::npos);
	UASSERT(fs::PathExists(path + DIR_DELIM "frames.jsonl"));
	UASSERT(fs::PathExists(path + DIR_DELIM "spans.jsonl"));
	UASSERT(read(path + DIR_DELIM "trace.json").find("traceEvents") != std::string::npos);
	fs::RecursiveDelete(dir);
}
