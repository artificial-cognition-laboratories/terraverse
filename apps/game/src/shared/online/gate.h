// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/online/http.h"
#include <map>
#include <string>
#include <vector>

namespace online {

/*
	How a hosting server admits players (network: AUTH_MECHANISM_TICKET).
	Its owner's own client presents a secret that never leaves the process;
	anyone else presents the single-use ticket ArcLabs gave them for this
	world, which the gate checks with ArcLabs (and ArcLabs uses up). Both must
	run the host's build. Checks are asynchronous: the server thread asks with
	begin() and collects answers with poll() every step.
*/
class Gate
{
public:
	struct Opts {
		Api api;            // as the host, signed in
		std::string owner;  // the host's player name
		std::string secret; // the owner's own client's ticket
		std::string build;  // the host's build (g_version_hash)
	};

	explicit Gate(Opts opts);
	~Gate();
	Gate(const Gate &) = delete;
	Gate &operator=(const Gate &) = delete;

	const std::string &owner() const { return m_opts.owner; }

	void begin(u16 peer, const std::string &name, const std::string &ticket, const std::string &build);

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
