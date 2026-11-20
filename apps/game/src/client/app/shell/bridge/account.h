// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <functional>
#include <string>

namespace shell {

/*
	The player's ArcLabs account. Signing in is the backend's device flow
	(arclabs: platform/auth/device.ts, client "terrafirma"): the browser opens
	on the approval page, the game polls until it's approved and keeps the
	key it's given. The key can only prove who the player is
	(scope terrafirma:play); it's kept in the user folder and checked against
	the backend at startup.

	All HTTP is asynchronous: step() collects answers once a frame, and every
	change of state goes to the page as an `account` message.
*/
class Account
{
public:
	using Send = std::function<void(const Json::Value &message)>;

	Account(std::string api, std::string key_path, Send send);
	~Account();
	Account(const Account &) = delete;
	Account &operator=(const Account &) = delete;

	// As the page sees it (ui/src/bridge/types.ts: Account).
	Json::Value get() const;
	// The key, while signed in (for the other ArcLabs callers: Social).
	const std::string &key() const { return m_key; }
	// The player's ArcLabs username; empty unless signed in.
	std::string username() const { return m_status == Status::SignedIn ? m_username : ""; }
	// Starts the device flow and opens the browser. Throws Refused unless signed out.
	void signIn();
	// Stops waiting for an approval.
	void cancel();
	// Forgets the key on this machine.
	void signOut();
	// Checks the saved key again after the backend couldn't be reached.
	void retry();

	// Once a frame: answers that arrived, and the next poll when it's due.
	void step();

private:
	enum class Status { SignedOut, Starting, Pending, Checking, SignedIn, Unreachable };
	enum class Ask { Authorize, Poll, Me };

	// A POST when there's a body, else a GET.
	void request(Ask ask, const std::string &path, const std::string &body = "");
	void answer(Ask ask, long code, const std::string &data, bool reached);
	void become(Status status, const std::string &problem = "");
	void check();
	void forget();

	const std::string m_api;      // the ArcLabs API (setting arclabs_url)
	const std::string m_key_path; // <user>/account.json
	const Send m_send;
	unsigned long long m_caller;
	// Bumped whenever a flow is abandoned: answers to older requests are dropped.
	unsigned long long m_generation = 0;
	bool m_in_flight = false;

	Status m_status = Status::SignedOut;
	std::string m_problem;
	std::string m_key;
	// signed in
	std::string m_id, m_username, m_name;
	// pending
	std::string m_device_code, m_user_code, m_url;
	unsigned long long m_next_poll = 0; // ms
	unsigned long long m_expires = 0;   // ms
	int m_interval = 5;                  // s
};

} // namespace shell
