// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/account.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/httpfetch.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/system/porting.h"
#include <sstream>
#include <sys/stat.h>

namespace shell {

static const char *statusName(int status)
{
	static const char *NAMES[] = { "signedOut", "starting", "pending", "checking", "signedIn", "unreachable" };
	return NAMES[status];
}

static Json::Value parse(const std::string &data)
{
	Json::Value out;
	std::string errors;
	std::istringstream in(data);
	if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &out, &errors))
		throw BaseException("shell: the ArcLabs backend answered with something that isn't JSON: " + errors);
	return out;
}

static std::string field(const Json::Value &value, const char *name, const char *what)
{
	if (!value[name].isString())
		throw BaseException(std::string("shell: the ArcLabs backend's ") + what + " has no \"" + name + "\"");
	return value[name].asString();
}

Account::Account(std::string api, std::string key_path, Send send) :
	m_api(std::move(api)), m_key_path(std::move(key_path)), m_send(std::move(send)),
	m_caller(httpfetch_caller_alloc_secure())
{
	resume();
}

void Account::resume()
{
	if (m_status != Status::SignedOut)
		throw Refused("already signed in, or signing in");
	std::string saved;
	if (!fs::PathExists(m_key_path))
		return;
	if (!fs::ReadFile(m_key_path, saved, true))
		throw BaseException("shell: could not read " + m_key_path);
	m_key = field(parse(saved), "key", "saved account");
	check();
}

Account::~Account()
{
	httpfetch_caller_free(m_caller);
}

Json::Value Account::get() const
{
	Json::Value out;
	out["status"] = statusName(static_cast<int>(m_status));
	switch (m_status) {
	case Status::SignedOut:
		if (!m_problem.empty())
			out["problem"] = m_problem;
		break;
	case Status::Pending:
		out["code"] = m_user_code;
		out["url"] = m_url;
		break;
	case Status::SignedIn:
		out["id"] = m_id;
		out["username"] = m_username;
		out["name"] = m_name.empty() ? Json::Value() : Json::Value(m_name);
		break;
	case Status::Unreachable:
		out["problem"] = m_problem;
		break;
	case Status::Starting:
	case Status::Checking:
		break;
	}
	return out;
}

void Account::signIn()
{
	if (m_status != Status::SignedOut)
		throw Refused("already signed in, or signing in");
	Json::Value body;
	body["client"] = "terrafirma";
	become(Status::Starting);
	request(Ask::Authorize, "/api/user/device/authorize", Json::writeString(Json::StreamWriterBuilder(), body));
}

void Account::cancel()
{
	if (m_status != Status::Starting && m_status != Status::Pending)
		throw Refused("not signing in");
	++m_generation;
	m_in_flight = false;
	become(Status::SignedOut);
}

void Account::signOut()
{
	if (m_status == Status::SignedOut)
		throw Refused("not signed in");
	++m_generation;
	m_in_flight = false;
	forget();
	become(Status::SignedOut);
}

void Account::leave()
{
	if (m_status == Status::SignedOut)
		throw Refused("not signed in");
	++m_generation;
	m_in_flight = false;
	m_key.clear();
	become(Status::SignedOut);
}

void Account::retry()
{
	if (m_status != Status::Unreachable)
		throw Refused("nothing to retry");
	check();
}

void Account::check()
{
	become(Status::Checking);
	request(Ask::Me, "/api/terrafirma/me");
}

void Account::forget()
{
	m_key.clear();
	if (fs::PathExists(m_key_path) && !fs::DeleteSingleFileOrEmptyDirectory(m_key_path, true))
		throw BaseException("shell: could not delete " + m_key_path);
}

void Account::become(Status status, const std::string &problem)
{
	m_status = status;
	m_problem = problem;
	Json::Value message;
	message["kind"] = "account";
	message["account"] = get();
	m_send(message);
}

