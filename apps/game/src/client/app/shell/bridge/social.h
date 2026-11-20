// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/online/http.h"
#include "src/shared/platform/online/join.h"
#include <json/json.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace shell {

/*
	Friends, presence and invites, from the ArcLabs backend
	(arclabs: platform/terrafirma/social.ts). Only while signed in.

	Two kinds of traffic. In the background, on the main thread and never
	blocking: the friends view is polled and the player's presence beats
	(where they are, whether friends can join). The player's own actions —
	add a friend, accept, invite — are slow verbs on the bridge's worker,
	answered when the backend has: so a refusal ("no account has that
	username") reaches the page as a refusal.
*/
class Social
{
public:
	using Send = std::function<void(const Json::Value &message)>;

	Social(std::string api, Send send);
	// Says the player went offline, briefly blocking (a closing game).
	~Social();
	Social(const Social &) = delete;
	Social &operator=(const Social &) = delete;

	// The account's key, or empty when signed out (from Account).
	void signedIn(const std::string &key);
	// Entering a world, and back to the menu. `joinable`: whether friends may.
	void enter(const std::string &world, bool joinable);
	void leave();
	// The host's "friends can join". Throws Refused outside a joinable world.
	void open(bool open);
	// Where this world can be reached, while hosting (src/online/host.h); sent with presence.
	void hosting(const Json::Value &endpoint);

	// The ArcLabs API as this player, while signed in.
	std::optional<online::Api> api() const;
	// Gets ready to join a friend's world (the bridge's worker). Throws Refused.
	online::JoinPlan join(const std::string &id, const std::string &name);

	// As the page sees it (ui/src/bridge/types.ts: Social); null while signed out.
	Json::Value get() const;

	// The player's actions, on the bridge's worker. Throw Refused with the backend's reason.
	void request(const std::string &username);
	void accept(const std::string &id);
	void remove(const std::string &id);
	void invite(const std::string &id);
	void dismiss(const std::string &id);

	// Once a frame: answers that arrived, the next poll and heartbeat when due.
	void step();

private:
	enum class Ask { View, Beat };

	void background(Ask ask, const std::string &path, const Json::Value &body = Json::Value());
	void answer(Ask ask, long code, const std::string &data, bool reached);
	// Blocking, for the worker's verbs.
	Json::Value call(HttpMethod method, const std::string &path, const Json::Value &body);
	Json::Value presence() const;
	void publish();
	// Poll and beat now, not when they're next due.
	void soon() { m_refresh = true; }

	const std::string m_api;
	const Send m_send;
	unsigned long long m_caller;
	unsigned long long m_generation = 0; // bumped when the account changes
	bool m_view_in_flight = false, m_beat_in_flight = false;
	unsigned long long m_next_view = 0, m_next_beat = 0; // ms
	std::atomic<bool> m_refresh{ false };

	// Read by the worker's verbs.
	mutable std::mutex m_key_lock;
	std::string m_key;

	Json::Value m_view;     // the backend's last answer; null before the first
	std::string m_problem;  // why the view is stale, or empty
	std::string m_world;    // empty in the menu
	Json::Value m_endpoint; // null unless hosting
	bool m_joinable = false;
	bool m_open = false;
};

} // namespace shell
