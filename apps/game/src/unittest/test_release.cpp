// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "exceptions.h"
#include "filesys.h"
#include "release/release.h"
#include "settings.h"
#include <fstream>

using namespace release;

// A throwaway key and one signature made with it, by openssl: the tool CI signs with.
static const char *const KEY = R"(-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEAVhPpcp2Lnp+8HxPU66ZtJL870nhPCl5XWcWwDwdq66g=
-----END PUBLIC KEY-----
)";
static const char *const MANIFEST = "{\"version\":\"1.2.3\"}\n";
static const char *const SIGNATURE = "hixukWzthgzTjltqRPO77V4r/IVUOqobvdT+3sTS3KDUtqFM1IDgvp/vboRJv6kPdXyM1o6Ve/55JZKxupejAw==";

class TestRelease : public TestBase
{
public:
	TestRelease() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestRelease"; }

	void runTests(IGameDef *gamedef);

	void testParse();
	void testVerify();
	void testOpenWorld();
};

static TestRelease g_test_instance;

void TestRelease::runTests(IGameDef *gamedef)
{
	TEST(testParse);
	TEST(testVerify);
	TEST(testOpenWorld);
}

void TestRelease::testParse()
{
	UASSERT(parse("v0.1.10") > parse("v0.1.9"));
	UASSERT(parse("0.2") > parse("0.1.9"));
	UASSERT(parse("v0.1.1") == parse("0.1.1"));
	UASSERT(parse("v0.3.0-beta.2") > parse("0.3.0-beta.1"));
	UASSERT(parse("0.3.0-beta.10") > parse("0.3.0-beta.9"));
	UASSERT(parse("0.3.0") > parse("0.3.0-beta.2"));
	UASSERT(parse("0.3.0-rc.1") > parse("0.3.0-beta.9"));
	UASSERT(parse("0.3.0-beta.1") > parse("0.2.9"));
	UASSERT(parse("0.3.0-beta.1") == parse("v0.3.0-beta.1"));
	UASSERT(!parse("0.3.0-"));
	UASSERT(!parse("6ec085b4e"));
	UASSERT(!parse("dev-test"));
	UASSERT(!parse(""));
}

void TestRelease::testVerify()
{
	UASSERT(verify(MANIFEST, SIGNATURE, KEY));
	UASSERT(latest(MANIFEST, SIGNATURE, KEY) == std::optional<std::string>("1.2.3"));
	UASSERT(!verify("{\"version\":\"6.6.6\"}\n", SIGNATURE, KEY));
	UASSERT(!latest("{\"version\":\"6.6.6\"}\n", SIGNATURE, KEY));
	UASSERT(!verify(MANIFEST, SIGNATURE, UPDATE_KEY));
	UASSERT(!verify(MANIFEST, "not base64!", KEY));
	UASSERT(!verify(MANIFEST, SIGNATURE, "not a key"));
}

void TestRelease::testOpenWorld()
{
	const std::string root = getTestTempDirectory();
	const std::string world = root + DIR_DELIM "w", backups = root + DIR_DELIM "backups";
	const std::string conf = world + DIR_DELIM "world.mt";
	fs::RecursiveDelete(world);
	fs::RecursiveDelete(backups);
	fs::CreateAllDirs(world);
	std::ofstream(conf) << "gameid = terrafirma\n";

	const auto savedBy = [&] {
		Settings s;
		UASSERT(s.readConfigFile(conf.c_str()));
		return s.exists("tf_saved_by") ? s.get("tf_saved_by") : std::string();
	};

	openWorld(world, "6ec085b4e", backups); // a development build leaves it alone
	UASSERTEQ(std::string, savedBy(), "");

	openWorld(world, "v0.1.1", backups); // never played: stamped, nothing to back up
	UASSERTEQ(std::string, savedBy(), "v0.1.1");
	UASSERT(!fs::PathExists(backups));

	std::ofstream(world + DIR_DELIM "map.sqlite") << "the map";
	openWorld(world, "v0.1.1", backups); // the same version: nothing happens
	UASSERT(!fs::PathExists(backups));

	openWorld(world, "v0.1.2", backups); // a new version: the old world is kept first
	UASSERTEQ(std::string, savedBy(), "v0.1.2");
	UASSERT(fs::PathExists(backups + DIR_DELIM "w-v0.1.1" DIR_DELIM "map.sqlite"));

	EXCEPTION_CHECK(BaseException, openWorld(world, "v0.1.1", backups)); // too old to open it
	UASSERTEQ(std::string, savedBy(), "v0.1.2");
}
