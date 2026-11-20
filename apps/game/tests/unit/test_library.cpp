// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/server/world/run/machines/workshop/library.h"
#include "src/shared/core/exceptions.h"
#include "src/shared/platform/filesys.h"

using namespace workshop;

class TestLibrary : public TestBase {
public:
	TestLibrary() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestLibrary"; }

	void runTests(IGameDef *gamedef);

	void testBlueprintWrittenAndRead();
	void testKeptByPlayerAndName();
	void testWorldsAdopted();
};

static TestLibrary g_test_instance;

void TestLibrary::runTests(IGameDef *gamedef)
{
	TEST(testBlueprintWrittenAndRead);
	TEST(testKeptByPlayerAndName);
	TEST(testWorldsAdopted);
}

// A blueprint's machines and where each goes survive its text; text that
// isn't one, or one of none, throws.
void TestLibrary::testBlueprintWrittenAndRead()
{
	const Blueprint b{ { { v3s16(0, 0, 0), std::string("one\0piece", 9) }, { v3s16(-2, 3, 1), "two" } } };
	const Blueprint back = Blueprint::read(b.write());
	UASSERTEQ(size_t, back.parts.size(), 2);
	UASSERT(back.parts[0].machine == b.parts[0].machine && back.parts[1].offset == v3s16(-2, 3, 1));
	EXCEPTION_CHECK(SerializationError, Blueprint::read("TFP2 not a blueprint"));
	EXCEPTION_CHECK(SerializationError, Blueprint::read(Blueprint{}.write()));
}

// Each player's own, by name, sorted; saved again under a name, it's the
// new one. A name that isn't one is refused, and an unknown one isn't there.
void TestLibrary::testKeptByPlayerAndName()
{
	const std::string root = getTestTempDirectory() + DIR_DELIM + "parts";
	fs::RecursiveDelete(root);
	Library lib(root, "");
	const Blueprint cart{ { { v3s16(0), "cart" } } }, pot{ { { v3s16(0), "pot" } } };
	lib.save("cody", "small cart", cart);
	lib.save("cody", "Pot_2", pot);
	lib.save("ann", "pot", pot);
	UASSERT(lib.names("cody") == std::vector<std::string>({ "Pot_2", "small cart" }));
	UASSERT(lib.names("ann") == std::vector<std::string>({ "pot" }));
	UASSERT(lib.names("nobody").empty());
	UASSERT(lib.load("cody", "small cart").parts[0].machine == "cart");
	lib.save("cody", "small cart", pot);
	UASSERT(lib.load("cody", "small cart").parts[0].machine == "pot");

	for (const char *bad : { "", " pad", "../ann/pot", "a/b", "dot.dot", "012345678901234567890123456789012" }) {
		UASSERT(!Library::named(bad));
		EXCEPTION_CHECK(std::invalid_argument, lib.save("cody", bad, cart));
	}
	EXCEPTION_CHECK(std::out_of_range, lib.load("cody", "pot"));
	EXCEPTION_CHECK(std::out_of_range, lib.load("cody", "../ann/pot"));
	lib.remove("cody", "Pot_2");
	UASSERT(lib.names("cody") == std::vector<std::string>({ "small cart" }));
	EXCEPTION_CHECK(std::out_of_range, lib.remove("cody", "Pot_2"));
}

// A world's blueprints from before are copied into the library as it
// opens, beside what's there; a name already taken keeps the library's.
void TestLibrary::testWorldsAdopted()
{
	const std::string tmp = getTestTempDirectory(), root = tmp + DIR_DELIM + "parts2",
			world = tmp + DIR_DELIM + "oldworld";
	fs::RecursiveDelete(root);
	fs::RecursiveDelete(world);
	Library(world + DIR_DELIM + "blueprints", "").save("cody", "cart", Blueprint{ { { v3s16(0), "old cart" } } });
	Library(world + DIR_DELIM + "blueprints", "").save("cody", "pot", Blueprint{ { { v3s16(0), "old pot" } } });
	Library(root, "").save("cody", "pot", Blueprint{ { { v3s16(0), "new pot" } } });
	Library lib(root, world);
	UASSERT(lib.names("cody") == std::vector<std::string>({ "cart", "pot" }));
	UASSERT(lib.load("cody", "cart").parts[0].machine == "old cart");
	UASSERT(lib.load("cody", "pot").parts[0].machine == "new pot");
	UASSERT(fs::PathExists(world + DIR_DELIM + "blueprints" + DIR_DELIM + "cody" + DIR_DELIM + "cart.tfb"));
}
