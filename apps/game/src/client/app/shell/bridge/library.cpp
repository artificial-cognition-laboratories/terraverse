// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/library.h"
#include "src/client/app/shell/bridge/bridge.h"

namespace shell {

Library::Library(Opts opts) :
	m_intents(opts.shell),
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
	m_intents.on("servers.list", [](const Json::Value &) {
		Json::Value out(Json::objectValue);
		out["status"] = "unavailable";
		out["servers"] = Json::Value(Json::arrayValue);
		return out;
	});
	m_intents.on("servers.join", [](const Json::Value &) -> Json::Value {
		throw Refused("Public server discovery is not connected yet.");
	});
	m_intents.on("servers.favorite.set", [](const Json::Value &) -> Json::Value {
		throw Refused("Public server discovery is not connected yet.");
	});
}

} // namespace shell
