// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include "src/shared/platform/network/networkprotocol.h" // session_t
#include "src/shared/platform/network/socket.h" // Address, UDPSocket
#include <functional>

class NetworkPacket;
class PeerHandler;

namespace con
{

enum rtt_stat_type : int {
	MIN_RTT,
	MAX_RTT,
	AVG_RTT,
	MIN_JITTER,
	MAX_JITTER,
	AVG_JITTER
};

enum rate_stat_type : int {
	CUR_DL_RATE,
	AVG_DL_RATE,
	CUR_INC_RATE,
	AVG_INC_RATE,
	CUR_LOSS_RATE,
	AVG_LOSS_RATE,
};

class IPeer {
public:
	// Unique id of the peer
	const session_t id;

	virtual const Address &getAddress() const = 0;

protected:
	IPeer(session_t id) : id(id) {}
	~IPeer() {}
};

class IConnection
{
public:
	virtual ~IConnection() = default;

	virtual void Connect(Address address) = 0;
	virtual bool Connected() = 0;
	virtual void Disconnect() = 0;
	virtual void DisconnectPeer(session_t peer_id) = 0;

	virtual bool ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms) = 0;
	bool TryReceive(NetworkPacket *pkt) {
		return ReceiveTimeoutMs(pkt, 0);
	}

	virtual void Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable) = 0;

	virtual session_t GetPeerID() const = 0;
	virtual Address GetPeerAddress(session_t peer_id) = 0;
	virtual float getPeerStat(session_t peer_id, rtt_stat_type type) = 0;
	virtual float getLocalStat(rate_stat_type type) = 0;

	// Terra Firma: datagrams on this socket that aren't the game's (a STUN
	// server's answers, src/online/host.h) go here instead of being dropped.
	// Called on the receive thread.
	using StrayHandler = std::function<void(const Address &sender, const u8 *data, size_t size)>;
	virtual void setStrayHandler(StrayHandler handler) = 0;
};

// MTP = Minetest Protocol
IConnection *createMTP(bool is_server, UDPSocket &&socket, PeerHandler *handler);

} // namespace
