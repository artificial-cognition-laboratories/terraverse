// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/join.h"
#include "src/shared/platform/online/endpoint.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/network/socket.h"
#include "src/shared/platform/online/stun.h"

namespace online {

JoinPlan prepareJoin(const Api &api, const std::string &host_id, const std::string &host_name)
{
	auto socket = std::make_shared<UDPSocket>(UDPSocket::CreateEphemeral(false));
	Mapping mapping = probe(*socket);
	Endpoint me;
	me.pub = mapping.pub;
	me.lan = lanAddresses(socket->GetLocalAddress().getPort());
	me.nat = !mapping.pub ? "unknown" : mapping.symmetric ? "symmetric" : "cone";

	Json::Value body;
	body["host"] = host_id;
	body["endpoint"] = toJson(me);
	HTTPFetchResult result = fetch(request(api, HTTP_POST, "/api/terrafirma/join", body));
	if (!result.succeeded)
		throw JoinRefused("Couldn't reach ArcLabs. Check your connection.");
	if (result.response_code >= 500)
		throw BaseException("online: join failed (" + std::to_string(result.response_code) + "): " + result.data);
	if (result.response_code != 200)
		throw JoinRefused(reason(result.response_code, result.data));

	Json::Value answer = parse(result.data);
	if (!answer["ticket"].isString() || !answer["world"].isString())
		throw BaseException("online: join answered without a ticket or world");
	Endpoint host = endpointFromJson(answer["host"]);

	if (host.nat == "symmetric" && me.nat == "symmetric")
		throw JoinRefused("Both your network and " + host_name + "'s change ports for every connection, "
				"so your games can't reach each other directly.");
	std::optional<Address> target = route(me, host);
	if (!target)
		throw JoinRefused("Couldn't find a way to reach " + host_name + "'s world: their game doesn't know its address.");

	infostream << "online: joining " << host_name << " at " << str(*target)
			<< " (they're " << host.nat << ", we're " << me.nat << ")" << std::endl;
	return { socket, *target, answer["ticket"].asString(), answer["world"].asString(), host_name };
}

} // namespace online
