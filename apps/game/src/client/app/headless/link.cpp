// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/headless/link.h"
#include "src/shared/base/convert_json.h"
#include "src/shared/base/exceptions.h"
#include <cstdio>
#include <iostream>
#include <sstream>
#ifdef _WIN32
	#include <io.h>
	#define dup _dup
	#define dup2 _dup2
	#define write _write
#else
	#include <unistd.h>
#endif

namespace headless {

Link::Link() :
	m_wire(dup(1))
{
	if (m_wire < 0)
		throw BaseException("body: couldn't keep stdout for the wire");
	std::fflush(stdout);
	std::cout.flush();
	if (dup2(2, 1) < 0)
		throw BaseException("body: couldn't point stdout at stderr");
	m_reader = std::thread([this] { read(); });
	// it blocks on stdin till terra closes it; nothing joins a reader that may never return
	m_reader.detach();
}

Link::~Link() = default;

void Link::read()
{
	std::string line;
	while (std::getline(std::cin, line)) {
		if (line.empty())
			continue;
		std::lock_guard<std::mutex> lock(m_mutex);
		m_lines.push_back(std::move(line));
	}
	m_closed = true;
}

std::optional<Json::Value> Link::take()
{
	std::string line;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_lines.empty())
			return std::nullopt;
		line = std::move(m_lines.front());
		m_lines.pop_front();
	}
	Json::Value message;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream in(line);
	if (!Json::parseFromStream(builder, in, &message, &errors) || !message.isObject())
		throw BaseException("body: terra sent a line that isn't a JSON object: " + line.substr(0, 200));
	return message;
}

void Link::say(const Json::Value &message)
{
	const std::string line = fastWriteJson(message) + "\n";
	size_t done = 0;
	while (done < line.size()) {
		const auto n = write(m_wire, line.data() + done, line.size() - done);
		if (n <= 0)
			throw BaseException("body: terra's end of the wire is gone");
		done += n;
	}
}

} // namespace headless
