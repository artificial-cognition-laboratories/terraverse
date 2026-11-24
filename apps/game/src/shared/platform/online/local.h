// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/online/gate.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace online {

/*
	A server terra started for this machine (apps/cli: terra serve, terra dev): terra
	hands it the run's secret and whose it is through its environment, never its
	command line (anyone on the machine can read that). The secret is also in the
	run's running/ record, which only the user can read: whoever has it joins.
*/
struct LocalRun
{
	std::string secret;
	std::string owner; // the account terra is signed in as; empty: nobody's

	// The run terra started, or none when this server wasn't started so.
	static std::optional<LocalRun> fromEnvironment();

	// The gate admitting by its secret, from this machine only.
	std::unique_ptr<Gate> gate(const std::string &build) const;
};

// Joining a server terra runs here: its address, and the secret its running/ record holds.
struct LocalJoin
{
	std::string host;
	uint16_t port;
	std::string secret;
	std::string title; // for the messages a failed connection gives
};

} // namespace online
