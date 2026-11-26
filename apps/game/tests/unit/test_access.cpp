// Terraverse: who may do what (server/access, sprint 119)

#include "tests/unit/test.h"

#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include "src/server/app/access/access.h"
#include <fstream>
#include <stdexcept>

class TestAccess : public TestBase
{
public:
	TestAccess() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestAccess"; }

	void runTests(IGameDef *gamedef);

private:
	void testEveryoneIsAPlayer();
	void testTheOwnerIsAlwaysAdmin();
	void testRolesAreKeptWithTheWorld();
	void testGrantsLastWhileTheyAreHere();
	void testABrokenFileFailsLoudly();

	std::string fresh();
};

static TestAccess g_test_instance;

void TestAccess::runTests(IGameDef *gamedef)
{
	TEST(testEveryoneIsAPlayer);
	TEST(testTheOwnerIsAlwaysAdmin);
	TEST(testRolesAreKeptWithTheWorld);
	TEST(testGrantsLastWhileTheyAreHere);
	TEST(testABrokenFileFailsLoudly);
}

// An empty world folder of its own.
std::string TestAccess::fresh()
{
	static int n = 0;
	const std::string dir = getTestTempDirectory() + DIR_DELIM "access" + std::to_string(n++);
	fs::RecursiveDelete(dir);
	fs::CreateAllDirs(dir);
	return dir;
}

void TestAccess::testEveryoneIsAPlayer()
{
	Access a(fresh(), "@cody");
	UASSERT(a.roles("@sam") == std::vector<std::string>{ "player" });
	UASSERT(a.can("@sam", "core:interact"));
	UASSERT(a.can("@sam", "core:shout"));
	UASSERT(!a.can("@sam", "core:fly"));
	UASSERT(!a.can("@sam", "commands:kick"));
}

void TestAccess::testTheOwnerIsAlwaysAdmin()
{
	Access a(fresh(), "@cody");
	UASSERT((a.roles("@cody") == std::vector<std::string>{ "player", "admin" }));
	UASSERT(a.can("@cody", "core:fly"));
	UASSERT(a.can("@cody", "commands:kick")); // a mod's too
	UASSERT(a.permissions("@cody").count("core:debug"));
	EXCEPTION_CHECK(std::invalid_argument, a.removeRole("@cody", "admin"));
	// a world nobody owns from inside (headless): nobody is its admin by name
	Access headless(fresh(), "");
	UASSERT(!headless.can("", "core:fly"));
}

void TestAccess::testRolesAreKeptWithTheWorld()
{
	const std::string world = fresh();
	{
		Access a(world, "@cody");
		a.addRole("@sam", "builder");
		a.addRole("@ren", "admin");
		EXCEPTION_CHECK(std::invalid_argument, a.addRole("@sam", "wizard"));
	}
	Access again(world, "@cody");
	UASSERT((again.roles("@sam") == std::vector<std::string>{ "player", "builder" }));
	UASSERT(again.can("@sam", "core:creative"));
	UASSERT(!again.can("@sam", "commands:kick"));
	UASSERT(again.can("@ren", "commands:kick"));
	again.removeRole("@ren", "admin");
	Access third(world, "@cody");
	UASSERT(!third.can("@ren", "commands:kick"));
}

void TestAccess::testGrantsLastWhileTheyAreHere()
{
	Access a(fresh(), "@cody");
	a.grant("@sam", "core:fly");
	a.grant("@sam", "kilns:light");
	UASSERT(a.can("@sam", "core:fly"));
	UASSERT(a.can("@sam", "kilns:light"));
	a.revoke("@sam", "kilns:light");
	UASSERT(!a.can("@sam", "kilns:light"));
	a.left("@sam");
	UASSERT(!a.can("@sam", "core:fly"));
	EXCEPTION_CHECK(std::invalid_argument, a.grant("@sam", "fly"));
}

void TestAccess::testABrokenFileFailsLoudly()
{
	const std::string world = fresh();
	{
		std::ofstream out(world + DIR_DELIM "access.json");
		out << "{ \"roles\": { \"@sam\": [\"wizard\"] } }";
	}
	EXCEPTION_CHECK(BaseException, Access(world, "@cody"));
}
