// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/network/address.h"
#include <memory>
#include <optional>

namespace online {

/*
	A UDP port opened on the router by UPnP, for as long as this lives. With
	one, friends reach the host directly whatever its NAT does, which is what
	saves a host behind a symmetric NAT. Many routers don't allow it; then
	open() gives null and hole punching is all there is.
*/
class PortMapping
{
public:
	// Blocks for up to ~2 s looking for the router.
	static std::unique_ptr<PortMapping> open(u16 port);
	~PortMapping();
	PortMapping(const PortMapping &) = delete;
	PortMapping &operator=(const PortMapping &) = delete;

	// The router's public address, with the mapped port.
	const Address &external() const { return m_external; }

private:
	struct Router;
	PortMapping(std::unique_ptr<Router> router, Address external, u16 port);
	std::unique_ptr<Router> m_router;
	Address m_external;
	u16 m_port;
};

} // namespace online
