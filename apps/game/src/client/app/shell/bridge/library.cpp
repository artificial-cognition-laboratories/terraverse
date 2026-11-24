// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/library.h"
#include "src/client/app/shell/bridge/bridge.h"

namespace shell {

Library::Library(Opts opts) :
	m_intents(opts.shell),
	m_local_servers(opts.local_servers),
	m_local_mods({ .dir = opts.contents_dir, .session = opts.session }),
	m_registry({ .intents = m_intents, .api = opts.api })
{
	m_intents.onAsync("mods.local", [this](const Json::Value &) {
		return m_local_mods.list();
	});
	m_intents.onAsync("mods.contents", [this](const Json::Value &intent) {
		if (!intent["path"].isString())
			throw Refused("missing string field 'path'");
		return m_local_mods.contents(intent["path"].asString());
	});
	// the public directory isn't wired yet (unavailable); servers on this machine are
	m_intents.on("servers.list", [this](const Json::Value &) {
		Json::Value out(Json::objectValue);
		out["status"] = "unavailable";
		out["servers"] = Json::Value(Json::arrayValue);
		out["local"] = m_local_servers.list();
		return out;
	});
	m_intents.on("servers.favorite.set", [](const Json::Value &) -> Json::Value {
		throw Refused("Public server discovery is not connected yet.");
	});
}

} // namespace shell
