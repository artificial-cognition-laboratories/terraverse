// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/http.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/porting.h"
#include <sstream>

namespace online {

HTTPFetchRequest request(const Api &api, HttpMethod method, const std::string &path,
		const Json::Value &body, long timeout_ms)
{
	HTTPFetchRequest req;
	req.url = api.url + path;
	req.method = method;
	req.timeout = timeout_ms;
	req.connect_timeout = std::min<long>(timeout_ms, 5000);
	req.quiet = true; // failures are the caller's to report
	req.extra_headers.push_back("Accept: application/json");
	if (!api.key.empty())
		req.extra_headers.push_back("Authorization: Bearer " + api.key);
	if (!body.isNull()) {
		req.raw_data = write(body);
		req.extra_headers.push_back("Content-Type: application/json");
	}
	return req;
}

HTTPFetchResult fetch(HTTPFetchRequest req)
{
	req.caller = httpfetch_caller_alloc_secure();
	httpfetch_async(req);
	HTTPFetchResult result;
	while (!httpfetch_async_get(req.caller, result))
		sleep_ms(20);
	httpfetch_caller_free(req.caller);
	return result;
}

Json::Value parse(const std::string &data)
{
	Json::Value out;
	std::string errors;
	std::istringstream in(data);
	if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &out, &errors))
		throw BaseException("online: the ArcLabs API answered with something that isn't JSON: " + errors);
	return out;
}

std::string write(const Json::Value &value)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	return Json::writeString(builder, value);
}

std::string reason(long code, const std::string &data)
{
	Json::Value value;
	std::string errors;
	std::istringstream in(data);
	if (Json::parseFromStream(Json::CharReaderBuilder(), in, &value, &errors) && value["statusMessage"].isString())
		return value["statusMessage"].asString();
	return "ArcLabs answered " + std::to_string(code);
}

} // namespace online
