// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/online/http.h"
#include <memory>
#include <string>

class UDPSocket;

namespace online {

/*
	How a game goes online, decided by the menu and carried to the game in
	GameStartData. The ticket itself rides in GameClientData::password: the
	host's own secret, or the one ArcLabs gave a joiner.
*/
struct Launch
{
	enum class Kind { Offline, Host, Join } kind = Kind::Offline;
	Api api;
	// Join: the socket STUN mapped, which the client must connect from.
	std::shared_ptr<UDPSocket> socket;
	// Join: whose world, for the messages a failed connection gives.
	std::string host_name;
};

} // namespace online
