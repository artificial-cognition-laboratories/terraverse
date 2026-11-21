// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/registry.h"
#include "src/client/app/shell/host/shell.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/platform/online/http.h"
#include "src/shared/util/text/string.h"
#include <regex>

namespace shell {

namespace {
	// The backend's owner of the official mods: featured on Discover.
	const char *OFFICIAL = "terraverse";

	const std::string &packageName(const std::string &name);
	Json::Value listing(const Json::Value &row, const Json::Value &latest);
	void refuseUnsupported(const Json::Value &search);
}

Registry::Registry(Opts opts) : m_api(std::move(opts.api))
{
	opts.intents.onAsync("registry.front", [this](const Json::Value &) { return front(); });
	opts.intents.onAsync("registry.search", [this](const Json::Value &intent) { return search(intent); });
	opts.intents.onAsync("registry.mod", [this](const Json::Value &intent) {
		if (!intent["name"].isString())
			throw Refused("missing string field 'name'");
		return mod(intent["name"].asString());
	});
	opts.intents.onAsync("registry.file", [](const Json::Value &) -> Json::Value {
		throw Refused("the registry doesn't serve a mod's files yet");
	});
}

// Discover from what the backend has: the catalogue by stars, the official mods out of it.
Json::Value Registry::front() const
{
	const Json::Value page = get("/api/terrafirma/registry/search?page=1");
	Json::Value out;
	out["featured"] = Json::Value(Json::arrayValue);
	out["categories"] = Json::Value(Json::arrayValue);
	out["shelves"] = Json::Value(Json::arrayValue);
	Json::Value shelf;
	shelf["id"] = "stars";
	shelf["title"] = "Most starred";
	shelf["search"]["sort"] = "relevance";
	shelf["listings"] = Json::Value(Json::arrayValue);
	for (const Json::Value &row : page["mods"]) {
		const Json::Value item = listing(row, Json::Value());
		shelf["listings"].append(item);
		if (item["official"].asBool())
			out["featured"].append(item);
	}
	if (!shelf["listings"].empty())
		out["shelves"].append(shelf);
	return out;
}

Json::Value Registry::search(const Json::Value &search) const
{
	refuseUnsupported(search);
	const std::string query = search["query"].isString() ? search["query"].asString() : "";
	const int page = search["page"].isInt() && search["page"].asInt() > 0 ? search["page"].asInt() : 1;
	const Json::Value answer = get("/api/terrafirma/registry/search?q=" + urlencode(query) + "&page=" + std::to_string(page));
	Json::Value out;
	out["listings"] = Json::Value(Json::arrayValue);
	for (const Json::Value &row : answer["mods"])
		out["listings"].append(listing(row, Json::Value()));
	out["total"] = answer["total"];
	out["page"] = answer["page"];
	out["pageSize"] = answer["pageSize"];
	return out;
}

Json::Value Registry::mod(const std::string &name) const
{
	const Json::Value page = get("/api/terrafirma/registry/packages/" + packageName(name));
	const Json::Value &latest = page["versions"][0];
	Json::Value out = listing(page, latest);
	out["readme"] = page["readme"].isString() ? page["readme"] : Json::Value("");
	out["deprecated"] = page["deprecated"];
	out["files"] = Json::Value();
	// README media is served per version; its relative links resolve against this
	out["media"] = latest.isNull() ? Json::Value()
		: Json::Value(m_api + "/api/terrafirma/registry/assets/" + page["name"].asString() + "/" + latest["version"].asString() + "/");
	out["versions"] = Json::Value(Json::arrayValue);
	for (const Json::Value &v : page["versions"]) {
		Json::Value version;
		version["version"] = v["version"];
		version["publishedAt"] = v["publishedAt"];
		version["api"] = v["api"];
		version["title"] = v["mod"].isObject() ? v["mod"]["title"] : Json::Value();
		version["dependencies"] = v["dependencies"].isObject() ? v["dependencies"] : Json::Value(Json::objectValue);
		version["withheld"] = v["withheld"];
		out["versions"].append(version);
	}
	return out;
}

// One blocking GET; a refusal carries the backend's reason to the page.
Json::Value Registry::get(const std::string &path) const
{
	const HTTPFetchResult res = online::fetch(online::request({ m_api, "" }, HTTP_GET, path));
	if (!res.succeeded)
		throw Refused("the registry couldn't be reached");
	if (res.response_code != 200)
		throw Refused(online::reason(res.response_code, res.data));
	return online::parse(res.data);
}

namespace {
	// Names from the page go into a URL path: only @scope/name gets there.
	const std::string &packageName(const std::string &name)
	{
		static const std::regex shape("^@[a-z0-9][a-z0-9._-]*/[a-z0-9][a-z0-9._-]*$");
		if (!std::regex_match(name, shape))
			throw Refused("not a mod name: " + name);
		return name;
	}

	// A search row or a package page as the page's RegistryListing. What the backend
	// doesn't keep yet is null or empty, and the page draws nothing for it.
	Json::Value listing(const Json::Value &row, const Json::Value &latest)
	{
		Json::Value out;
		out["name"] = row["name"];
		out["title"] = latest["mod"].isObject() ? latest["mod"]["title"] : Json::Value();
		out["description"] = row["description"].isString() ? row["description"] : Json::Value("");
		out["owner"] = row["owner"];
		out["latestVersion"] = row["latestVersion"];
		out["stars"] = row["stars"];
		out["updatedAt"] = row["updatedAt"].isString() ? row["updatedAt"] : latest["publishedAt"];
		out["type"] = "mod";
		out["categories"] = Json::Value(Json::arrayValue);
		out["icon"] = Json::Value();
		out["banner"] = Json::Value();
		out["installs"] = Json::Value();
		out["installed"] = false;
		out["official"] = row["owner"].asString() == OFFICIAL;
		return out;
	}

	void refuseUnsupported(const Json::Value &search)
	{
		if (search["sort"].isString() && search["sort"].asString() != "relevance")
			throw Refused("the registry only sorts by best match so far");
		if (search["categories"].isArray() && !search["categories"].empty())
			throw Refused("the registry doesn't keep categories yet");
		if (search["type"].isString() && search["type"].asString() != "mod")
			throw Refused("the registry has no texture packs yet");
		if (search["installed"].asBool())
			throw Refused("installing from the registry isn't wired yet");
	}
}

} // namespace shell
