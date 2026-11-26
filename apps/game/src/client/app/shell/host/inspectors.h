// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <functional>
#include <json/json.h>
#include <map>
#include <optional>
#include <string>

namespace shell {

/*
	What the debugger's World tab shows of the game's systems while they run:
	each says what it is now (stats, and controls with their values, as the
	page draws them: InspectorState in bridge/types.ts) and is told when a
	control's used. A system adds itself while it's there and removes itself
	as it goes.
*/
class Inspectors
{
public:
	struct Inspector
	{
		std::function<Json::Value()> describe;
		// A control used: why not, if it can't be (a refusal, shown on the page), or nothing
		std::function<std::optional<std::string>(const std::string &control, const Json::Value &value)> act;
	};

	void add(const std::string &name, Inspector inspector);
	void remove(const std::string &name);
	// Each one's description, by name
	Json::Value describe() const;
	// A control used, from the page: { name, control, value }. Refused: no such one.
	void act(const Json::Value &intent);

private:
	std::map<std::string, Inspector> m_inspectors;
};

} // namespace shell
