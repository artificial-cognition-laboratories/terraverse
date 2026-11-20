// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/app/shell/host/inbox.h"
#include <include/wrapper/cef_message_router.h>
#include <json/json.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace shell {

// An intent's handler. Returns the response; throws Refused for an expected
// "no" (the page shows it), anything else is an engine bug.
using Verb = std::function<Json::Value(const Json::Value &intent)>;

struct Refused : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

/*
	The engine's side of the page contract (ui/src/bridge/types.ts). Intents
	arrive as JSON queries and are answered by whichever verb is registered for
	their kind right now; engine messages go out as JS calls.

	Queries arrive on Chromium's thread; each is handed to the main thread
	(Inbox) and its verb runs there, in the order the page sent them.
*/
class Bridge : public CefMessageRouterBrowserSide::Handler
{
public:
	explicit Bridge(Inbox &inbox) : m_inbox(inbox) {}
	~Bridge() { stop(); }

	void on(const std::string &kind, Verb verb);
	// For slow work: the verb runs on the bridge's worker thread, one at a
	// time, and its answer goes straight back to the page. The verb must be
	// safe off the main thread (pure, or its own locking).
	void onAsync(const std::string &kind, Verb verb);
	void off(const std::string &kind);
	// Finishes the job in hand and stops the worker. Before Chromium shuts down.
	void stop();
	void drain(); // until the worker has nothing left

	using CefMessageRouterBrowserSide::Handler::OnQuery;
	bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			int64_t query_id, const CefString &request, bool persistent,
			CefRefPtr<Callback> callback) override;

	// From the main thread (Chromium takes JS from any thread of its process).
	void send(CefRefPtr<CefBrowser> browser, const Json::Value &message);

private:
	struct Job
	{
		std::string kind;
		Verb verb;
		Json::Value intent;
		CefRefPtr<Callback> callback;
	};

	void work();
	// On the main thread: the query's verb, answered.
	void answer(const std::string &request, CefRefPtr<Callback> callback);

	Inbox &m_inbox;

	std::unordered_map<std::string, Verb> m_verbs;
	std::unordered_set<std::string> m_async;

	std::thread m_worker;
	std::mutex m_lock;
	std::condition_variable m_wake, m_idle;
	bool m_working = false;
	std::deque<Job> m_jobs;
	bool m_stopping = false;
};

} // namespace shell
