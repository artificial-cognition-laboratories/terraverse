// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/library/check.h"
#include <cstdlib>
#include <iostream>

namespace tests {
std::ostream &note()
{
	static std::ostream quiet(nullptr); // (a stream with no buffer drops what it's given)
	static const bool loud = std::getenv("TEST_NOTES") != nullptr;
	return loud ? std::cout : quiet;
}
}
