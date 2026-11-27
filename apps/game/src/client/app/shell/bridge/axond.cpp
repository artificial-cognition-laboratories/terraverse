// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/axond.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/base/config.h"
#include "src/shared/base/exceptions.h"
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <cerrno>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace shell {

namespace {

// The whole exchange: the request written, the response read to its end (the daemon closes it).
std::string exchange(const std::string &socket, int timeout_ms, const std::string &request);
// The body of an HTTP/1.1 response, its status beside it; chunked or not.
std::pair<int, std::string> parseResponse(const std::string &raw);
std::string unchunk(const std::string &body);

} // namespace

std::string Axond::socketPath()
{
	if (const char *dir = std::getenv("AXON_DAEMON_DIR"))
		return std::string(dir) + "/axond.sock";
	const char *home = std::getenv("HOME");
	if (!home)
		throw BaseException("axond: no HOME to find Axon's daemon in");
#if RUN_IN_PLACE
	// a checkout's game asks the source build's daemon (axonl), as source terra (terral) is its lab's
	return std::string(home) + "/.axon-dev/cache/daemon/axond.sock";
#else
	return std::string(home) + "/.axon/cache/daemon/axond.sock";
#endif
}

Json::Value Axond::call(const std::vector<std::string> &path, const Json::Value &arg) const
{
	Json::Value body(Json::objectValue);
	body["path"] = Json::Value(Json::arrayValue);
	for (const std::string &segment : path)
		body["path"].append(segment);
	body["arg"] = arg;
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	const std::string json = Json::writeString(writer, body);
	const std::string request = "POST / HTTP/1.1\r\nHost: axond\r\nContent-Type: application/json\r\n"
		"Content-Length: " + std::to_string(json.size()) + "\r\nConnection: close\r\n\r\n" + json;
	const auto [status, text] = parseResponse(exchange(m_opts.socket, m_opts.timeout_ms, request));

	Json::Value answer;
	std::string errors;
	std::unique_ptr<Json::CharReader> reader(Json::CharReaderBuilder().newCharReader());
	if (!reader->parse(text.data(), text.data() + text.size(), &answer, &errors) || !answer.isObject() || !answer["ok"].isBool())
		throw BaseException("axond answered " + std::to_string(status) + " with what isn't its answer: " + text.substr(0, 200));
	if (!answer["ok"].asBool())
		throw Refused(answer["error"].isString() ? answer["error"].asString() : "Axon refused, saying nothing");
	return answer["value"];
}

namespace {

#ifdef _WIN32
std::string exchange(const std::string &, int, const std::string &)
{
	throw Refused("Agents aren't wired on Windows yet: Axon's daemon listens on a unix socket.");
}
#else
std::string exchange(const std::string &socket, int timeout_ms, const std::string &request)
{
	sockaddr_un address{};
	address.sun_family = AF_UNIX;
	if (socket.size() >= sizeof(address.sun_path))
		throw BaseException("axond: its socket's path is too long: " + socket);
	std::memcpy(address.sun_path, socket.c_str(), socket.size() + 1);
	const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		throw BaseException(std::string("axond: no socket: ") + std::strerror(errno));
	struct Closer { int fd; ~Closer() { ::close(fd); } } closer{ fd };
	timeval timeout{ timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
		throw Axond::Down(std::string("nothing answers at ") + socket + " (" + std::strerror(errno) + ")");
	for (size_t sent = 0; sent < request.size();) {
		const ssize_t n = ::send(fd, request.data() + sent, request.size() - sent, MSG_NOSIGNAL);
		if (n <= 0)
			throw BaseException(std::string("axond: couldn't ask: ") + std::strerror(errno));
		sent += (size_t)n;
	}
	std::string raw;
	char buffer[16384];
	for (;;) {
		const ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);
		if (n == 0)
			return raw;
		if (n < 0)
			throw Refused(errno == EAGAIN || errno == EWOULDBLOCK
					? "Axon didn't answer in time."
					: std::string("Axon's answer was cut off: ") + std::strerror(errno));
		raw.append(buffer, (size_t)n);
	}
}
#endif

std::pair<int, std::string> parseResponse(const std::string &raw)
{
	const size_t end = raw.find("\r\n\r\n");
	if (raw.compare(0, 9, "HTTP/1.1 ") != 0 || end == std::string::npos)
		throw BaseException("axond: not an HTTP answer: " + raw.substr(0, 120));
	const int status = std::atoi(raw.c_str() + 9);
	std::string head = raw.substr(0, end);
	for (char &c : head)
		c = (char)std::tolower((unsigned char)c);
	std::string body = raw.substr(end + 4);
	if (head.find("transfer-encoding: chunked") != std::string::npos)
		body = unchunk(body);
	return { status, body };
}

std::string unchunk(const std::string &body)
{
	std::string out;
	size_t at = 0;
	for (;;) {
		const size_t line = body.find("\r\n", at);
		if (line == std::string::npos)
			throw BaseException("axond: a chunked answer cut short");
		const size_t size = std::strtoul(body.c_str() + at, nullptr, 16);
		if (size == 0)
			return out;
		if (line + 2 + size > body.size())
			throw BaseException("axond: a chunk longer than the answer");
		out.append(body, line + 2, size);
		at = line + 2 + size + 2;
	}
}

} // namespace

} // namespace shell
