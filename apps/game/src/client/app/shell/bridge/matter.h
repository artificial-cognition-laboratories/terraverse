// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/matter/matter/matter.h"
#include <json/json.h>
#include <mutex>
#include <optional>

namespace shell {

/*
	The Matter screen's verbs, over the engine's matter tables (matter/*.json,
	loaded on first use and kept): `matter.list` gives every element and
	substance, each substance with its phase curves sampled for drawing;
	`matter.state` says what a substance is at a temperature and pressure;
	`matter.react` steps a mixture the page holds (the Reactor). The page
	draws; the physics is here.
*/
class MatterVerbs
{
public:
	Json::Value list();
	Json::Value state(const Json::Value &intent); // throws Refused
	Json::Value react(const Json::Value &intent); // throws Refused

	// The tables, loaded on first use.
	const matter::Matter &table();

private:
	std::optional<matter::Matter> m_table;
	std::once_flag m_loaded; // loaded by whichever asks first: the bridge's worker or the main thread
};

} // namespace shell
