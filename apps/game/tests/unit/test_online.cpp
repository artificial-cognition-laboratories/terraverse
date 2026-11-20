// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/shared/base/exceptions.h"
#include "src/shared/platform/online/endpoint.h"
#include "src/shared/platform/online/stun.h"

using namespace online;

class TestOnline : public TestBase
{
public:
	TestOnline() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestOnline"; }

	void runTests(IGameDef *gamedef);

	void testBindingRequest();
	void testRealAnswer();
	void testNotOurs();
	void testEndpointJson();
	void testRoute();
};

static TestOnline g_test_instance;

void TestOnline::runTests(IGameDef *gamedef)
{
	TEST(testBindingRequest);
	TEST(testRealAnswer);
	TEST(testNotOurs);
	TEST(testEndpointJson);
	TEST(testRoute);
}

static TxId tx(const char *hex)
{
	TxId id;
	for (int i = 0; i < 12; i++)
		id[i] = (u8)std::stoul(std::string(hex + 2 * i, 2), nullptr, 16);
	return id;
}

static std::string bytes(const char *hex)
{
	std::string out;
	for (const char *p = hex; p[0] && p[1]; p += 2)
		out += (char)std::stoul(std::string(p, 2), nullptr, 16);
	return out;
}

void TestOnline::testBindingRequest()
{
	TxId id = tx("3a2c963801a17fa6b563cf4b");
	UASSERT(bindingRequest(id) == bytes("000100002112a4423a2c963801a17fa6b563cf4b"));
}

// stun.l.google.com's answer to that request, captured: XOR-MAPPED-ADDRESS
// 94.4.137.143:39724.
void TestOnline::testRealAnswer()
{
	TxId id = tx("3a2c963801a17fa6b563cf4b");
	std::string answer = bytes("0101000c2112a4423a2c963801a17fa6b563cf4b002000080001ba3e7f162dcd");
	auto seen = mappedAddress((const u8 *)answer.data(), answer.size(), id);
	UASSERT(seen.has_value());
	UASSERTEQ(std::string, str(*seen), "94.4.137.143:39724");
}

void TestOnline::testNotOurs()
{
	std::string answer = bytes("0101000c2112a4423a2c963801a17fa6b563cf4b002000080001ba3e7f162dcd");
	// another transaction's answer
	UASSERT(!mappedAddress((const u8 *)answer.data(), answer.size(), tx("000000000000000000000000")));
	// cut short
	UASSERT(!mappedAddress((const u8 *)answer.data(), 24, tx("3a2c963801a17fa6b563cf4b")));
	// a game packet arriving instead
	std::string game = bytes("4f457403000000030000000000000000000000000000");
	UASSERT(!mappedAddress((const u8 *)game.data(), game.size(), tx("3a2c963801a17fa6b563cf4b")));
}

void TestOnline::testEndpointJson()
{
	Endpoint e;
	e.pub = parseAddress("94.4.137.143:39724");
	e.lan = { parseAddress("192.168.1.20:39724") };
	e.nat = "cone";
	Endpoint back = endpointFromJson(toJson(e));
	UASSERT(back.pub && str(*back.pub) == "94.4.137.143:39724");
	UASSERT(back.lan.size() == 1 && str(back.lan[0]) == "192.168.1.20:39724");
	UASSERTEQ(std::string, back.nat, "cone");
	EXCEPTION_CHECK(BaseException, parseAddress("94.4.137.143"));
	EXCEPTION_CHECK(BaseException, parseAddress("300.1.1.1:5"));
}

void TestOnline::testRoute()
{
	Endpoint host, me;
	host.pub = parseAddress("94.4.137.143:40000");
	host.lan = { parseAddress("192.168.1.20:40000") };
	// across the internet: the host's public address
	me.pub = parseAddress("81.2.3.4:50000");
	UASSERTEQ(std::string, str(*route(me, host)), "94.4.137.143:40000");
	// behind the same router (or on the same machine): its LAN address
	me.pub = parseAddress("94.4.137.143:50000");
	UASSERTEQ(std::string, str(*route(me, host)), "192.168.1.20:40000");
	// we don't know our own address: still the public one
	me.pub.reset();
	UASSERTEQ(std::string, str(*route(me, host)), "94.4.137.143:40000");
	// nobody knows anything: nowhere
	UASSERT(!route(me, Endpoint()));
}
