// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/social.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/httpfetch.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/system/porting.h"
#include <cctype>

namespace shell {

using online::parse;
using online::reason;

// The friends view is polled; presence beats. The backend reads a beat older
// than 90 s as offline (arclabs: platform/terrafirma/social.ts FRESH_MS).
static const u64 VIEW_MS = 15000;
static const u64 BEAT_MS = 30000;

// Ids from the page go into URL paths: only the shape of a uuid gets there.
static const std::string &checked(const std::string &id)
{
	bool ok = id.size() == 36;
	for (size_t i = 0; ok && i < id.size(); ++i) {
		char c = id[i];
		ok = (i == 8 || i == 13 || i == 18 || i == 23) ? c == '-' : std::isxdigit(static_cast<unsigned char>(c));
	}
	if (!ok)
		throw Refused("not a player or invite id: " + id);
	return id;
}

Social::Social(std::string api, Send send) :
	m_api(std::move(api)), m_send(std::move(send)), m_caller(httpfetch_caller_alloc_secure())
{
}

Social::~Social()
{
	// Offline now, rather than when the last beat goes stale.
	std::string key;
	{
		std::lock_guard<std::mutex> lock(m_key_lock);
		key = m_key;
	}
	if (!key.empty()) {
		Json::Value body;
		body["presence"] = Json::Value();
		online::fetch(online::request({ m_api, key }, HTTP_PUT, "/api/terrafirma/presence", body, 2000));
	}
	httpfetch_caller_free(m_caller);
}

void Social::signedIn(const std::string &key)
{
	std::string old;
	{
		std::lock_guard<std::mutex> lock(m_key_lock);
		if (key == m_key)
			return;
		old = m_key;
		m_key = key;
	}
	if (!old.empty()) { // the old account is offline now; nobody waits for the answer
		Json::Value body;
		body["presence"] = Json::Value();
		httpfetch_async(online::request({ m_api, old }, HTTP_PUT, "/api/terrafirma/presence", body, 5000));
	}
	++m_generation;
	m_view_in_flight = m_beat_in_flight = false;
	m_view = Json::Value();
	m_problem.clear();
	m_next_view = m_next_beat = 0;
	publish();
}

void Social::enter(const std::string &world, bool joinable)
{
	m_world = world;
	m_joinable = joinable;
	m_open = false; // friends join only when the host says so, each time
	soon();
	publish();
}

void Social::hosting(const Json::Value &endpoint)
{
	m_endpoint = endpoint;
	soon();
}

std::optional<online::Api> Social::api() const
{
	std::lock_guard<std::mutex> lock(m_key_lock);
	if (m_key.empty())
		return std::nullopt;
	return online::Api{ m_api, m_key };
}

online::JoinPlan Social::join(const std::string &id, const std::string &name)
{
	std::optional<online::Api> as = api();
	if (!as)
		throw Refused("sign in to ArcLabs first");
	try {
		return online::prepareJoin(*as, checked(id), name);
	} catch (const online::JoinRefused &e) {
		throw Refused(e.what());
	}
}

void Social::leave()
{
	m_endpoint = Json::Value();
	if (m_world.empty())
		return;
	m_world.clear();
	m_joinable = m_open = false;
	soon();
	publish();
}

void Social::open(bool open)
{
	if (m_world.empty() || !m_joinable)
		throw Refused("friends can only join your own world");
	m_open = open;
	soon();
	publish();
}

Json::Value Social::presence() const
{
	Json::Value out;
	if (m_world.empty()) {
		out["state"] = "menu";
		out["world"] = Json::Value();
	} else {
		out["state"] = m_joinable ? "hosting" : "playing";
		out["world"] = m_world;
	}
	out["open"] = m_open;
	return out;
}

Json::Value Social::get() const
{
	{
		std::lock_guard<std::mutex> lock(m_key_lock);
		if (m_key.empty())
			return Json::Value();
	}
	Json::Value out;
	out["me"] = presence();
	out["joinable"] = !m_world.empty() && m_joinable;
	// null until the backend first answers: the page shows nothing rather than an empty list
	out["view"] = m_view;
	if (!m_problem.empty())
		out["problem"] = m_problem;
	return out;
}

void Social::publish()
{
	Json::Value message;
	message["kind"] = "social";
	message["social"] = get();
	m_send(message);
}

void Social::background(Ask ask, const std::string &path, const Json::Value &body)
{
	HTTPFetchRequest req = online::request({ m_api, m_key }, body.isNull() ? HTTP_GET : HTTP_PUT, path, body);
	req.caller = m_caller;
	req.request_id = (m_generation << 1) | static_cast<u64>(ask);
	(ask == Ask::View ? m_view_in_flight : m_beat_in_flight) = true;
	httpfetch_async(req);
}

void Social::step()
{
	HTTPFetchResult result;
	while (httpfetch_async_get(m_caller, result)) {
		if ((result.request_id >> 1) != m_generation)
			continue; // for an account that's gone
		auto ask = static_cast<Ask>(result.request_id & 1);
		(ask == Ask::View ? m_view_in_flight : m_beat_in_flight) = false;
		answer(ask, result.response_code, result.data, result.succeeded);
	}

	if (m_key.empty()) // only the main thread writes it
		return;
	u64 now = porting::getTimeMs();
	if (m_refresh.exchange(false))
		m_next_view = m_next_beat = now;
	if (!m_beat_in_flight && now >= m_next_beat) {
		Json::Value body;
		body["presence"] = presence();
		if (!m_world.empty() && m_joinable && !m_endpoint.isNull())
			body["endpoint"] = m_endpoint;
		background(Ask::Beat, "/api/terrafirma/presence", body);
		m_next_beat = now + BEAT_MS;
	}
	if (!m_view_in_flight && now >= m_next_view) {
		background(Ask::View, "/api/terrafirma/social");
		m_next_view = now + VIEW_MS;
	}
}

void Social::answer(Ask ask, long code, const std::string &data, bool reached)
{
	if (!reached || code != 200) {
		std::string why = reached ? reason(code, data) : "couldn't reach ArcLabs";
		warningstream << "shell: social: " << why << std::endl;
		if (why != m_problem) {
			m_problem = why;
			publish();
		}
		return;
	}
	if (ask == Ask::Beat)
		return;

	Json::Value view = parse(data);
	for (const char *list : { "friends", "incoming", "outgoing", "invites" }) {
		if (!view[list].isArray())
			throw BaseException(std::string("shell: the ArcLabs friends view has no \"") + list + "\" list");
	}
	if (view == m_view && m_problem.empty())
		return;
	m_view = view;
	m_problem.clear();
	publish();
}

Json::Value Social::call(HttpMethod method, const std::string &path, const Json::Value &body)
{
	std::optional<online::Api> as = api();
	if (!as)
		throw Refused("sign in to ArcLabs first");
	HTTPFetchRequest req = online::request(*as, method, path, body);
	HTTPFetchResult result = online::fetch(req);
	if (!result.succeeded)
		throw Refused("Couldn't reach ArcLabs. Check your connection.");
	if (result.response_code >= 500)
		throw BaseException("shell: social: " + path + " failed (" +
				std::to_string(result.response_code) + "): " + result.data);
	if (result.response_code != 200)
		throw Refused(reason(result.response_code, result.data));
	soon(); // show the change without waiting for the next poll
	return parse(result.data);
}

void Social::request(const std::string &username)
{
	if (username.empty() || username.size() > 64)
		throw Refused("type their ArcLabs username");
	Json::Value body;
	body["username"] = username;
	call(HTTP_POST, "/api/terrafirma/friends", body);
}

void Social::accept(const std::string &id)
{
	call(HTTP_POST, "/api/terrafirma/friends/" + checked(id) + "/accept", Json::Value());
}

void Social::remove(const std::string &id)
{
	call(HTTP_DELETE, "/api/terrafirma/friends/" + checked(id), Json::Value());
}

void Social::invite(const std::string &id)
{
	Json::Value body;
	body["to"] = checked(id);
	call(HTTP_POST, "/api/terrafirma/invites", body);
}

void Social::dismiss(const std::string &id)
{
	call(HTTP_DELETE, "/api/terrafirma/invites/" + checked(id), Json::Value());
}

} // namespace shell
