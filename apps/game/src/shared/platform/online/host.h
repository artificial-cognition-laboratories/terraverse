// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/online/endpoint.h"
#include "src/shared/platform/online/http.h"
#include "src/shared/platform/online/stun.h"
#include "src/shared/platform/online/upnp.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace online {

/*
	A world being hosted for friends, outside the server: where its socket
	can be reached, and the packets that let joiners in.

	Nothing here holds the world up: it's made as the server starts, and
	learns the rest alongside. A worker resolves the STUN servers and asks the
	router to open the port (UPnP); step() sends STUN requests from the
	server's socket, whose answers come back through the connection's stray
	handler (stray(), on its receive thread). The same requests, repeated,
	keep the router's mapping alive and notice if it changes.

	Once a frame it also waits on ArcLabs for knocks — joiners on their way,
	with their own public address — and sends packets toward each for a few
	seconds, so this side's router expects the joiner's (hole punching).
*/
class Host
{
public:
	// `handle` is the server's socket, which outlives this.
	Host(Api api, int handle, u16 port);
	// Waits for the worker, and closes the router's port.
	~Host();
	Host(const Host &) = delete;
	Host &operator=(const Host &) = delete;

	// A datagram on the server's socket that isn't the game's. Any thread.
	void stray(const Address &sender, const u8 *data, size_t size);

	// The endpoint, once known and whenever it changes since last asked.
	std::optional<Endpoint> changed();

	void step();

private:
	void work();
	void ask(u64 now);
	void settle();
	void punch(const Endpoint &joiner);

	const Api m_api;
	const int m_handle;
	const u16 m_port;

	// Shared with the worker and the receive thread.
	std::mutex m_lock;
	bool m_resolved = false;
	bool m_upnp_tried = false;
	struct Ask { Address server; TxId id; std::optional<Address> seen; };
	std::vector<Ask> m_asks;
	std::unique_ptr<PortMapping> m_mapping;
	Endpoint m_endpoint;
	bool m_known = false;   // m_endpoint is worth reporting
	bool m_changed = false; // since changed() last said

	std::thread m_worker;

	int m_round = 0;       // STUN requests sent in the current round
	u64 m_next_ask = 0;    // ms

	// Main thread only.
	u64 m_caller;
	bool m_waiting = false; // a knocks request is out
	u64 m_next_knocks = 0;
	struct Punch { Address to; u64 until; };
	std::vector<Punch> m_punches;
	u64 m_next_punch = 0;
};

} // namespace online
