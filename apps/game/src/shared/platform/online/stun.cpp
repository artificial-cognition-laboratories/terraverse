// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/stun.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/network/networkexceptions.h"
#include "src/shared/platform/network/socket.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/util/math/numeric.h"

#ifndef _WIN32
#include <arpa/inet.h>
#endif

namespace online {

static const u32 MAGIC = 0x2112A442;
static const u16 BINDING_REQUEST = 0x0001, BINDING_SUCCESS = 0x0101;
static const u16 MAPPED_ADDRESS = 0x0001, XOR_MAPPED_ADDRESS = 0x0020;

// Two operators, so a symmetric NAT shows as two different ports.
static const struct { const char *host; u16 port; } SERVERS[] = {
	{ "stun.l.google.com", 19302 },
	{ "stun.cloudflare.com", 3478 },
};

static u16 get16(const u8 *p) { return (u16(p[0]) << 8) | p[1]; }
static u32 get32(const u8 *p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3]; }

std::string bindingRequest(const TxId &id)
{
	std::string out(20, '\0');
	out[0] = char(BINDING_REQUEST >> 8);
	out[1] = char(BINDING_REQUEST & 0xff);
	// length 0: no attributes
	for (int i = 0; i < 4; i++)
		out[4 + i] = char((MAGIC >> (24 - 8 * i)) & 0xff);
	for (int i = 0; i < 12; i++)
		out[8 + i] = char(id[i]);
	return out;
}

std::optional<Address> mappedAddress(const u8 *data, size_t size, const TxId &id)
{
	if (size < 20 || get16(data) != BINDING_SUCCESS || get32(data + 4) != MAGIC)
		return std::nullopt;
	for (int i = 0; i < 12; i++) {
		if (data[8 + i] != id[i])
			return std::nullopt;
	}
	size_t length = get16(data + 2);
	if (20 + length > size)
		return std::nullopt;

	std::optional<Address> plain;
	for (size_t at = 20; at + 4 <= 20 + length;) {
		u16 type = get16(data + at), len = get16(data + at + 2);
		const u8 *value = data + at + 4;
		if (at + 4 + len > 20 + length)
			break;
		// family 0x01: IPv4 (8 bytes); the only kind our sockets ask about
		if ((type == XOR_MAPPED_ADDRESS || type == MAPPED_ADDRESS) && len >= 8 && value[1] == 0x01) {
			u16 port = get16(value + 2);
			u32 ip = get32(value + 4);
			if (type == XOR_MAPPED_ADDRESS)
				return Address(ip ^ MAGIC, port ^ u16(MAGIC >> 16));
			plain = Address(ip, port);
		}
		at += 4 + ((len + 3) & ~size_t(3)); // attributes are padded to 4 bytes
	}
	return plain;
}

TxId newId()
{
	TxId id;
	for (u8 &b : id)
		b = u8(myrand_range(0, 255));
	return id;
}

std::vector<Address> stunServers()
{
	std::vector<Address> out;
	for (const auto &s : SERVERS) {
		Address server(0, 0, 0, 0, s.port);
		Address fallback;
		try {
			server.Resolve(s.host, &fallback);
		} catch (const ResolveError &e) {
			warningstream << "online: couldn't resolve STUN server " << s.host << ": " << e.what() << std::endl;
			continue;
		}
		// our sockets are IPv4
		if (server.isIPv6())
			server = fallback;
		if (!server.isValid() || server.isIPv6())
			continue;
		server.setPort(s.port);
		out.push_back(server);
	}
	return out;
}

Mapping probe(UDPSocket &socket)
{
	struct Ask { Address server; TxId id; std::optional<Address> seen; };
	std::vector<Ask> asks;
	for (const Address &server : stunServers())
		asks.push_back({ server, newId(), std::nullopt });

	Mapping out;
	if (asks.empty())
		return out;

	socket.setTimeoutMs(100);
	const u64 start = porting::getTimeMs();
	u64 next_send = start;
	u8 buffer[512];
	while (porting::getTimeMs() - start < 1500) {
		bool waiting = false;
		for (const Ask &ask : asks)
			waiting = waiting || !ask.seen;
		if (!waiting)
			break;
		if (porting::getTimeMs() >= next_send) { // UDP: ask again until answered
			for (const Ask &ask : asks) {
				if (ask.seen)
					continue;
				std::string req = bindingRequest(ask.id);
				try {
					socket.Send(ask.server, req.data(), req.size());
				} catch (const SendFailedException &e) {
					warningstream << "online: STUN request to " << ask.server.serializeString()
							<< " failed: " << e.what() << std::endl;
				}
			}
			next_send = porting::getTimeMs() + 400;
		}
		Address sender;
		int got = socket.Receive(sender, buffer, sizeof(buffer));
		if (got <= 0)
			continue;
		for (Ask &ask : asks) {
			if (!ask.seen)
				ask.seen = mappedAddress(buffer, got, ask.id);
		}
	}

	for (const Ask &ask : asks) {
		if (!ask.seen)
			continue;
		if (!out.pub)
			out.pub = ask.seen;
		else if (ask.seen->getPort() != out.pub->getPort())
			out.symmetric = true;
	}
	infostream << "online: STUN says this socket is "
			<< (out.pub ? out.pub->serializeString() + ":" + std::to_string(out.pub->getPort()) : std::string("unknown"))
			<< (out.symmetric ? " (symmetric NAT)" : "") << std::endl;
	return out;
}

void sendRaw(int handle, const Address &to, const void *data, size_t size)
{
	struct sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_addr = to.getAddress();
	address.sin_port = htons(to.getPort());
	sendto(handle, (const char *)data, size, 0, (struct sockaddr *)&address, sizeof(address));
}

} // namespace online
