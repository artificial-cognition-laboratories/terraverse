// Terraverse: where Terraverse keeps things (shared/platform/home, .agents/planning/home.md)

#include "tests/unit/test.h"

#include "src/shared/platform/home/home.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"

class TestHome : public TestBase
{
public:
	TestHome() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestHome"; }

	void runTests(IGameDef *gamedef);

private:
	void testAFreshHomeStartsLocal();
	void testAnOldLayoutMovesIn();
	void testTheFirstSignInAdoptsLocal();
	void testALaterSignInBringsOnlyTheKey();
	void testARenamedAccountTakesItsProfile();
	void testSigningOutGoesBackToLocal();
	void testTheActiveProfileIsRemembered();
	void testNamesAreChecked();

	std::string fresh();
	std::unique_ptr<home::Home> boot(const std::string &root, const std::string &chosen = "");
	void write(const std::string &path, const std::string &text);
};

static TestHome g_test_instance;

void TestHome::runTests(IGameDef *gamedef)
{
	// booting points the engine's legacy globals at the home: put them back after
	const std::string user = porting::path_user, cache = porting::path_cache;
	TEST(testAFreshHomeStartsLocal);
	TEST(testAnOldLayoutMovesIn);
	TEST(testTheFirstSignInAdoptsLocal);
	TEST(testALaterSignInBringsOnlyTheKey);
	TEST(testARenamedAccountTakesItsProfile);
	TEST(testSigningOutGoesBackToLocal);
	TEST(testTheActiveProfileIsRemembered);
	TEST(testNamesAreChecked);
	porting::path_user = user;
	porting::path_cache = cache;
}

std::string TestHome::fresh()
{
	static int n = 0;
	const std::string dir = getTestTempDirectory() + DIR_DELIM "home" + std::to_string(n++);
	fs::RecursiveDelete(dir);
	fs::CreateAllDirs(dir);
	return dir;
}

std::unique_ptr<home::Home> TestHome::boot(const std::string &root, const std::string &chosen)
{
	return home::Home::boot({ /* .root = */ root, /* .chosen = */ chosen, /* .legacy = */ "" });
}

void TestHome::write(const std::string &path, const std::string &text)
{
	fs::CreateAllDirs(fs::RemoveLastPathComponent(path));
	UASSERT(fs::safeWriteToFile(path, text));
}

void TestHome::testAFreshHomeStartsLocal()
{
	const std::string root = fresh();
	auto h = boot(root);
	UASSERTEQ(std::string, h->profile(), "local");
	UASSERTEQ(std::string, porting::path_user, root + DIR_DELIM "profiles" DIR_DELIM "local");
	UASSERTEQ(std::string, porting::path_cache, root + DIR_DELIM "cache" DIR_DELIM "engine");
	UASSERT(fs::PathExists(root + DIR_DELIM "profiles" DIR_DELIM "local" DIR_DELIM "profile.json"));
}

void TestHome::testAnOldLayoutMovesIn()
{
	const std::string root = fresh();
	write(root + DIR_DELIM "worlds" DIR_DELIM "mine" DIR_DELIM "world.mt", "x");
	write(root + DIR_DELIM "minetest.conf", "enable_damage = true\n");
	write(root + DIR_DELIM "account.json", "{\"key\":\"k\"}");
	boot(root);
	const std::string local = root + DIR_DELIM "profiles" DIR_DELIM "local";
	UASSERT(fs::PathExists(local + DIR_DELIM "worlds" DIR_DELIM "mine" DIR_DELIM "world.mt"));
	UASSERT(fs::PathExists(local + DIR_DELIM "account.json"));
	UASSERT(fs::PathExists(root + DIR_DELIM "settings.conf"));
	UASSERT(!fs::PathExists(root + DIR_DELIM "worlds"));
	UASSERT(fs::PathExists(root + DIR_DELIM "MOVED.md"));
}