void Account::request(Ask ask, const std::string &path, const std::string &body)
{
	HTTPFetchRequest req;
	req.url = m_api + path;
	req.caller = m_caller;
	req.request_id = (m_generation << 2) | static_cast<unsigned long long>(ask);
	req.timeout = 15000;
	req.connect_timeout = 5000;
	req.quiet = true; // failures are ours to report, to the player
	req.extra_headers.push_back("Accept: application/json");
	if (!m_key.empty())
		req.extra_headers.push_back("Authorization: Bearer " + m_key);
	if (!body.empty()) {
		req.method = HTTP_POST;
		req.raw_data = body;
		req.extra_headers.push_back("Content-Type: application/json");
	}
	m_in_flight = true;
	httpfetch_async(req);
}

void Account::step()
{
	HTTPFetchResult result;
	while (httpfetch_async_get(m_caller, result)) {
		if ((result.request_id >> 2) != m_generation)
			continue; // a flow that was abandoned
		m_in_flight = false;
		auto ask = static_cast<Ask>(result.request_id & 3);
		answer(ask, result.response_code, result.data, result.succeeded);
	}

	if (m_status != Status::Pending || m_in_flight)
		return;
	u64 now = porting::getTimeMs();
	if (now >= m_expires) {
		++m_generation;
		become(Status::SignedOut, "The sign-in code expired. Try again.");
	} else if (now >= m_next_poll) {
		request(Ask::Poll, "/api/user/device/poll?device_code=" + m_device_code);
	}
}

void Account::answer(Ask ask, long code, const std::string &data, bool reached)
{
	if (!reached || code >= 500 || code == 0) {
		std::string why = reached ? "the ArcLabs backend failed (" + std::to_string(code) + ")"
				: "couldn't reach " + m_api;
		warningstream << "shell: account: " << why << std::endl;
		if (ask == Ask::Me)
			become(Status::Unreachable, why);
		else
			become(Status::SignedOut, "Sign-in stopped: " + why + ".");
		return;
	}
	// A refusal while signing in (a rate limit, a code the backend no longer
	// knows) ends the attempt; the player can start again.
	if (ask != Ask::Me && code != 200) {
		errorstream << "shell: account: the backend refused (" << code << "): " << data << std::endl;
		become(Status::SignedOut, "Sign-in was refused by ArcLabs (" + std::to_string(code) + "). Try again.");
		return;
	}

	switch (ask) {
	case Ask::Authorize: {
		Json::Value value = parse(data);
		m_device_code = field(value, "device_code", "device authorization");
		m_user_code = field(value, "user_code", "device authorization");
		m_url = field(value, "verification_uri_complete", "device authorization");
		m_interval = value["interval"].asInt();
		u64 now = porting::getTimeMs();
		m_expires = now + value["expires_in"].asUInt64() * 1000;
		m_next_poll = now + m_interval * 1000;
		become(Status::Pending);
		if (!porting::open_url(m_url))
			warningstream << "shell: account: couldn't open the browser; the page shows the link" << std::endl;
		break;
	}
	case Ask::Poll: {
		Json::Value value = parse(data);
		std::string status = field(value, "status", "device poll");
		if (status == "pending") {
			m_next_poll = porting::getTimeMs() + m_interval * 1000;
		} else if (status == "expired") {
			become(Status::SignedOut, "The sign-in code expired. Try again.");
		} else if (status == "approved") {
			m_key = field(value, "access_token", "device poll");
			Json::Value saved;
			saved["key"] = m_key;
			if (!fs::safeWriteToFile(m_key_path, Json::writeString(Json::StreamWriterBuilder(), saved)))
				throw BaseException("shell: could not save the account to " + m_key_path);
#ifndef _WIN32
			chmod(m_key_path.c_str(), 0600); // it's a credential
#endif
			check();
		} else {
			throw BaseException("shell: device poll says \"" + status + "\"");
		}
		break;
	}
	case Ask::Me: {
		if (code == 401 || code == 403) {
			forget();
			become(Status::SignedOut, "Your sign-in was revoked or has expired. Sign in again.");
			return;
		}
		if (code != 200)
			throw BaseException("shell: /api/terrafirma/me answered " + std::to_string(code) + ": " + data);
		Json::Value value = parse(data);
		m_id = field(value, "id", "account");
		m_username = field(value, "username", "account");
		m_name = value["name"].isString() ? value["name"].asString() : "";
		become(Status::SignedIn);
		infostream << "shell: signed in to ArcLabs as " << m_username << std::endl;
		break;
	}
	}
}

} // namespace shell
