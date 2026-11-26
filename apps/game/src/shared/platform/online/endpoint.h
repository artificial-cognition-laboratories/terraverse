// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/network/address.h"
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>

namespace online {

/*
	Where a game's socket can be reached (arclabs: db/schema/terrafirma.ts
	Endpoint): its public address as a STUN server saw it (or UPnP mapped it),
	its addresses on each local network, and what kind of NAT it's behind.
*/
struct Endpoint
{
	std::optional<Address> pub;
	std::vector<Address> lan;
	// open: the public address is our own; upnp: the router forwards to us;
	// cone: one public port whoever we send to (punchable); symmetric: a new
	// port per destination (not punchable); unknown: STUN didn't answer.
	std::string nat = "unknown";
};

Json::Value toJson(const Endpoint &endpoint);
// Throws BaseException when it isn't one.
Endpoint endpointFromJson(const Json::Value &value);

// "1.2.3.4:5678"
std::string str(const Address &address);
// "1.2.3.4:5678" → Address; throws BaseException.
Address parseAddress(const std::string &text);

// This machine's IPv4 addresses on its local networks, with `port`.
std::vector<Address> lanAddresses(u16 port);

/*
	Where a joiner should send to reach a host: on the same network (one
	public address for both), the host's LAN address — routers rarely loop
	a packet back to themselves; otherwise its public address. Empty when
	there's nowhere it could work.
*/
std::optional<Address> route(const Endpoint &me, const Endpoint &host);

} // namespace online
