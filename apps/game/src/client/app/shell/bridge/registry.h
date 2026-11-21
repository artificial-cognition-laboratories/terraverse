// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>

namespace shell {

class Intents;

/*
	The ArcLabs mod registry, as the community panel asks for it
	(ui: bridge/types.ts, registry.*; arclabs: api/terrafirma/registry).
	Answers on the bridge's worker: each verb is one blocking call to the
	backend, its answer reshaped for the page. What the backend can't do yet
	(sorts but its own, categories, kinds, file listings) is refused with the
	reason, not faked.
*/
class Registry
{
public:
	struct Opts {
		Intents &intents; // where the verbs are registered, and removed with the screen
		std::string api;  // the backend's base URL (arclabs_url)
	};

	explicit Registry(Opts opts);

private:
	Json::Value front() const;
	Json::Value search(const Json::Value &search) const;
	Json::Value mod(const std::string &name) const;
	Json::Value get(const std::string &path) const;

	const std::string m_api;
};

} // namespace shell
