// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/online/http.h"
#include <map>
#include <string>
#include <vector>

namespace online {

/*
	How a hosting server admits players (network: AUTH_MECHANISM_TICKET).
	Its owner's own client presents a secret that never leaves the process;
	anyone else presents the single-use ticket ArcLabs gave them for this
	world, which the gate checks with ArcLabs (and ArcLabs uses up). A server
	terra runs has a run secret instead (its running/ record, readable only by
	the user who started it): anyone on this machine presenting it is let in,
	under the name they ask for. Everyone must run the host's build. Checks are
	asynchronous: the server thread asks with begin() and collects answers with
	poll() every step.
*/
class Gate
{
public:
	struct Opts {
		Api api;            // as the host, signed in; no url: no ArcLabs tickets
		std::string owner;  // the host's player name
		std::string secret; // the owner's own client's ticket
		std::string build;  // the host's build (g_version_hash)
		std::string local;  // the run secret, admitting from this machine; empty: none
	};

	// Someone asking to come in.
	struct Asking {
		u16 peer;
		std::string name;
		std::string ticket;
		std::string build;
		bool loopback; // they're on this machine
	};

	explicit Gate(Opts opts);
	~Gate();
	Gate(const Gate &) = delete;
	Gate &operator=(const Gate &) = delete;

	const std::string &owner() const { return m_opts.owner; }

	void begin(const Asking &asking);

	struct Decision
	{
		u16 peer;
		bool admit;
		std::string reason; // why not, for the player
	};
	std::vector<Decision> poll();

private:
	const Opts m_opts;
	u64 m_caller;
	u64 m_next_request = 0;
	struct Pending { u16 peer; std::string name; };
	std::map<u64, Pending> m_pending;
	std::vector<Decision> m_ready;
};

} // namespace online
