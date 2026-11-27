// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/system/httpfetch.h"
#include <json/json.h>
#include <string>

namespace shell {

class Bridge;
class Social;

/*
	The signed-in player's own ArcLabs profile, edited from the Account page
	(ui: bridge/types.ts, account.profile and account.edit; arclabs:
	api/terrafirma/profile). Each verb is one blocking call on the bridge's
	worker, refused with the backend's reason. The handle isn't edited: it's
	chosen once, on the site, at the first sign-in.
*/
class Identity
{
public:
	struct Opts {
		Bridge &bridge; // where the verbs are registered, for the session
		Social &social; // the API as this player (its key, behind its lock)
	};
	explicit Identity(Opts opts);

private:
	Json::Value profile() const;
	Json::Value edit(const Json::Value &edit) const;
	Json::Value call(HttpMethod method, const std::string &path, const Json::Value &body) const;

	Social &m_social;
};

} // namespace shell
