// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/exceptions.h"
#include "src/shared/platform/online/http.h"
#include "src/shared/platform/network/address.h"
#include <memory>
#include <string>

class UDPSocket;

namespace online {

// An expected "no" on the way to a friend's world; the message is for the player.
struct JoinRefused : BaseException
{
	using BaseException::BaseException;
};

// Everything the game needs to connect to a friend's world.
struct JoinPlan
{
	std::shared_ptr<UDPSocket> socket; // STUN mapped: the client connects from it
	Address target;
	std::string ticket;
	std::string world;
	std::string host_name;
};

/*
	Gets ready to join a friend's world: makes the socket the game will
	connect from, asks STUN what it looks like from outside, and asks ArcLabs
	for a ticket — which also tells the host where to send its punches. Blocks
	for a few seconds; off the main thread. Throws JoinRefused.
*/
JoinPlan prepareJoin(const Api &api, const std::string &host_id, const std::string &host_name);

} // namespace online
