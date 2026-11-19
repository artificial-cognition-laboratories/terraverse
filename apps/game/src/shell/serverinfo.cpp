// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "serverinfo.h"
#include "server.h"
#include "client/client.h"
#include "serverenvironment.h"
#include "version.h"

namespace shell {

Json::Value describeServer(Server *local, Client &client)
{
	Json::Value out;
	out["local"] = local != nullptr;
	out["address"] = client.getAddressName();
	if (!local)
		return out;

	out["version"] = g_version_string;
	const SubgameSpec *game = local->getGameSpec();
	out["game"] = game->title.empty() ? game->id : game->title;
	out["world"] = local->getWorldPath();
	out["uptime"] = local->getUptime();
	// read without the env lock: one float, and only ever an estimate
	out["lag"] = local->getEnv().getMaxLagEstimate();

	Json::Value &players = out["players"] = Json::Value(Json::arrayValue);
	for (const Server::PlayerReport &p : local->reportPlayers()) {
		Json::Value one;
		one["name"] = p.name;
		one["position"][0] = p.position.X;
		one["position"][1] = p.position.Y;
		one["position"][2] = p.position.Z;
		one["hp"] = p.hp;
		one["rtt"] = p.rtt < 0 ? Json::Value() : Json::Value(p.rtt * 1000);
		one["connected"] = p.connected;
		one["protocol"] = p.protocol;
		players.append(std::move(one));
	}

	Json::Value &mods = out["mods"] = Json::Value(Json::arrayValue);
	for (const Server::ModReport &mod : local->modList()) {
		Json::Value one;
		one["name"] = mod.name;
		one["version"] = mod.version;
		// the game's own (@terraverse/*), or one the player added
		one["source"] = mod.name.rfind("@terraverse/", 0) == 0 ? "game" : "user";
		mods.append(std::move(one));
	}
	return out;
}

} // namespace shell
