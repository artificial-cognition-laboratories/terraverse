// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/online/local.h"
#include "src/shared/platform/online/ref.h"
#include <json/json.h>
#include <string>

namespace shell {

/*
	The servers terra runs on this machine: a record each in the home's running/
	(apps/cli: src/terra/running), named by port. The page sees what they are, never
	their secrets; joining one hands the secret to the game's connection alone.
*/
class LocalServers
{
public:
	struct Opts {
		std::string dir; // the home's running/
	};

	explicit LocalServers(Opts opts) : m_opts(std::move(opts)) {}

	// Those running now, as the page shows them (bridge LocalServer[]).
	Json::Value list() const;
	// How to join the one at `address`. Throws Refused when nothing of terra's runs there.
	online::LocalJoin join(const online::ref::Address &address) const;

private:
	Opts m_opts;
};

} // namespace shell
