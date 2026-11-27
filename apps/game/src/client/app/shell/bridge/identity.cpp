// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/identity.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/app/shell/bridge/social.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/online/http.h"
#include <optional>

namespace shell {

namespace {
	// Only the fields the page may change are passed on; the backend checks their values.
	Json::Value editOf(const Json::Value &intent);
}

Identity::Identity(Opts opts) : m_social(opts.social)
{
	opts.bridge.onAsync("account.profile", [this](const Json::Value &) { return profile(); });
	opts.bridge.onAsync("account.edit", [this](const Json::Value &intent) { return edit(editOf(intent)); });
}

Json::Value Identity::profile() const
{
	return call(HTTP_GET, "/api/terrafirma/profile", Json::Value());
}

Json::Value Identity::edit(const Json::Value &edit) const
{
	return call(HTTP_PATCH, "/api/terrafirma/profile", edit);
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