void TestHome::testTheFirstSignInAdoptsLocal()
{
	const std::string root = fresh();
	auto h = boot(root);
	write(porting::path_user + DIR_DELIM "worlds" DIR_DELIM "mine" DIR_DELIM "world.mt", "x");
	const std::string key = porting::path_user + DIR_DELIM "account.json";
	write(key, "{\"key\":\"k\"}");
	const std::string moved = h->signedIn("cody", "id-1", key);
	const std::string mine = root + DIR_DELIM "profiles" DIR_DELIM "@cody";
	UASSERTEQ(std::string, h->profile(), "@cody");
	UASSERTEQ(std::string, moved, mine + DIR_DELIM "account.json");
	UASSERT(fs::PathExists(moved));
	// what was played signed out is the account's now, and local starts again
	UASSERT(fs::PathExists(mine + DIR_DELIM "worlds" DIR_DELIM "mine" DIR_DELIM "world.mt"));
	UASSERT(!fs::PathExists(root + DIR_DELIM "profiles" DIR_DELIM "local"));
	UASSERTEQ(std::string, porting::path_user, mine);
}

void TestHome::testALaterSignInBringsOnlyTheKey()
{
	const std::string root = fresh();
	auto h = boot(root, "@cody");
	h->signedOut();
	write(porting::path_user + DIR_DELIM "worlds" DIR_DELIM "other" DIR_DELIM "world.mt", "x");
	const std::string key = porting::path_user + DIR_DELIM "account.json";
	write(key, "{\"key\":\"k\"}");
	const std::string moved = h->signedIn("cody", "id-1", key);
	UASSERTEQ(std::string, moved, root + DIR_DELIM "profiles" DIR_DELIM "@cody" DIR_DELIM "account.json");
	UASSERT(!fs::PathExists(key));
	// @cody had a profile: local keeps what it played
	UASSERT(fs::PathExists(root + DIR_DELIM "profiles" DIR_DELIM "local" DIR_DELIM "worlds" DIR_DELIM "other" DIR_DELIM "world.mt"));
}

void TestHome::testARenamedAccountTakesItsProfile()
{
	const std::string root = fresh();
	auto h = boot(root);
	write(porting::path_user + DIR_DELIM "worlds" DIR_DELIM "mine" DIR_DELIM "world.mt", "x");
	const std::string key = porting::path_user + DIR_DELIM "account.json";
	write(key, "{\"key\":\"k\"}");
	const std::string first = h->signedIn("cody", "id-1", key);
	// the same account, under its new handle: the profile and its worlds follow
	const std::string moved = h->signedIn("cody2", "id-1", first);
	const std::string now = root + DIR_DELIM "profiles" DIR_DELIM "@cody2";
	UASSERTEQ(std::string, h->profile(), "@cody2");
	UASSERTEQ(std::string, moved, now + DIR_DELIM "account.json");
	UASSERT(fs::PathExists(now + DIR_DELIM "worlds" DIR_DELIM "mine" DIR_DELIM "world.mt"));
	UASSERT(!fs::PathExists(root + DIR_DELIM "profiles" DIR_DELIM "@cody"));
	// another account signing in from there gets its own profile, and @cody2's stays
	write(porting::path_user + DIR_DELIM "other.json", "{}");
	h->signedIn("sam", "id-2", porting::path_user + DIR_DELIM "other.json");
	UASSERTEQ(std::string, h->profile(), "@sam");
	UASSERT(fs::PathExists(now + DIR_DELIM "worlds" DIR_DELIM "mine" DIR_DELIM "world.mt"));
}

void TestHome::testSigningOutGoesBackToLocal()
{
	const std::string root = fresh();
	auto h = boot(root, "@cody");
	h->signedOut();
	UASSERTEQ(std::string, h->profile(), "local");
	UASSERT(fs::PathExists(root + DIR_DELIM "profiles" DIR_DELIM "@cody"));
}

void TestHome::testTheActiveProfileIsRemembered()
{
	const std::string root = fresh();
	boot(root)->use("@sam");
	UASSERTEQ(std::string, boot(root)->profile(), "@sam");
	// asked for by name, a process runs as it without changing what's active
	UASSERTEQ(std::string, boot(root, "two")->profile(), "two");
	UASSERTEQ(std::string, boot(root)->profile(), "@sam");
}

void TestHome::testNamesAreChecked()
{
	UASSERT(home::Home::validName("local"));
	UASSERT(home::Home::validName("@cody"));
	UASSERT(home::Home::validName("@cody.w"));
	UASSERT(home::Home::validName("two"));
	UASSERT(!home::Home::validName(""));
	UASSERT(!home::Home::validName("@"));
	UASSERT(!home::Home::validName("../x"));
	UASSERT(!home::Home::validName("a/b"));
	UASSERT(!home::Home::validName("@.."));
	EXCEPTION_CHECK(home::Error, boot(fresh(), "../x"));
}
