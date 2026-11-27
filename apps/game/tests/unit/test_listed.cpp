// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/shared/platform/online/listed.h"
#include "src/shared/platform/system/filesys.h"
#include <json/json.h>
#include <memory>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

class TestListed : public TestBase {
public:
	TestListed() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestListed"; }

	void runTests(IGameDef *gamedef);

	void testRecordedWhileOpen();
	void testAnotherRunsRecordKept();
};

static TestListed g_test_instance;

void TestListed::runTests(IGameDef *gamedef)
{
	TEST(testRecordedWhileOpen);
	TEST(testAnotherRunsRecordKept);
}

namespace {

Json::Value readJson(const std::string &file)
{
	std::string text;
	UASSERT(fs::ReadFile(file, text));
	Json::Value v;
	std::string errors;
	std::unique_ptr<Json::CharReader> reader(Json::CharReaderBuilder().newCharReader());
	UASSERT(reader->parse(text.data(), text.data() + text.size(), &v, &errors));
	return v;
}

} // namespace

// A world opened is a running/ record as terra writes one (apps/cli: running.ts), marked the game's,
// the user's alone, holding the secret the gate admits by; closed, it's gone.
void TestListed::testRecordedWhileOpen()
{
	const std::string dir = getTestTempDirectory() + DIR_DELIM + "running";
	fs::RecursiveDelete(dir);
	const std::string file = dir + DIR_DELIM + "41234.json";
	{
		auto listed = online::Listed::open({ dir, 41234, "Bramble Mesa", "/worlds/bramble", "cody" });
		UASSERTEQ(size_t, listed->secret().size(), 64);
		Json::Value r = readJson(file);
		UASSERT(r["ref"].asString() == "terra://localhost:41234");
		UASSERT(r["title"].asString() == "Bramble Mesa");
		UASSERT(r["server"].asString() == "/worlds/bramble");
		UASSERT(r["owner"].asString() == "cody");
		UASSERT(r["by"].asString() == "game");
		UASSERT(r["secret"].asString() == listed->secret());
		UASSERT(r["port"].asUInt() == 41234 && r["mods"].isArray() && r["started"].isIntegral());
#ifndef _WIN32
		UASSERT(r["pid"].asInt64() == getpid());
		struct stat st;
		UASSERT(stat(file.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
		UASSERT(stat(dir.c_str(), &st) == 0 && (st.st_mode & 0777) == 0700);
#endif
		UASSERT(online::Listed::open({ dir, 41235, "b", "/b", "cody" })->secret() != listed->secret());
	}
	UASSERT(!fs::PathExists(file));
	UASSERT(!fs::PathExists(dir + DIR_DELIM + "41235.json"));
}

// A record another run wrote over ours on the port (we're gone; terra started one there) stays.
void TestListed::testAnotherRunsRecordKept()
{
	const std::string dir = getTestTempDirectory() + DIR_DELIM + "running";
	fs::RecursiveDelete(dir);
	const std::string file = dir + DIR_DELIM + "41236.json";
	{
		auto listed = online::Listed::open({ dir, 41236, "a", "/a", "cody" });
		UASSERT(fs::safeWriteToFile(file, "{\"pid\": 1}\n"));
	}
	UASSERT(fs::PathExists(file));
}
