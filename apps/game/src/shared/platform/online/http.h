// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/system/httpfetch.h"
#include <json/json.h>
#include <string>

namespace online {

// The ArcLabs API, and the key a player signed in with (scope terrafirma:play).
struct Api
{
	std::string url;
	std::string key;
};

// A request to the API. A body makes it carry JSON.
HTTPFetchRequest request(const Api &api, HttpMethod method, const std::string &path,
		const Json::Value &body = Json::Value(), long timeout_ms = 10000);

// Waits for a request on the calling thread. Not httpfetch_sync_interruptible:
// that one runs only on the engine's own Thread class. The request's timeout
// bounds the wait.
HTTPFetchResult fetch(HTTPFetchRequest req);

// JSON from the API; throws if it isn't.
Json::Value parse(const std::string &data);
std::string write(const Json::Value &value);
// Why the API said no: its statusMessage, or the status code.
std::string reason(long code, const std::string &data);

} // namespace online
