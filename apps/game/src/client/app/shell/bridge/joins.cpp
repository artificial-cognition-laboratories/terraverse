// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/joins.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/app/shell/host/shell.h"
#include <stdexcept>

namespace shell {

namespace {
	std::string field(const Json::Value &intent, const char *name);
}

Joins::Joins(Opts opts) : m_pending(std::move(opts.first))
{
	opts.intents.on("join.later", [this](const Json::Value &intent) {
		m_pending = Friend{ field(intent, "id"), field(intent, "name") };
		return Json::Value();
	});
	// taken once: a menu shown again later doesn't join again
	opts.intents.on("join.pending", [this](const Json::Value &) {
		if (!m_pending)
			return Json::Value();
		Json::Value out;
		out["id"] = m_pending->id;
		out["name"] = m_pending->name;
		m_pending.reset();
		return out;
	});
}

Joins::Friend Joins::parse(const std::string &arg)
{
	const size_t colon = arg.find(':');
	if (colon == std::string::npos || colon == 0 || colon + 1 == arg.size())
		throw std::invalid_argument("--join takes <account id>:<handle>, not \"" + arg + "\"");
	return { arg.substr(0, colon), arg.substr(colon + 1) };
}

namespace {

std::string field(const Json::Value &intent, const char *name)
{
	if (!intent[name].isString() || intent[name].asString().empty())
		throw Refused(std::string("join.later needs a string '") + name + "'");
	return intent[name].asString();
}

} // namespace

} // namespace shell
