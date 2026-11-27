// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include "src/shared/platform/system/httpfetch.h"
#include <atomic>
#include <string>

namespace shell {

class Bridge;
class Account;
class Social;

/*
	The signed-in player's own ArcLabs profile, edited from the Account page
	(ui: bridge/types.ts, account.profile and kin; arclabs: api/terrafirma/profile).
	Each verb is one blocking call on the bridge's worker, refused with the
	backend's reason. A rename also changes who the account is: once it's done
	the account reads itself again on the main thread, in the menu only, as
	that moves the profile's folder (home::Home::signedIn).
*/
class Identity
{
public:
	struct Opts {
		Bridge &bridge;   // where the verbs are registered, for the session
		Account &account; // read again after a rename
		Social &social;   // the API as this player (its key, behind its lock)
	};
	explicit Identity(Opts opts);

	// Once a frame, on the main thread. `in_menu`: no world is open, so its folder can move.
	void step(bool in_menu);

private:
	Json::Value profile() const;
	Json::Value edit(const Json::Value &edit) const;
	Json::Value available(const std::string &username) const;
	Json::Value rename(const std::string &username);
	Json::Value call(HttpMethod method, const std::string &path, const Json::Value &body) const;

	Account &m_account;
	Social &m_social;
	std::atomic<bool> m_in_menu{ true };
	std::atomic<bool> m_renamed{ false };
};

} // namespace shell
