// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

// What every library's tests include: doctest, and somewhere to say what a
// test measured. Notes are quiet unless TEST_NOTES is set, so a passing run
// prints one line, and a tuning session can still see the numbers.
#pragma once

#include "library/vendors/doctest/doctest.h"
#include <ostream>

namespace tests {
std::ostream &note();
}
using tests::note;
