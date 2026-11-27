// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/identity.h"
#include "src/client/app/shell/bridge/account.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/app/shell/bridge/social.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/online/http.h"
#include "src/shared/util/text/string.h"
#include <optional>

namespace shell {

namespace {
	std::string handleOf(const Json::Value &intent);
	// Only the fields the page may change are passed on; the backend checks their values.
	Json::Value editOf(const Json::Value &intent);
}

Identity::Identity(Opts opts) : m_account(opts.account), m_social(opts.social)
{
	opts.bridge.onAsync("account.profile", [this](const Json::Value &) { return profile(); });
	opts.bridge.onAsync("account.edit", [this](const Json::Value &intent) { return edit(editOf(intent)); });
	opts.bridge.onAsync("account.available", [this](const Json::Value &intent) { return available(handleOf(intent)); });
	opts.bridge.onAsync("account.rename", [this](const Json::Value &intent) { return rename(handleOf(intent)); });
}

void Identity::step(bool in_menu)
{
	m_in_menu = in_menu;
	if (in_menu && m_renamed.exchange(false))
		m_account.reread();
}

Json::Value Identity::profile() const
{
	return call(HTTP_GET, "/api/terrafirma/profile", Json::Value());
}

Json::Value Identity::edit(const Json::Value &edit) const
{
	return call(HTTP_PATCH, "/api/terrafirma/profile", edit);
}

Json::Value Identity::available(const std::string &username) const
{
	return call(HTTP_GET, "/api/users/" + urlencode(username) + "/available", Json::Value());
}

Json::Value Identity::rename(const std::string &username)
{
	// the profile's folder follows the handle, and a world open in it can't move
	if (!m_in_menu)
		throw Refused("Leave your world first: renaming moves this account's worlds with it.");
	Json::Value body;
	body["username"] = username;
	Json::Value out = call(HTTP_POST, "/api/terrafirma/profile/username", body);
	m_renamed = true;
	return out;
}

Json::Value Identity::call(HttpMethod method, const std::string &path, const Json::Value &body) const
{
	std::optional<online::Api> as = m_social.api();
	if (!as)
		throw Refused("Sign in to ArcLabs first.");
	HTTPFetchResult result = online::fetch(online::request(*as, method, path, body));
	if (!result.succeeded)
		throw Refused("Couldn't reach ArcLabs. Check your connection.");
	if (result.response_code >= 500)
		throw BaseException("shell: identity: " + path + " failed (" +
				std::to_string(result.response_code) + "): " + result.data);
	if (result.response_code != 200)
		throw Refused(online::reason(result.response_code, result.data));
	return online::parse(result.data);
}

namespace {

std::string handleOf(const Json::Value &intent)
{
	if (!intent["username"].isString() || intent["username"].asString().empty() || intent["username"].asString().size() > 64)
		throw Refused("type a username");
	return intent["username"].asString();
}

Json::Value editOf(const Json::Value &intent)
{
	static const char *const FIELDS[] = { "bio", "location", "website", "links" };
	Json::Value out(Json::objectValue);
	for (const char *field : FIELDS)
		if (intent.isMember(field))
			out[field] = intent[field];
	if (out.empty())
		throw Refused("nothing to change");
	return out;
}

} // namespace

} // namespace shell
