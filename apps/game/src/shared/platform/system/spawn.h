// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>
#include <vector>

namespace spawn {

// Starts `argv[0]` (a path) with the rest as its arguments, beside this process and not waited
// for: it lives on if this one exits. Inherits the environment. False if it couldn't start.
bool detached(const std::vector<std::string> &argv);

} // namespace spawn
