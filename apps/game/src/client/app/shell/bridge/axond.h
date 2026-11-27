// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>
#include <vector>

namespace shell {

/*
	Axon's daemon on this machine (axond, sprint 127), asked one verb at a time over its socket:
	HTTP on a unix socket, `POST {path, arg}` answered `{ok, value}` or `{ok: false, error}`.
	A run-in-place build (a checkout, `bun dev`) asks the source build's daemon (`axonl`,
	~/.axon-dev); an installed game asks the installed one (~/.axon). AXON_DAEMON_DIR overrides
	both, as it does for axon itself. Blocking: call it off the main thread, or with a short
	timeout. Any thread; it holds no state between calls.
*/
class Axond
{
public:
	struct Opts {
		std::string socket; // the daemon's socket (Axond::socketPath())
		int timeout_ms;     // for the whole call
	};
	explicit Axond(Opts opts) : m_opts(std::move(opts)) {}

	// Where this build's daemon listens.
	static std::string socketPath();

	// Nothing answers on the socket: axond isn't running (or Axon isn't installed).
	struct Down : std::runtime_error { using std::runtime_error::runtime_error; };

	// One verb's answer. Throws Down when nothing answers, Refused with the daemon's own words
	// when the verb fails, and BaseException when what came back isn't the daemon's.
	Json::Value call(const std::vector<std::string> &path, const Json::Value &arg) const;

private:
	Opts m_opts;
};

} // namespace shell
