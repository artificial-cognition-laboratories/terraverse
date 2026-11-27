// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/client/app/shell/bridge/axond.h"
#include "src/shared/platform/system/filesys.h"
#ifndef _WIN32
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#endif

class TestAxond : public TestBase {
public:
	TestAxond() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestAxond"; }

	void runTests(IGameDef *gamedef);

	void testAnswered();
	void testRefusedAndDown();
};

static TestAxond g_test_instance;

void TestAxond::runTests(IGameDef *gamedef)
{
#ifndef _WIN32
	TEST(testAnswered);
	TEST(testRefusedAndDown);
#endif
}

#ifndef _WIN32
namespace {

// A daemon for one call: it reads the request, keeps it, answers `reply` whole and closes.
struct FakeDaemon {
	std::string path, asked;
	int fd;
	std::thread thread;

	FakeDaemon(const std::string &dir, std::string reply) : path(dir + DIR_DELIM "axond.sock")
	{
		fs::CreateAllDirs(dir);
		unlink(path.c_str());
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		sockaddr_un a{};
		a.sun_family = AF_UNIX;
		std::memcpy(a.sun_path, path.c_str(), path.size() + 1);
		UASSERT(bind(fd, (sockaddr *)&a, sizeof(a)) == 0 && listen(fd, 1) == 0);
		thread = std::thread([this, reply] {
			int c = accept(fd, nullptr, nullptr);
			char buf[4096];
			ssize_t n;
			while (asked.find("\r\n\r\n") == std::string::npos && (n = recv(c, buf, sizeof(buf), 0)) > 0)
				asked.append(buf, n);
			send(c, reply.data(), reply.size(), MSG_NOSIGNAL);
			close(c);
		});
	}
	~FakeDaemon() { thread.join(); close(fd); unlink(path.c_str()); }
};

std::string http(const std::string &body, bool chunked)
{
	if (!chunked)
		return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
				std::to_string(body.size()) + "\r\n\r\n" + body;
	const size_t half = body.size() / 2;
	auto hex = [](size_t n) { char b[16]; snprintf(b, sizeof(b), "%zx", n); return std::string(b); };
	return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + hex(half) + "\r\n" + body.substr(0, half) +
			"\r\n" + hex(body.size() - half) + "\r\n" + body.substr(half) + "\r\n0\r\n\r\n";
}

} // namespace

// A verb is posted as {path, arg}, and its value comes back, whole or chunked.
void TestAxond::testAnswered()
{
	const std::string dir = getTestTempDirectory() + DIR_DELIM "axond";
	for (bool chunked : { false, true }) {
		Json::Value value;
		{
			FakeDaemon daemon(dir, http("{\"ok\":true,\"value\":{\"session\":\"s1\",\"agents\":[1,2]}}", chunked));
			Json::Value arg(Json::objectValue);
			arg["agent"] = "@cody/terry";
			value = shell::Axond({ daemon.path, 2000 }).call({ "agents", "list" }, arg);
			UASSERT(daemon.asked.compare(0, 7, "POST / ") == 0);
		}
		UASSERT(value["session"].asString() == "s1" && value["agents"].size() == 2);
	}
}

// The daemon's no is the player's to read; nothing listening is said apart from it.
void TestAxond::testRefusedAndDown()
{
	const std::string dir = getTestTempDirectory() + DIR_DELIM "axond";
	{
		FakeDaemon daemon(dir, http("{\"ok\":false,\"error\":\"no such agent: @x/y\"}", false));
		try {
			shell::Axond({ daemon.path, 2000 }).call({ "embark" }, Json::Value());
			UASSERT(false);
		} catch (const shell::Axond::Down &) {
			UASSERT(false);
		} catch (const std::runtime_error &e) { // shell::Refused (bridge.h, C++20 with CEF)
			UASSERT(std::string(e.what()) == "no such agent: @x/y");
		}
	}
	EXCEPTION_CHECK(shell::Axond::Down, shell::Axond({ dir + DIR_DELIM "nobody.sock", 2000 }).call({ "version" }, Json::Value()));
}
#endif
