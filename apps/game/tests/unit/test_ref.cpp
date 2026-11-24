// Terra Firma: terra:// refs against the golden terra's parser passes too (assets/misc/refs.json)
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/shared/platform/online/ref.h"
#include "src/shared/platform/system/porting.h"

#include <fstream>
#include <json/json.h>

using namespace online;

class TestRef : public TestBase
{
public:
	TestRef() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestRef"; }

	void runTests(IGameDef *gamedef);

	void testGolden();
};

static TestRef g_test_instance;

void TestRef::runTests(IGameDef *gamedef)
{
	TEST(testGolden);
}

// A ref as the golden writes it.
static Json::Value json(const ref::Ref &r)
{
	Json::Value out(Json::objectValue);
	if (auto *a = std::get_if<ref::Address>(&r)) {
		out["kind"] = "address"; out["host"] = a->host; out["port"] = a->port;
	} else if (auto *c = std::get_if<ref::Code>(&r)) {
		out["kind"] = "code"; out["code"] = c->code;
	} else if (auto *p = std::get_if<ref::Published>(&r)) {
		out["kind"] = "published"; out["scope"] = p->scope; out["name"] = p->name;
	} else {
		out["kind"] = "friend"; out["player"] = std::get<ref::Friend>(r).player;
	}
	return out;
}

void TestRef::testGolden()
{
	std::ifstream file(porting::path_share + DIR_DELIM "misc" DIR_DELIM "refs.json");
	UASSERT(file.good());
	Json::Value golden;
	file >> golden;
	UASSERT(golden["refs"].size() > 10);
	for (const Json::Value &row : golden["refs"]) {
		const std::string text = row["ref"].asString();
		auto parsed = ref::parse(text);
		infostream << "TestRef: " << text << std::endl;
		if (row["is"].isNull()) {
			UASSERT(!parsed);
			continue;
		}
		UASSERT(parsed);
		UASSERT(json(*parsed) == row["is"]);
		UASSERTEQ(std::string, ref::print(*parsed), row["printed"].asString());
		UASSERT(ref::parse(ref::print(*parsed)).has_value());
	}
}
