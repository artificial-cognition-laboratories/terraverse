// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/inspectors.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/base/exceptions.h"

namespace shell {

void Inspectors::add(const std::string &name, Inspector inspector)
{
	if (!m_inspectors.emplace(name, std::move(inspector)).second)
		throw BaseException("inspectors: '" + name + "' is already inspected");
}

void Inspectors::remove(const std::string &name)
{
	m_inspectors.erase(name);
}

Json::Value Inspectors::describe() const
{
	Json::Value out(Json::objectValue);
	for (const auto &[name, inspector] : m_inspectors)
		out[name] = inspector.describe();
	return out;
}

void Inspectors::act(const Json::Value &intent)
{
	if (!intent["name"].isString() || !intent["control"].isString())
		throw Refused("inspect.act needs strings 'name' and 'control'");
	const auto it = m_inspectors.find(intent["name"].asString());
	if (it == m_inspectors.end())
		throw Refused("nothing called '" + intent["name"].asString() + "' is being inspected");
	if (const std::optional<std::string> refused = it->second.act(intent["control"].asString(), intent["value"]))
		throw Refused(*refused);
}

} // namespace shell
