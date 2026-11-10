// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "endpoint.h"
#include "exceptions.h"
#include "log.h"
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace online {

std::string str(const Address &address)
{
	return address.isIPv6() ? "[" + address.serializeString() + "]:" + std::to_string(address.getPort())
			: address.serializeString() + ":" + std::to_string(address.getPort());
}

Address parseAddress(const std::string &text)
{
	unsigned a, b, c, d, port;
	char tail;
	if (std::sscanf(text.c_str(), "%u.%u.%u.%u:%u%c", &a, &b, &c, &d, &port, &tail) != 5 ||
			a > 255 || b > 255 || c > 255 || d > 255 || port == 0 || port > 65535)
		throw BaseException("online: \"" + text + "\" isn't an IPv4 address with a port");
	return Address(a, b, c, d, port);
}

Json::Value toJson(const Endpoint &endpoint)
{
	Json::Value out;
	out["public"] = endpoint.pub ? Json::Value(str(*endpoint.pub)) : Json::Value();
	out["lan"] = Json::Value(Json::arrayValue);
	for (const Address &address : endpoint.lan)
		out["lan"].append(str(address));
	out["nat"] = endpoint.nat;
	return out;
}

Endpoint endpointFromJson(const Json::Value &value)
{
	if (!value.isObject() || !value["lan"].isArray() || !value["nat"].isString())
		throw BaseException("online: an endpoint needs public, lan and nat");
	Endpoint out;
	if (value["public"].isString())
		out.pub = parseAddress(value["public"].asString());
	for (const Json::Value &address : value["lan"]) {
		if (!address.isString())
			throw BaseException("online: an endpoint's lan addresses are strings");
		out.lan.push_back(parseAddress(address.asString()));
	}
	out.nat = value["nat"].asString();
	return out;
}

// Home and office networks first; container bridges (172.17–172.31 are
// Docker's) and carrier-grade ranges (100.64/10: Tailscale, mobile) last.
static int rank(const Address &a)
{
	u32 ip = ntohl(a.getAddress().s_addr);
	if ((ip >> 16) == 0xC0A8) return 0;                          // 192.168/16
	if ((ip >> 24) == 10) return 1;                              // 10/8
	if ((ip >> 20) == 0xAC1 && ((ip >> 16) & 0xF) == 0) return 2; // 172.16/16
	if ((ip >> 20) == 0xAC1) return 3;                           // 172.17–31: bridges
	return 4;
}

std::vector<Address> lanAddresses(u16 port)
{
	std::vector<Address> out;
#ifdef _WIN32
	// The adapter list's size isn't known up front: ask, grow, ask again.
	std::vector<unsigned char> buffer(16 * 1024);
	ULONG size = (ULONG)buffer.size();
	const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
	ULONG result;
	while ((result = GetAdaptersAddresses(AF_INET, flags, nullptr,
			reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size)) == ERROR_BUFFER_OVERFLOW)
		buffer.resize(size);
	if (result != NO_ERROR)
		throw BaseException("online: couldn't list this machine's network interfaces");
	for (auto *a = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()); a; a = a->Next) {
		if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
			continue;
		for (auto *u = a->FirstUnicastAddress; u; u = u->Next) {
			if (u->Address.lpSockaddr->sa_family != AF_INET)
				continue;
			auto *in = reinterpret_cast<const struct sockaddr_in *>(u->Address.lpSockaddr);
			out.emplace_back(ntohl(in->sin_addr.s_addr), port);
		}
	}
#else
	struct ifaddrs *list = nullptr;
	if (getifaddrs(&list) != 0)
		throw BaseException("online: couldn't list this machine's network interfaces");
	for (struct ifaddrs *i = list; i; i = i->ifa_next) {
		if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET)
			continue;
		if ((i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP))
			continue;
		auto *in = reinterpret_cast<const struct sockaddr_in *>(i->ifa_addr);
		out.emplace_back(ntohl(in->sin_addr.s_addr), port);
	}
	freeifaddrs(list);
#endif
	std::stable_sort(out.begin(), out.end(), [](const Address &a, const Address &b) { return rank(a) < rank(b); });
	if (out.size() > 8) // ArcLabs keeps at most 8
		out.resize(8);
	return out;
}

static bool sameHost(const Address &a, const Address &b)
{
	return a.getFamily() == b.getFamily() && a.serializeString() == b.serializeString();
}

std::optional<Address> route(const Endpoint &me, const Endpoint &host)
{
	bool same_network = me.pub && host.pub && sameHost(*me.pub, *host.pub);
	if (same_network && !host.lan.empty())
		return host.lan.front();
	if (host.pub)
		return host.pub;
	// Neither of us knows our public address: a LAN address is the only hope.
	if (!host.lan.empty())
		return host.lan.front();
	return std::nullopt;
}

} // namespace online
