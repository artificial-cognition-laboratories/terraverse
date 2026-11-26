// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/upnp.h"
#include "src/shared/base/config.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/network/networkexceptions.h"
#include <string>

#if USE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#endif

namespace online {

#if USE_UPNP

struct PortMapping::Router
{
	UPNPUrls urls{};
	IGDdatas data{};
	~Router() { FreeUPNPUrls(&urls); }
};

std::unique_ptr<PortMapping> PortMapping::open(u16 port)
{
	int error = 0;
	UPNPDev *devices = upnpDiscover(2000, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &error);
	if (!devices) {
		infostream << "online: no UPnP router answered" << std::endl;
		return nullptr;
	}
	auto router = std::make_unique<Router>();
	char lan[64] = {};
#if MINIUPNPC_API_VERSION >= 18
	char wan[64] = {};
	int found = UPNP_GetValidIGD(devices, &router->urls, &router->data, lan, sizeof(lan), wan, sizeof(wan));
#else // older distributions (Ubuntu 22.04's is 17)
	int found = UPNP_GetValidIGD(devices, &router->urls, &router->data, lan, sizeof(lan));
#endif
	freeUPNPDevlist(devices);
	if (found != 1) { // 1: a connected internet gateway
		infostream << "online: no connected UPnP gateway (" << found << ")" << std::endl;
		return nullptr;
	}

	char external_ip[40] = {};
	if (UPNP_GetExternalIPAddress(router->urls.controlURL, router->data.first.servicetype, external_ip) != UPNPCOMMAND_SUCCESS) {
		infostream << "online: the UPnP gateway won't say its public address" << std::endl;
		return nullptr;
	}
	std::string p = std::to_string(port);
	// lease 0: until removed; we remove it when the world closes
	int added = UPNP_AddPortMapping(router->urls.controlURL, router->data.first.servicetype,
			p.c_str(), p.c_str(), lan, "Terraverse", "UDP", nullptr, "0");
	if (added != UPNPCOMMAND_SUCCESS) {
		infostream << "online: the UPnP gateway refused to open UDP " << port << " (" << added << ")" << std::endl;
		return nullptr;
	}

	Address external(0, 0, 0, 0, port);
	try {
		external.Resolve(external_ip);
	} catch (const ResolveError &) {
		warningstream << "online: the UPnP gateway's address \"" << external_ip << "\" isn't one" << std::endl;
		UPNP_DeletePortMapping(router->urls.controlURL, router->data.first.servicetype, p.c_str(), "UDP", nullptr);
		return nullptr;
	}
	external.setPort(port);
	infostream << "online: UPnP opened UDP " << port << " on " << external_ip << std::endl;
	return std::unique_ptr<PortMapping>(new PortMapping(std::move(router), external, port));
}

PortMapping::PortMapping(std::unique_ptr<Router> router, Address external, u16 port) :
	m_router(std::move(router)), m_external(external), m_port(port)
{
}

PortMapping::~PortMapping()
{
	std::string p = std::to_string(m_port);
	UPNP_DeletePortMapping(m_router->urls.controlURL, m_router->data.first.servicetype, p.c_str(), "UDP", nullptr);
}

#else

struct PortMapping::Router {};

std::unique_ptr<PortMapping> PortMapping::open(u16)
{
	infostream << "online: built without UPnP (miniupnpc); hole punching only" << std::endl;
	return nullptr;
}

PortMapping::PortMapping(std::unique_ptr<Router> router, Address external, u16 port) :
	m_router(std::move(router)), m_external(external), m_port(port)
{
}

PortMapping::~PortMapping() = default;

#endif

} // namespace online
