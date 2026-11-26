// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace headless {

/*
	The pipe between a body and terra, which serves it to a mind (sprint 126). Each way a JSON
	line: terra's on stdin, read on a thread of their own so a quiet pipe never stalls a step;
	the body's on what was stdout. Opening it moves stdout aside: fd 1 becomes stderr, so a stray
	print anywhere in the engine can never land on the wire.
*/
class Link
{
public:
	Link();
	~Link();
	Link(const Link &) = delete;
	Link &operator=(const Link &) = delete;

	// The next message terra sent, if one's waiting. A line that isn't a JSON object throws:
	// terra and the body are one program's halves, so a bad line is a bug, said loudly.
	std::optional<Json::Value> take();
	// One message to terra, whole.
	void say(const Json::Value &message);
	// terra closed its end: the body is no longer anyone's.
	bool closed() const { return m_closed; }

private:
	void read();

	int m_wire;
	std::mutex m_mutex;
	std::deque<std::string> m_lines;
	std::atomic<bool> m_closed{ false };
	std::thread m_reader;
};

} // namespace headless
