// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/local.h"
#include "src/shared/base/exceptions.h"
#include <cstdlib>

namespace online {

std::optional<LocalRun> LocalRun::fromEnvironment()
{
	const char *secret = std::getenv("TERRA_LOCAL_SECRET");
	if (!secret)
		return std::nullopt;
	// a guessable secret is no gate: terra makes 32 random bytes, hex
	if (std::string(secret).size() < 32)
		throw BaseException("TERRA_LOCAL_SECRET is too short to be a secret");
	const char *owner = std::getenv("TERRA_OWNER");
	return LocalRun{ secret, owner ? owner : "" };
}

std::unique_ptr<Gate> LocalRun::gate(const std::string &build) const
{
	// no ArcLabs: a server for this machine admits by its secret alone
	return std::make_unique<Gate>(Gate::Opts{ Api{}, owner, "", build, secret });
}

} // namespace online
