// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace home {

/*
	A Chromium cache folder this process can have. Chromium takes one folder per process
	(its SingletonLock): a second game of the same profile, beside the first, gets the next
	free one (`base-2`, `base-3`…) rather than failing to start.
*/
std::string freeBrowserCache(const std::string &base);

} // namespace home
