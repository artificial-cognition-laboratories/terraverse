// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include "src/shared/platform/network/address.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

class UDPSocket;

namespace online {

/*
	STUN (RFC 5389), just the binding request: a public server answers with
	the address our packet came from, which is this socket as the internet
	sees it — the address a friend's game can send to.
*/

using TxId = std::array<u8, 12>;

TxId newId();
std::string bindingRequest(const TxId &id);
// The address a binding success for `id` carries; nullopt for anything else.
std::optional<Address> mappedAddress(const u8 *data, size_t size, const TxId &id);

struct Mapping
{
	std::optional<Address> pub;
	// Two servers saw two different ports: the NAT picks a port per
	// destination, so no one can predict where to send to reach us.
	bool symmetric = false;
};

// The STUN servers, resolved (IPv4). Blocks on DNS.
std::vector<Address> stunServers();

// Asks two STUN servers about this socket. Blocks for at most ~1.5 s. Only
// on a socket no Connection owns yet (it reads the answers itself).
Mapping probe(UDPSocket &socket);

// A datagram from a socket by its handle — for one a Connection owns, which
// it can share: sendto on one socket from two threads is safe.
void sendRaw(int handle, const Address &to, const void *data, size_t size);

} // namespace online
